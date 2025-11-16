#include "remote_event_adapter.h"

#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

#include "event_types.h"
#include "event_bus.h"

static const char *TAG = "REMOTE_ADAPTER";

static event_bus_t *s_bus = NULL;

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
        return (float)item->valuedouble;
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

// --- API publique ---

void remote_event_adapter_init(event_bus_t *bus)
{
    s_bus = bus;
    memset(&s_batt_status, 0, sizeof(s_batt_status));
    memset(&s_sys_status,  0, sizeof(s_sys_status));
    memset(&s_pack_stats,  0, sizeof(s_pack_stats));

    ESP_LOGI(TAG, "remote_event_adapter initialized");
}

void remote_event_adapter_start(void)
{
    ESP_LOGI(TAG, "remote_event_adapter start (no separate task)");
}

// JSON provenant de /ws/telemetry
// payload = { battery: {...} } ou directement { ... }
void remote_event_adapter_on_telemetry_json(const char *json, size_t length)
{
    (void)length;
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

    // Heuristiques "OK" (à affiner plus tard)
    s_batt_status.bms_ok     = (pack_v > 0.0f);                             // pack visible
    s_batt_status.can_ok     = (energy_in_wh > 0.0f || energy_out_wh > 0.0f); // CAN énergisé
    s_batt_status.mqtt_ok    = true;                                        // à relier à /ws/events
    s_batt_status.tinybms_ok = s_batt_status.bms_ok;

    // --- pack_stats_t : cellules + balancing ---

    memset(&s_pack_stats, 0, sizeof(s_pack_stats));

    // Tableau des tensions cellules en mV (dashboard: voltagesMv = data.cell_voltage_mv)
    cJSON *cells_arr = cJSON_GetObjectItemCaseSensitive(data_obj, "cell_voltage_mv");
    if (cJSON_IsArray(cells_arr)) {
        int arr_size = cJSON_GetArraySize(cells_arr);
        if (arr_size > PACK_MAX_CELLS) arr_size = PACK_MAX_CELLS;

        s_pack_stats.cell_count = (uint8_t)arr_size;

        float sum         = 0.0f;
        int   valid_count  = 0;
        float min_mv       = 0.0f;
        float max_mv       = 0.0f;

        for (int i = 0; i < arr_size; ++i) {
            cJSON *item = cJSON_GetArrayItem(cells_arr, i);
            float  mv   = 0.0f;
            if (cJSON_IsNumber(item)) {
                mv = (float)item->valuedouble;
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

        // dashboard utilise aussi min_cell_mv / max_cell_mv : on priorise ces champs si présents
        float json_min = json_get_number(data_obj, "min_cell_mv", 0.0f);
        float json_max = json_get_number(data_obj, "max_cell_mv", 0.0f);

        if (json_min > 0.0f) min_mv = json_min;
        if (json_max > 0.0f) max_mv = json_max;

        if (valid_count > 0) {
            s_pack_stats.cell_min   = min_mv;
            s_pack_stats.cell_max   = max_mv;
            s_pack_stats.cell_delta = max_mv - min_mv;
            s_pack_stats.cell_avg   = sum / (float)valid_count;
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

    // balancing_bits > 0 : équilibrage actif global (utile pour un indicateur global si tu veux)
    int balancing_bits = 0;
    cJSON *bal_bits = cJSON_GetObjectItemCaseSensitive(data_obj, "balancing_bits");
    if (cJSON_IsNumber(bal_bits)) {
        balancing_bits = bal_bits->valueint;
    }
    // On pourrait, si tu veux, déduire un flag global ici ou colorer un label sur l’écran Pack.

    // Seuils de balancing :
    // → pas présents dans ton JSON telemetry actuel, on laisse à 0.0f
    s_pack_stats.bal_start_mv = 0.0f;
    s_pack_stats.bal_stop_mv  = 0.0f;

    cJSON_Delete(root);

    // --- Publication des événements « propres » ---

    // Batterie globale → Home + Pack + Power + Cells (infos pack)
    event_t evt_batt = {
        .type = EVENT_BATTERY_STATUS_UPDATED,
        .data = &s_batt_status,
    };
    event_bus_publish(s_bus, &evt_batt);

    // Stats pack/cellules + balancing → Pack + Cells
    event_t evt_pack = {
        .type = EVENT_PACK_STATS_UPDATED,
        .data = &s_pack_stats,
    };
    event_bus_publish(s_bus, &evt_pack);
}

// JSON provenant de /ws/events
void remote_event_adapter_on_event_json(const char *json, size_t length)
{
    (void)length;
    if (!s_bus || !json) {
        return;
    }

    ESP_LOGD(TAG, "Event JSON: %s", json);

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse event JSON");
        return;
    }

    // Version minimale → tu pourras copier la même logique que systemStatus.js
    cJSON *type  = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "status");

    if (cJSON_IsString(type) && type->valuestring) {
        const char *t = type->valuestring;

        if (strcmp(t, "wifi") == 0) {
            if (cJSON_IsString(state) && state->valuestring) {
                if (strcmp(state->valuestring, "connected") == 0) {
                    s_sys_status.wifi_connected = true;
                } else if (strcmp(state->valuestring, "disconnected") == 0) {
                    s_sys_status.wifi_connected = false;
                }
            }
        }
        else if (strcmp(t, "storage") == 0) {
            if (cJSON_IsString(state) && state->valuestring) {
                s_sys_status.storage_ok = (strcmp(state->valuestring, "ok") == 0);
            }
        }
        // TODO : compléter avec mqtt, errors, etc. (en copiant systemStatus.js)
    }

    cJSON_Delete(root);

    event_t evt_sys = {
        .type = EVENT_SYSTEM_STATUS_UPDATED,
        .data = &s_sys_status,
    };
    event_bus_publish(s_bus, &evt_sys);
}
