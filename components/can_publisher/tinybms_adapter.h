/**
 * @file tinybms_adapter.h
 * @brief TinyBMS Adapter - Convert ESP32-P4 tinybms_model to BMS uart_bms_live_data_t
 *
 * This adapter bridges the gap between ESP32-P4's tinybms_model (register-based cache)
 * and the BMS reference project's uart_bms_live_data_t structure used by can_publisher.
 *
 * Architecture:
 *   tinybms_client (UART) → tinybms_model (cache) → tinybms_adapter → can_publisher
 */

#ifndef TINYBMS_ADAPTER_H
#define TINYBMS_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Constants from BMS reference project
#define UART_BMS_SERIAL_NUMBER_MAX_LENGTH 16
#define UART_BMS_CELL_COUNT 16
#define UART_BMS_MAX_REGISTERS 64

/**
 * @brief Register entry (from BMS reference)
 */
typedef struct {
    uint16_t address;
    uint16_t value;
} uart_bms_register_entry_t;

/**
 * @brief BMS live data structure (from BMS reference project)
 *
 * This is the exact structure used by can_publisher in Exemple/mac-local/BMS.
 * All CAN message encoders expect this format.
 */
typedef struct {
    // Timestamp
    uint64_t timestamp_ms;

    // Pack measurements
    float pack_voltage_v;
    float pack_current_a;
    uint16_t min_cell_mv;
    uint16_t max_cell_mv;

    // State
    float state_of_charge_pct;
    float state_of_health_pct;

    // Temperatures
    float average_temperature_c;
    float mosfet_temperature_c;
    float auxiliary_temperature_c;
    float pack_temperature_min_c;
    float pack_temperature_max_c;

    // Status bits
    uint16_t balancing_bits;
    uint16_t alarm_bits;
    uint16_t warning_bits;

    // Statistics
    uint32_t uptime_seconds;
    uint32_t estimated_time_left_seconds;
    uint32_t cycle_count;

    // Configuration
    float battery_capacity_ah;
    uint16_t series_cell_count;

    // Safety limits
    uint16_t overvoltage_cutoff_mv;
    uint16_t undervoltage_cutoff_mv;
    float discharge_overcurrent_limit_a;
    float charge_overcurrent_limit_a;
    float max_discharge_current_limit_a;
    float max_charge_current_limit_a;
    float peak_discharge_current_limit_a;
    float overheat_cutoff_c;
    float low_temp_charge_cutoff_c;

    // Version information
    uint8_t hardware_version;
    uint8_t hardware_changes_version;
    uint8_t firmware_version;
    uint8_t firmware_flags;
    uint16_t internal_firmware_version;

    // Serial number
    char serial_number[UART_BMS_SERIAL_NUMBER_MAX_LENGTH + 1];
    uint8_t serial_length;

    // Cell data
    uint16_t cell_voltage_mv[UART_BMS_CELL_COUNT];
    uint8_t cell_balancing[UART_BMS_CELL_COUNT];

    // Raw registers (optional)
    size_t register_count;
    uart_bms_register_entry_t registers[UART_BMS_MAX_REGISTERS];
} uart_bms_live_data_t;

/**
 * @brief Convert tinybms_model cached data to uart_bms_live_data_t
 *
 * Reads all cached registers from tinybms_model and fills the uart_bms_live_data_t
 * structure for use with can_publisher.
 *
 * This function maps the 34 TinyBMS registers to the corresponding fields in
 * uart_bms_live_data_t according to the conversion table in PLAN_BMS_CAN.md.
 *
 * @param dst Output: uart_bms_live_data_t structure to fill
 * @return ESP_OK on success, ESP_FAIL if critical registers are not cached
 *
 * @note Call tinybms_model_read_all() first to ensure fresh data
 * @note This function is thread-safe (uses tinybms_model mutex internally)
 */
esp_err_t tinybms_adapter_convert(uart_bms_live_data_t *dst);

/**
 * @brief Check if adapter has sufficient cached data
 *
 * Verifies that critical registers needed for CAN messages are cached.
 *
 * Critical registers:
 * - 0x0024 (36): pack_voltage_v
 * - 0x0026 (38): pack_current_a
 * - 0x002E (46): state_of_charge_pct
 * - 0x0030 (48): average_temperature_c
 * - 0x0132 (306): battery_capacity_ah
 *
 * @return true if all critical registers are cached
 */
bool tinybms_adapter_is_ready(void);

/**
 * @brief Get adapter statistics
 *
 * @param conversions Output: total conversions performed
 * @param failures Output: number of failed conversions
 */
void tinybms_adapter_get_stats(uint32_t *conversions, uint32_t *failures);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_ADAPTER_H
