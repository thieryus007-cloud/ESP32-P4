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
#include <string.h>
#include <stdlib.h>

#include "event_bus.h"
#include "event_types.h"
#include "remote_event_adapter.h"

static const char *TAG = "NET_CLIENT";

// WiFi event bits
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

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
};

// Config par défaut via menuconfig
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

typedef enum {
    WS_CHANNEL_TELEMETRY = 0,
    WS_CHANNEL_EVENTS    = 1,
    WS_CHANNEL_ALERTS    = 2,
} ws_channel_t;

// --- WiFi station ---

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying WiFi connection (%d)...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "WiFi disconnected");
        s_net_status.wifi_connected = false;
        s_net_status.server_reachable = false;
        publish_system_status();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_net_status.wifi_connected = true;
        publish_system_status();
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id;
    esp_event_handler_instance_t got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &got_ip));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *) wifi_config.sta.ssid, CONFIG_HMI_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *) wifi_config.sta.password, CONFIG_HMI_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init STA finished. SSID:%s", CONFIG_HMI_WIFI_SSID);

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID:%s", CONFIG_HMI_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", CONFIG_HMI_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
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
            s_net_status.server_reachable = true;
            publish_system_status();
            remote_event_adapter_on_network_online();
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected (channel=%d)", channel);
            s_net_status.server_reachable = false;
            publish_system_status();
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == WS_TEXT_FRAME && data->data_len > 0) {
                // data->data n'est pas null-terminated, on copie
                char *payload = (char *) malloc(data->data_len + 1);
                if (!payload) {
                    ESP_LOGE(TAG, "Failed to alloc payload buffer");
                    return;
                }
                memcpy(payload, data->data_ptr, data->data_len);
                payload[data->data_len] = '\0';

                if (channel == WS_CHANNEL_TELEMETRY) {
                    remote_event_adapter_on_telemetry_json(payload, data->data_len);
                } else if (channel == WS_CHANNEL_EVENTS) {
                    remote_event_adapter_on_event_json(payload, data->data_len);
                } else if (channel == WS_CHANNEL_ALERTS) {
                    remote_event_adapter_on_alerts_json(payload, data->data_len);
                }

                free(payload);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error (channel=%d)", channel);
            break;
        default:
            break;
    }
}

static void websocket_start(void)
{
    char uri[128];

    // --- Telemetry WS ---
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/telemetry",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);

    esp_websocket_client_config_t ws_cfg_telemetry = {
        .uri        = uri,
        .buffer_size = 4096,
    };

    s_ws_telemetry = esp_websocket_client_init(&ws_cfg_telemetry);
    ESP_ERROR_CHECK(esp_websocket_register_events(
        s_ws_telemetry,
        WEBSOCKET_EVENT_ANY,
        websocket_event_handler,
        (void *) (intptr_t) WS_CHANNEL_TELEMETRY));

    ESP_LOGI(TAG, "Connecting WebSocket telemetry: %s", uri);
    ESP_ERROR_CHECK(esp_websocket_client_start(s_ws_telemetry));

    // --- Events WS ---
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/events",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);

    esp_websocket_client_config_t ws_cfg_events = {
        .uri        = uri,
        .buffer_size = 4096,
    };

    s_ws_events = esp_websocket_client_init(&ws_cfg_events);
    ESP_ERROR_CHECK(esp_websocket_register_events(
        s_ws_events,
        WEBSOCKET_EVENT_ANY,
        websocket_event_handler,
        (void *) (intptr_t) WS_CHANNEL_EVENTS));

    ESP_LOGI(TAG, "Connecting WebSocket events: %s", uri);
    ESP_ERROR_CHECK(esp_websocket_client_start(s_ws_events));

    // --- Alerts WS ---
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/alerts",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);

    esp_websocket_client_config_t ws_cfg_alerts = {
        .uri        = uri,
        .buffer_size = 4096,
    };

    s_ws_alerts = esp_websocket_client_init(&ws_cfg_alerts);
    ESP_ERROR_CHECK(esp_websocket_register_events(
        s_ws_alerts,
        WEBSOCKET_EVENT_ANY,
        websocket_event_handler,
        (void *) (intptr_t) WS_CHANNEL_ALERTS));

    ESP_LOGI(TAG, "Connecting WebSocket alerts: %s", uri);
    ESP_ERROR_CHECK(esp_websocket_client_start(s_ws_alerts));
}

static void publish_request_started(const char *path, const char *method)
{
    if (!s_bus || !path || !method) {
        return;
    }

    network_request_t req = { 0 };
    strncpy(req.path, path, sizeof(req.path) - 1);
    strncpy(req.method, method, sizeof(req.method) - 1);

    event_t evt = {
        .type = EVENT_NETWORK_REQUEST_STARTED,
        .data = &req,
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_request_finished(const char *path,
                                     const char *method,
                                     bool success,
                                     int status)
{
    if (!s_bus || !path || !method) {
        return;
    }

    network_request_status_t info = {
        .success = success,
        .status = status,
    };
    strncpy(info.request.path, path, sizeof(info.request.path) - 1);
    strncpy(info.request.method, method, sizeof(info.request.method) - 1);

    event_t evt = {
        .type = EVENT_NETWORK_REQUEST_FINISHED,
        .data = &info,
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_system_status(void)
{
    if (!s_bus) {
        return;
    }

    s_net_status.has_error = (!s_net_status.wifi_connected || !s_net_status.server_reachable);

    event_t evt = {
        .type = EVENT_SYSTEM_STATUS_UPDATED,
        .data = &s_net_status,
    };
    event_bus_publish(s_bus, &evt);
}

// --- API publique ---

void net_client_init(event_bus_t *bus)
{
    s_bus = bus;
    ESP_LOGI(TAG, "net_client initialized (bridge host=%s port=%d)",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT);
}

void net_client_start(void)
{
    ESP_LOGI(TAG, "Starting net_client: WiFi + WebSockets");
    wifi_init_sta();
    websocket_start();
}

bool net_client_send_command_ws(const char *data, size_t len)
{
    if (!s_ws_events || !esp_websocket_client_is_connected(s_ws_events)) {
        ESP_LOGW(TAG, "WS events not connected, cannot send command");
        return false;
    }

    int res = esp_websocket_client_send_text(s_ws_events, data, len, pdMS_TO_TICKS(1000));
    if (res < 0) {
        ESP_LOGE(TAG, "Failed to send WS command");
        return false;
    }
    return true;
}

bool net_client_send_http_request(const char *path,
                                  const char *method,
                                  const char *body,
                                  size_t body_len)
{
    if (!path || !method) {
        return false;
    }

    publish_request_started(path, method);

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s",
             CONFIG_HMI_BRIDGE_HOST, CONFIG_HMI_BRIDGE_PORT, path);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        publish_request_finished(path, method, false, -1);
        return false;
    }

    esp_http_client_set_method(client,
        strcmp(method, "POST") == 0 ? HTTP_METHOD_POST :
        strcmp(method, "PUT")  == 0 ? HTTP_METHOD_PUT  :
        HTTP_METHOD_GET);

    if (body && body_len > 0) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, body_len);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %s %s -> status=%d", method, path, status);
        char *resp_buf = NULL;
        int   resp_len = 0;

        int content_length = esp_http_client_get_content_length(client);
        int buffer_size    = (content_length > 0 && content_length < 4096) ? (content_length + 1) : 1024;

        resp_buf = (char *) malloc(buffer_size);
        if (resp_buf) {
            resp_len = esp_http_client_read_response(client, resp_buf, buffer_size - 1);
            if (resp_len < 0) {
                resp_len = 0;
            }
            resp_buf[resp_len] = '\0';
        }

        remote_event_adapter_on_http_response(path, method, status, resp_buf);

        publish_request_finished(path, method,
                                 (status >= 200 && status < 300), status);

        if (resp_buf) {
            free(resp_buf);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        publish_request_finished(path, method, false, -1);
        return false;
    }

    esp_http_client_cleanup(client);
    return true;
}
