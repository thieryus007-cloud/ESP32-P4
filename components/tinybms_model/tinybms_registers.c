/**
 * @file tinybms_registers.c
 * @brief TinyBMS Register Catalog Implementation
 *
 * Complete catalog of 34 TinyBMS registers
 * Source: Exemple/mac-local/data/registers.json
 */

#include "tinybms_registers.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char *TAG = "tinybms_regs";

// Enum definitions for registers

static const enum_entry_t enum_cell_count[] = {
    {4, "4 cells"}, {5, "5 cells"}, {6, "6 cells"}, {7, "7 cells"},
    {8, "8 cells"}, {9, "9 cells"}, {10, "10 cells"}, {11, "11 cells"},
    {12, "12 cells"}, {13, "13 cells"}, {14, "14 cells"}, {15, "15 cells"},
    {16, "16 cells"}
};

static const enum_entry_t enum_invert_current[] = {
    {0, "Normal"}, {1, "Invert"}
};

static const enum_entry_t enum_charger_type[] = {
    {0, "Variable (Reserved)"}, {1, "Constant Current"}
};

static const enum_entry_t enum_load_switch_type[] = {
    {0, "FET"}, {1, "AIDO1"}, {2, "AIDO2"}, {3, "DIDO1"}, {4, "DIDO2"},
    {5, "AIHO1 Active Low"}, {6, "AIHO1 Active High"},
    {7, "AIHO2 Active Low"}, {8, "AIHO2 Active High"}
};

static const enum_entry_t enum_charger_switch_type[] = {
    {1, "Charge FET"}, {2, "AIDO1"}, {3, "AIDO2"}, {4, "DIDO1"}, {5, "DIDO2"},
    {6, "AIHO1 Active Low"}, {7, "AIHO1 Active High"},
    {8, "AIHO2 Active Low"}, {9, "AIHO2 Active High"}
};

static const enum_entry_t enum_ignition_source[] = {
    {0, "Disabled"}, {1, "AIDO1"}, {2, "AIDO2"}, {3, "DIDO1"},
    {4, "DIDO2"}, {5, "AIHO1"}, {6, "AIHO2"}
};

static const enum_entry_t enum_charger_detection[] = {
    {1, "Internal"}, {2, "AIDO1"}, {3, "AIDO2"}, {4, "DIDO1"},
    {5, "DIDO2"}, {6, "AIHO1"}, {7, "AIHO2"}
};

static const enum_entry_t enum_precharge_pin[] = {
    {0, "Disabled"}, {2, "Discharge FET"}, {3, "AIDO1"}, {4, "AIDO2"},
    {5, "DIDO1"}, {6, "DIDO2"}, {7, "AIHO1 Active Low"},
    {8, "AIHO1 Active High"}, {9, "AIHO2 Active Low"}, {16, "AIHO2 Active High"}
};

static const enum_entry_t enum_precharge_duration[] = {
    {0, "0.1 s"}, {1, "0.2 s"}, {2, "0.5 s"}, {3, "1 s"},
    {4, "2 s"}, {5, "3 s"}, {6, "4 s"}, {7, "5 s"}
};

static const enum_entry_t enum_temp_sensor_type[] = {
    {0, "Dual 10K NTC"}, {1, "Multipoint Active Sensor"}
};

static const enum_entry_t enum_operation_mode[] = {
    {0, "Dual Port"}, {1, "Single Port"}
};

static const enum_entry_t enum_broadcast_interval[] = {
    {0, "Disabled"}, {1, "0.1 s"}, {2, "0.2 s"}, {3, "0.5 s"},
    {4, "1 s"}, {5, "2 s"}, {6, "5 s"}, {7, "10 s"}
};

static const enum_entry_t enum_comm_protocol[] = {
    {0, "Binary"}, {1, "ASCII"}
};

// Main register catalog (34 registers)
static const register_descriptor_t register_catalog[TINYBMS_REGISTER_COUNT] = {
    // Battery group (9 registers)
    {0x012C, "fully_charged_voltage_mv", "Fully Charged Voltage", "mV", REG_GROUP_BATTERY, "Cell voltage when considered fully charged", TYPE_UINT16, false, 1.0f, 0, true, 1200, true, 4500, 10, 3650, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x012D, "fully_discharged_voltage_mv", "Fully Discharged Voltage", "mV", REG_GROUP_BATTERY, "Cell voltage considered fully discharged", TYPE_UINT16, false, 1.0f, 0, true, 1000, true, 3500, 10, 3250, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x012F, "early_balancing_threshold_mv", "Early Balancing Threshold", "mV", REG_GROUP_BATTERY, "Cell voltage threshold that enables balancing", TYPE_UINT16, false, 1.0f, 0, true, 1000, true, 4500, 10, 3400, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0130, "charge_finished_current_ma", "Charge Finished Current", "mA", REG_GROUP_BATTERY, "Current threshold signalling charge completion", TYPE_UINT16, false, 1.0f, 0, true, 100, true, 5000, 10, 1000, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0131, "peak_discharge_current_a", "Peak Discharge Current Cutoff", "A", REG_GROUP_BATTERY, "Instantaneous discharge protection limit", TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 70, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0132, "battery_capacity_ah", "Battery Capacity", "Ah", REG_GROUP_BATTERY, "Pack capacity used for SOC calculations", TYPE_UINT16, false, 0.01f, 2, true, 10, true, 65500, 1, 31400, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0133, "cell_count", "Number of Series Cells", "cells", REG_GROUP_BATTERY, "Configured number of series-connected cells", TYPE_ENUM, false, 1.0f, 0, true, 4, true, 16, 1, 16, VALUE_CLASS_ENUM, enum_cell_count, 13},
    {0x0134, "allowed_disbalance_mv", "Allowed Cell Disbalance", "mV", REG_GROUP_BATTERY, "Maximum per-cell delta before alarms", TYPE_UINT16, false, 1.0f, 0, true, 15, true, 100, 1, 15, VALUE_CLASS_NUMERIC, NULL, 0},

    // Charger group (2 registers)
    {0x0136, "charger_startup_delay_s", "Charger Startup Delay", "s", REG_GROUP_CHARGER, "Delay before enabling the charger", TYPE_UINT16, false, 1.0f, 0, true, 5, true, 60, 1, 20, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0137, "charger_disable_delay_s", "Charger Disable Delay", "s", REG_GROUP_CHARGER, "Delay before disabling charger after fault", TYPE_UINT16, false, 1.0f, 0, true, 0, true, 60, 1, 5, VALUE_CLASS_NUMERIC, NULL, 0},

    // Safety group (6 registers)
    {0x013B, "overvoltage_cutoff_mv", "Over-voltage Cutoff", "mV", REG_GROUP_SAFETY, "Cell voltage threshold to stop charging", TYPE_UINT16, false, 1.0f, 0, true, 1200, true, 4500, 10, 3800, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x013C, "undervoltage_cutoff_mv", "Under-voltage Cutoff", "mV", REG_GROUP_SAFETY, "Cell voltage threshold to stop discharging", TYPE_UINT16, false, 1.0f, 0, true, 800, true, 3500, 10, 2800, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x013D, "discharge_overcurrent_a", "Discharge Over-current Cutoff", "A", REG_GROUP_SAFETY, "Current limit for discharge protection", TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 65, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x013E, "charge_overcurrent_a", "Charge Over-current Cutoff", "A", REG_GROUP_SAFETY, "Current limit for charge protection", TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 90, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x013F, "overheat_cutoff_c", "Overheat Cutoff", "°C", REG_GROUP_SAFETY, "Temperature threshold to stop charging/discharging", TYPE_UINT16, false, 1.0f, 0, true, 20, true, 90, 1, 60, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0140, "low_temp_charge_cutoff_c", "Low Temperature Charge Cutoff", "°C", REG_GROUP_SAFETY, "Temperature below which charging is disabled", TYPE_INT16, false, 1.0f, 0, true, -40, true, 10, 1, 0, VALUE_CLASS_NUMERIC, NULL, 0},

    // Advanced group (5 registers)
    {0x0141, "charge_restart_level_percent", "Charge Restart Level", "%", REG_GROUP_ADVANCED, "SOC threshold to re-enable charging", TYPE_UINT16, false, 1.0f, 0, true, 60, true, 95, 1, 80, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0142, "battery_max_cycles", "Battery Maximum Cycles Count", "cycles", REG_GROUP_ADVANCED, "Total cycle counter limit", TYPE_UINT16, false, 1.0f, 0, true, 10, true, 65000, 10, 5000, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0143, "state_of_health_permille", "State Of Health", "‰", REG_GROUP_ADVANCED, "Settable SOH value", TYPE_UINT16, false, 0.01f, 2, true, 0, true, 50000, 1, 100, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0148, "state_of_charge_permille", "State Of Charge", "‰", REG_GROUP_ADVANCED, "Manual SOC override", TYPE_UINT16, false, 0.01f, 2, true, 0, true, 50000, 1, 40, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x0149, "invert_ext_current_sensor", "Invert External Current Sensor", "flag", REG_GROUP_ADVANCED, "Invert external shunt polarity", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_invert_current, 2},

    // System group (13 registers)
    {0x014A, "charger_type", "Charger Type", "mode", REG_GROUP_SYSTEM, "Defines charger behavior", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1, VALUE_CLASS_ENUM, enum_charger_type, 2},
    {0x014B, "load_switch_type", "Load Switch Type", "mode", REG_GROUP_SYSTEM, "Output used for load switching", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_load_switch_type, 9},
    {0x014C, "automatic_recovery_count", "Automatic Recovery Attempts", "count", REG_GROUP_SYSTEM, "Number of automatic recovery tries", TYPE_UINT16, false, 1.0f, 0, true, 1, true, 30, 1, 5, VALUE_CLASS_NUMERIC, NULL, 0},
    {0x014D, "charger_switch_type", "Charger Switch Type", "mode", REG_GROUP_SYSTEM, "Output controlling the charger", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1, VALUE_CLASS_ENUM, enum_charger_switch_type, 9},
    {0x014E, "ignition_source", "Ignition Source", "mode", REG_GROUP_SYSTEM, "Input used to sense ignition", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_ignition_source, 7},
    {0x014F, "charger_detection_source", "Charger Detection Source", "mode", REG_GROUP_SYSTEM, "Source used to detect presence of charger", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1, VALUE_CLASS_ENUM, enum_charger_detection, 7},
    {0x0151, "precharge_pin", "Precharge Output", "mode", REG_GROUP_SYSTEM, "Output used to precharge the contactor", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_precharge_pin, 10},
    {0x0152, "precharge_duration", "Precharge Duration", "s", REG_GROUP_SYSTEM, "Duration of precharge before closing main contactor", TYPE_ENUM, false, 1.0f, 1, false, 0, false, 0, 0, 7, VALUE_CLASS_ENUM, enum_precharge_duration, 8},
    {0x0153, "temperature_sensor_type", "Temperature Sensor Type", "mode", REG_GROUP_SYSTEM, "Defines type of connected temp sensors", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_temp_sensor_type, 2},
    {0x0154, "operation_mode", "BMS Operation Mode", "mode", REG_GROUP_SYSTEM, "Dual or single port operation", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_operation_mode, 2},
    {0x0155, "single_port_switch_type", "Single Port Switch Type", "mode", REG_GROUP_SYSTEM, "Output used when operating in single-port mode", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_load_switch_type, 9},
    {0x0156, "broadcast_interval", "Broadcast Interval", "mode", REG_GROUP_SYSTEM, "UART broadcast period", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0, VALUE_CLASS_ENUM, enum_broadcast_interval, 8},
    {0x0157, "communication_protocol", "Communication Protocol", "mode", REG_GROUP_SYSTEM, "Protocol used on UART port", TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1, VALUE_CLASS_ENUM, enum_comm_protocol, 2},
};

// Public functions

const register_descriptor_t* tinybms_get_register_catalog(void)
{
    return register_catalog;
}

const register_descriptor_t* tinybms_get_register_by_address(uint16_t address)
{
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        if (register_catalog[i].address == address) {
            return &register_catalog[i];
        }
    }
    return NULL;
}

const register_descriptor_t* tinybms_get_register_by_key(const char *key)
{
    if (key == NULL) {
        return NULL;
    }

    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        if (strcmp(register_catalog[i].key, key) == 0) {
            return &register_catalog[i];
        }
    }
    return NULL;
}

float tinybms_raw_to_user(const register_descriptor_t *desc, uint16_t raw_value)
{
    if (desc == NULL) {
        return 0.0f;
    }

    if (desc->value_class == VALUE_CLASS_ENUM) {
        return (float)raw_value;
    }

    // Handle signed int16
    float value;
    if (desc->type == TYPE_INT16) {
        value = (int16_t)raw_value;
    } else {
        value = (float)raw_value;
    }

    // Apply scale
    value *= desc->scale;

    // Apply precision rounding
    if (desc->precision > 0) {
        float factor = powf(10.0f, desc->precision);
        value = roundf(value * factor) / factor;
    }

    return value;
}

esp_err_t tinybms_user_to_raw(const register_descriptor_t *desc, float user_value,
                               uint16_t *raw_value)
{
    if (desc == NULL || raw_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (desc->value_class == VALUE_CLASS_ENUM) {
        // Validate enum value
        bool valid = false;
        for (int i = 0; i < desc->enum_count; i++) {
            if (desc->enum_values[i].value == (uint16_t)user_value) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            ESP_LOGW(TAG, "Invalid enum value %d for register %s",
                     (int)user_value, desc->key);
            return ESP_ERR_INVALID_ARG;
        }
        *raw_value = (uint16_t)user_value;
        return ESP_OK;
    }

    // Convert user to raw
    float raw_float = user_value / desc->scale;

    // Align to step if specified
    if (desc->step_raw > 0) {
        float base = desc->has_min ? (float)desc->min_raw : 0.0f;
        int32_t steps = roundf((raw_float - base) / desc->step_raw);
        raw_float = base + steps * desc->step_raw;
    }

    // Clamp to min/max
    if (desc->has_min && raw_float < desc->min_raw) {
        ESP_LOGW(TAG, "Value %.2f below minimum for %s", raw_float, desc->key);
        raw_float = desc->min_raw;
    }
    if (desc->has_max && raw_float > desc->max_raw) {
        ESP_LOGW(TAG, "Value %.2f above maximum for %s", raw_float, desc->key);
        raw_float = desc->max_raw;
    }

    // Handle signed values
    if (desc->type == TYPE_INT16) {
        int16_t signed_val = (int16_t)roundf(raw_float);
        *raw_value = (uint16_t)signed_val;
    } else {
        *raw_value = (uint16_t)roundf(raw_float);
    }

    return ESP_OK;
}

bool tinybms_validate_raw(const register_descriptor_t *desc, uint16_t raw_value)
{
    if (desc == NULL) {
        return false;
    }

    if (desc->value_class == VALUE_CLASS_ENUM) {
        for (int i = 0; i < desc->enum_count; i++) {
            if (desc->enum_values[i].value == raw_value) {
                return true;
            }
        }
        return false;
    }

    // For numeric values, check min/max
    int32_t value = (desc->type == TYPE_INT16) ? (int16_t)raw_value : raw_value;

    if (desc->has_min && value < desc->min_raw) {
        return false;
    }
    if (desc->has_max && value > desc->max_raw) {
        return false;
    }

    return true;
}

const char* tinybms_get_enum_label(const register_descriptor_t *desc, uint16_t value)
{
    if (desc == NULL || desc->value_class != VALUE_CLASS_ENUM) {
        return NULL;
    }

    for (int i = 0; i < desc->enum_count; i++) {
        if (desc->enum_values[i].value == value) {
            return desc->enum_values[i].label;
        }
    }

    return NULL;
}

const char* tinybms_get_group_name(register_group_t group)
{
    static const char *group_names[] = {
        "battery", "charger", "safety", "advanced", "system"
    };

    if (group >= REG_GROUP_MAX) {
        return "unknown";
    }

    return group_names[group];
}
