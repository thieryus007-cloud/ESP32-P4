/**
 * @file tinybms_model.c
 * @brief TinyBMS Model Implementation
 */

#include "tinybms_model.h"
#include "tinybms_client.h"
#include "event_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "tinybms_model";

// Module state
static struct {
    event_bus_t *bus;
    register_cache_entry_t cache[TINYBMS_REGISTER_COUNT];
    uint32_t total_reads;
    uint32_t total_writes;
    uint32_t cache_hits;
    bool initialized;
} g_model = {0};

/**
 * @brief Get current time in milliseconds
 */
static uint32_t get_time_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief Find cache entry by address
 */
static register_cache_entry_t* find_cache_entry(uint16_t address)
{
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        if (g_model.cache[i].address == address) {
            return &g_model.cache[i];
        }
    }
    return NULL;
}

/**
 * @brief Initialize cache entry
 */
static void init_cache_entry(register_cache_entry_t *entry, uint16_t address)
{
    entry->address = address;
    entry->raw_value = 0;
    entry->valid = false;
    entry->last_update_ms = 0;
}

/**
 * @brief Update cache and publish event
 */
static void update_cache_and_publish(uint16_t address, uint16_t raw_value)
{
    const register_descriptor_t *desc = tinybms_get_register_by_address(address);
    if (desc == NULL) {
        return;
    }

    // Update cache
    register_cache_entry_t *entry = find_cache_entry(address);
    if (entry != NULL) {
        entry->raw_value = raw_value;
        entry->valid = true;
        entry->last_update_ms = get_time_ms();
    }

    // Publish event
    if (g_model.bus != NULL) {
        tinybms_register_update_t update;
        update.address = address;
        update.raw_value = raw_value;
        update.user_value = tinybms_raw_to_user(desc, raw_value);
        strncpy(update.key, desc->key, sizeof(update.key) - 1);
        update.key[sizeof(update.key) - 1] = '\0';

        event_t evt = {
            .type = EVENT_TINYBMS_REGISTER_UPDATED,
            .data = &update,
        };

        event_bus_publish(g_model.bus, &evt);

        ESP_LOGD(TAG, "Register updated: %s = %.2f (0x%04X)",
                 desc->key, update.user_value, raw_value);
    }
}

/**
 * @brief Event handler for user write requests
 */
static void on_user_write_request(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const user_input_tinybms_write_t *req = (const user_input_tinybms_write_t *) event->data;

    ESP_LOGI(TAG, "User write request: address=0x%04X, value=0x%04X",
             req->address, req->value);

    // Write via model (will validate and publish events)
    const register_descriptor_t *desc = tinybms_get_register_by_address(req->address);
    if (desc != NULL) {
        float user_value = tinybms_raw_to_user(desc, req->value);
        esp_err_t ret = tinybms_model_write_register(req->address, user_value);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Register write successful");
        } else {
            ESP_LOGE(TAG, "Register write failed: %s", esp_err_to_name(ret));
        }
    }
}

// Public API

esp_err_t tinybms_model_init(event_bus_t *bus)
{
    if (g_model.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (bus == NULL) {
        ESP_LOGE(TAG, "EventBus is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing TinyBMS model");

    memset(&g_model, 0, sizeof(g_model));
    g_model.bus = bus;

    // Initialize cache with all register addresses
    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        init_cache_entry(&g_model.cache[i], catalog[i].address);
    }

    // Subscribe to user write events
    event_bus_subscribe(bus, EVENT_USER_INPUT_TINYBMS_WRITE_REG, on_user_write_request, NULL);

    g_model.initialized = true;
    ESP_LOGI(TAG, "TinyBMS model initialized with %d registers", TINYBMS_REGISTER_COUNT);

    return ESP_OK;
}

esp_err_t tinybms_model_read_all(void)
{
    if (!g_model.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Reading all %d registers...", TINYBMS_REGISTER_COUNT);

    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    uint32_t success_count = 0;
    uint32_t fail_count = 0;

    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        uint16_t raw_value;
        esp_err_t ret = tinybms_read_register(catalog[i].address, &raw_value);

        if (ret == ESP_OK) {
            update_cache_and_publish(catalog[i].address, raw_value);
            success_count++;
            g_model.total_reads++;
        } else {
            ESP_LOGW(TAG, "Failed to read register 0x%04X (%s): %s",
                     catalog[i].address, catalog[i].key, esp_err_to_name(ret));
            fail_count++;
        }

        // Small delay between reads to avoid overwhelming TinyBMS
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "Read complete: %lu successful, %lu failed",
             success_count, fail_count);

    if (success_count > 0) {
        // Publish config changed event
        event_t evt = {
            .type = EVENT_TINYBMS_CONFIG_CHANGED,
            .data = NULL,
        };
        event_bus_publish(g_model.bus, &evt);
    }

    return (success_count > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t tinybms_model_read_register(uint16_t address, float *user_value)
{
    if (!g_model.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const register_descriptor_t *desc = tinybms_get_register_by_address(address);
    if (desc == NULL) {
        ESP_LOGE(TAG, "Unknown register address 0x%04X", address);
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t raw_value;
    esp_err_t ret = tinybms_read_register(address, &raw_value);

    if (ret == ESP_OK) {
        update_cache_and_publish(address, raw_value);
        g_model.total_reads++;

        if (user_value != NULL) {
            *user_value = tinybms_raw_to_user(desc, raw_value);
        }
    }

    return ret;
}

esp_err_t tinybms_model_write_register(uint16_t address, float user_value)
{
    if (!g_model.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const register_descriptor_t *desc = tinybms_get_register_by_address(address);
    if (desc == NULL) {
        ESP_LOGE(TAG, "Unknown register address 0x%04X", address);
        return ESP_ERR_NOT_FOUND;
    }

    if (desc->read_only) {
        ESP_LOGE(TAG, "Register %s is read-only", desc->key);
        return ESP_ERR_NOT_ALLOWED;
    }

    // Convert user value to raw
    uint16_t raw_value;
    esp_err_t ret = tinybms_user_to_raw(desc, user_value, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid value %.2f for register %s", user_value, desc->key);
        return ret;
    }

    // Validate
    if (!tinybms_validate_raw(desc, raw_value)) {
        ESP_LOGE(TAG, "Validation failed for register %s (raw=0x%04X)",
                 desc->key, raw_value);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Writing register %s: user=%.2f, raw=0x%04X",
             desc->key, user_value, raw_value);

    // Write with verification
    uint16_t verified_value;
    ret = tinybms_write_register(address, raw_value, &verified_value);

    if (ret == ESP_OK) {
        update_cache_and_publish(address, verified_value);
        g_model.total_writes++;

        // Publish config changed event
        event_t evt = {
            .type = EVENT_TINYBMS_CONFIG_CHANGED,
            .data = NULL,
        };
        event_bus_publish(g_model.bus, &evt);
    }

    return ret;
}

esp_err_t tinybms_model_get_cached(uint16_t address, float *user_value)
{
    if (!g_model.initialized || user_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const register_descriptor_t *desc = tinybms_get_register_by_address(address);
    if (desc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    register_cache_entry_t *entry = find_cache_entry(address);
    if (entry == NULL || !entry->valid) {
        return ESP_ERR_NOT_FOUND;
    }

    *user_value = tinybms_raw_to_user(desc, entry->raw_value);
    g_model.cache_hits++;

    return ESP_OK;
}

esp_err_t tinybms_model_get_config(tinybms_config_t *config)
{
    if (!g_model.initialized || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(tinybms_config_t));

    // Helper macro to get cached value
    #define GET_CACHED(field, key) do { \
        const register_descriptor_t *d = tinybms_get_register_by_key(key); \
        if (d != NULL) { \
            float val; \
            if (tinybms_model_get_cached(d->address, &val) == ESP_OK) { \
                config->field = val; \
            } \
        } \
    } while(0)

    // Battery settings
    GET_CACHED(fully_charged_voltage_mv, "fully_charged_voltage_mv");
    GET_CACHED(fully_discharged_voltage_mv, "fully_discharged_voltage_mv");
    GET_CACHED(early_balancing_threshold_mv, "early_balancing_threshold_mv");
    GET_CACHED(charge_finished_current_ma, "charge_finished_current_ma");
    GET_CACHED(peak_discharge_current_a, "peak_discharge_current_a");
    GET_CACHED(battery_capacity_ah, "battery_capacity_ah");
    GET_CACHED(cell_count, "cell_count");
    GET_CACHED(allowed_disbalance_mv, "allowed_disbalance_mv");

    // Safety settings
    GET_CACHED(overvoltage_cutoff_mv, "overvoltage_cutoff_mv");
    GET_CACHED(undervoltage_cutoff_mv, "undervoltage_cutoff_mv");
    GET_CACHED(discharge_overcurrent_a, "discharge_overcurrent_a");
    GET_CACHED(charge_overcurrent_a, "charge_overcurrent_a");
    GET_CACHED(overheat_cutoff_c, "overheat_cutoff_c");
    GET_CACHED(low_temp_charge_cutoff_c, "low_temp_charge_cutoff_c");

    // Advanced settings
    GET_CACHED(charge_restart_level_percent, "charge_restart_level_percent");
    GET_CACHED(battery_max_cycles, "battery_max_cycles");
    GET_CACHED(state_of_health_permille, "state_of_health_permille");
    GET_CACHED(state_of_charge_permille, "state_of_charge_permille");

    // System settings
    GET_CACHED(charger_type, "charger_type");
    GET_CACHED(load_switch_type, "load_switch_type");
    GET_CACHED(operation_mode, "operation_mode");

    #undef GET_CACHED

    return ESP_OK;
}

bool tinybms_model_is_cached(uint16_t address)
{
    register_cache_entry_t *entry = find_cache_entry(address);
    return (entry != NULL && entry->valid);
}

uint32_t tinybms_model_get_cache_age(uint16_t address)
{
    register_cache_entry_t *entry = find_cache_entry(address);
    if (entry == NULL || !entry->valid) {
        return 0;
    }

    return get_time_ms() - entry->last_update_ms;
}

void tinybms_model_invalidate_cache(void)
{
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        g_model.cache[i].valid = false;
    }
    ESP_LOGI(TAG, "Cache invalidated");
}

void tinybms_model_get_stats(uint32_t *total_reads, uint32_t *total_writes,
                              uint32_t *cache_hits)
{
    if (total_reads != NULL) {
        *total_reads = g_model.total_reads;
    }
    if (total_writes != NULL) {
        *total_writes = g_model.total_writes;
    }
    if (cache_hits != NULL) {
        *cache_hits = g_model.cache_hits;
    }
}
