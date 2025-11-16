/**
 * @file tinybms_adapter.c
 * @brief TinyBMS Adapter Implementation
 *
 * Converts ESP32-P4 tinybms_model cached registers to BMS uart_bms_live_data_t structure.
 */

#include "tinybms_adapter.h"
#include "tinybms_model.h"
#include "tinybms_registers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>

static const char *TAG = "tinybms_adapter";

// Statistics
static uint32_t s_conversions = 0;
static uint32_t s_failures = 0;

/**
 * @brief Helper: Get cached register value or default
 */
static float get_cached_or_default(uint16_t address, float default_value)
{
    float value;
    if (tinybms_model_get_cached(address, &value) == ESP_OK) {
        return value;
    }
    return default_value;
}

/**
 * @brief Helper: Check if register is cached
 */
static bool is_cached(uint16_t address)
{
    return tinybms_model_is_cached(address);
}

esp_err_t tinybms_adapter_convert(uart_bms_live_data_t *dst)
{
    if (dst == NULL) {
        ESP_LOGE(TAG, "dst is NULL");
        s_failures++;
        return ESP_ERR_INVALID_ARG;
    }

    // Clear structure
    memset(dst, 0, sizeof(uart_bms_live_data_t));

    // Timestamp
    dst->timestamp_ms = esp_timer_get_time() / 1000;

    //==========================================================================
    // NOTE: Real-time measurement registers (0x0000-0x004F) are NOT YET
    // implemented in tinybms_model. These addresses will need to be added
    // to tinybms_model in a future phase.
    //
    // For now, we use placeholder values for compilation/testing.
    //
    // Missing registers:
    // - 0x0000-0x000F: cell_voltage_mv[16] (UINT16, scale 0.1mV)
    // - 0x0024 (36): pack_voltage_v (FLOAT)
    // - 0x0026 (38): pack_current_a (FLOAT)
    // - 0x002D (45): state_of_health (UINT16, scale 0.002%)
    // - 0x002E (46): state_of_charge (UINT32, scale 0.000001)
    // - 0x0030 (48): average_temperature_c (INT16, scale 0.1°C)
    // - 0x0066 (102): max_discharge_current_a (UINT16, scale 0.1A)
    // - 0x0067 (103): max_charge_current_a (UINT16, scale 0.1A)
    //==========================================================================

    // Pack measurements (TODO: implement real-time register reading)
    dst->pack_voltage_v = 0.0f;        // TODO: Read from 0x0024
    dst->pack_current_a = 0.0f;        // TODO: Read from 0x0026
    dst->min_cell_mv = 0;              // TODO: Calculate from 0x0000-0x000F
    dst->max_cell_mv = 0;              // TODO: Calculate from 0x0000-0x000F

    // State (TODO: implement real-time register reading)
    dst->state_of_charge_pct = 0.0f;   // TODO: Read from 0x002E
    dst->state_of_health_pct = 100.0f; // TODO: Read from 0x002D

    // Temperatures (TODO: implement real-time register reading)
    dst->average_temperature_c = 25.0f;  // TODO: Read from 0x0030
    dst->mosfet_temperature_c = 25.0f;
    dst->auxiliary_temperature_c = 25.0f;
    dst->pack_temperature_min_c = 25.0f;
    dst->pack_temperature_max_c = 25.0f;

    // Status bits (TODO: implement)
    dst->balancing_bits = 0;
    dst->alarm_bits = 0;
    dst->warning_bits = 0;

    // Statistics (TODO: implement)
    dst->uptime_seconds = 0;
    dst->estimated_time_left_seconds = 0;
    dst->cycle_count = 0;

    //==========================================================================
    // Configuration registers (0x012C-0x0157) - AVAILABLE in tinybms_model
    //==========================================================================

    // Battery configuration
    dst->battery_capacity_ah = get_cached_or_default(0x0132, 100.0f);
    dst->series_cell_count = (uint16_t)get_cached_or_default(0x0133, 16);

    // Safety limits
    dst->overvoltage_cutoff_mv = (uint16_t)get_cached_or_default(0x013B, 3800);
    dst->undervoltage_cutoff_mv = (uint16_t)get_cached_or_default(0x013C, 2800);
    dst->discharge_overcurrent_limit_a = get_cached_or_default(0x013D, 65.0f);
    dst->charge_overcurrent_limit_a = get_cached_or_default(0x013E, 90.0f);
    dst->overheat_cutoff_c = get_cached_or_default(0x013F, 60.0f);
    dst->low_temp_charge_cutoff_c = get_cached_or_default(0x0140, 0.0f);

    // Current limits (TODO: these should come from 0x0066/0x0067, not config)
    dst->max_discharge_current_limit_a = dst->discharge_overcurrent_limit_a;
    dst->max_charge_current_limit_a = dst->charge_overcurrent_limit_a;
    dst->peak_discharge_current_limit_a = get_cached_or_default(0x0131, 70.0f);

    // Version information (TODO: implement firmware/hardware version registers)
    dst->hardware_version = 1;
    dst->hardware_changes_version = 0;
    dst->firmware_version = 1;
    dst->firmware_flags = 0;
    dst->internal_firmware_version = 100;

    // Serial number (TODO: implement serial number register)
    strncpy(dst->serial_number, "ESP32P4-TINYBMS", UART_BMS_SERIAL_NUMBER_MAX_LENGTH);
    dst->serial_number[UART_BMS_SERIAL_NUMBER_MAX_LENGTH] = '\0';
    dst->serial_length = strlen(dst->serial_number);

    // Cell data (TODO: implement cell voltage reading from 0x0000-0x000F)
    for (int i = 0; i < UART_BMS_CELL_COUNT; i++) {
        dst->cell_voltage_mv[i] = 3300; // Default 3.3V per cell
        dst->cell_balancing[i] = 0;
    }

    // Raw registers (optional - populate from cache if needed)
    dst->register_count = 0;

    s_conversions++;
    ESP_LOGD(TAG, "Conversion complete (total=%lu, failures=%lu)", s_conversions, s_failures);

    return ESP_OK;
}

bool tinybms_adapter_is_ready(void)
{
    // Check if critical configuration registers are cached
    // For now, we only require basic configuration to be available
    bool ready = true;

    // Battery configuration
    ready &= is_cached(0x0132); // battery_capacity_ah
    ready &= is_cached(0x0133); // series_cell_count

    // Safety limits
    ready &= is_cached(0x013B); // overvoltage_cutoff_mv
    ready &= is_cached(0x013C); // undervoltage_cutoff_mv
    ready &= is_cached(0x013D); // discharge_overcurrent_a
    ready &= is_cached(0x013E); // charge_overcurrent_a

    if (!ready) {
        ESP_LOGD(TAG, "Adapter not ready - missing configuration registers");
    }

    return ready;
}

void tinybms_adapter_get_stats(uint32_t *conversions, uint32_t *failures)
{
    if (conversions) {
        *conversions = s_conversions;
    }
    if (failures) {
        *failures = s_failures;
    }
}
