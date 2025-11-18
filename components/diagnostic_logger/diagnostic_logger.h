#ifndef DIAGNOSTIC_LOGGER_H
#define DIAGNOSTIC_LOGGER_H

#include "esp_err.h"
#include "event_bus.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIAG_LOG_SOURCE_UART  = 1,
    DIAG_LOG_SOURCE_RS485 = 2,
    DIAG_LOG_SOURCE_MAIN  = 3,
} diag_log_source_t;

typedef struct {
    uint32_t dropped;
    bool     healthy;
    uint32_t event_queue_capacity;
    uint32_t event_queue_depth;
    uint32_t event_queue_drops;
    bool     event_queue_ready;
} diag_logger_status_t;

typedef struct {
    uint32_t used;
    uint32_t capacity;
    uint32_t dropped;
    bool     healthy;
} diag_logger_ring_info_t;

esp_err_t diagnostic_logger_init(event_bus_t *bus);

diag_logger_status_t diagnostic_logger_get_status(void);

diag_logger_ring_info_t diagnostic_logger_get_ring_info(void);

#ifdef __cplusplus
}
#endif

#endif // DIAGNOSTIC_LOGGER_H
