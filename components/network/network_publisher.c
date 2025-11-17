// components/network/network_publisher.c

#include "network_publisher.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

    telemetry_point_t buffer[TELEMETRY_BUFFER_DEPTH];
    size_t            head;
    size_t            tail;
    size_t            count;
} publisher_state_t;

static publisher_state_t s_state = {
    .enable_offline_buffer = CONFIG_NETWORK_TELEMETRY_OFFLINE_BUFFER,
};

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
    if (!s_state.enable_offline_buffer || !pt) {
        return;
    }

    if (s_state.count == TELEMETRY_BUFFER_DEPTH) {
        s_state.tail = (s_state.tail + 1) % TELEMETRY_BUFFER_DEPTH;
        s_state.count--;
    }

    s_state.buffer[s_state.head] = *pt;
    s_state.head = (s_state.head + 1) % TELEMETRY_BUFFER_DEPTH;
    s_state.count++;
}

static bool buffer_pop(telemetry_point_t *out)
{
    if (!out || s_state.count == 0) {
        return false;
    }

    *out = s_state.buffer[s_state.tail];
    s_state.tail = (s_state.tail + 1) % TELEMETRY_BUFFER_DEPTH;
    s_state.count--;
    return true;
}

static bool build_point(telemetry_point_t *out)
{
    if (!out || !s_state.has_batt) {
        return false;
    }

    memset(out, 0, sizeof(*out));
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

    return true;
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
    bool mqtt_ok = publish_mqtt(pt);
    bool http_ok = publish_http(pt);
    return mqtt_ok && http_ok;
}

static void flush_buffer_if_online(void)
{
    if (!s_state.connected || s_state.count == 0) {
        return;
    }

    telemetry_point_t cached = {0};
    while (buffer_pop(&cached)) {
        if (!publish_point(&cached)) {
            buffer_push(&cached);
            break;
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
        if (build_point(&point)) {
            if (s_state.connected) {
                if (!publish_point(&point)) {
                    buffer_push(&point);
                    s_state.connected = false; // force un retry après reconnection détectée
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

    s_state.last_batt = *(const battery_status_t *) event->data;
    s_state.has_batt = true;
}

static void on_pack_stats(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    s_state.last_pack = *(const pack_stats_t *) event->data;
    s_state.has_pack = true;
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

    if (now_connected && !s_state.connected) {
        ESP_LOGI(TAG, "Network reachable again, flushing telemetry buffer (%d)", (int) s_state.count);
        s_state.connected = true;
        flush_buffer_if_online();
    } else if (!now_connected) {
        s_state.connected = false;
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
