#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file software_watchdog.h
 * @brief Software watchdog for deadlock detection in critical BMS tasks
 *
 * This module monitors critical tasks in the system and detects when they fail
 * to check in within their configured timeout period. This helps detect deadlocks,
 * infinite loops, and other conditions that prevent tasks from making progress.
 *
 * Usage:
 * 1. Initialize the watchdog system with software_watchdog_init()
 * 2. Register tasks to monitor with software_watchdog_register_task()
 * 3. Each monitored task must call software_watchdog_checkin() periodically
 * 4. The watchdog task runs automatically and publishes timeout events
 * 5. Query health status with software_watchdog_get_status()
 */

#define SOFTWARE_WATCHDOG_MAX_TASKS           16U
#define SOFTWARE_WATCHDOG_TASK_NAME_LENGTH    32U
#define SOFTWARE_WATCHDOG_DEFAULT_TIMEOUT_MS  30000U
#define SOFTWARE_WATCHDOG_CHECK_INTERVAL_MS   5000U
#define SOFTWARE_WATCHDOG_JSON_BUFFER_SIZE    2048U

/**
 * @brief Information about a single monitored task
 */
typedef struct {
    char task_name[SOFTWARE_WATCHDOG_TASK_NAME_LENGTH];  /**< Name of the monitored task */
    uint64_t last_checkin_ms;                            /**< Last check-in timestamp (ms since boot) */
    uint32_t timeout_ms;                                 /**< Timeout period in milliseconds */
    uint32_t missed_checkins;                            /**< Number of consecutive missed check-ins */
    bool is_alive;                                       /**< True if task is within timeout window */
} watchdog_task_info_t;

/**
 * @brief Overall system health status from watchdog perspective
 */
typedef struct {
    uint32_t total_tasks_monitored;  /**< Total number of registered tasks */
    uint32_t tasks_alive;            /**< Number of tasks that are alive */
    uint32_t tasks_timeout;          /**< Number of tasks in timeout state */
    bool system_healthy;             /**< True if all monitored tasks are alive */
} watchdog_system_status_t;

/**
 * @brief Initialize the software watchdog system
 *
 * Creates the watchdog monitoring task and initializes internal data structures.
 * This function is thread-safe and idempotent.
 *
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if task creation fails
 *         ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t software_watchdog_init(void);

/**
 * @brief Shutdown the software watchdog system
 *
 * Stops the monitoring task and releases all resources.
 * This function is thread-safe.
 */
void software_watchdog_deinit(void);

/**
 * @brief Register a task for watchdog monitoring
 *
 * Adds a task to the watchdog monitoring list. The task must call
 * software_watchdog_checkin() at least once per timeout_ms period.
 *
 * @param task_name Unique name for the task (max 31 characters)
 * @param timeout_ms Timeout period in milliseconds (0 = use default)
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if task_name is NULL
 *         ESP_ERR_NO_MEM if maximum tasks reached
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 */
esp_err_t software_watchdog_register_task(const char *task_name, uint32_t timeout_ms);

/**
 * @brief Unregister a task from watchdog monitoring
 *
 * Removes a task from the watchdog monitoring list.
 *
 * @param task_name Name of the task to unregister
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if task_name is NULL
 *         ESP_ERR_NOT_FOUND if task is not registered
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 */
esp_err_t software_watchdog_unregister_task(const char *task_name);

/**
 * @brief Report that a monitored task is alive and making progress
 *
 * Each monitored task must call this function periodically (at least once
 * per configured timeout period) to signal that it is alive and not deadlocked.
 *
 * This function is thread-safe and designed to be called from task context.
 *
 * @param task_name Name of the task checking in (must match registered name)
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if task_name is NULL
 *         ESP_ERR_NOT_FOUND if task is not registered
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 */
esp_err_t software_watchdog_checkin(const char *task_name);

/**
 * @brief Get current system health status
 *
 * Returns aggregate health information about all monitored tasks.
 * This function is thread-safe.
 *
 * @param out_status Pointer to status structure to populate
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if out_status is NULL
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 */
esp_err_t software_watchdog_get_status(watchdog_system_status_t *out_status);

/**
 * @brief Get detailed information about a specific monitored task
 *
 * @param task_name Name of the task to query
 * @param out_info Pointer to task info structure to populate
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if task_name or out_info is NULL
 *         ESP_ERR_NOT_FOUND if task is not registered
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 */
esp_err_t software_watchdog_get_task_info(const char *task_name, watchdog_task_info_t *out_info);

/**
 * @brief Serialize watchdog status to JSON format
 *
 * Creates a JSON object containing the watchdog system status and detailed
 * information about all monitored tasks.
 *
 * JSON format:
 * {
 *   "total_tasks": <number>,
 *   "tasks_alive": <number>,
 *   "tasks_timeout": <number>,
 *   "system_healthy": <bool>,
 *   "tasks": [
 *     {
 *       "name": "<task_name>",
 *       "last_checkin_ms": <timestamp>,
 *       "timeout_ms": <timeout>,
 *       "missed_checkins": <count>,
 *       "is_alive": <bool>
 *     },
 *     ...
 *   ]
 * }
 *
 * @param buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 * @param out_length Optional pointer to receive actual JSON length
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if buffer is NULL or buffer_size is 0
 *         ESP_ERR_INVALID_SIZE if buffer is too small
 *         ESP_ERR_INVALID_STATE if watchdog not initialized
 *         ESP_ERR_NO_MEM if JSON construction fails
 */
esp_err_t software_watchdog_get_json(char *buffer, size_t buffer_size, size_t *out_length);

/**
 * @brief Watchdog monitoring task function (internal use)
 *
 * This task runs periodically to check all monitored tasks for timeouts.
 * It is started automatically by software_watchdog_init().
 *
 * Do not call this function directly.
 *
 * @param arg Unused task parameter
 */
void software_watchdog_task(void *arg);

#ifdef __cplusplus
}
#endif
