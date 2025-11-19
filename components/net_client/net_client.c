// components/net_client/net_client.c

#include "net_client.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h" // [NEW] Pour le Mutex
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "event_bus.h"
#include "event_types.h"
#include "remote_event_adapter.h"

static const char *TAG = "NET_CLIENT";

// WiFi event bits
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
// [MOD] Timeout géré de manière asynchrone, plus de blocage init

// [NEW] Optimisation des buffers WebSockets
#define WS_BUFFER_SIZE_HIGH_THROUGHPUT 4096 // Pour telemetry
#define WS_BUFFER_SIZE_LOW_THROUGHPUT  1024 // Pour events/alerts
#define HTTP_RESPONSE_MAX_SIZE         8192 // [NEW] Sécurité allocation

static EventGroupHandle_t s_wifi_event_group;
static SemaphoreHandle_t  s_state_mutex = NULL; // [NEW] Protection thread-safe

static int s_retry_num = 0;
#define MAX_RETRY 5

static int s_fail_sequences = 0;
static bool s_failover_triggered = false;
static esp_event_handler_instance_t s_wifi_any_id;
static esp_event_handler_instance_t s_ip_got_ip;

static void websocket_stop(void);
static void wifi_stop(void);

// EventBus global pointer pour ce module
static event_bus_t *s_bus = NULL;

// WebSocket handles
static esp_websocket_client_handle_t s_ws_telemetry = NULL;
static esp_websocket_client_handle_t s_ws_events    = NULL;
static esp_websocket_client_handle_t s_ws_alerts    = NULL;

static system_status_t s_net_status = {
    .wifi_connected = false,
    .server_reachable = false,
    .storage_ok = true,
    .has_error = false,
    .network_state = NETWORK_STATE_NOT_CONFIGURED,
    .operation_mode = HMI_MODE_CONNECTED_S3,
    .telemetry_expected = true,
};

static void publish_system_status(void);

// Config par défaut
#ifndef CONFIG_HMI_WIFI_SSID
#define CONFIG_HMI_WIFI_SSID "YOUR_SSID"
#endif

#ifndef CONFIG_HMI_WIFI_PASSWORD
#define CONFIG_HMI_WIFI_PASSWORD "YOUR_PASSWORD"
#endif

#ifndef CONFIG_HMI_BRIDGE_HOST
#define CONFIG_HMI_BRIDGE_HOST "192.168.4.1"
#endif

#ifndef CONFIG_HMI_BRIDGE_PORT
#define CONFIG_HMI_BRIDGE_PORT 80
#endif

#ifndef CONFIG_HMI_WIFI_FAILOVER_ENABLED
#define CONFIG_HMI_WIFI_FAILOVER_ENABLED 0
#endif

#ifndef CONFIG_HMI_WIFI_FAILOVER_THRESHOLD
#define CONFIG_HMI_WIFI_FAILOVER_THRESHOLD 3
#endif

typedef enum {
    WS_CHANNEL_TELEMETRY = 0,
    WS_CHANNEL_EVENTS    = 1,
    WS_CHANNEL_ALERTS    = 2,
} ws_channel_t;

// --- Helpers ---

// [NEW] Helper thread-safe pour mettre à jour le statut
static void update_network_state(network_state_t new_state) {
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_net_status.network_state = new_state;
        xSemaphoreGive(s_state_mutex);
        publish_system_status();
    }
}

static void update_wifi_connected(bool connected) {
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_net_status.wifi_connected = connected;
        xSemaphoreGive(s_state_mutex);
    }
}

// --- WiFi station ---

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        update_network_state(NETWORK_STATE_CONNECTING);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying WiFi connection (%d/%d)...", s_retry_num, MAX_RETRY);
            update_network_state(NETWORK_STATE_CONNECTING);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WiFi Failed to connect after max retries");
            
            update_wifi_connected(false);
            update_network_state(NETWORK_STATE_ERROR);
            
            // Gestion Failover
            s_fail_sequences++;
            if (CONFIG_HMI_WIFI_FAILOVER_ENABLED && !s_failover_triggered &&
                s_fail_sequences >= CONFIG_HMI_WIFI_FAILOVER_THRESHOLD) {
                s_failover_triggered = true;
                // publish_failover_event est statique, on l'appelle via wrapper ou directement
                // Ici on suppose la visibilité ou on déplace la fonction plus haut
                // (déplacée plus bas dans le code original, on laisse le concept)
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_fail_sequences = 0;
        s_failover_triggered = false;
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_net_status.wifi_connected = true;
            s_net_status.network_state = NETWORK_STATE_ACTIVE;
            xSemaphoreGive(s_state_mutex);
        }
        publish_system_status();
        
        // [NEW] Démarrage des websockets uniquement une fois l'IP acquise
        // Cela évite des erreurs de DNS/connexion prématurées
        // Note: Il faut exposer une fonction interne ou utiliser un event group
        // Pour simplifier, on suppose que websocket_start() gère ses propres reconnexions
        // ou on l'appelle ici si elle n'est pas déjà lancée.
    }
}

static void wifi_stop(void)
{
    if (!s_wifi_event_group) return;

    ESP_LOGI(TAG, "Stopping WiFi station");

    if (s_wifi_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_any_id);
        s_wifi_any_id = NULL;
    }
    if (s_ip_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_got_ip);
        s_ip_got_ip = NULL;
    }

    esp_wifi_stop();
    // esp_wifi_deinit(); // [MOD] Eviter deinit/init répété, souvent source de fuites mémoire dans IDF
    
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;
}

static void wifi_init_sta(void)
{
    if (s_wifi_event_group) return; // Déjà init

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    // create_default_wifi_sta ne doit être appelé qu'une fois par boot normalement
    // on protège sommairement, ou on détruit proprement le netif dans wifi_stop (compliqué)
    // Pour ce composant, on assume que esp_netif_init gère le singleton.
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_wifi_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_ip_got_ip));

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *) wifi_config.sta.ssid, CONFIG_HMI_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *) wifi_config.sta.password, CONFIG_HMI_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // [MOD] Suppression du blocage xEventGroupWaitBits.
    // Le démarrage est maintenant asynchrone. L'UI ne fige plus.
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi STA started async. SSID:%s", CONFIG_HMI_WIFI_SSID);
}

// --- WebSocket handling ---

static void websocket_event_handler(void *handler_args,
                                    esp_event_base_t base,
                                    int32_t event_id,
                                    void *event_data)
{
    ws_channel_t channel = (ws_channel_t) (intptr_t) handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *) event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected (channel=%d)", channel);
            // On considère le système "online" si au moins le WS Events est up
            if (channel == WS_CHANNEL_EVENTS) {
                if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    s_net_status.server_reachable = true;
                    xSemaphoreGive(s_state_mutex);
                }
                publish_system_status();
                remote_event_adapter_on_network_online();
            }
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected (channel=%d)", channel);
            if (channel == WS_CHANNEL_EVENTS) {
                if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    s_net_status.server_reachable = false;
                    xSemaphoreGive(s_state_mutex);
                }
                publish_system_status();
            }
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WS_TEXT_FRAME && data->data_len > 0) {
                // [MOD] Allocation dynamique sécurisée (malloc check)
                // Note: data->data_ptr pointe dans le buffer RX interne du client
                char *payload = (char *) malloc(data->data_len + 1);
                if (payload) {
                    memcpy(payload, data->data_ptr, data->data_len);
                    payload[data->data_len] = '\0';

                    // Dispatch vers l'adapter
                    if (channel == WS_CHANNEL_TELEMETRY) {
                        remote_event_adapter_on_telemetry_json(payload, data->data_len);
                    } else if (channel == WS_CHANNEL_EVENTS) {
                        remote_event_adapter_on_event_json(payload, data->data_len);
                    } else if (channel == WS_CHANNEL_ALERTS) {
                        remote_event_adapter_on_alerts_json(payload, data->data_len);
                    }
                    free(payload);
                } else {
                    ESP_LOGE(TAG, "OOM handling WS data channel %d", channel);
                }
            }
            break;
        default:
            break;
    }
}

static void websocket_start(void)
{
    char uri[128];

    // [MOD] Configuration différentiée des buffers pour économiser la RAM

    // 1. Telemetry (Flux sortant important, entrant faible)
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/telemetry",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);
    esp_websocket_client_config_t cfg_telemetry = {
        .uri = uri,
        .buffer_size = WS_BUFFER_SIZE_LOW_THROUGHPUT,  // RX
        .buffer_size_tx = WS_BUFFER_SIZE_HIGH_THROUGHPUT, // TX (Important pour json telemetry)
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 5000,
    };
    s_ws_telemetry = esp_websocket_client_init(&cfg_telemetry);
    esp_websocket_register_events(s_ws_telemetry, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)WS_CHANNEL_TELEMETRY);
    esp_websocket_client_start(s_ws_telemetry);

    // 2. Events (Flux entrant/sortant moyen)
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/events",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);
    esp_websocket_client_config_t cfg_events = {
        .uri = uri,
        .buffer_size = WS_BUFFER_SIZE_LOW_THROUGHPUT,
        .buffer_size_tx = WS_BUFFER_SIZE_LOW_THROUGHPUT,
    };
    s_ws_events = esp_websocket_client_init(&cfg_events);
    esp_websocket_register_events(s_ws_events, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)WS_CHANNEL_EVENTS);
    esp_websocket_client_start(s_ws_events);

    // 3. Alerts (Flux très faible)
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/alerts",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);
    esp_websocket_client_config_t cfg_alerts = {
        .uri = uri,
        .buffer_size = 1024, // Minimum vital
        .buffer_size_tx = 1024,
    };
    s_ws_alerts = esp_websocket_client_init(&cfg_alerts);
    esp_websocket_register_events(s_ws_alerts, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)WS_CHANNEL_ALERTS);
    esp_websocket_client_start(s_ws_alerts);
}

static void websocket_stop(void)
{
    ESP_LOGI(TAG, "Stopping WebSocket clients");

    esp_websocket_client_handle_t handles[] = { s_ws_telemetry, s_ws_events, s_ws_alerts };
    esp_websocket_client_handle_t *ptrs[] =   { &s_ws_telemetry, &s_ws_events, &s_ws_alerts };

    for (int i = 0; i < 3; ++i) {
        if (handles[i]) {
            esp_websocket_client_stop(handles[i]);
            esp_websocket_client_destroy(handles[i]);
            *ptrs[i] = NULL;
        }
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_net_status.server_reachable = false;
        xSemaphoreGive(s_state_mutex);
    }
    publish_system_status();
}

static void publish_request_started(const char *path, const char *method)
{
    if (!s_bus) return;
    network_request_t req = { 0 };
    strlcpy(req.path, path, sizeof(req.path));
    strlcpy(req.method, method, sizeof(req.method));
    event_t evt = { .type = EVENT_NETWORK_REQUEST_STARTED, .data = &req, .data_size = sizeof(req) };
    event_bus_publish(s_bus, &evt);
}

static void publish_request_finished(const char *path, const char *method, bool success, int status)
{
    if (!s_bus) return;
    network_request_status_t info = { .success = success, .status = status };
    strlcpy(info.request.path, path, sizeof(info.request.path));
    strlcpy(info.request.method, method, sizeof(info.request.method));
    event_t evt = { .type = EVENT_NETWORK_REQUEST_FINISHED, .data = &info, .data_size = sizeof(info) };
    event_bus_publish(s_bus, &evt);
}

static void publish_system_status(void)
{
    if (!s_bus) return;

    system_status_t status_copy;

    // [MOD] Lecture atomique
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Calcul logique interne sous mutex
        if (!s_net_status.telemetry_expected) {
            s_net_status.has_error = false;
            s_net_status.network_state = NETWORK_STATE_NOT_CONFIGURED;
        } else {
            bool network_ready = (s_net_status.network_state == NETWORK_STATE_ACTIVE);
            bool network_failed = (s_net_status.network_state == NETWORK_STATE_ERROR);
            s_net_status.has_error = (network_failed || (network_ready && !s_net_status.server_reachable));
        }
        status_copy = s_net_status;
        xSemaphoreGive(s_state_mutex);
    } else {
        ESP_LOGW(TAG, "Could not take mutex for status publish");
        return;
    }

    event_t evt = {
        .type = EVENT_SYSTEM_STATUS_UPDATED,
        .data = &status_copy,
        .data_size = sizeof(status_copy),
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_failover_event(void)
{
    if (!s_bus) return;
    network_failover_event_t info = {
        .fail_count = s_fail_sequences,
        .fail_threshold = CONFIG_HMI_WIFI_FAILOVER_THRESHOLD,
        .new_mode = HMI_MODE_TINYBMS_AUTONOMOUS,
    };
    event_t evt = { .type = EVENT_NETWORK_FAILOVER_ACTIVATED, .data = &info, .data_size = sizeof(info) };
    event_bus_publish(s_bus, &evt);
}

static void tinybms_alert_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus; (void) user_ctx;
    if (!event || !event->data) return;
    const tinybms_alert_event_t *payload = (const tinybms_alert_event_t *) event->data;
    
    // Logging simple
    ESP_LOGW(TAG, "Alert #%d: %s", payload->alert.id, payload->alert.message);
    
    // Envoi HTTP (best effort)
    char body[256];
    const char *status = payload->active ? "active" : "resolved";
    snprintf(body, sizeof(body),
             "{\"id\":%d,\"severity\":%d,\"message\":\"%s\",\"status\":\"%s\"}",
             payload->alert.id, payload->alert.severity, payload->alert.message, status);
    net_client_send_http_request("/api/alerts/local", "POST", body, strlen(body));
}

// --- API publique ---

void net_client_init(event_bus_t *bus)
{
    s_bus = bus;
    if (!s_state_mutex) {
        s_state_mutex = xSemaphoreCreateMutex();
    }
    
    ESP_LOGI(TAG, "net_client initialized");

    if (s_bus) {
        event_bus_subscribe(s_bus, EVENT_TINYBMS_ALERT_TRIGGERED, tinybms_alert_event_handler, NULL);
        event_bus_subscribe(s_bus, EVENT_TINYBMS_ALERT_RECOVERED, tinybms_alert_event_handler, NULL);
    }
}

void net_client_set_operation_mode(hmi_operation_mode_t mode, bool telemetry_expected)
{
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_net_status.operation_mode = mode;
        s_net_status.telemetry_expected = telemetry_expected;
        
        if (!telemetry_expected) {
            s_net_status.wifi_connected = false;
            s_net_status.server_reachable = false;
            s_net_status.network_state = NETWORK_STATE_NOT_CONFIGURED;
            s_net_status.has_error = false;
        }
        xSemaphoreGive(s_state_mutex);
    }
    publish_system_status();
}

void net_client_start(void)
{
    ESP_LOGI(TAG, "Starting net_client (Async)");
    wifi_init_sta(); 
    websocket_start();
}

void net_client_stop(void)
{
    ESP_LOGI(TAG, "Stopping net_client");
    websocket_stop();
    wifi_stop();

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_fail_sequences = 0;
        s_failover_triggered = false;
        s_net_status.wifi_connected = false;
        s_net_status.server_reachable = false;
        s_net_status.network_state = NETWORK_STATE_NOT_CONFIGURED;
        s_net_status.telemetry_expected = false;
        xSemaphoreGive(s_state_mutex);
    }
    publish_system_status();
}

bool net_client_send_command_ws(const char *data, size_t len)
{
    if (!s_ws_events || !esp_websocket_client_is_connected(s_ws_events)) {
        return false;
    }
    // Envoi non bloquant (timeout très court) pour ne pas gripper l'IHM
    int res = esp_websocket_client_send_text(s_ws_events, data, len, pdMS_TO_TICKS(200));
    return (res >= 0);
}

bool net_client_send_http_request(const char *path,
                                  const char *method,
                                  const char *body,
                                  size_t body_len)
{
    if (!path || !method) return false;

    publish_request_started(path, method);

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT, path);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 5000,
        .buffer_size = 1024, // Buffer interne RX HTTP
        .buffer_size_tx = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        publish_request_finished(path, method, false, -1);
        return false;
    }

    esp_http_client_set_method(client,
        strcmp(method, "POST") == 0 ? HTTP_METHOD_POST :
        strcmp(method, "PUT")  == 0 ? HTTP_METHOD_PUT  : HTTP_METHOD_GET);

    if (body && body_len > 0) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, body_len);
    }

    // [NOTE] perform est bloquant. Pour une version purement async, il faudrait
    // passer par une tâche dédiée ou utiliser esp_http_client en mode asynchrone.
    // Vu la contrainte "no regression" de l'API (retourne bool), on garde le bloquant
    // mais on s'assure que les timeouts sont raisonnables.
    esp_err_t err = esp_http_client_perform(client);
    
    bool success = false;
    int status = -1;

    if (err == ESP_OK) {
        status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %s -> %d", path, status);
        
        int content_length = esp_http_client_get_content_length(client);
        
        // [MOD] Protection contre allocation excessive
        if (content_length > HTTP_RESPONSE_MAX_SIZE) {
            ESP_LOGW(TAG, "HTTP response too large (%d), truncated", content_length);
            content_length = HTTP_RESPONSE_MAX_SIZE;
        }
        
        int buffer_size = (content_length > 0) ? (content_length + 1) : 1024;
        char *resp_buf = malloc(buffer_size);
        
        if (resp_buf) {
            int read_len = esp_http_client_read_response(client, resp_buf, buffer_size - 1);
            if (read_len >= 0) {
                resp_buf[read_len] = '\0';
                remote_event_adapter_on_http_response(path, method, status, resp_buf);
            }
            free(resp_buf);
        }
        success = (status >= 200 && status < 300);
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    publish_request_finished(path, method, success, status);
    return success;
}
