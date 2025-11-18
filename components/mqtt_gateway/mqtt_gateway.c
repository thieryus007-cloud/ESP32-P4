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

typedef struct {
    event_bus_t *bus;
    esp_mqtt_client_handle_t client;
    bool initialized;
    bool connected;
    bool started;
    char broker[96];
    char topic_pub[96];
    char topic_sub[96];
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
    };
    event_bus_publish(s_state.bus, &evt);
}

static void publish_register_value(const register_descriptor_t *desc, float user_value)
{
    if (!desc || !s_state.connected || !s_state.client) {
        return;
    }

    char topic[192];
    char payload[64];
    snprintf(topic, sizeof(topic), "%s/%s", s_state.topic_pub, desc->key);
    snprintf(payload, sizeof(payload), "%.*f", desc->precision, user_value);

    int msg_id = esp_mqtt_client_publish(s_state.client, topic, payload, 0, 1, 0);
    if (msg_id >= 0) {
        ESP_LOGD(TAG, "Published %s=%s (msg_id=%d)", desc->key, payload, msg_id);
    } else {
        ESP_LOGW(TAG, "Failed to publish %s (msg_id=%d)", desc->key, msg_id);
    }
}

static void publish_cached_registers(void)
{
    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        float user_val = 0.0f;
        if (tinybms_model_get_cached(catalog[i].address, &user_val) == ESP_OK) {
            publish_register_value(&catalog[i], user_val);
        }
    }
}

static void handle_command_json(const char *payload, int len)
{
    if (!payload || len <= 0) {
        return;
    }

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
    if (cJSON_IsString(key) && key->valuestring) {
        desc = tinybms_get_register_by_key(key->valuestring);
    } else if (cJSON_IsNumber(address)) {
        desc = tinybms_get_register_by_address((uint16_t) address->valuedouble);
    }

    if (!desc) {
        ESP_LOGW(TAG, "Unknown register in MQTT command");
        cJSON_Delete(root);
        return;
    }

    bool request_read = cJSON_IsTrue(read);

    if (request_read) {
        float user_val = 0.0f;
        if (tinybms_model_read_register(desc->address, &user_val) == ESP_OK ||
            tinybms_model_get_cached(desc->address, &user_val) == ESP_OK) {
            publish_register_value(desc, user_val);
        }
        cJSON_Delete(root);
        return;
    }

    if (cJSON_IsNumber(value)) {
        float user_val = (float) value->valuedouble;
        esp_err_t ret = tinybms_model_write_register(desc->address, user_val);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MQTT write %s=%.3f", desc->key, user_val);
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
        esp_mqtt_client_subscribe(s_state.client, s_state.topic_sub, 1);
        publish_cached_registers();
        publish_mqtt_status(true, "connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_state.connected = false;
        publish_mqtt_status(false, "disconnected");
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        char buf[256];
        int copy_len = (event->data_len < (int) sizeof(buf) - 1) ? event->data_len : (int) sizeof(buf) - 1;
        memcpy(buf, event->data, copy_len);
        buf[copy_len] = '\0';
        ESP_LOGI(TAG, "MQTT RX topic=%.*s", event->topic_len, event->topic);
        handle_command_json(buf, copy_len);
        break;
    }
    default:
        break;
    }
}

static void rebuild_client(void)
{
    if (s_state.client) {
        esp_mqtt_client_stop(s_state.client);
        esp_mqtt_client_destroy(s_state.client);
        s_state.client = NULL;
    }

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s_state.broker,
        .credentials.client_id = "esp32p4-tinybms",
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

    strlcpy(s_state.broker, config->mqtt_broker, sizeof(s_state.broker));
    strlcpy(s_state.topic_pub, config->mqtt_topic_pub, sizeof(s_state.topic_pub));
    strlcpy(s_state.topic_sub, config->mqtt_topic_sub, sizeof(s_state.topic_sub));

    rebuild_client();
    if (s_state.started) {
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
    if (!req) {
        return;
    }

    hmi_persistent_config_t cfg = *config_manager_get();
    strlcpy(cfg.mqtt_broker, req->config.mqtt_broker, sizeof(cfg.mqtt_broker));
    strlcpy(cfg.mqtt_topic, req->config.mqtt_topic_pub, sizeof(cfg.mqtt_topic));
    config_manager_save(&cfg);

    apply_hmi_config(&req->config);
}

static void sync_task(void *arg)
{
    (void) arg;
    tinybms_model_read_all();
    publish_cached_registers();
    vTaskDelete(NULL);
}

void mqtt_gateway_init(event_bus_t *bus)
{
    if (s_state.initialized) {
        return;
    }

    s_state.bus = bus;
    const hmi_persistent_config_t *cfg = config_manager_get();
    strlcpy(s_state.broker, cfg->mqtt_broker, sizeof(s_state.broker));
    strlcpy(s_state.topic_pub, cfg->mqtt_topic, sizeof(s_state.topic_pub));
    strlcpy(s_state.topic_sub, cfg->mqtt_topic, sizeof(s_state.topic_sub));

    rebuild_client();

    if (bus) {
        event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, on_register_update, NULL);
        event_bus_subscribe(bus, EVENT_USER_INPUT_WRITE_CONFIG, on_user_config, NULL);
    }

    publish_config_snapshot();

    s_state.initialized = true;
    ESP_LOGI(TAG, "MQTT gateway initialized (broker=%s pub=%s sub=%s)",
             s_state.broker, s_state.topic_pub, s_state.topic_sub);
}

void mqtt_gateway_start(void)
{
    if (!s_state.client) {
        return;
    }

    s_state.started = true;
    esp_mqtt_client_start(s_state.client);

    // déclenche une lecture initiale pour peupler les écrans/config
    xTaskCreate(sync_task, "mqtt_sync", 4096, NULL, 4, NULL);
}

void mqtt_gateway_stop(void)
{
    s_state.started = false;
    if (s_state.client) {
        esp_mqtt_client_stop(s_state.client);
    }
    publish_mqtt_status(false, "stopped");
}
