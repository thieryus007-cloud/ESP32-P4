#ifndef STATUS_ENDPOINT_H
#define STATUS_ENDPOINT_H

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t log_used;
    uint32_t log_capacity;
    uint32_t log_dropped;
    bool     log_healthy;
    uint32_t event_subscribers;
    uint32_t events_published;
    uint64_t uptime_ms;
    network_state_t network_state;
    hmi_operation_mode_t operation_mode;
    bool telemetry_expected;
    bool wifi_connected;
    bool server_reachable;
    uint32_t telemetry_backlog;
    uint64_t last_backend_sync_ms;
    uint32_t last_publish_duration_ms;
    uint32_t publish_errors;
} status_snapshot_t;

esp_err_t status_endpoint_init(event_bus_t *bus);

esp_err_t status_endpoint_start(void);

status_snapshot_t status_endpoint_get_snapshot(void);

#ifdef __cplusplus
}
#endif

#endif // STATUS_ENDPOINT_H
