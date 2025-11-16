#pragma once

/**
 * @file memory_metrics.h
 * @brief Memory fragmentation monitoring module for BMS system
 *
 * Provides real-time memory fragmentation tracking, heap health monitoring,
 * and JSON serialization for REST API and diagnostics.
 *
 * @section memory_metrics_features Features
 * - Real-time fragmentation calculation
 * - Heap watermark tracking (minimum free memory ever)
 * - Allocation failure monitoring
 * - Automatic health status assessment (OK/WARNING/CRITICAL)
 * - Thread-safe API with mutex protection
 * - JSON serialization for REST endpoints
 *
 * @section memory_metrics_thread_safety Thread Safety
 *
 * The memory_metrics module uses an internal mutex to protect all shared state.
 *
 * **Protected Resources**:
 * - Current fragmentation metrics
 * - Watermark (minimum free memory)
 * - Allocation failure counters
 * - Health status
 *
 * **Thread-Safe Functions** (all public functions are mutex-protected):
 * - memory_metrics_init() - Initialize module
 * - memory_metrics_get_fragmentation() - Get current metrics
 * - memory_metrics_update() - Update metrics (call every 10 seconds)
 * - memory_metrics_get_json() - Serialize to JSON
 * - memory_metrics_check_health() - Get health status
 *
 * **Concurrency Pattern**:
 * The module handles concurrent access from:
 * - Monitoring task (periodic updates)
 * - Web server task (reads for /api/system/memory)
 * - System diagnostics tasks
 *
 * @note All mutex operations use 100ms timeout to prevent deadlocks.
 *
 * @section memory_metrics_usage Usage Example
 * @code
 * // Initialize module
 * memory_metrics_init();
 *
 * // Periodic update (call every 10 seconds from a task)
 * memory_metrics_update();
 *
 * // Read current metrics (thread-safe)
 * memory_fragmentation_metrics_t metrics;
 * esp_err_t err = memory_metrics_get_fragmentation(&metrics);
 *
 * // Check health status
 * memory_health_status_t health = memory_metrics_check_health();
 *
 * // Serialize to JSON for API
 * char json[1024];
 * size_t len;
 * err = memory_metrics_get_json(json, sizeof(json), &len);
 * @endcode
 */

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_heap_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory health status levels
 */
typedef enum {
    MEMORY_HEALTH_OK = 0,       /**< Memory is healthy */
    MEMORY_HEALTH_WARNING = 1,  /**< Fragmentation > 50% or low free memory */
    MEMORY_HEALTH_CRITICAL = 2, /**< Free memory < 10KB or severe fragmentation */
} memory_health_status_t;

/**
 * @brief Memory fragmentation metrics structure
 *
 * Contains comprehensive memory statistics including fragmentation analysis,
 * heap watermarks, and allocation failure tracking.
 */
typedef struct {
    uint64_t timestamp_ms;              /**< Timestamp when metrics were captured (ms since boot) */
    uint32_t total_free_bytes;          /**< Total free heap memory in bytes */
    uint32_t largest_free_block;        /**< Size of largest contiguous free block in bytes */
    float fragmentation_percentage;     /**< Fragmentation level: 100 * (1 - largest_block / total_free) */
    uint32_t minimum_free_ever;         /**< Minimum free memory since boot (watermark) */
    uint32_t allocation_failures;       /**< Number of heap allocation failures */
    uint32_t total_allocated_bytes;     /**< Total allocated heap memory in bytes */
    uint32_t total_heap_size;           /**< Total heap size in bytes */

    /**
     * @brief Detailed heap information from ESP-IDF heap_caps
     */
    struct {
        size_t total_free_bytes;        /**< Total free bytes (all capabilities) */
        size_t total_allocated_bytes;   /**< Total allocated bytes (all capabilities) */
        size_t largest_free_block;      /**< Largest free block (all capabilities) */
        size_t minimum_free_bytes;      /**< Minimum free bytes ever (all capabilities) */
        size_t allocated_blocks;        /**< Number of allocated blocks */
        size_t free_blocks;             /**< Number of free blocks */
        size_t total_blocks;            /**< Total number of blocks */
    } heap_info;
} memory_fragmentation_metrics_t;

/**
 * @brief Initialize the memory metrics module
 *
 * Creates mutex for thread safety and initializes internal state.
 * Must be called before any other memory_metrics functions.
 *
 * @return ESP_OK on success
 * @return ESP_ERR_NO_MEM if mutex creation fails
 */
esp_err_t memory_metrics_init(void);

/**
 * @brief Deinitialize the memory metrics module
 *
 * Releases all resources including mutex.
 * After calling this, memory_metrics_init() must be called again before use.
 */
void memory_metrics_deinit(void);

/**
 * @brief Get current memory fragmentation metrics
 *
 * Thread-safe function to retrieve current memory metrics.
 * Mutex-protected to ensure consistent snapshot.
 *
 * @param[out] out_metrics Pointer to structure to receive metrics
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if out_metrics is NULL
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 */
esp_err_t memory_metrics_get_fragmentation(memory_fragmentation_metrics_t *out_metrics);

/**
 * @brief Update memory metrics (call periodically)
 *
 * Collects current heap statistics, calculates fragmentation,
 * updates watermarks, and logs warnings/critical conditions.
 *
 * Recommended to call every 10 seconds from a monitoring task.
 *
 * @note Logs WARNING if fragmentation > 50%
 * @note Logs CRITICAL if free memory < 10KB
 *
 * @return ESP_OK on success
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 */
esp_err_t memory_metrics_update(void);

/**
 * @brief Serialize memory metrics to JSON
 *
 * Creates JSON object with all memory metrics for REST API endpoint.
 * Thread-safe, uses mutex-protected copy of metrics.
 *
 * Example output:
 * @code
 * {
 *   "timestamp_ms": 123456,
 *   "total_free_bytes": 100000,
 *   "largest_free_block": 50000,
 *   "fragmentation_percentage": 50.0,
 *   "minimum_free_ever": 90000,
 *   "allocation_failures": 0,
 *   "total_allocated_bytes": 150000,
 *   "total_heap_size": 250000,
 *   "health_status": "ok",
 *   "heap_info": {
 *     "total_free_bytes": 100000,
 *     "total_allocated_bytes": 150000,
 *     "largest_free_block": 50000,
 *     "minimum_free_bytes": 90000,
 *     "allocated_blocks": 45,
 *     "free_blocks": 12,
 *     "total_blocks": 57
 *   }
 * }
 * @endcode
 *
 * @param[out] buffer Buffer to receive JSON string
 * @param[in] buffer_size Size of output buffer in bytes
 * @param[out] out_length Actual length of JSON string (optional, can be NULL)
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if buffer is NULL or buffer_size is 0
 * @return ESP_ERR_INVALID_SIZE if buffer is too small for JSON output
 * @return ESP_ERR_NO_MEM if JSON object creation fails
 * @return ESP_ERR_TIMEOUT if mutex acquisition fails
 */
esp_err_t memory_metrics_get_json(char *buffer, size_t buffer_size, size_t *out_length);

/**
 * @brief Check memory health status
 *
 * Evaluates current memory state and returns health level:
 * - CRITICAL: Free memory < 10KB
 * - WARNING: Fragmentation > 50% or free memory < 50KB
 * - OK: Otherwise
 *
 * @return Current health status level
 */
memory_health_status_t memory_metrics_check_health(void);

/**
 * @brief Get human-readable health status string
 *
 * Converts health status enum to string for logging and JSON output.
 *
 * @param[in] status Health status enum value
 * @return String representation ("ok", "warning", "critical", "unknown")
 */
const char *memory_metrics_health_status_to_string(memory_health_status_t status);

#ifdef __cplusplus
}
#endif
