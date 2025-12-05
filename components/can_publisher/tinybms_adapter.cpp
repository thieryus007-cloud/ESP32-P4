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
    // LIVE DATA REGISTERS (0x0000-0x004F)
    // These registers are now polled periodically by tinybms_poller and
    // cached in tinybms_model. We read from cache for best performance.
    //==========================================================================

    // Pack measurements (REG 36, 38)
    dst->pack_voltage_v = get_cached_or_default(0x0024, 0.0f);
    dst->pack_current_a = get_cached_or_default(0x0026, 0.0f);

    // Min/Max cell voltages (REG 40, 41) - stored in mV
    dst->min_cell_mv = (uint16_t)get_cached_or_default(0x0028, 0.0f);
    dst->max_cell_mv = (uint16_t)get_cached_or_default(0x0029, 0.0f);

    // State of Charge and Health (REG 45, 46)
    dst->state_of_health_pct = get_cached_or_default(0x002D, 100.0f);
    dst->state_of_charge_pct = get_cached_or_default(0x002E, 0.0f);

    // Temperatures (REG 42, 43, 48)
    float temp_ext1 = get_cached_or_default(0x002A, 25.0f);
    float temp_ext2 = get_cached_or_default(0x002B, 25.0f);
    float temp_int = get_cached_or_default(0x0030, 25.0f);

    dst->average_temperature_c = (temp_ext1 + temp_ext2 + temp_int) / 3.0f;
    dst->mosfet_temperature_c = temp_int;
    dst->auxiliary_temperature_c = temp_ext1;
    dst->pack_temperature_min_c = fminf(fminf(temp_ext1, temp_ext2), temp_int);
    dst->pack_temperature_max_c = fmaxf(fmaxf(temp_ext1, temp_ext2), temp_int);

    // Status bits (REG 50, 52)
    uint16_t online_status = (uint16_t)get_cached_or_default(0x0032, 0x97); // Default: IDLE
    uint16_t balancing = (uint16_t)get_cached_or_default(0x0034, 0);

    dst->balancing_bits = balancing;

    // Parse alarm bits from online_status
    // 0x9B = FAULT, set alarm bit
    if (online_status == 0x9B) {
        dst->alarm_bits = 0x0001; // Generic fault alarm
    } else {
        dst->alarm_bits = 0;
    }
    dst->warning_bits = 0;

    // Statistics (not available in TinyBMS protocol)
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

    // Cell data (REG 0-15) - Individual cell voltages in mV
    for (int i = 0; i < UART_BMS_CELL_COUNT; i++) {
        dst->cell_voltage_mv[i] = (uint16_t)get_cached_or_default(i, 0.0f);
        // Cell balancing status from balancing_bits (bit i indicates cell i)
        dst->cell_balancing[i] = (balancing >> i) & 0x01;
    }

    // Raw registers (optional - populate from cache if needed)
    dst->register_count = 0;

    s_conversions++;
    ESP_LOGD(TAG, "Conversion complete (total=%lu, failures=%lu)", s_conversions, s_failures);

    return ESP_OK;
}

bool tinybms_adapter_is_ready(void)
{
    // Check if critical live data AND configuration registers are cached
    bool ready = true;

    // Critical live data registers (must be available for real-time telemetry)
    ready &= is_cached(0x0024); // pack_voltage_v
    ready &= is_cached(0x0026); // pack_current_a
    ready &= is_cached(0x002E); // state_of_charge

    // Battery configuration
    ready &= is_cached(0x0132); // battery_capacity_ah
    ready &= is_cached(0x0133); // series_cell_count

    // Safety limits
    ready &= is_cached(0x013B); // overvoltage_cutoff_mv
    ready &= is_cached(0x013C); // undervoltage_cutoff_mv
    ready &= is_cached(0x013D); // discharge_overcurrent_a
    ready &= is_cached(0x013E); // charge_overcurrent_a

    if (!ready) {
        ESP_LOGD(TAG, "Adapter not ready - missing critical registers");
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
