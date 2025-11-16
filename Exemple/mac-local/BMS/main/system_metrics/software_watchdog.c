#include "software_watchdog.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_events.h"
#include "event_bus.h"

static const char *TAG = "sw_watchdog";

/**
 * @brief Internal watchdog task entry structure
 */
typedef struct {
    char task_name[SOFTWARE_WATCHDOG_TASK_NAME_LENGTH];
    uint64_t last_checkin_ms;
    uint32_t timeout_ms;
    uint32_t missed_checkins;
    bool is_registered;
    bool is_alive;
} watchdog_task_entry_t;

/**
 * @brief Watchdog global state
 */
typedef struct {
    watchdog_task_entry_t tasks[SOFTWARE_WATCHDOG_MAX_TASKS];
    size_t task_count;
    SemaphoreHandle_t mutex;
    TaskHandle_t monitor_task;
    bool initialized;
    uint32_t total_timeouts;
    uint32_t consecutive_timeouts;
    event_bus_publish_fn_t event_publisher;
} watchdog_state_t;

static watchdog_state_t s_watchdog = {
    .task_count = 0,
    .mutex = NULL,
    .monitor_task = NULL,
    .initialized = false,
    .total_timeouts = 0,
    .consecutive_timeouts = 0,
    .event_publisher = NULL,
};

// Maximum consecutive timeouts before triggering system restart (0 = disabled)
#define SOFTWARE_WATCHDOG_MAX_CONSECUTIVE_TIMEOUTS 3U

/**
 * @brief Forward declaration of internal functions
 */
static void software_watchdog_check_tasks(void);
static esp_err_t software_watchdog_find_task(const char *task_name, size_t *out_index);

/**
 * @brief Set event publisher for watchdog events
 *
 * This is called during initialization to enable event publishing.
 *
 * @param publisher Event bus publish function
 */
static void software_watchdog_set_event_publisher(event_bus_publish_fn_t publisher)
{
    s_watchdog.event_publisher = publisher;
}

esp_err_t software_watchdog_init(void)
{
    if (s_watchdog.initialized) {
        ESP_LOGW(TAG, "Software watchdog already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Create mutex for thread-safe access
    s_watchdog.mutex = xSemaphoreCreateMutex();
    if (s_watchdog.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create watchdog mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize task array
    memset(s_watchdog.tasks, 0, sizeof(s_watchdog.tasks));
    s_watchdog.task_count = 0;
    s_watchdog.total_timeouts = 0;
    s_watchdog.consecutive_timeouts = 0;

    // Get event bus publish hook
    s_watchdog.event_publisher = event_bus_get_publish_hook();

    // Create watchdog monitoring task
    BaseType_t result = xTaskCreate(
        software_watchdog_task,
        "sw_watchdog",
        4096,
        NULL,
        tskIDLE_PRIORITY + 5,  // Higher priority to ensure timely checks
        &s_watchdog.monitor_task
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog monitoring task");
        vSemaphoreDelete(s_watchdog.mutex);
        s_watchdog.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_watchdog.initialized = true;
    ESP_LOGI(TAG, "Software watchdog initialized (check interval: %u ms, default timeout: %u ms)",
             SOFTWARE_WATCHDOG_CHECK_INTERVAL_MS,
             SOFTWARE_WATCHDOG_DEFAULT_TIMEOUT_MS);

    return ESP_OK;
}

void software_watchdog_deinit(void)
{
    if (!s_watchdog.initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing software watchdog...");

    // Delete monitoring task
    if (s_watchdog.monitor_task != NULL) {
        vTaskDelete(s_watchdog.monitor_task);
        s_watchdog.monitor_task = NULL;
    }

    // Delete mutex
    if (s_watchdog.mutex != NULL) {
        vSemaphoreDelete(s_watchdog.mutex);
        s_watchdog.mutex = NULL;
    }

    // Reset state
    memset(s_watchdog.tasks, 0, sizeof(s_watchdog.tasks));
    s_watchdog.task_count = 0;
    s_watchdog.total_timeouts = 0;
    s_watchdog.consecutive_timeouts = 0;
    s_watchdog.initialized = false;
    s_watchdog.event_publisher = NULL;

    ESP_LOGI(TAG, "Software watchdog deinitialized");
}

static esp_err_t software_watchdog_find_task(const char *task_name, size_t *out_index)
{
    if (task_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
        if (s_watchdog.tasks[i].is_registered &&
            strcmp(s_watchdog.tasks[i].task_name, task_name) == 0) {
            if (out_index != NULL) {
                *out_index = i;
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t software_watchdog_register_task(const char *task_name, uint32_t timeout_ms)
{
    if (task_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        ESP_LOGE(TAG, "Watchdog not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for task registration");
        return ESP_ERR_TIMEOUT;
    }

    // Check if task already registered
    size_t existing_index = 0;
    if (software_watchdog_find_task(task_name, &existing_index) == ESP_OK) {
        ESP_LOGW(TAG, "Task '%s' already registered", task_name);
        xSemaphoreGive(s_watchdog.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    // Find free slot
    size_t free_index = SOFTWARE_WATCHDOG_MAX_TASKS;
    for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
        if (!s_watchdog.tasks[i].is_registered) {
            free_index = i;
            break;
        }
    }

    if (free_index >= SOFTWARE_WATCHDOG_MAX_TASKS) {
        ESP_LOGE(TAG, "Maximum number of tasks reached (%u)", SOFTWARE_WATCHDOG_MAX_TASKS);
        xSemaphoreGive(s_watchdog.mutex);
        return ESP_ERR_NO_MEM;
    }

    // Register task
    watchdog_task_entry_t *entry = &s_watchdog.tasks[free_index];
    strncpy(entry->task_name, task_name, SOFTWARE_WATCHDOG_TASK_NAME_LENGTH - 1);
    entry->task_name[SOFTWARE_WATCHDOG_TASK_NAME_LENGTH - 1] = '\0';
    entry->timeout_ms = (timeout_ms > 0) ? timeout_ms : SOFTWARE_WATCHDOG_DEFAULT_TIMEOUT_MS;
    entry->last_checkin_ms = esp_timer_get_time() / 1000ULL;
    entry->missed_checkins = 0;
    entry->is_registered = true;
    entry->is_alive = true;

    s_watchdog.task_count++;

    ESP_LOGI(TAG, "Registered task '%s' (timeout: %u ms) [%zu/%u]",
             task_name, entry->timeout_ms, s_watchdog.task_count, SOFTWARE_WATCHDOG_MAX_TASKS);

    xSemaphoreGive(s_watchdog.mutex);
    return ESP_OK;
}

esp_err_t software_watchdog_unregister_task(const char *task_name)
{
    if (task_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for task unregistration");
        return ESP_ERR_TIMEOUT;
    }

    size_t index = 0;
    esp_err_t err = software_watchdog_find_task(task_name, &index);
    if (err != ESP_OK) {
        xSemaphoreGive(s_watchdog.mutex);
        return err;
    }

    // Unregister task
    memset(&s_watchdog.tasks[index], 0, sizeof(watchdog_task_entry_t));
    s_watchdog.task_count--;

    ESP_LOGI(TAG, "Unregistered task '%s' [%zu/%u]",
             task_name, s_watchdog.task_count, SOFTWARE_WATCHDOG_MAX_TASKS);

    xSemaphoreGive(s_watchdog.mutex);
    return ESP_OK;
}

esp_err_t software_watchdog_checkin(const char *task_name)
{
    if (task_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for checkin from '%s'", task_name);
        return ESP_ERR_TIMEOUT;
    }

    size_t index = 0;
    esp_err_t err = software_watchdog_find_task(task_name, &index);
    if (err != ESP_OK) {
        xSemaphoreGive(s_watchdog.mutex);
        return err;
    }

    // Update check-in timestamp
    watchdog_task_entry_t *entry = &s_watchdog.tasks[index];
    entry->last_checkin_ms = esp_timer_get_time() / 1000ULL;

    // Reset missed checkins if task was previously in timeout
    if (!entry->is_alive) {
        ESP_LOGI(TAG, "Task '%s' recovered from timeout", task_name);
        entry->missed_checkins = 0;
    }

    entry->is_alive = true;

    xSemaphoreGive(s_watchdog.mutex);
    return ESP_OK;
}

esp_err_t software_watchdog_get_status(watchdog_system_status_t *out_status)
{
    if (out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for status query");
        return ESP_ERR_TIMEOUT;
    }

    memset(out_status, 0, sizeof(*out_status));
    out_status->total_tasks_monitored = (uint32_t)s_watchdog.task_count;
    out_status->tasks_alive = 0;
    out_status->tasks_timeout = 0;

    for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
        if (!s_watchdog.tasks[i].is_registered) {
            continue;
        }

        if (s_watchdog.tasks[i].is_alive) {
            out_status->tasks_alive++;
        } else {
            out_status->tasks_timeout++;
        }
    }

    out_status->system_healthy = (out_status->tasks_timeout == 0);

    xSemaphoreGive(s_watchdog.mutex);
    return ESP_OK;
}

esp_err_t software_watchdog_get_task_info(const char *task_name, watchdog_task_info_t *out_info)
{
    if (task_name == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for task info query");
        return ESP_ERR_TIMEOUT;
    }

    size_t index = 0;
    esp_err_t err = software_watchdog_find_task(task_name, &index);
    if (err != ESP_OK) {
        xSemaphoreGive(s_watchdog.mutex);
        return err;
    }

    // Copy task info to output
    const watchdog_task_entry_t *entry = &s_watchdog.tasks[index];
    strncpy(out_info->task_name, entry->task_name, SOFTWARE_WATCHDOG_TASK_NAME_LENGTH - 1);
    out_info->task_name[SOFTWARE_WATCHDOG_TASK_NAME_LENGTH - 1] = '\0';
    out_info->last_checkin_ms = entry->last_checkin_ms;
    out_info->timeout_ms = entry->timeout_ms;
    out_info->missed_checkins = entry->missed_checkins;
    out_info->is_alive = entry->is_alive;

    xSemaphoreGive(s_watchdog.mutex);
    return ESP_OK;
}

esp_err_t software_watchdog_get_json(char *buffer, size_t buffer_size, size_t *out_length)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_watchdog.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for JSON serialization");
        return ESP_ERR_TIMEOUT;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        xSemaphoreGive(s_watchdog.mutex);
        return ESP_ERR_NO_MEM;
    }

    // Calculate status
    uint32_t tasks_alive = 0;
    uint32_t tasks_timeout = 0;
    for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
        if (s_watchdog.tasks[i].is_registered) {
            if (s_watchdog.tasks[i].is_alive) {
                tasks_alive++;
            } else {
                tasks_timeout++;
            }
        }
    }

    // Add status fields
    cJSON_AddNumberToObject(root, "total_tasks", (double)s_watchdog.task_count);
    cJSON_AddNumberToObject(root, "tasks_alive", (double)tasks_alive);
    cJSON_AddNumberToObject(root, "tasks_timeout", (double)tasks_timeout);
    cJSON_AddBoolToObject(root, "system_healthy", (tasks_timeout == 0));
    cJSON_AddNumberToObject(root, "total_timeouts", (double)s_watchdog.total_timeouts);
    cJSON_AddNumberToObject(root, "consecutive_timeouts", (double)s_watchdog.consecutive_timeouts);

    // Add tasks array
    cJSON *tasks_array = cJSON_AddArrayToObject(root, "tasks");
    if (tasks_array != NULL) {
        for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
            if (!s_watchdog.tasks[i].is_registered) {
                continue;
            }

            const watchdog_task_entry_t *entry = &s_watchdog.tasks[i];
            cJSON *task_obj = cJSON_CreateObject();
            if (task_obj != NULL) {
                cJSON_AddStringToObject(task_obj, "name", entry->task_name);
                cJSON_AddNumberToObject(task_obj, "last_checkin_ms", (double)entry->last_checkin_ms);
                cJSON_AddNumberToObject(task_obj, "timeout_ms", (double)entry->timeout_ms);
                cJSON_AddNumberToObject(task_obj, "missed_checkins", (double)entry->missed_checkins);
                cJSON_AddBoolToObject(task_obj, "is_alive", entry->is_alive);
                cJSON_AddItemToArray(tasks_array, task_obj);
            }
        }
    }

    xSemaphoreGive(s_watchdog.mutex);

    // Convert to string
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    size_t json_len = strlen(json_str);
    if (json_len >= buffer_size) {
        cJSON_free(json_str);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(buffer, json_str, json_len + 1);
    if (out_length != NULL) {
        *out_length = json_len;
    }

    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

static void software_watchdog_check_tasks(void)
{
    if (!s_watchdog.initialized) {
        return;
    }

    if (xSemaphoreTake(s_watchdog.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for task check");
        return;
    }

    uint64_t now_ms = esp_timer_get_time() / 1000ULL;
    uint32_t timeouts_detected = 0;
    bool any_timeout = false;

    for (size_t i = 0; i < SOFTWARE_WATCHDOG_MAX_TASKS; i++) {
        watchdog_task_entry_t *entry = &s_watchdog.tasks[i];

        if (!entry->is_registered) {
            continue;
        }

        uint64_t elapsed_ms = now_ms - entry->last_checkin_ms;

        if (elapsed_ms > entry->timeout_ms) {
            // Task has timed out
            if (entry->is_alive) {
                // First timeout detection
                entry->is_alive = false;
                entry->missed_checkins = 1;
                timeouts_detected++;
                any_timeout = true;

                ESP_LOGE(TAG, "TIMEOUT: Task '%s' failed to check in (elapsed: %" PRIu64 " ms, timeout: %" PRIu32 " ms)",
                         entry->task_name, elapsed_ms, entry->timeout_ms);
            } else {
                // Still in timeout
                entry->missed_checkins++;
                any_timeout = true;
            }
        } else {
            // Task is alive
            if (!entry->is_alive) {
                ESP_LOGI(TAG, "Task '%s' recovered from timeout", entry->task_name);
                entry->is_alive = true;
                entry->missed_checkins = 0;
            }
        }
    }

    xSemaphoreGive(s_watchdog.mutex);

    // Update timeout counters
    if (timeouts_detected > 0) {
        s_watchdog.total_timeouts += timeouts_detected;

        if (any_timeout) {
            s_watchdog.consecutive_timeouts++;
        } else {
            s_watchdog.consecutive_timeouts = 0;
        }

        // Publish timeout event
        if (s_watchdog.event_publisher != NULL) {
            event_bus_event_t event = {
                .id = APP_EVENT_ID_SYSTEM_WATCHDOG_TIMEOUT,
                .payload = NULL,
                .payload_size = 0,
            };

            if (!s_watchdog.event_publisher(&event, pdMS_TO_TICKS(10))) {
                ESP_LOGW(TAG, "Failed to publish watchdog timeout event");
            }
        }

        // Log critical warning for multiple timeouts
        if (timeouts_detected > 1) {
            ESP_LOGE(TAG, "CRITICAL: Multiple tasks (%u) in timeout state!", timeouts_detected);
        }

        // Optional: Trigger system restart after consecutive timeouts
        #if SOFTWARE_WATCHDOG_MAX_CONSECUTIVE_TIMEOUTS > 0
        if (s_watchdog.consecutive_timeouts >= SOFTWARE_WATCHDOG_MAX_CONSECUTIVE_TIMEOUTS) {
            ESP_LOGE(TAG, "CRITICAL: %u consecutive timeout cycles detected! System restart recommended.",
                     s_watchdog.consecutive_timeouts);
            // Uncomment to enable automatic restart:
            // ESP_LOGE(TAG, "Triggering system restart in 5 seconds...");
            // vTaskDelay(pdMS_TO_TICKS(5000));
            // esp_restart();
        }
        #endif
    } else if (any_timeout) {
        // No new timeouts, but some tasks still in timeout state
        s_watchdog.consecutive_timeouts++;
    } else {
        // All tasks healthy
        if (s_watchdog.consecutive_timeouts > 0) {
            ESP_LOGI(TAG, "All tasks recovered, consecutive timeouts reset");
        }
        s_watchdog.consecutive_timeouts = 0;
    }
}

void software_watchdog_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Software watchdog monitoring task started");

    // Auto-register critical system tasks
    const char *critical_tasks[] = {
        "event_bus",
        "mqtt_gateway",
        "uart_bms",
        "web_server",
        "monitoring"
    };

    for (size_t i = 0; i < sizeof(critical_tasks) / sizeof(critical_tasks[0]); i++) {
        esp_err_t err = software_watchdog_register_task(critical_tasks[i], 0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Failed to auto-register task '%s': %s",
                     critical_tasks[i], esp_err_to_name(err));
        }
    }

    while (s_watchdog.initialized) {
        // Perform watchdog check
        software_watchdog_check_tasks();

        // Wait for next check interval
        vTaskDelay(pdMS_TO_TICKS(SOFTWARE_WATCHDOG_CHECK_INTERVAL_MS));
    }

    ESP_LOGI(TAG, "Software watchdog monitoring task stopped");
    vTaskDelete(NULL);
}
