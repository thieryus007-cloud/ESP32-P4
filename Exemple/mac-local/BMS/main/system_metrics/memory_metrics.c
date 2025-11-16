#include "memory_metrics.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "mem_metrics";

// Mutex to protect access to shared memory metrics state
static SemaphoreHandle_t s_memory_metrics_mutex = NULL;

// Current memory metrics (protected by mutex)
static memory_fragmentation_metrics_t s_current_metrics = {0};

// Health status thresholds
#define MEMORY_CRITICAL_FREE_BYTES      (10U * 1024U)   // 10KB
#define MEMORY_WARNING_FREE_BYTES       (50U * 1024U)   // 50KB
#define MEMORY_WARNING_FRAGMENTATION    50.0f           // 50%

// Mutex timeout for all operations
#define MEMORY_METRICS_MUTEX_TIMEOUT_MS 100U

/**
 * @brief Helper function to acquire mutex with timeout
 *
 * @return true if mutex acquired, false if timeout
 */
static bool memory_metrics_lock(void)
{
    if (s_memory_metrics_mutex == NULL) {
        return false;
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(MEMORY_METRICS_MUTEX_TIMEOUT_MS);
    if (xSemaphoreTake(s_memory_metrics_mutex, timeout_ticks) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex (timeout after %u ms)", MEMORY_METRICS_MUTEX_TIMEOUT_MS);
        return false;
    }

    return true;
}

/**
 * @brief Helper function to release mutex
 */
static void memory_metrics_unlock(void)
{
    if (s_memory_metrics_mutex != NULL) {
        xSemaphoreGive(s_memory_metrics_mutex);
    }
}

/**
 * @brief Calculate fragmentation percentage
 *
 * Fragmentation = 100 * (1 - largest_block / total_free)
 *
 * @param total_free Total free memory in bytes
 * @param largest_block Largest contiguous free block in bytes
 * @return Fragmentation percentage (0.0 to 100.0)
 */
static float memory_metrics_calculate_fragmentation(uint32_t total_free, uint32_t largest_block)
{
    if (total_free == 0) {
        return 0.0f;
    }

    if (largest_block >= total_free) {
        return 0.0f; // No fragmentation
    }

    float ratio = (float)largest_block / (float)total_free;
    float fragmentation = (1.0f - ratio) * 100.0f;

    // Clamp to valid range
    if (fragmentation < 0.0f) {
        fragmentation = 0.0f;
    }
    if (fragmentation > 100.0f) {
        fragmentation = 100.0f;
    }

    return fragmentation;
}

/**
 * @brief Collect heap capabilities information
 *
 * @param[out] metrics Pointer to metrics structure to update
 */
static void memory_metrics_collect_heap_caps(memory_fragmentation_metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }

    // Get multi-heap info for default capabilities
    multi_heap_info_t heap_info = {0};
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    metrics->heap_info.total_free_bytes = heap_info.total_free_bytes;
    metrics->heap_info.total_allocated_bytes = heap_info.total_allocated_bytes;
    metrics->heap_info.largest_free_block = heap_info.largest_free_block;
    metrics->heap_info.minimum_free_bytes = heap_info.minimum_free_bytes;
    metrics->heap_info.allocated_blocks = heap_info.allocated_blocks;
    metrics->heap_info.free_blocks = heap_info.free_blocks;
    metrics->heap_info.total_blocks = heap_info.total_blocks;
}

/**
 * @brief Update memory metrics (internal, assumes mutex is held)
 *
 * @param[out] metrics Pointer to metrics structure to update
 */
static void memory_metrics_update_internal(memory_fragmentation_metrics_t *metrics)
{
    if (metrics == NULL) {
        return;
    }

    // Get timestamp
    uint64_t timestamp_us = esp_timer_get_time();
    metrics->timestamp_ms = timestamp_us / 1000ULL;

    // Get basic heap statistics
    metrics->total_free_bytes = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    metrics->largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    metrics->minimum_free_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    metrics->total_heap_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

    // Calculate allocated bytes
    if (metrics->total_heap_size >= metrics->total_free_bytes) {
        metrics->total_allocated_bytes = metrics->total_heap_size - metrics->total_free_bytes;
    } else {
        metrics->total_allocated_bytes = 0;
    }

    // Calculate fragmentation
    metrics->fragmentation_percentage = memory_metrics_calculate_fragmentation(
        metrics->total_free_bytes,
        metrics->largest_free_block
    );

    // Get detailed heap capabilities info
    memory_metrics_collect_heap_caps(metrics);

    // Note: allocation_failures would need to be tracked separately
    // as ESP-IDF doesn't provide this metric directly.
    // For now, we keep the existing value (would be incremented by a custom allocator wrapper)
}

esp_err_t memory_metrics_init(void)
{
    if (s_memory_metrics_mutex != NULL) {
        ESP_LOGW(TAG, "Memory metrics already initialized");
        return ESP_OK;
    }

    // Create mutex for thread safety
    s_memory_metrics_mutex = xSemaphoreCreateMutex();
    if (s_memory_metrics_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize metrics
    memset(&s_current_metrics, 0, sizeof(s_current_metrics));

    // Perform initial update
    if (memory_metrics_lock()) {
        memory_metrics_update_internal(&s_current_metrics);
        memory_metrics_unlock();
    }

    ESP_LOGI(TAG, "Memory metrics initialized (free: %" PRIu32 " bytes, frag: %.1f%%)",
             s_current_metrics.total_free_bytes,
             s_current_metrics.fragmentation_percentage);

    return ESP_OK;
}

void memory_metrics_deinit(void)
{
    if (s_memory_metrics_mutex != NULL) {
        vSemaphoreDelete(s_memory_metrics_mutex);
        s_memory_metrics_mutex = NULL;
    }

    memset(&s_current_metrics, 0, sizeof(s_current_metrics));
    ESP_LOGI(TAG, "Memory metrics deinitialized");
}

esp_err_t memory_metrics_get_fragmentation(memory_fragmentation_metrics_t *out_metrics)
{
    if (out_metrics == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!memory_metrics_lock()) {
        return ESP_ERR_TIMEOUT;
    }

    // Copy current metrics
    memcpy(out_metrics, &s_current_metrics, sizeof(*out_metrics));

    memory_metrics_unlock();
    return ESP_OK;
}

esp_err_t memory_metrics_update(void)
{
    if (!memory_metrics_lock()) {
        return ESP_ERR_TIMEOUT;
    }

    // Store previous values for comparison
    float prev_fragmentation = s_current_metrics.fragmentation_percentage;
    uint32_t prev_free = s_current_metrics.total_free_bytes;

    // Update metrics
    memory_metrics_update_internal(&s_current_metrics);

    // Log warnings based on current state
    if (s_current_metrics.total_free_bytes < MEMORY_CRITICAL_FREE_BYTES) {
        ESP_LOGE(TAG, "CRITICAL: Free memory very low (%" PRIu32 " bytes < %" PRIu32 " bytes)",
                 s_current_metrics.total_free_bytes,
                 MEMORY_CRITICAL_FREE_BYTES);
    } else if (s_current_metrics.total_free_bytes < MEMORY_WARNING_FREE_BYTES) {
        ESP_LOGW(TAG, "WARNING: Free memory low (%" PRIu32 " bytes < %" PRIu32 " bytes)",
                 s_current_metrics.total_free_bytes,
                 MEMORY_WARNING_FREE_BYTES);
    }

    if (s_current_metrics.fragmentation_percentage > MEMORY_WARNING_FRAGMENTATION) {
        ESP_LOGW(TAG, "WARNING: High memory fragmentation (%.1f%% > %.1f%%)",
                 s_current_metrics.fragmentation_percentage,
                 MEMORY_WARNING_FRAGMENTATION);
    }

    // Log significant changes
    if (fabsf(s_current_metrics.fragmentation_percentage - prev_fragmentation) > 10.0f) {
        ESP_LOGI(TAG, "Fragmentation changed: %.1f%% -> %.1f%%",
                 prev_fragmentation,
                 s_current_metrics.fragmentation_percentage);
    }

    if (prev_free > s_current_metrics.total_free_bytes) {
        uint32_t decrease = prev_free - s_current_metrics.total_free_bytes;
        if (decrease > (50U * 1024U)) { // Log if decrease > 50KB
            ESP_LOGI(TAG, "Free memory decreased by %" PRIu32 " bytes (%" PRIu32 " -> %" PRIu32 ")",
                     decrease, prev_free, s_current_metrics.total_free_bytes);
        }
    }

    memory_metrics_unlock();
    return ESP_OK;
}

memory_health_status_t memory_metrics_check_health(void)
{
    memory_health_status_t status = MEMORY_HEALTH_OK;

    if (!memory_metrics_lock()) {
        // If we can't get the mutex, assume worst case
        return MEMORY_HEALTH_CRITICAL;
    }

    // Check critical conditions first
    if (s_current_metrics.total_free_bytes < MEMORY_CRITICAL_FREE_BYTES) {
        status = MEMORY_HEALTH_CRITICAL;
    }
    // Check warning conditions
    else if (s_current_metrics.total_free_bytes < MEMORY_WARNING_FREE_BYTES ||
             s_current_metrics.fragmentation_percentage > MEMORY_WARNING_FRAGMENTATION) {
        status = MEMORY_HEALTH_WARNING;
    }

    memory_metrics_unlock();
    return status;
}

const char *memory_metrics_health_status_to_string(memory_health_status_t status)
{
    switch (status) {
    case MEMORY_HEALTH_OK:
        return "ok";
    case MEMORY_HEALTH_WARNING:
        return "warning";
    case MEMORY_HEALTH_CRITICAL:
        return "critical";
    default:
        return "unknown";
    }
}

esp_err_t memory_metrics_get_json(char *buffer, size_t buffer_size, size_t *out_length)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get current metrics (thread-safe copy)
    memory_fragmentation_metrics_t metrics;
    esp_err_t err = memory_metrics_get_fragmentation(&metrics);
    if (err != ESP_OK) {
        return err;
    }

    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Add basic metrics
    cJSON_AddNumberToObject(root, "timestamp_ms", (double)metrics.timestamp_ms);
    cJSON_AddNumberToObject(root, "total_free_bytes", (double)metrics.total_free_bytes);
    cJSON_AddNumberToObject(root, "largest_free_block", (double)metrics.largest_free_block);
    cJSON_AddNumberToObject(root, "fragmentation_percentage", (double)metrics.fragmentation_percentage);
    cJSON_AddNumberToObject(root, "minimum_free_ever", (double)metrics.minimum_free_ever);
    cJSON_AddNumberToObject(root, "allocation_failures", (double)metrics.allocation_failures);
    cJSON_AddNumberToObject(root, "total_allocated_bytes", (double)metrics.total_allocated_bytes);
    cJSON_AddNumberToObject(root, "total_heap_size", (double)metrics.total_heap_size);

    // Add health status
    memory_health_status_t health = memory_metrics_check_health();
    cJSON_AddStringToObject(root, "health_status", memory_metrics_health_status_to_string(health));

    // Add detailed heap info
    cJSON *heap_info_obj = cJSON_AddObjectToObject(root, "heap_info");
    if (heap_info_obj != NULL) {
        cJSON_AddNumberToObject(heap_info_obj, "total_free_bytes", (double)metrics.heap_info.total_free_bytes);
        cJSON_AddNumberToObject(heap_info_obj, "total_allocated_bytes", (double)metrics.heap_info.total_allocated_bytes);
        cJSON_AddNumberToObject(heap_info_obj, "largest_free_block", (double)metrics.heap_info.largest_free_block);
        cJSON_AddNumberToObject(heap_info_obj, "minimum_free_bytes", (double)metrics.heap_info.minimum_free_bytes);
        cJSON_AddNumberToObject(heap_info_obj, "allocated_blocks", (double)metrics.heap_info.allocated_blocks);
        cJSON_AddNumberToObject(heap_info_obj, "free_blocks", (double)metrics.heap_info.free_blocks);
        cJSON_AddNumberToObject(heap_info_obj, "total_blocks", (double)metrics.heap_info.total_blocks);
    }

    // Serialize to string
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // Check buffer size
    size_t json_len = strnlen(json_str, buffer_size + 1U);
    if (json_len >= buffer_size) {
        cJSON_free(json_str);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    // Copy to output buffer
    memcpy(buffer, json_str, json_len + 1U);

    if (out_length != NULL) {
        *out_length = json_len;
    }

    cJSON_free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}
