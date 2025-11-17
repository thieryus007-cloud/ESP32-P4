// components/remote_event_adapter/remote_event_adapter.c

#include "remote_event_adapter.h"

#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#include "event_types.h"
#include "event_bus.h"
#include "net_client.h"

static const char *TAG = "REMOTE_ADAPTER";

static event_bus_t *s_bus = NULL;

// Configuration locale (snapshot /api/config + /api/mqtt/config)
static hmi_config_t s_config = {
    .wifi_ssid     = CONFIG_HMI_WIFI_SSID,
    .wifi_password = CONFIG_HMI_WIFI_PASSWORD,
    .static_ip     = "",
    .mqtt_broker   = CONFIG_HMI_BRIDGE_HOST,
    .mqtt_topic_pub = "",
    .mqtt_topic_sub = "",
    .can_bitrate    = 500000,
    .uart_baudrate  = 115200,
    .uart_parity    = "N",
};

static hmi_config_t s_pending_config;
static bool         s_has_pending_config = false;

// Buffers statiques : évite malloc/free à chaque message
static battery_status_t s_batt_status;
static system_status_t  s_sys_status;
static pack_stats_t     s_pack_stats;

// --- Helpers JSON ---

static float json_get_number(cJSON *obj, const char *key, float def)
{
    if (!obj || !key) return def;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return (float) item->valuedouble;
    }
    return def;
}

static bool json_get_bool(cJSON *obj, const char *key, bool def)
{
    if (!obj || !key) return def;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return def;
}

static int json_get_event_id(cJSON *obj, const char *key)
{
    if (!obj || !key) return -1;
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    if (cJSON_IsString(item) && item->valuestring) {
        const char *s = item->valuestring;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
            s++;
        }
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            return (int) strtol(s + 2, NULL, 16);
        }
        return atoi(s);
    }
    return -1;
}

static void set_string_field(char *dst, size_t dst_size, cJSON *root, const char *key)
{
    if (!dst || dst_size == 0 || !root || !key) {
        return;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dst, item->valuestring, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
}

static void publish_config_update(void)
{
    event_t evt = {
        .type = EVENT_CONFIG_UPDATED,
        .data = &s_config,
    };
    event_bus_publish(s_bus, &evt);
}

static void parse_main_config_json(cJSON *root);
static void parse_mqtt_config_json(cJSON *root);

static void handle_user_reload_config(event_bus_t *bus,
                                      const event_t *event,
                                      void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    const user_input_reload_config_t *req = (const user_input_reload_config_t *) event->data;
    bool include_mqtt = true;
    if (req) {
        include_mqtt = req->include_mqtt;
    }

    net_client_send_http_request("/api/config", "GET", NULL, 0);
    if (include_mqtt) {
        net_client_send_http_request("/api/mqtt/config", "GET", NULL, 0);
    }
}

static void handle_user_write_config(event_bus_t *bus,
                                     const event_t *event,
                                     void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    const user_input_write_config_t *req = (const user_input_write_config_t *) event->data;
    if (!req) {
        return;
    }

    s_pending_config = req->config;
    s_has_pending_config = true;

    char body_config[256];
    snprintf(body_config, sizeof(body_config),
             "{\"wifi_ssid\":\"%s\",\"wifi_password\":\"%s\",\"static_ip\":\"%s\",\"can_bitrate\":%d,\"uart_baudrate\":%d,\"uart_parity\":\"%s\"}",
             req->config.wifi_ssid,
             req->config.wifi_password,
             req->config.static_ip,
             req->config.can_bitrate,
             req->config.uart_baudrate,
             req->config.uart_parity);

    net_client_send_http_request("/api/config", "POST", body_config, strlen(body_config));

    char body_mqtt[256];
    snprintf(body_mqtt, sizeof(body_mqtt),
             "{\"mqtt_broker\":\"%s\",\"mqtt_topic_pub\":\"%s\",\"mqtt_topic_sub\":\"%s\"}",
             req->config.mqtt_broker,
             req->config.mqtt_topic_pub,
             req->config.mqtt_topic_sub);

    if (!req->mqtt_only) {
        net_client_send_http_request("/api/mqtt/config", "POST", body_mqtt, strlen(body_mqtt));
    }
}

// --- API publique ---

void remote_event_adapter_init(event_bus_t *bus)
{
    s_bus = bus;
    memset(&s_batt_status, 0, sizeof(s_batt_status));
    memset(&s_sys_status,  0, sizeof(s_sys_status));
    memset(&s_pack_stats,  0, sizeof(s_pack_stats));

    // valeurs par défaut
    s_batt_status.mqtt_ok        = false;  // sera piloté par MQTT status
    s_sys_status.wifi_connected   = false;
    s_sys_status.server_reachable = false;
    s_sys_status.storage_ok       = false;
    s_sys_status.has_error        = false;

    ESP_LOGI(TAG, "remote_event_adapter initialized");

    event_bus_subscribe(bus, EVENT_USER_INPUT_WRITE_CONFIG, handle_user_write_config, NULL);
    event_bus_subscribe(bus, EVENT_USER_INPUT_RELOAD_CONFIG, handle_user_reload_config, NULL);
}

void remote_event_adapter_start(void)
{
    ESP_LOGI(TAG, "remote_event_adapter start (no separate task)");
}

// ============================================================================
//  TELEMETRY  (/ws/telemetry)  → battery_status_t + pack_stats_t
// ============================================================================

void remote_event_adapter_on_telemetry_json(const char *json, size_t length)
{
    (void) length;
    if (!s_bus || !json) {
        return;
    }

    ESP_LOGD(TAG, "Telemetry JSON: %s", json);

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse telemetry JSON");
        return;
    }

    // dashboard.js : data = payload.battery ?? payload;
    cJSON *battery_obj = cJSON_GetObjectItemCaseSensitive(root, "battery");
    cJSON *data_obj    = (battery_obj && cJSON_IsObject(battery_obj)) ? battery_obj : root;

    // --- battery_status_t : mapping 1:1 sur tes champs JS ---

    float pack_v = json_get_number(data_obj, "pack_voltage_v", 0.0f);
    float pack_i = json_get_number(data_obj, "pack_current_a", 0.0f);

    s_batt_status.voltage     = pack_v;
    s_batt_status.current     = pack_i;
    s_batt_status.soc         = json_get_number(data_obj, "state_of_charge_pct", 0.0f);
    s_batt_status.soh         = json_get_number(data_obj, "state_of_health_pct", 0.0f);
    s_batt_status.temperature = json_get_number(data_obj, "average_temperature_c", 0.0f);

    // Puissance : dashboard calcule pack_voltage_v * pack_current_a
    float power_w = pack_v * pack_i;
    s_batt_status.power = power_w;

    float energy_in_wh  = json_get_number(data_obj, "energy_charged_wh",    0.0f);
    float energy_out_wh = json_get_number(data_obj, "energy_discharged_wh", 0.0f);

    // Heuristiques BMS/CAN proches de handleTelemetryUpdate()
    s_batt_status.bms_ok     = (pack_v > 0.0f);                                  // UART/BMS OK
    s_batt_status.can_ok     = (energy_in_wh > 0.0f || energy_out_wh > 0.0f);    // CAN énergisé
    // ⚠️ mqtt_ok N’EST PLUS TOUCHÉ ICI : il est piloté par remote_event_adapter_on_mqtt_status_json()
    s_batt_status.tinybms_ok = s_batt_status.bms_ok;

    // --- pack_stats_t : cellules + balancing ---

    memset(&s_pack_stats, 0, sizeof(s_pack_stats));

    cJSON *cells_arr = cJSON_GetObjectItemCaseSensitive(data_obj, "cell_voltage_mv");
    if (cJSON_IsArray(cells_arr)) {
        int arr_size = cJSON_GetArraySize(cells_arr);
        if (arr_size > PACK_MAX_CELLS) arr_size = PACK_MAX_CELLS;

        s_pack_stats.cell_count = (uint8_t) arr_size;

        float sum         = 0.0f;
        int   valid_count = 0;
        float min_mv      = 0.0f;
        float max_mv      = 0.0f;

        for (int i = 0; i < arr_size; ++i) {
            cJSON *item = cJSON_GetArrayItem(cells_arr, i);
            float  mv   = 0.0f;
            if (cJSON_IsNumber(item)) {
                mv = (float) item->valuedouble;
            }
            s_pack_stats.cells[i] = mv;

            if (mv > 0.0f) {
                if (valid_count == 0) {
                    min_mv = max_mv = mv;
                } else {
                    if (mv < min_mv) min_mv = mv;
                    if (mv > max_mv) max_mv = mv;
                }
                sum += mv;
                valid_count++;
            }
        }

        float json_min = json_get_number(data_obj, "min_cell_mv", 0.0f);
        float json_max = json_get_number(data_obj, "max_cell_mv", 0.0f);

        if (json_min > 0.0f) min_mv = json_min;
        if (json_max > 0.0f) max_mv = json_max;

        if (valid_count > 0) {
            s_pack_stats.cell_min   = min_mv;
            s_pack_stats.cell_max   = max_mv;
            s_pack_stats.cell_delta = max_mv - min_mv;
            s_pack_stats.cell_avg   = sum / (float) valid_count;
        }
    }

    // Flags d’équilibrage par cellule : dashboard passe balancingStates = data.cell_balancing
    memset(s_pack_stats.balancing, 0, sizeof(s_pack_stats.balancing));

    cJSON *bal_arr = cJSON_GetObjectItemCaseSensitive(data_obj, "cell_balancing");
    if (cJSON_IsArray(bal_arr) && s_pack_stats.cell_count > 0) {
        int arr_size = cJSON_GetArraySize(bal_arr);
        int n = arr_size;
        if (n > s_pack_stats.cell_count) n = s_pack_stats.cell_count;
        if (n > PACK_MAX_CELLS)          n = PACK_MAX_CELLS;

        for (int i = 0; i < n; ++i) {
            cJSON *item = cJSON_GetArrayItem(bal_arr, i);
            bool active = false;
            if (cJSON_IsBool(item)) {
                active = cJSON_IsTrue(item);
            } else if (cJSON_IsNumber(item)) {
                active = (item->valueint != 0);
            }
            s_pack_stats.balancing[i] = active;
        }
    }

    int balancing_bits = 0;
    cJSON *bal_bits = cJSON_GetObjectItemCaseSensitive(data_obj, "balancing_bits");
    if (cJSON_IsNumber(bal_bits)) {
        balancing_bits = bal_bits->valueint;
    }
    (void) balancing_bits; // usage futur si besoin

    s_pack_stats.bal_start_mv = 0.0f;
    s_pack_stats.bal_stop_mv  = 0.0f;

    cJSON_Delete(root);

    // --- Publication des événements « propres » ---

    event_t evt_batt = {
        .type = EVENT_BATTERY_STATUS_UPDATED,
        .data = &s_batt_status,
    };
    event_bus_publish(s_bus, &evt_batt);

    event_t evt_pack = {
        .type = EVENT_PACK_STATS_UPDATED,
        .data = &s_pack_stats,
    };
    event_bus_publish(s_bus, &evt_pack);
}

// ============================================================================
//  EVENTS  (/ws/events)  → system_status_t (WiFi, Storage, ALARM…)
// ============================================================================

void remote_event_adapter_on_event_json(const char *json, size_t length)
{
    (void) length;
    if (!s_bus || !json) {
        return;
    }

    ESP_LOGD(TAG, "Event JSON: %s", json);

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse event JSON");
        return;
    }

    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *key_item  = cJSON_GetObjectItemCaseSensitive(root, "key");

    const char *eventType = (cJSON_IsString(type_item) && type_item->valuestring)
                            ? type_item->valuestring
                            : NULL;
    const char *eventKey  = (cJSON_IsString(key_item) && key_item->valuestring)
                            ? key_item->valuestring
                            : NULL;

    int eventId = json_get_event_id(root, "event_id");

    bool has_error_field = json_get_bool(root, "has_error", false);

    // --- 1) WiFi / Storage : alignement SystemStatus.handleEvent() ---
    if (eventKey || (eventType &&
                     (!strcmp(eventType, "wifi") || !strcmp(eventType, "storage")))) {

        if (eventKey) {
            if (!strcmp(eventKey, "wifi_sta_start") ||
                !strcmp(eventKey, "wifi_sta_connected")) {
                s_sys_status.wifi_connected = false;
            } else if (!strcmp(eventKey, "wifi_sta_got_ip")) {
                s_sys_status.wifi_connected = true;
            } else if (!strcmp(eventKey, "wifi_sta_disconnected") ||
                       !strcmp(eventKey, "wifi_sta_lost_ip") ||
                       !strcmp(eventKey, "wifi_ap_started")) {
                s_sys_status.wifi_connected = false;
            } else if (!strcmp(eventKey, "wifi_ap_stopped") ||
                       !strcmp(eventKey, "wifi_ap_client_connected") ||
                       !strcmp(eventKey, "wifi_ap_client_disconnected")) {
                s_sys_status.wifi_connected = false;
            } else if (!strcmp(eventKey, "storage_history_ready")) {
                s_sys_status.storage_ok = true;
            } else if (!strcmp(eventKey, "storage_history_unavailable")) {
                s_sys_status.storage_ok = false;
                s_sys_status.has_error  = true;
            }
        }

        if (eventId >= 0) {
            if (eventId == 0x1300 || eventId == 0x1301) {
                s_sys_status.wifi_connected = false;
            } else if (eventId == 0x1303) {
                s_sys_status.wifi_connected = true;
            } else if (eventId == 0x1302 || eventId == 0x1304 || eventId == 0x1310) {
                s_sys_status.wifi_connected = false;
            } else if (eventId == 0x1400) {
                s_sys_status.storage_ok = true;
            } else if (eventId == 0x1401) {
                s_sys_status.storage_ok = false;
                s_sys_status.has_error  = true;
            } else if (eventId == 0x1100 || eventId == 0x1101 || eventId == 0x1102) {
                // UART OK – exploitable plus tard
            } else if (eventId == 0x1200 || eventId == 0x1201 || eventId == 0x1202) {
                // CAN OK – exploitable plus tard
            }
        }
    }

    // --- 2) Alarm / Error → badge ALM global ---
    if (eventType &&
        (!strcmp(eventType, "alarm") || !strcmp(eventType, "error"))) {

        bool active = has_error_field;

        if (!active) {
            active = json_get_bool(root, "active", false);
        }

        cJSON *status_item = cJSON_GetObjectItemCaseSensitive(root, "status");
        if (cJSON_IsString(status_item) && status_item->valuestring) {
            const char *s = status_item->valuestring;
            if (!active &&
                (!strcmp(s, "on") ||
                 !strcmp(s, "active") ||
                 !strcmp(s, "error") ||
                 !strcmp(s, "critical"))) {
                active = true;
            }
            if (!strcmp(s, "ok") || !strcmp(s, "off")) {
                active = false;
            }
        }

        s_sys_status.has_error = active;
    }

    if (has_error_field) {
        s_sys_status.has_error = true;
    }

    if (!s_sys_status.storage_ok) {
        s_sys_status.has_error = true;
    }

    cJSON_Delete(root);

    event_t evt_sys = {
        .type = EVENT_SYSTEM_STATUS_UPDATED,
        .data = &s_sys_status,
    };
    event_bus_publish(s_bus, &evt_sys);
}

void remote_event_adapter_on_http_response(const char *path,
                                           const char *method,
                                           int status,
                                           const char *body)
{
    if (!s_bus || !path || !method) {
        return;
    }

    bool success = (status >= 200 && status < 300);

    cmd_result_t result = {
        .success    = success,
        .error_code = status,
    };
    snprintf(result.message, sizeof(result.message), "%s %s -> %d", method, path, status);

    event_t evt_result = {
        .type = EVENT_REMOTE_CMD_RESULT,
        .data = &result,
    };
    event_bus_publish(s_bus, &evt_result);

    if (success && body && strlen(body) > 0) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            if (strcmp(path, "/api/config") == 0) {
                parse_main_config_json(root);
                publish_config_update();
            } else if (strcmp(path, "/api/mqtt/config") == 0) {
                parse_mqtt_config_json(root);
                publish_config_update();
            }
            cJSON_Delete(root);
        }
    }

    if (success && strcmp(method, "POST") == 0 && s_has_pending_config) {
        s_config = s_pending_config;
        s_has_pending_config = false;
        publish_config_update();
    }
}

// ============================================================================
//  MQTT STATUS  (JSON → battery_status.mqtt_ok)
// ============================================================================

void remote_event_adapter_on_mqtt_status_json(const char *json, size_t length)
{
    (void) length;
    if (!s_bus || !json) {
        return;
    }

    ESP_LOGD(TAG, "MQTT Status JSON: %s", json);

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse MQTT status JSON");
        return;
    }

    // Alignement avec un handleMqttStatus(status) typique :
    // - status.enabled : bool
    // - status.connected : bool
    bool enabled   = json_get_bool(root, "enabled", true);
    bool connected = json_get_bool(root, "connected", false);

    if (!enabled) {
        s_batt_status.mqtt_ok = false;
    } else {
        s_batt_status.mqtt_ok = connected;
    }

    cJSON_Delete(root);

    // On republie l’état batterie pour mettre à jour le badge MQTT sur Home
    event_t evt_batt = {
        .type = EVENT_BATTERY_STATUS_UPDATED,
        .data = &s_batt_status,
    };
    event_bus_publish(s_bus, &evt_batt);
}

// ============================================================================
//  CONFIGURATION  (HTTP /api/config et /api/mqtt/config)
// ============================================================================

static void parse_main_config_json(cJSON *root)
{
    if (!root) {
        return;
    }

    set_string_field(s_config.wifi_ssid, sizeof(s_config.wifi_ssid), root, "wifi_ssid");
    set_string_field(s_config.wifi_password, sizeof(s_config.wifi_password), root, "wifi_password");
    set_string_field(s_config.static_ip, sizeof(s_config.static_ip), root, "static_ip");

    cJSON *can_bitrate = cJSON_GetObjectItemCaseSensitive(root, "can_bitrate");
    if (cJSON_IsNumber(can_bitrate)) {
        s_config.can_bitrate = can_bitrate->valueint;
    }

    cJSON *uart_baud = cJSON_GetObjectItemCaseSensitive(root, "uart_baudrate");
    if (cJSON_IsNumber(uart_baud)) {
        s_config.uart_baudrate = uart_baud->valueint;
    }

    set_string_field(s_config.uart_parity, sizeof(s_config.uart_parity), root, "uart_parity");
}

static void parse_mqtt_config_json(cJSON *root)
{
    if (!root) {
        return;
    }

    set_string_field(s_config.mqtt_broker, sizeof(s_config.mqtt_broker), root, "mqtt_broker");
    set_string_field(s_config.mqtt_topic_pub, sizeof(s_config.mqtt_topic_pub), root, "mqtt_topic_pub");
    set_string_field(s_config.mqtt_topic_sub, sizeof(s_config.mqtt_topic_sub), root, "mqtt_topic_sub");
}
