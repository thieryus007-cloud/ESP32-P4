// components/telemetry_model/telemetry_model.c

#include "telemetry_model.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "string.h"
#include "stdlib.h"
#include "tinybms_model.h"

static const char *TAG = "telemetry_model";

#ifndef CONFIG_TELEMETRY_MODEL_MIN_PUBLISH_MS
#define TELEMETRY_MODEL_MIN_PUBLISH_MS 250
#else
#define TELEMETRY_MODEL_MIN_PUBLISH_MS CONFIG_TELEMETRY_MODEL_MIN_PUBLISH_MS
#endif

typedef struct {
    event_bus_t     *bus;
    TaskHandle_t     poll_task;
    bool             initialized;
    bool             telemetry_expected;  // true si flux S3 attendu
    bool             tinybms_connected;
    battery_status_t batt;
    pack_stats_t     pack;
    uint32_t         last_publish_ms;
    bool             dirty;
} telemetry_state_t;

static telemetry_state_t s_state = {
    .telemetry_expected = true,
};
static SemaphoreHandle_t s_state_mutex = NULL;

static bool lock_state(TickType_t timeout)
{
    if (!s_state_mutex) {
        return false;
    }
    if (xSemaphoreTake(s_state_mutex, timeout) != pdTRUE) {
        return false;
    }
    return true;
}

static void unlock_state(void)
{
    if (s_state_mutex) {
        xSemaphoreGive(s_state_mutex);
    }
}

static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void mark_state_dirty_locked(void)
{
    s_state.dirty = true;
}

static void publish_updates_if_dirty(bool force)
{
    if (!s_state.bus) {
        return;
    }

    battery_status_t batt = {0};
    pack_stats_t pack = {0};
    bool should_publish = false;
    uint32_t now = get_time_ms();

    if (lock_state(pdMS_TO_TICKS(50))) {
        uint32_t elapsed = now - s_state.last_publish_ms;
        if (force || s_state.dirty || elapsed >= TELEMETRY_MODEL_MIN_PUBLISH_MS) {
            batt = s_state.batt;
            pack = s_state.pack;
            s_state.dirty = false;
            s_state.last_publish_ms = now;
            should_publish = true;
        }
        unlock_state();
    }

    if (!should_publish) {
        return;
    }

    event_t evt_batt = {
        .type = EVENT_BATTERY_STATUS_UPDATED,
        .data = &batt,
        .data_size = sizeof(batt),
    };
    event_bus_publish(s_state.bus, &evt_batt);

    event_t evt_pack = {
        .type = EVENT_PACK_STATS_UPDATED,
        .data = &pack,
        .data_size = sizeof(pack),
    };
    event_bus_publish(s_state.bus, &evt_pack);
}

static void recompute_pack_stats(void)
{
    // Supposé appelé sous mutex
    if (s_state.pack.cell_count == 0) {
        s_state.pack.cell_min = 0.0f;
        s_state.pack.cell_max = 0.0f;
        s_state.pack.cell_delta = 0.0f;
        s_state.pack.cell_avg = 0.0f;
        mark_state_dirty_locked();
        return;
    }

    float min_mv = s_state.pack.cells[0];
    float max_mv = s_state.pack.cells[0];
    float sum = 0.0f;
    uint8_t count = s_state.pack.cell_count;
    if (count > PACK_MAX_CELLS) {
        count = PACK_MAX_CELLS;
    }

    for (uint8_t i = 0; i < count; ++i) {
        float mv = s_state.pack.cells[i];
        if (mv < min_mv) min_mv = mv;
        if (mv > max_mv) max_mv = mv;
        sum += mv;
    }

    s_state.pack.cell_min = min_mv;
    s_state.pack.cell_max = max_mv;
    s_state.pack.cell_delta = max_mv - min_mv;
    s_state.pack.cell_avg = (count > 0) ? (sum / (float) count) : 0.0f;
    mark_state_dirty_locked();
}

static void apply_register_update(const tinybms_register_update_t *update)
{
    if (!update || !lock_state(pdMS_TO_TICKS(100))) {
        return;
    }

    // SOC (permille → %)
    if (strcmp(update->key, "state_of_charge_permille") == 0) {
        s_state.batt.soc = update->user_value * 0.1f;
    }

    // SOH (permille → %)
    if (strcmp(update->key, "state_of_health_permille") == 0) {
        s_state.batt.soh = update->user_value * 0.1f;
    }

    // Pack voltage / current (mV/mA)
    if (strcmp(update->key, "pack_voltage_mv") == 0) {
        s_state.batt.voltage = update->user_value / 1000.0f;
        s_state.batt.bms_ok = (update->user_value > 0.0f);
    }
    if (strcmp(update->key, "pack_current_ma") == 0) {
        s_state.batt.current = update->user_value / 1000.0f;
    }

    // Température moyenne (°C)
    if (strcmp(update->key, "average_temperature_c") == 0) {
        s_state.batt.temperature = update->user_value;
    }

    // Cellules : recherche d'un index dans la clé (ex: cell1_voltage_mv)
    if (strncmp(update->key, "cell", 4) == 0) {
        const char *ptr = update->key + 4;
        int idx = atoi(ptr);
        if (idx > 0 && idx <= PACK_MAX_CELLS) {
            s_state.pack.cells[idx - 1] = update->user_value;
            if (idx > s_state.pack.cell_count) {
                s_state.pack.cell_count = (uint8_t) idx;
            }
            recompute_pack_stats();
        }
    }

    // Puissance dérivée
    s_state.batt.power = s_state.batt.voltage * s_state.batt.current;
    s_state.batt.tinybms_ok = s_state.tinybms_connected;
    s_state.batt.can_ok = false;
    mark_state_dirty_locked();
    unlock_state();
}

static void poll_tinybms_task(void *arg)
{
    (void) arg;

    const char *keys_to_poll[] = {
        "pack_voltage_mv",
        "pack_current_ma",
        "state_of_charge_permille",
        "state_of_health_permille",
        "average_temperature_c",
    };

    while (1) {
        bool telemetry_expected = true;
        if (lock_state(pdMS_TO_TICKS(20))) {
            telemetry_expected = s_state.telemetry_expected;
            unlock_state();
        }

        if (!telemetry_expected) {
            for (size_t i = 0; i < sizeof(keys_to_poll) / sizeof(keys_to_poll[0]); ++i) {
                const register_descriptor_t *desc = tinybms_get_register_by_key(keys_to_poll[i]);
                if (!desc) {
                    continue;
                }
                float user_value = 0.0f;
                if (tinybms_model_read_register(desc->address, &user_value) == ESP_OK) {
                    tinybms_register_update_t upd = {
                        .address = desc->address,
                        .raw_value = 0,
                        .user_value = user_value,
                    };
                    strncpy(upd.key, desc->key, sizeof(upd.key) - 1);
                    upd.key[sizeof(upd.key) - 1] = '\0';
                    apply_register_update(&upd);
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            if (lock_state(pdMS_TO_TICKS(50))) {
                recompute_pack_stats();
                unlock_state();
            }
            publish_updates_if_dirty(false);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void on_tinybms_register(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    bool telemetry_expected = true;
    if (lock_state(pdMS_TO_TICKS(20))) {
        telemetry_expected = s_state.telemetry_expected;
        unlock_state();
    }
    if (telemetry_expected) {
        return; // on laisse S3 fournir la télémétrie
    }

    const tinybms_register_update_t *update = (const tinybms_register_update_t *) event->data;
    apply_register_update(update);

    // Publie immédiatement pour la réactivité UI
    publish_updates_if_dirty(true);
}

static void on_tinybms_connected(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;
    if (lock_state(pdMS_TO_TICKS(50))) {
        s_state.tinybms_connected = true;
        s_state.batt.bms_ok = true;
        mark_state_dirty_locked();
        unlock_state();
    }
    publish_updates_if_dirty(true);
}

static void on_tinybms_disconnected(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;
    if (lock_state(pdMS_TO_TICKS(50))) {
        s_state.tinybms_connected = false;
        s_state.batt.bms_ok = false;
        mark_state_dirty_locked();
        unlock_state();
    }
    publish_updates_if_dirty(true);
}

static void on_operation_mode(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const operation_mode_event_t *mode = (const operation_mode_event_t *) event->data;
    bool changed = false;
    if (lock_state(pdMS_TO_TICKS(50))) {
        changed = (s_state.telemetry_expected != mode->telemetry_expected);
        s_state.telemetry_expected = mode->telemetry_expected;
        if (changed) {
            memset(&s_state.pack, 0, sizeof(s_state.pack));
            memset(&s_state.batt, 0, sizeof(s_state.batt));
            s_state.batt.tinybms_ok = s_state.tinybms_connected;
            mark_state_dirty_locked();
        }
        unlock_state();
    }
    ESP_LOGI(TAG, "Operation mode changed: telemetry_expected=%d", mode->telemetry_expected);
    if (changed) {
        publish_updates_if_dirty(true);
    }
}

static void on_mqtt_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const mqtt_status_event_t *status = (const mqtt_status_event_t *) event->data;
    bool mqtt_ok = status->enabled && status->connected;
    bool changed = false;
    if (lock_state(pdMS_TO_TICKS(50))) {
        if (s_state.batt.mqtt_ok != mqtt_ok) {
            s_state.batt.mqtt_ok = mqtt_ok;
            mark_state_dirty_locked();
            changed = true;
        }
        unlock_state();
    }
    if (changed) {
        publish_updates_if_dirty(true);
    }
}

esp_err_t telemetry_model_init(event_bus_t *bus)
{
    if (s_state.initialized) {
        return ESP_OK;
    }

    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_state_mutex) {
        s_state_mutex = xSemaphoreCreateMutex();
        if (!s_state_mutex) {
            ESP_LOGE(TAG, "Failed to create telemetry mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&s_state.batt, 0, sizeof(s_state.batt));
    memset(&s_state.pack, 0, sizeof(s_state.pack));
    s_state.bus = bus;
    s_state.last_publish_ms = 0;
    s_state.dirty = true;

    event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, on_tinybms_register, NULL);
    event_bus_subscribe(bus, EVENT_TINYBMS_CONNECTED, on_tinybms_connected, NULL);
    event_bus_subscribe(bus, EVENT_TINYBMS_DISCONNECTED, on_tinybms_disconnected, NULL);
    event_bus_subscribe(bus, EVENT_OPERATION_MODE_CHANGED, on_operation_mode, NULL);
    event_bus_subscribe(bus, EVENT_MQTT_STATUS_UPDATED, on_mqtt_status, NULL);

    s_state.initialized = true;
    ESP_LOGI(TAG, "telemetry_model initialized");
    return ESP_OK;
}

esp_err_t telemetry_model_start(void)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state.poll_task) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(poll_tinybms_task, "telemetry_poll", 4096, NULL, 5, &s_state.poll_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry poll task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
