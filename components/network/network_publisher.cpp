// components/network/network_publisher.cpp

#include <array>
#include <chrono>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>

extern "C" {
#include "network_publisher.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#include "event_types.h"
#include "net_client.h"
}

namespace {

constexpr size_t kTelemetryBufferDepth = CONFIG_NETWORK_TELEMETRY_BUFFER_DEPTH;

struct telemetry_point_t {
    uint64_t timestamp_ms = 0;
    float    voltage_v = 0.0f;
    float    current_a = 0.0f;
    float    power_w = 0.0f;
    float    soc_pct = 0.0f;
    float    soh_pct = 0.0f;
    float    temperature_c = 0.0f;
    float    cell_min_mv = 0.0f;
    float    cell_max_mv = 0.0f;
    float    cell_delta_mv = 0.0f;
};

class SpinlockGuard {
public:
    explicit SpinlockGuard(portMUX_TYPE& lock) : lock_(lock) { portENTER_CRITICAL(&lock_); }
    ~SpinlockGuard() { portEXIT_CRITICAL(&lock_); }

    SpinlockGuard(const SpinlockGuard&) = delete;
    SpinlockGuard& operator=(const SpinlockGuard&) = delete;

private:
    portMUX_TYPE& lock_;
};

static std::chrono::milliseconds monotonic_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(esp_timer_get_time()));
}

struct PublisherState {
    event_bus_t   *bus = nullptr;
    TaskHandle_t   task = nullptr;
    bool           initialized = false;
    bool           connected = false;
    bool           enable_offline_buffer = false;
    battery_status_t last_batt = {};
    pack_stats_t     last_pack = {};
    bool             has_batt = false;
    bool             has_pack = false;
    uint32_t         publish_errors = 0;
    uint32_t         published_points = 0;
    std::chrono::milliseconds last_sync{0};
    std::chrono::milliseconds last_duration{0};
    std::array<telemetry_point_t, kTelemetryBufferDepth> buffer{};
    size_t            head = 0;
    size_t            tail = 0;
    size_t            count = 0;
};

PublisherState make_default_state()
{
    PublisherState state;
    state.enable_offline_buffer = CONFIG_NETWORK_TELEMETRY_OFFLINE_BUFFER;
    return state;
}

static PublisherState s_state = make_default_state();
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

bool is_publisher_enabled()
{
    return CONFIG_NETWORK_TELEMETRY_PUBLISHER_ENABLED;
}

void buffer_push(const telemetry_point_t *pt)
{
    if (!pt || !s_state.enable_offline_buffer) {
        return;
    }

    SpinlockGuard guard(s_state_lock);
    if (s_state.count == kTelemetryBufferDepth) {
        s_state.tail = (s_state.tail + 1) % kTelemetryBufferDepth;
        s_state.count--;
    }

    s_state.buffer[s_state.head] = *pt;
    s_state.head = (s_state.head + 1) % kTelemetryBufferDepth;
    s_state.count++;
}

bool buffer_pop(telemetry_point_t *out)
{
    if (!out) {
        return false;
    }

    SpinlockGuard guard(s_state_lock);
    if (s_state.count == 0) {
        return false;
    }

    *out = s_state.buffer[s_state.tail];
    s_state.tail = (s_state.tail + 1) % kTelemetryBufferDepth;
    s_state.count--;
    return true;
}

bool build_point(telemetry_point_t *out)
{
    if (!out) {
        return false;
    }

    telemetry_point_t snapshot;

    SpinlockGuard guard(s_state_lock);
    if (!s_state.has_batt) {
        return false;
    }

    snapshot.timestamp_ms  = monotonic_ms().count();
    snapshot.voltage_v     = s_state.last_batt.voltage;
    snapshot.current_a     = s_state.last_batt.current;
    snapshot.power_w       = s_state.last_batt.power;
    snapshot.soc_pct       = s_state.last_batt.soc;
    snapshot.soh_pct       = s_state.last_batt.soh;
    snapshot.temperature_c = s_state.last_batt.temperature;

    if (s_state.has_pack) {
        snapshot.cell_min_mv   = s_state.last_pack.cell_min;
        snapshot.cell_max_mv   = s_state.last_pack.cell_max;
        snapshot.cell_delta_mv = s_state.last_pack.cell_delta;
    }

    *out = snapshot;
    return true;
}

bool publish_http(const telemetry_point_t *pt)
{
    if (!pt) {
        return false;
    }

    char body[256];
    const int len = std::snprintf(body, sizeof(body),
                                  "{\"ts_ms\":%llu,\"soc\":%.2f,\"soh\":%.2f,"
                                  "\"voltage_v\":%.3f,\"current_a\":%.3f,\"power_w\":%.3f,"
                                  "\"temperature_c\":%.2f,\"cell_min_mv\":%.1f,"
                                  "\"cell_max_mv\":%.1f,\"cell_delta_mv\":%.1f}",
                                  static_cast<unsigned long long>(pt->timestamp_ms),
                                  pt->soc_pct,
                                  pt->soh_pct,
                                  pt->voltage_v,
                                  pt->current_a,
                                  pt->power_w,
                                  pt->temperature_c,
                                  pt->cell_min_mv,
                                  pt->cell_max_mv,
                                  pt->cell_delta_mv);

    if (len <= 0 || len >= static_cast<int>(sizeof(body))) {
        ESP_LOGE("network_publisher", "Telemetry JSON truncated (len=%d)", len);
        return false;
    }

    return net_client_send_http_request(CONFIG_NETWORK_TELEMETRY_HTTP_PATH,
                                        "POST",
                                        body,
                                        static_cast<size_t>(len));
}

bool publish_mqtt(const telemetry_point_t *pt)
{
    if (!pt) {
        return false;
    }

    char payload[256];
    const int len = std::snprintf(payload, sizeof(payload),
                                  "ts_ms=%llu soc=%.2f voltage_v=%.3f current_a=%.3f power_w=%.3f temp_c=%.2f",
                                  static_cast<unsigned long long>(pt->timestamp_ms),
                                  pt->soc_pct,
                                  pt->voltage_v,
                                  pt->current_a,
                                  pt->power_w,
                                  pt->temperature_c);

    if (len <= 0 || len >= static_cast<int>(sizeof(payload))) {
        ESP_LOGE("network_publisher", "Telemetry MQTT payload truncated (len=%d)", len);
        return false;
    }

    ESP_LOGI("network_publisher", "[MQTT] topic=%s payload=%s", CONFIG_NETWORK_TELEMETRY_MQTT_TOPIC, payload);
    return true;
}

bool publish_point(const telemetry_point_t *pt)
{
    const auto start = monotonic_ms();
    const bool mqtt_ok = publish_mqtt(pt);
    const bool http_ok = publish_http(pt);
    const auto duration = monotonic_ms() - start;

    SpinlockGuard guard(s_state_lock);
    s_state.last_duration = duration;
    if (mqtt_ok && http_ok) {
        s_state.published_points++;
        s_state.last_sync = monotonic_ms();
    } else {
        s_state.publish_errors++;
    }

    return mqtt_ok && http_ok;
}

void flush_buffer_if_online(void)
{
    while (true) {
        bool connected = false;
        size_t buffered = 0;
        {
            SpinlockGuard guard(s_state_lock);
            connected = s_state.connected;
            buffered = s_state.count;
        }

        if (!connected || buffered == 0) {
            return;
        }

        telemetry_point_t cached;
        if (!buffer_pop(&cached)) {
            return;
        }

        if (!publish_point(&cached)) {
            buffer_push(&cached);
            SpinlockGuard guard(s_state_lock);
            s_state.connected = false;
            return;
        }
    }
}

void publisher_task(void *arg)
{
    (void) arg;

    while (true) {
        if (!is_publisher_enabled()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        telemetry_point_t point;
        bool connected = false;
        {
            SpinlockGuard guard(s_state_lock);
            connected = s_state.connected;
        }

        if (build_point(&point)) {
            if (connected) {
                if (!publish_point(&point)) {
                    buffer_push(&point);
                    SpinlockGuard guard(s_state_lock);
                    s_state.connected = false;
                } else {
                    flush_buffer_if_online();
                }
            } else {
                buffer_push(&point);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_NETWORK_TELEMETRY_PERIOD_MS));
    }
}

void on_battery_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    SpinlockGuard guard(s_state_lock);
    s_state.last_batt = *static_cast<const battery_status_t *>(event->data);
    s_state.has_batt = true;
}

void on_pack_stats(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    SpinlockGuard guard(s_state_lock);
    s_state.last_pack = *static_cast<const pack_stats_t *>(event->data);
    s_state.has_pack = true;
}

void on_system_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const system_status_t *status = static_cast<const system_status_t *>(event->data);
    const bool now_connected = status->telemetry_expected && status->wifi_connected && status->server_reachable;

    bool was_connected = false;
    size_t buffered = 0;

    {
        SpinlockGuard guard(s_state_lock);
        was_connected = s_state.connected;
        buffered = s_state.count;
        s_state.connected = now_connected;
    }

    if (now_connected && !was_connected) {
        ESP_LOGI("network_publisher", "Network reachable again, flushing telemetry buffer (%d)", static_cast<int>(buffered));
        flush_buffer_if_online();
    }
}

} // namespace

extern "C" {

esp_err_t network_publisher_init(event_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_state.initialized) {
        return ESP_OK;
    }

    s_state.bus = bus;
    event_bus_subscribe(bus, EVENT_BATTERY_STATUS_UPDATED, on_battery_status, nullptr);
    event_bus_subscribe(bus, EVENT_PACK_STATS_UPDATED, on_pack_stats, nullptr);
    event_bus_subscribe(bus, EVENT_SYSTEM_STATUS_UPDATED, on_system_status, nullptr);

    s_state.initialized = true;
    ESP_LOGI("network_publisher",
             "network_publisher initialized (period=%d ms, buffer=%d)",
             CONFIG_NETWORK_TELEMETRY_PERIOD_MS,
             CONFIG_NETWORK_TELEMETRY_BUFFER_DEPTH);

    return ESP_OK;
}

network_publisher_metrics_t network_publisher_get_metrics(void)
{
    network_publisher_metrics_t metrics = {};
    {
        SpinlockGuard guard(s_state_lock);
        metrics.last_sync_ms = s_state.last_sync.count();
        metrics.buffered_points = static_cast<uint32_t>(s_state.count);
        metrics.buffer_capacity = CONFIG_NETWORK_TELEMETRY_BUFFER_DEPTH;
        metrics.publish_errors = s_state.publish_errors;
        metrics.published_points = s_state.published_points;
        metrics.last_duration_ms = static_cast<uint32_t>(s_state.last_duration.count());
    }
    return metrics;
}

esp_err_t network_publisher_start(void)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state.task || !is_publisher_enabled()) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(publisher_task, "net_pub", 4096, nullptr, 4, &s_state.task);
    if (ok != pdPASS) {
        ESP_LOGE("network_publisher", "Failed to create network publisher task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

} // extern "C"
