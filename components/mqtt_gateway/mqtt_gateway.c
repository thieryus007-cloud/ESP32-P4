#include "mqtt_gateway.h"

#include "config_manager.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "tinybms_model.h"
#include "tinybms_registers.h"
#include "event_types.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt_gateway";

// [NEW] Constantes pour éviter les nombres magiques
#define MQTT_TOPIC_MAX_LEN      128
#define MQTT_PAYLOAD_MAX_LEN    64
#define MQTT_BROKER_URL_LEN     96
#define MQTT_FULL_TOPIC_LEN     (MQTT_TOPIC_MAX_LEN + 32)

typedef struct {
    event_bus_t *bus;
    esp_mqtt_client_handle_t client;
    bool initialized;
    bool connected;
    bool started; // Indique si l'utilisateur a demandé le start
    char broker[MQTT_BROKER_URL_LEN];
    char topic_pub[MQTT_TOPIC_MAX_LEN];
    char topic_sub[MQTT_TOPIC_MAX_LEN];
} mqtt_gateway_state_t;

static mqtt_gateway_state_t s_state = {0};

static void publish_mqtt_status(bool connected, const char *reason)
{
    if (!s_state.bus) {
        return;
    }

    mqtt_status_event_t status = {
        .enabled = true,
        .connected = connected,
    };
    if (reason) {
        strlcpy(status.reason, reason, sizeof(status.reason));
    }

    event_t evt = {
        .type = EVENT_MQTT_STATUS_UPDATED,
        .data = &status,
        .data_size = sizeof(status),
    };
    event_bus_publish(s_state.bus, &evt);
}

static void publish_register_value(const register_descriptor_t *desc, float user_value)
{
    // [MOD] Vérification stricte de l'état avant tentative
    if (!desc || !s_state.connected || !s_state.client) {
        return;
    }

    char topic[MQTT_FULL_TOPIC_LEN];
    char payload[MQTT_PAYLOAD_MAX_LEN];
    
    // Construction sécurisée du topic
    int len = snprintf(topic, sizeof(topic), "%s/%s", s_state.topic_pub, desc->key);
    if (len >= sizeof(topic)) {
        ESP_LOGW(TAG, "Topic truncated for register %s", desc->key);
        return;
    }

    snprintf(payload, sizeof(payload), "%.*f", desc->precision, user_value);

    // [MOD] Utilisation de QoS 0 par défaut (plus rapide), 
    // mais on pourrait envisager QoS 1 pour les alarmes critiques.
    int msg_id = esp_mqtt_client_publish(s_state.client, topic, payload, 0, 0, 0);
    
    if (msg_id < 0) {
        // Ne pas loguer en erreur systématique pour éviter le flood de logs si déconnecté
        ESP_LOGD(TAG, "Failed to publish %s (err/queue full)", desc->key);
    }
}

static void publish_cached_registers(void)
{
    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    int pub_count = 0;

    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        float user_val = 0.0f;
        if (tinybms_model_get_cached(catalog[i].address, &user_val) == ESP_OK) {
            publish_register_value(&catalog[i], user_val);
            
            // [NEW] Backpressure control:
            // Tous les 10 messages, on rend la main à FreeRTOS/LwIP pour traiter les paquets.
            // Cela évite de saturer le buffer TX et de provoquer des erreurs d'envoi.
            pub_count++;
            if (pub_count % 10 == 0) {
                vTaskDelay(pdMS_TO_TICKS(10)); 
            }
        }
    }
}

static void handle_command_json(const char *payload, int len)
{
    if (!payload || len <= 0) {
        return;
    }

    // [MOD] Protection : cJSON utilise malloc. Sur des payloads corrompus ou énormes,
    // cela peut être risqué. ParseWithLength est bien utilisé ici.
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (!root) {
        ESP_LOGW(TAG, "Invalid MQTT JSON command");
        return;
    }

    const cJSON *key = cJSON_GetObjectItemCaseSensitive(root, "key");
    const cJSON *address = cJSON_GetObjectItemCaseSensitive(root, "address");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
    const cJSON *read = cJSON_GetObjectItemCaseSensitive(root, "read");

    const register_descriptor_t *desc = NULL;
    
    // Priorité à l'adresse (O(1)) plutôt qu'à la clé (O(n)) si disponible
    if (cJSON_IsNumber(address)) {
        desc = tinybms_get_register_by_address((uint16_t) address->valuedouble);
    } else if (cJSON_IsString(key) && key->valuestring) {
        desc = tinybms_get_register_by_key(key->valuestring);
    }

    if (!desc) {
        ESP_LOGW(TAG, "Unknown register in MQTT command");
        cJSON_Delete(root);
        return;
    }

    bool request_read = cJSON_IsTrue(read);

    if (request_read) {
        float user_val = 0.0f;
        // [MOD] Préférer la lecture en cache pour la réactivité immédiate via MQTT
        // sauf si une lecture explicite matériel est requise (ce qui bloquerait le task MQTT).
        // Pour l'instant, on garde la logique existante mais on note le risque de blocage.
        if (tinybms_model_get_cached(desc->address, &user_val) == ESP_OK) {
            publish_register_value(desc, user_val);
        } else if (tinybms_model_read_register(desc->address, &user_val) == ESP_OK) {
             publish_register_value(desc, user_val);
        }
    } else if (cJSON_IsNumber(value)) {
        float user_val = (float) value->valuedouble;
        // [NOTE] model_write_register peut être bloquant (I2C/UART).
        // Idéalement, cela devrait poster un événement sur le bus pour traitement asynchrone.
        esp_err_t ret = tinybms_model_write_register(desc->address, user_val);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MQTT write %s=%.3f", desc->key, user_val);
            // Feedback immédiat de la nouvelle valeur
            publish_register_value(desc, user_val); 
        } else {
            ESP_LOGW(TAG, "MQTT write failed for %s: %s", desc->key, esp_err_to_name(ret));
        }
    }

    cJSON_Delete(root);
}

static void on_mqtt_event(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void) handler_args;
    (void) base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        s_state.connected = true;
        ESP_LOGI(TAG, "MQTT connected to %s", s_state.broker);
        esp_mqtt_client_subscribe(s_state.client, s_state.topic_sub, 1); // QoS 1 pour les commandes
        publish_cached_registers(); // Full sync
        publish_mqtt_status(true, "connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_state.connected = false;
        publish_mqtt_status(false, "disconnected");
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        // [NEW] Filtrage strict du topic pour éviter de parser du bruit
        if (event->topic_len == strlen(s_state.topic_sub) && 
            strncmp(event->topic, s_state.topic_sub, event->topic_len) == 0) {
                
            // Utilisation d'un buffer static si possible ou limité à la stack
            // Attention : MQTT task a souvent une stack limitée (4096 bytes par défaut)
            if (event->data_len < 512) { 
                char *buf = malloc(event->data_len + 1); // Heap allocation safer for large JSON
                if (buf) {
                    memcpy(buf, event->data, event->data_len);
                    buf[event->data_len] = '\0';
                    handle_command_json(buf, event->data_len);
                    free(buf);
                } else {
                    ESP_LOGE(TAG, "OOM processing MQTT JSON");
                }
            } else {
                ESP_LOGW(TAG, "MQTT JSON payload too large, ignored");
            }
        }
        break;
    }
    default:
        break;
    }
}

static void rebuild_client(void)
{
    // [MOD] Séquence de destruction sécurisée
    if (s_state.client) {
        esp_mqtt_client_stop(s_state.client);
        // Un délai ou un check d'état serait idéal ici, mais destroy nettoie généralement bien.
        esp_mqtt_client_destroy(s_state.client);
        s_state.client = NULL;
        s_state.connected = false;
    }

    // Vérification de la validité de l'URI avant init
    if (strlen(s_state.broker) == 0) {
        ESP_LOGW(TAG, "MQTT Broker URL is empty, skipping init");
        return;
    }

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_state.broker,
        .credentials.client_id = "esp32p4-tinybms",
        .session.keepalive = 60, // Standard keepalive
        // [NEW] Augmenter la taille du buffer TX si on envoie beaucoup de données
        .buffer.size = 1024, 
    };

    s_state.client = esp_mqtt_client_init(&cfg);
    if (!s_state.client) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_state.client, ESP_EVENT_ANY_ID, on_mqtt_event, NULL);
}

static void apply_hmi_config(const hmi_config_t *config)
{
    if (!config) {
        return;
    }

    // Vérifier si changement réel avant de reconstruire (optimisation)
    bool changed = (strncmp(s_state.broker, config->mqtt_broker, MQTT_BROKER_URL_LEN) != 0) ||
                   (strncmp(s_state.topic_pub, config->mqtt_topic_pub, MQTT_TOPIC_MAX_LEN) != 0);

    if (!changed && s_state.client) {
        return;
    }

    strlcpy(s_state.broker, config->mqtt_broker, sizeof(s_state.broker));
    strlcpy(s_state.topic_pub, config->mqtt_topic_pub, sizeof(s_state.topic_pub));
    strlcpy(s_state.topic_sub, config->mqtt_topic_sub, sizeof(s_state.topic_sub));

    rebuild_client();
    if (s_state.started && s_state.client) {
        esp_mqtt_client_start(s_state.client);
    }
}

static void publish_config_snapshot(void)
{
    if (!s_state.bus) {
        return;
    }

    hmi_config_t cfg = {0};
    const hmi_persistent_config_t *persist = config_manager_get();
    if (persist) {
        strlcpy(cfg.mqtt_broker, persist->mqtt_broker, sizeof(cfg.mqtt_broker));
        strlcpy(cfg.mqtt_topic_pub, persist->mqtt_topic, sizeof(cfg.mqtt_topic_pub));
        strlcpy(cfg.mqtt_topic_sub, persist->mqtt_topic, sizeof(cfg.mqtt_topic_sub));
    }

    event_t evt = {
        .type = EVENT_CONFIG_UPDATED,
        .data = &cfg,
        .data_size = sizeof(cfg),
    };
    event_bus_publish(s_state.bus, &evt);
}

static void on_register_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_register_update_t *update = (const tinybms_register_update_t *) event->data;
    // Utilisation directe de l'adresse pour éviter lookup par string (lent)
    const register_descriptor_t *desc = tinybms_get_register_by_address(update->address);
    publish_register_value(desc, update->user_value);
}

static void on_user_config(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const user_input_write_config_t *req = (const user_input_write_config_t *) event->data;
    // [MOD] Ajout protection pointeur req
    if (!req) return;

    hmi_persistent_config_t cfg = *config_manager_get();
    strlcpy(cfg.mqtt_broker, req->config.mqtt_broker, sizeof(cfg.mqtt_broker));
    strlcpy(cfg.mqtt_topic, req->config.mqtt_topic_pub, sizeof(cfg.mqtt_topic));
    config_manager_save(&cfg);

    apply_hmi_config(&req->config);
}

static void sync_task(void *arg)
{
    (void) arg;
    // Cette lecture peut être lente (I2C/UART), c'est bien qu'elle soit dans une tâche dédiée
    tinybms_model_read_all();
    publish_cached_registers();
    
    // La tache s'autodétruit, c'est propre.
    vTaskDelete(NULL);
}

void mqtt_gateway_init(event_bus_t *bus)
{
    if (s_state.initialized) {
        return;
    }

    s_state.bus = bus;
    const hmi_persistent_config_t *cfg = config_manager_get();
    
    // Initialisation sécurisée des buffers
    if (cfg) {
        strlcpy(s_state.broker, cfg->mqtt_broker, sizeof(s_state.broker));
        strlcpy(s_state.topic_pub, cfg->mqtt_topic, sizeof(s_state.topic_pub));
        strlcpy(s_state.topic_sub, cfg->mqtt_topic, sizeof(s_state.topic_sub));
    }

    rebuild_client();

    if (bus) {
        event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, on_register_update, NULL);
        event_bus_subscribe(bus, EVENT_USER_INPUT_WRITE_CONFIG, on_user_config, NULL);
    }

    publish_config_snapshot();

    s_state.initialized = true;
    ESP_LOGI(TAG, "MQTT gateway initialized");
}

void mqtt_gateway_start(void)
{
    if (!s_state.client) {
        // Tentative de reconstruction si jamais init a échoué faute de config
        rebuild_client();
        if (!s_state.client) return;
    }

    if (!s_state.started) {
        s_state.started = true;
        esp_mqtt_client_start(s_state.client);
        
        // Lancement de la tâche de sync avec priorité basse/moyenne
        // Stack de 4096 est confortable pour ESP32-P4
        xTaskCreate(sync_task, "mqtt_sync", 4096, NULL, 5, NULL);
    }
}

void mqtt_gateway_stop(void)
{
    s_state.started = false;
    if (s_state.client) {
        esp_mqtt_client_stop(s_state.client);
    }
    publish_mqtt_status(false, "stopped");
}
