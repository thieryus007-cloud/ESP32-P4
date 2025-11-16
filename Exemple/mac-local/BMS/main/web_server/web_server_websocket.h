#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "web_server_private.h"

/**
 * @file web_server_websocket.h
 * @brief WebSocket management and event broadcasting
 */

// =============================================================================
// WebSocket Buffer Pool
// =============================================================================

/**
 * Buffer pool statistics
 */
typedef struct {
    uint32_t total_allocs;    // Total allocation attempts
    uint32_t pool_hits;       // Allocations served from pool
    uint32_t pool_misses;     // Allocations that fell back to malloc
    uint32_t peak_usage;      // Peak number of buffers in use
    uint32_t current_usage;   // Current number of buffers in use
} ws_buffer_pool_stats_t;

/**
 * Initialize WebSocket buffer pool
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws_buffer_pool_init(void);

/**
 * Cleanup WebSocket buffer pool
 */
void ws_buffer_pool_deinit(void);

/**
 * Allocate buffer from pool (O(1) operation)
 * @param size Required size (must be <= 4096 bytes for pool allocation)
 * @return Pointer to allocated buffer, or NULL on failure
 * @note Falls back to malloc() if pool is exhausted (logs warning)
 */
void *ws_buffer_pool_alloc(size_t size);

/**
 * Free buffer back to pool (O(1) operation)
 * @param ptr Pointer to buffer (must be from ws_buffer_pool_alloc)
 * @note Automatically detects if buffer is from pool or malloc
 */
void ws_buffer_pool_free(void *ptr);

/**
 * Get buffer pool statistics
 * @param stats Pointer to statistics structure to fill
 */
void ws_buffer_pool_get_stats(ws_buffer_pool_stats_t *stats);

// =============================================================================
// WebSocket Subsystem
// =============================================================================

/**
 * Initialize WebSocket subsystem (mutex and client lists)
 */
void web_server_websocket_init(void);

/**
 * Cleanup WebSocket subsystem (free all clients and mutex)
 */
void web_server_websocket_deinit(void);

/**
 * Start WebSocket event task
 */
void web_server_websocket_start_event_task(void);

/**
 * Stop WebSocket event task
 */
void web_server_websocket_stop_event_task(void);

// WebSocket handlers
esp_err_t web_server_telemetry_ws_handler(httpd_req_t *req);
esp_err_t web_server_events_ws_handler(httpd_req_t *req);
esp_err_t web_server_uart_ws_handler(httpd_req_t *req);
esp_err_t web_server_can_ws_handler(httpd_req_t *req);
