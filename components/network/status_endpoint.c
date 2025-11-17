#include "status_endpoint.h"

#include "diagnostic_logger.h"
#include "config_manager.h"
#include "net_client.h"
#include "network_publisher.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "status_ep";

typedef struct {
    event_bus_t        *bus;
    TaskHandle_t        task;
    system_status_t     last_status;
    bool                has_status;
    network_publisher_metrics_t last_metrics;
} status_state_t;

static status_state_t s_state = {0};

static uint64_t uptime_ms(void)
{
    return (uint64_t) (esp_timer_get_time() / 1000ULL);
}

static void on_system_status(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    s_state.last_status = *(const system_status_t *) event->data;
    s_state.has_status  = true;
}

status_snapshot_t status_endpoint_get_snapshot(void)
{
    status_snapshot_t snap = {0};

    diag_logger_ring_info_t ring = diagnostic_logger_get_ring_info();
    snap.log_used      = ring.used;
    snap.log_capacity  = ring.capacity;
    snap.log_dropped   = ring.dropped;
    snap.log_healthy   = ring.healthy;

    if (s_state.bus) {
        event_bus_metrics_t m = event_bus_get_metrics(s_state.bus);
        snap.event_subscribers = m.subscribers;
        snap.events_published  = m.published_total;
    }

    snap.uptime_ms = uptime_ms();

    if (s_state.has_status) {
        snap.network_state     = s_state.last_status.network_state;
        snap.operation_mode    = s_state.last_status.operation_mode;
        snap.telemetry_expected = s_state.last_status.telemetry_expected;
        snap.wifi_connected    = s_state.last_status.wifi_connected;
        snap.server_reachable  = s_state.last_status.server_reachable;
    }

    s_state.last_metrics = network_publisher_get_metrics();
    snap.telemetry_backlog       = s_state.last_metrics.buffered_points;
    snap.last_backend_sync_ms    = s_state.last_metrics.last_sync_ms;
    snap.last_publish_duration_ms = s_state.last_metrics.last_duration_ms;
    snap.publish_errors          = s_state.last_metrics.publish_errors;

    return snap;
}

static void publish_status_snapshot(const status_snapshot_t *snap)
{
    if (!snap) {
        return;
    }

    const hmi_persistent_config_t *cfg = config_manager_get();
    char body[256];
    int len = snprintf(body, sizeof(body),
                       "{\"uptime_ms\":%llu,\"log_used\":%u,\"log_capacity\":%u,"
                       "\"event_published\":%u,\"telemetry_backlog\":%u,"
                       "\"last_sync_ms\":%llu,\"publish_errors\":%u}",
                       (unsigned long long) snap->uptime_ms,
                       (unsigned) snap->log_used,
                       (unsigned) snap->log_capacity,
                       (unsigned) snap->events_published,
                       (unsigned) snap->telemetry_backlog,
                       (unsigned long long) snap->last_backend_sync_ms,
                       (unsigned) snap->publish_errors);

    if (len <= 0 || len >= (int) sizeof(body)) {
        ESP_LOGE(TAG, "Status JSON truncated (len=%d)", len);
        return;
    }

    if (!net_client_send_http_request(cfg->http_endpoint, "POST", body, (size_t) len)) {
        ESP_LOGW(TAG, "Failed to post status snapshot to %s", cfg->http_endpoint);
    }
}

static void status_task(void *arg)
{
    (void) arg;
    const hmi_persistent_config_t *cfg = config_manager_get();
    uint32_t period_ms = cfg->status_publish_period_ms;
    if (period_ms == 0) {
        period_ms = 60000;
    }

    while (1) {
        status_snapshot_t snap = status_endpoint_get_snapshot();
        publish_status_snapshot(&snap);
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
}

esp_err_t status_endpoint_init(event_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    s_state.bus = bus;
    event_bus_subscribe(bus, EVENT_SYSTEM_STATUS_UPDATED, on_system_status, NULL);
    return ESP_OK;
}

esp_err_t status_endpoint_start(void)
{
    if (s_state.task) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(status_task, "status_ep", 4096, NULL, 4, &s_state.task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status endpoint task");
        return ESP_FAIL;
    }
    return ESP_OK;
}
