// components/telemetry_model/telemetry_model.cpp

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <numeric>
#include <optional>

extern "C" {
#include "telemetry_model.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinybms_model.h"
}

namespace {

static const char *TAG = "telemetry_model";

#ifndef CONFIG_TELEMETRY_MODEL_MIN_PUBLISH_MS
#define TELEMETRY_MODEL_MIN_PUBLISH_MS 250
#else
#define TELEMETRY_MODEL_MIN_PUBLISH_MS CONFIG_TELEMETRY_MODEL_MIN_PUBLISH_MS
#endif

constexpr auto kMinPublishPeriod = std::chrono::milliseconds(TELEMETRY_MODEL_MIN_PUBLISH_MS);

struct PendingTelemetry {
    battery_status_t batt{};
    pack_stats_t     pack{};
};

struct TelemetryState {
    event_bus_t *bus = nullptr;
    TaskHandle_t poll_task = nullptr;
    bool initialized = false;
    bool telemetry_expected = true;
    bool tinybms_connected = false;
    battery_status_t batt{};
    pack_stats_t pack{};
    std::chrono::milliseconds last_publish{0};
    std::optional<PendingTelemetry> pending;
};

class StateLock {
public:
    StateLock(SemaphoreHandle_t handle, TickType_t timeout)
        : handle_(handle)
    {
        if (handle_ && xSemaphoreTake(handle_, timeout) == pdTRUE) {
            acquired_ = true;
        }
    }

    ~StateLock()
    {
        if (acquired_) {
            xSemaphoreGive(handle_);
        }
    }

    explicit operator bool() const { return acquired_; }

private:
    SemaphoreHandle_t handle_ = nullptr;
    bool acquired_ = false;
};

TelemetryState s_state;
SemaphoreHandle_t s_state_mutex = nullptr;

std::chrono::milliseconds monotonic_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(esp_timer_get_time()));
}

void mark_state_dirty_locked()
{
    s_state.pending = PendingTelemetry{ s_state.batt, s_state.pack };
}

void publish_updates_if_dirty(bool force)
{
    if (!s_state.bus) {
        return;
    }

    std::optional<PendingTelemetry> pending_copy;
    const auto now = monotonic_ms();

    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
        if (!lock) {
            return;
        }

        const auto elapsed = now - s_state.last_publish;
        if (force || s_state.pending.has_value() || elapsed >= kMinPublishPeriod) {
            if (!s_state.pending.has_value()) {
                s_state.pending = PendingTelemetry{ s_state.batt, s_state.pack };
            }
            pending_copy = s_state.pending;
            s_state.pending.reset();
            s_state.last_publish = now;
        }
    }

    if (!pending_copy) {
        return;
    }

    event_t evt_batt = {
        .type = EVENT_BATTERY_STATUS_UPDATED,
        .data = &pending_copy->batt,
        .data_size = sizeof(pending_copy->batt),
    };
    event_bus_publish(s_state.bus, &evt_batt);

    event_t evt_pack = {
        .type = EVENT_PACK_STATS_UPDATED,
        .data = &pending_copy->pack,
        .data_size = sizeof(pending_copy->pack),
    };
    event_bus_publish(s_state.bus, &evt_pack);
}

void recompute_pack_stats_locked()
{
    if (s_state.pack.cell_count == 0) {
        s_state.pack.cell_min = 0.0f;
        s_state.pack.cell_max = 0.0f;
        s_state.pack.cell_delta = 0.0f;
        s_state.pack.cell_avg = 0.0f;
        mark_state_dirty_locked();
        return;
    }

    const uint8_t count = std::min<uint8_t>(s_state.pack.cell_count, PACK_MAX_CELLS);
    const float *begin = s_state.pack.cells;
    const float *end = begin + count;

    if (count == 0) {
        s_state.pack.cell_min = 0.0f;
        s_state.pack.cell_max = 0.0f;
        s_state.pack.cell_delta = 0.0f;
        s_state.pack.cell_avg = 0.0f;
        mark_state_dirty_locked();
        return;
    }

    const auto [min_it, max_it] = std::minmax_element(begin, end);
    const float sum = std::accumulate(begin, end, 0.0f);

    s_state.pack.cell_min = (min_it != end) ? *min_it : 0.0f;
    s_state.pack.cell_max = (max_it != end) ? *max_it : 0.0f;
    s_state.pack.cell_delta = s_state.pack.cell_max - s_state.pack.cell_min;
    s_state.pack.cell_avg = sum / static_cast<float>(count);
    mark_state_dirty_locked();
}

void apply_register_update(const tinybms_register_update_t *update)
{
    if (!update) {
        return;
    }

    StateLock lock(s_state_mutex, pdMS_TO_TICKS(100));
    if (!lock) {
        return;
    }

    if (std::strcmp(update->key, "state_of_charge_raw") == 0) {
        // Protocol: UINT32 with scale 0.000001% (value 100000000 = 100%)
        // Descriptor already applies scale, so user_value is in %
        s_state.batt.soc = update->user_value;
    } else if (std::strcmp(update->key, "state_of_health_raw") == 0) {
        // Protocol: UINT16 with scale 0.002% (value 50000 = 100%)
        // Descriptor already applies scale, so user_value is in %
        s_state.batt.soh = update->user_value;
    } else if (std::strcmp(update->key, "pack_voltage_v") == 0) {
        // Protocol: FLOAT in Volts
        s_state.batt.voltage = update->user_value;
        s_state.batt.bms_ok = (update->user_value > 0.0f);
    } else if (std::strcmp(update->key, "pack_current_a") == 0) {
        // Protocol: FLOAT in Amperes
        s_state.batt.current = update->user_value;
    } else if (std::strcmp(update->key, "internal_temperature_decidegc") == 0) {
        // Protocol: INT16 with scale 0.1°C
        // Descriptor already applies scale, so user_value is in °C
        s_state.batt.temperature = update->user_value;
    }

    if (std::strncmp(update->key, "cell", 4) == 0) {
        const char *ptr = update->key + 4;
        int idx = 0;
        const char *end = update->key + std::strlen(update->key);
        const auto result = std::from_chars(ptr, end, idx);
        if (result.ec == std::errc{} && idx > 0 && idx <= PACK_MAX_CELLS) {
            // Protocol: Cell voltages are in mV (UINT16)
            // Descriptor scale is 1.0, so user_value is in mV
            // Convert mV to V for pack_stats_t.cells[] which expects V
            s_state.pack.cells[idx - 1] = update->user_value / 1000.0f;
            if (idx > s_state.pack.cell_count) {
                s_state.pack.cell_count = static_cast<uint8_t>(idx);
            }
            recompute_pack_stats_locked();
        }
    }

    s_state.batt.power = s_state.batt.voltage * s_state.batt.current;
    s_state.batt.tinybms_ok = s_state.tinybms_connected;
    s_state.batt.can_ok = false;
    mark_state_dirty_locked();
}

void poll_tinybms_task(void *arg)
{
    (void) arg;

    constexpr const char *keys_to_poll[] = {
        "pack_voltage_v",
        "pack_current_a",
        "state_of_charge_raw",
        "state_of_health_raw",
        "internal_temperature_decidegc",
    };

    while (true) {
        bool telemetry_expected = true;
        {
            StateLock lock(s_state_mutex, pdMS_TO_TICKS(20));
            if (lock) {
                telemetry_expected = s_state.telemetry_expected;
            }
        }

        if (!telemetry_expected) {
            for (const char *key : keys_to_poll) {
                const register_descriptor_t *desc = tinybms_get_register_by_key(key);
                if (!desc) {
                    continue;
                }
                float user_value = 0.0f;
                if (tinybms_model_read_register(desc->address, &user_value) == ESP_OK) {
                    tinybms_register_update_t update = {};
                    update.address = desc->address;
                    update.user_value = user_value;
                    std::strncpy(update.key, desc->key, sizeof(update.key) - 1);
                    apply_register_update(&update);
                }
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            {
                StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
                if (lock) {
                    recompute_pack_stats_locked();
                }
            }
            publish_updates_if_dirty(false);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void on_tinybms_register(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    bool telemetry_expected = true;
    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(20));
        if (lock) {
            telemetry_expected = s_state.telemetry_expected;
        }
    }
    if (telemetry_expected) {
        return;
    }

    const auto *update = static_cast<const tinybms_register_update_t *>(event->data);
    apply_register_update(update);
    publish_updates_if_dirty(true);
}

void on_tinybms_connected(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;

    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
        if (lock) {
            s_state.tinybms_connected = true;
            s_state.batt.bms_ok = true;
            mark_state_dirty_locked();
        }
    }
    publish_updates_if_dirty(true);
}

void on_tinybms_disconnected(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;

    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
        if (lock) {
            s_state.tinybms_connected = false;
            s_state.batt.bms_ok = false;
            mark_state_dirty_locked();
        }
    }
    publish_updates_if_dirty(true);
}

void on_operation_mode(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const auto *mode = static_cast<const operation_mode_event_t *>(event->data);
    bool changed = false;
    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
        if (lock) {
            changed = (s_state.telemetry_expected != mode->telemetry_expected);
            s_state.telemetry_expected = mode->telemetry_expected;
            if (changed) {
                std::memset(&s_state.pack, 0, sizeof(s_state.pack));
                std::memset(&s_state.batt, 0, sizeof(s_state.batt));
                s_state.batt.tinybms_ok = s_state.tinybms_connected;
                mark_state_dirty_locked();
            }
        }
    }

    ESP_LOGI(TAG, "Operation mode changed: telemetry_expected=%d", mode->telemetry_expected);
    if (changed) {
        publish_updates_if_dirty(true);
    }
}

void on_mqtt_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const auto *status = static_cast<const mqtt_status_event_t *>(event->data);
    const bool mqtt_ok = status->enabled && status->connected;
    bool changed = false;
    {
        StateLock lock(s_state_mutex, pdMS_TO_TICKS(50));
        if (lock) {
            if (s_state.batt.mqtt_ok != mqtt_ok) {
                s_state.batt.mqtt_ok = mqtt_ok;
                mark_state_dirty_locked();
                changed = true;
            }
        }
    }

    if (changed) {
        publish_updates_if_dirty(true);
    }
}

} // namespace

extern "C" {

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

    std::memset(&s_state.batt, 0, sizeof(s_state.batt));
    std::memset(&s_state.pack, 0, sizeof(s_state.pack));
    s_state.bus = bus;
    s_state.last_publish = std::chrono::milliseconds{0};
    s_state.pending = PendingTelemetry{ s_state.batt, s_state.pack };

    event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, on_tinybms_register, nullptr);
    event_bus_subscribe(bus, EVENT_TINYBMS_CONNECTED, on_tinybms_connected, nullptr);
    event_bus_subscribe(bus, EVENT_TINYBMS_DISCONNECTED, on_tinybms_disconnected, nullptr);
    event_bus_subscribe(bus, EVENT_OPERATION_MODE_CHANGED, on_operation_mode, nullptr);
    event_bus_subscribe(bus, EVENT_MQTT_STATUS_UPDATED, on_mqtt_status, nullptr);

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

    BaseType_t ok = xTaskCreate(poll_tinybms_task, "telemetry_poll", 4096, nullptr, 5, &s_state.poll_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry poll task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

} // extern "C"
