/**
 * @file tinybms_model.h
 * @brief TinyBMS Model - High Level Register Management
 *
 * Manages TinyBMS register cache and publishes events on changes.
 * Provides high-level API for reading/writing configuration.
 */

#ifndef TINYBMS_MODEL_H
#define TINYBMS_MODEL_H

#include "esp_err.h"
#include "event_bus.h"
#include "tinybms_registers.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TinyBMS configuration snapshot
 */
typedef struct {
    // Battery settings
    uint16_t fully_charged_voltage_mv;
    uint16_t fully_discharged_voltage_mv;
    uint16_t early_balancing_threshold_mv;
    uint16_t charge_finished_current_ma;
    uint16_t peak_discharge_current_a;
    float battery_capacity_ah;
    uint8_t cell_count;
    uint16_t allowed_disbalance_mv;

    // Safety settings
    uint16_t overvoltage_cutoff_mv;
    uint16_t undervoltage_cutoff_mv;
    uint16_t discharge_overcurrent_a;
    uint16_t charge_overcurrent_a;
    uint16_t overheat_cutoff_c;
    int16_t low_temp_charge_cutoff_c;

    // Advanced settings
    uint16_t charge_restart_level_percent;
    uint16_t battery_max_cycles;
    float state_of_health_permille;
    float state_of_charge_permille;

    // System settings
    uint8_t charger_type;
    uint8_t load_switch_type;
    uint8_t operation_mode;
} tinybms_config_t;

/**
 * @brief Initialize TinyBMS model
 *
 * Sets up register cache and subscribes to events.
 *
 * @param bus Pointer to EventBus
 * @return ESP_OK on success
 */
esp_err_t tinybms_model_init(event_bus_t *bus);

/**
 * @brief Read all registers from TinyBMS
 *
 * Performs a complete scan of all 34 registers and updates cache.
 * This may take several seconds (34 * 750ms max = ~25s worst case).
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_model_read_all(void);

/**
 * @brief Read a specific register
 *
 * Reads from TinyBMS, updates cache, and publishes event.
 *
 * @param address Register address
 * @param user_value Output: converted user value (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t tinybms_model_read_register(uint16_t address, float *user_value);

/**
 * @brief Write a register
 *
 * Validates value, writes to TinyBMS, verifies, updates cache, publishes event.
 *
 * @param address Register address
 * @param user_value User value to write
 * @return ESP_OK on success
 */
esp_err_t tinybms_model_write_register(uint16_t address, float user_value);

/**
 * @brief Get cached register value
 *
 * Returns cached value without reading from TinyBMS.
 *
 * @param address Register address
 * @param user_value Output: user value
 * @return ESP_OK if cached value is valid
 */
esp_err_t tinybms_model_get_cached(uint16_t address, float *user_value);

/**
 * @brief Get complete configuration snapshot
 *
 * Fills config structure with cached values.
 * Call tinybms_model_read_all() first to ensure cache is fresh.
 *
 * @param config Output: configuration structure
 * @return ESP_OK on success
 */
esp_err_t tinybms_model_get_config(tinybms_config_t *config);

/**
 * @brief Check if cache is valid for a register
 *
 * @param address Register address
 * @return true if cached value exists and is valid
 */
bool tinybms_model_is_cached(uint16_t address);

/**
 * @brief Get cache age in milliseconds
 *
 * @param address Register address
 * @return Age in ms, or 0 if not cached
 */
uint32_t tinybms_model_get_cache_age(uint16_t address);

/**
 * @brief Invalidate entire cache
 */
void tinybms_model_invalidate_cache(void);

/**
 * @brief Get statistics
 *
 * @param total_reads Output: total successful reads
 * @param total_writes Output: total successful writes
 * @param cache_hits Output: number of cache hits
 */
void tinybms_model_get_stats(uint32_t *total_reads, uint32_t *total_writes,
                              uint32_t *cache_hits);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_MODEL_H
