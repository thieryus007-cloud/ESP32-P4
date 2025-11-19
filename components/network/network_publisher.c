// components/network/network_publisher.c

#include "network_publisher.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "event_types.h"
#include "net_client.h"

#define TELEMETRY_BUFFER_DEPTH CONFIG_NETWORK_TELEMETRY_BUFFER_DEPTH

static const char *TAG = "network_publisher";

typedef struct {
    uint64_t timestamp_ms;
    float    voltage_v;
    float    current_a;
    float    power_w;
    float    soc_pct;
    float    soh_pct;
    float    temperature_c;
    float    cell_min_mv;
    float    cell_max_mv;
    float    cell_delta_mv;
} telemetry_point_t;

typedef struct {
    event_bus_t   *bus;
    TaskHandle_t   task;
    bool           initialized;
    bool           connected;
    bool           enable_offline_buffer;
    battery_status_t last_batt;
    pack_stats_t     last_pack;
    bool             has_batt;
    bool             has_pack;
    uint32_t         publish_errors;
    uint32_t         published_points;
    uint64_t         last_sync_ms;
    uint32_t         last_duration_ms;

    telemetry_point_t buffer[TELEMETRY_BUFFER_DEPTH];
    size_t            head;
    size_t            tail;
    size_t            count;
} publisher_state_t;

static publisher_state_t s_state = {
    .enable_offline_buffer = CONFIG_NETWORK_TELEMETRY_OFFLINE_BUFFER,
};
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static inline void state_lock(void)
{
    portENTER_CRITICAL(&s_state_lock);
}

static inline void state_unlock(void)
{
    portEXIT_CRITICAL(&s_state_lock);
}

static uint64_t get_time_ms(void)
{
    return (uint64_t) (esp_timer_get_time() / 1000ULL);
}

static bool is_publisher_enabled(void)
{
    return CONFIG_NETWORK_TELEMETRY_PUBLISHER_ENABLED;
}

static void buffer_push(const telemetry_point_t *pt)
{
    if (!pt || !s_state.enable_offline_buffer) {
        return;
    }

    state_lock();
    if (s_state.count == TELEMETRY_BUFFER_DEPTH) {
        s_state.tail = (s_state.tail + 1) % TELEMETRY_BUFFER_DEPTH;
        s_state.count--;
    }

    s_state.buffer[s_state.head] = *pt;
    s_state.head = (s_state.head + 1) % TELEMETRY_BUFFER_DEPTH;
    s_state.count++;
    state_unlock();
}

static bool buffer_pop(telemetry_point_t *out)
{
    if (!out) {
        return false;
    }

    state_lock();
    if (s_state.count == 0) {
        state_unlock();
        return false;
    }

    *out = s_state.buffer[s_state.tail];
    s_state.tail = (s_state.tail + 1) % TELEMETRY_BUFFER_DEPTH;
    s_state.count--;
    state_unlock();
    return true;
}

static bool build_point(telemetry_point_t *out)
{
    if (!out) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    state_lock();
    bool has_batt = s_state.has_batt;
    if (has_batt) {
        out->timestamp_ms  = get_time_ms();
        out->voltage_v     = s_state.last_batt.voltage;
        out->current_a     = s_state.last_batt.current;
        out->power_w       = s_state.last_batt.power;
        out->soc_pct       = s_state.last_batt.soc;
        out->soh_pct       = s_state.last_batt.soh;
        out->temperature_c = s_state.last_batt.temperature;

        if (s_state.has_pack) {
            out->cell_min_mv   = s_state.last_pack.cell_min;
            out->cell_max_mv   = s_state.last_pack.cell_max;
            out->cell_delta_mv = s_state.last_pack.cell_delta;
        }
    }
    state_unlock();

    return has_batt;
}

static bool publish_http(const telemetry_point_t *pt)
{
    if (!pt) {
        return false;
    }

    char body[256];
    int len = snprintf(body, sizeof(body),
                       "{\"ts_ms\":%llu,\"soc\":%.2f,\"soh\":%.2f,"
                       "\"voltage_v\":%.3f,\"current_a\":%.3f,\"power_w\":%.3f,"
                       "\"temperature_c\":%.2f,\"cell_min_mv\":%.1f,"
                       "\"cell_max_mv\":%.1f,\"cell_delta_mv\":%.1f}",
                       (unsigned long long) pt->timestamp_ms,
                       pt->soc_pct,
                       pt->soh_pct,
                       pt->voltage_v,
                       pt->current_a,
                       pt->power_w,
                       pt->temperature_c,
                       pt->cell_min_mv,
                       pt->cell_max_mv,
                       pt->cell_delta_mv);

    if (len <= 0 || len >= (int) sizeof(body)) {
        ESP_LOGE(TAG, "Telemetry JSON truncated (len=%d)", len);
        return false;
    }

    return net_client_send_http_request(CONFIG_NETWORK_TELEMETRY_HTTP_PATH,
                                        "POST",
                                        body,
                                        (size_t) len);
}

static bool publish_mqtt(const telemetry_point_t *pt)
{
    if (!pt) {
        return false;
    }

    char payload[256];
    int len = snprintf(payload, sizeof(payload),
                       "ts_ms=%llu soc=%.2f voltage_v=%.3f current_a=%.3f power_w=%.3f temp_c=%.2f",
                       (unsigned long long) pt->timestamp_ms,
                       pt->soc_pct,
                       pt->voltage_v,
                       pt->current_a,
                       pt->power_w,
                       pt->temperature_c);

    if (len <= 0 || len >= (int) sizeof(payload)) {
        ESP_LOGE(TAG, "Telemetry MQTT payload truncated (len=%d)", len);
        return false;
    }

    // MQTT client réel absent : on trace le publish pour garder la compatibilité.
    ESP_LOGI(TAG, "[MQTT] topic=%s payload=%s", CONFIG_NETWORK_TELEMETRY_MQTT_TOPIC, payload);
    return true;
}

static bool publish_point(const telemetry_point_t *pt)
{
    uint64_t start_us = esp_timer_get_time();
    bool mqtt_ok = publish_mqtt(pt);
    bool http_ok = publish_http(pt);
    uint32_t duration_ms = (uint32_t) ((esp_timer_get_time() - start_us) / 1000ULL);

    state_lock();
    s_state.last_duration_ms = duration_ms;
    if (mqtt_ok && http_ok) {
        s_state.published_points++;
        s_state.last_sync_ms = get_time_ms();
    } else {
        s_state.publish_errors++;
    }
    state_unlock();

    return mqtt_ok && http_ok;
}

static void flush_buffer_if_online(void)
{
    while (1) {
        state_lock();
        bool connected = s_state.connected;
        size_t buffered = s_state.count;
        state_unlock();

        if (!connected || buffered == 0) {
            return;
        }

        telemetry_point_t cached = {0};
        if (!buffer_pop(&cached)) {
            return;
        }

        if (!publish_point(&cached)) {
            buffer_push(&cached);
            return;
        }
    }
}

static void publisher_task(void *arg)
{
    (void) arg;

    while (1) {
        if (!is_publisher_enabled()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        telemetry_point_t point = {0};
        bool connected = false;
        state_lock();
        connected = s_state.connected;
        state_unlock();

        if (build_point(&point)) {
            if (connected) {
                if (!publish_point(&point)) {
                    buffer_push(&point);
                    state_lock();
                    s_state.connected = false; // force un retry après reconnection détectée
                    state_unlock();
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

static void on_battery_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    state_lock();
    s_state.last_batt = *(const battery_status_t *) event->data;
    s_state.has_batt = true;
    state_unlock();
}

static void on_pack_stats(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    state_lock();
    s_state.last_pack = *(const pack_stats_t *) event->data;
    s_state.has_pack = true;
    state_unlock();
}

static void on_system_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const system_status_t *status = (const system_status_t *) event->data;
    bool now_connected = status->telemetry_expected && status->wifi_connected && status->server_reachable;

    bool was_connected;
    size_t buffered;

    state_lock();
    was_connected = s_state.connected;
    buffered = s_state.count;
    if (now_connected) {
        s_state.connected = true;
    } else {
        s_state.connected = false;
    }
    state_unlock();

    if (now_connected && !was_connected) {
        ESP_LOGI(TAG, "Network reachable again, flushing telemetry buffer (%d)", (int) buffered);
        flush_buffer_if_online();
    }
}

esp_err_t network_publisher_init(event_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_state.initialized) {
        return ESP_OK;
    }

    s_state.bus = bus;
    event_bus_subscribe(bus, EVENT_BATTERY_STATUS_UPDATED, on_battery_status, NULL);
    event_bus_subscribe(bus, EVENT_PACK_STATS_UPDATED, on_pack_stats, NULL);
    event_bus_subscribe(bus, EVENT_SYSTEM_STATUS_UPDATED, on_system_status, NULL);

    s_state.initialized = true;
    ESP_LOGI(TAG, "network_publisher initialized (period=%d ms, buffer=%d)",
             CONFIG_NETWORK_TELEMETRY_PERIOD_MS,
             TELEMETRY_BUFFER_DEPTH);

    return ESP_OK;
}

network_publisher_metrics_t network_publisher_get_metrics(void)
{
    network_publisher_metrics_t m = {0};
    state_lock();
    m.last_sync_ms = s_state.last_sync_ms;
    m.buffered_points = (uint32_t) s_state.count;
    m.buffer_capacity = TELEMETRY_BUFFER_DEPTH;
    m.publish_errors = s_state.publish_errors;
    m.published_points = s_state.published_points;
    m.last_duration_ms = s_state.last_duration_ms;
    state_unlock();
    return m;
}

esp_err_t network_publisher_start(void)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state.task || !is_publisher_enabled()) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(publisher_task, "net_pub", 4096, NULL, 4, &s_state.task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network publisher task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
