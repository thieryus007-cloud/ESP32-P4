/**
 * @file tinybms_registers.cpp
 * @brief TinyBMS Register Catalog Implementation (C++ modernization)
 */

extern "C" {
#include "tinybms_registers.h"
#include "esp_log.h"
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string_view>

namespace {

using RegisterCatalog = std::array<register_descriptor_t, TINYBMS_REGISTER_COUNT>;

constexpr std::array<enum_entry_t, 13> kCellCount = {{{
    {4, "4 cells"}, {5, "5 cells"}, {6, "6 cells"}, {7, "7 cells"},
    {8, "8 cells"}, {9, "9 cells"}, {10, "10 cells"}, {11, "11 cells"},
    {12, "12 cells"}, {13, "13 cells"}, {14, "14 cells"}, {15, "15 cells"},
    {16, "16 cells"}
}};

constexpr std::array<enum_entry_t, 2> kInvertCurrent = {{{
    {0, "Normal"}, {1, "Invert"}
}};

constexpr std::array<enum_entry_t, 2> kChargerType = {{{
    {0, "Variable (Reserved)"}, {1, "Constant Current"}
}};

constexpr std::array<enum_entry_t, 9> kLoadSwitchType = {{{
    {0, "FET"}, {1, "AIDO1"}, {2, "AIDO2"}, {3, "DIDO1"}, {4, "DIDO2"},
    {5, "AIHO1 Active Low"}, {6, "AIHO1 Active High"},
    {7, "AIHO2 Active Low"}, {8, "AIHO2 Active High"}
}};

constexpr std::array<enum_entry_t, 9> kChargerSwitchType = {{{
    {1, "Charge FET"}, {2, "AIDO1"}, {3, "AIDO2"}, {4, "DIDO1"}, {5, "DIDO2"},
    {6, "AIHO1 Active Low"}, {7, "AIHO1 Active High"},
    {8, "AIHO2 Active Low"}, {9, "AIHO2 Active High"}
}};

constexpr std::array<enum_entry_t, 7> kIgnitionSource = {{{
    {0, "Disabled"}, {1, "AIDO1"}, {2, "AIDO2"}, {3, "DIDO1"},
    {4, "DIDO2"}, {5, "AIHO1"}, {6, "AIHO2"}
}};

constexpr std::array<enum_entry_t, 7> kChargerDetection = {{{
    {1, "Internal"}, {2, "AIDO1"}, {3, "AIDO2"}, {4, "DIDO1"},
    {5, "DIDO2"}, {6, "AIHO1"}, {7, "AIHO2"}
}};

constexpr std::array<enum_entry_t, 10> kPrechargePin = {{{
    {0, "Disabled"}, {2, "Discharge FET"}, {3, "AIDO1"}, {4, "AIDO2"},
    {5, "DIDO1"}, {6, "DIDO2"}, {7, "AIHO1 Active Low"},
    {8, "AIHO1 Active High"}, {9, "AIHO2 Active Low"}, {16, "AIHO2 Active High"}
}};

constexpr std::array<enum_entry_t, 8> kPrechargeDuration = {{{
    {0, "0.1 s"}, {1, "0.2 s"}, {2, "0.5 s"}, {3, "1 s"},
    {4, "2 s"}, {5, "3 s"}, {6, "4 s"}, {7, "5 s"}
}};

constexpr std::array<enum_entry_t, 2> kTempSensorType = {{{
    {0, "Dual 10K NTC"}, {1, "Multipoint Active Sensor"}
}};

constexpr std::array<enum_entry_t, 2> kOperationMode = {{{
    {0, "Dual Port"}, {1, "Single Port"}
}};

constexpr std::array<enum_entry_t, 8> kBroadcastInterval = {{{
    {0, "Disabled"}, {1, "0.1 s"}, {2, "0.2 s"}, {3, "0.5 s"},
    {4, "1 s"}, {5, "2 s"}, {6, "5 s"}, {7, "10 s"}
}};

constexpr std::array<enum_entry_t, 2> kCommProtocol = {{{
    {0, "Binary"}, {1, "ASCII"}
}};

constexpr register_descriptor_t make_descriptor(
    uint16_t address,
    const char *key,
    const char *label,
    const char *unit,
    register_group_t group,
    const char *comment,
    register_type_t type,
    bool read_only,
    float scale,
    uint8_t precision,
    bool has_min,
    int32_t min_raw,
    bool has_max,
    int32_t max_raw,
    uint16_t step_raw,
    uint16_t default_raw,
    value_class_t value_class,
    const enum_entry_t *enum_values,
    uint8_t enum_count) {
    register_descriptor_t desc = {
        address,
        key,
        label,
        unit,
        group,
        comment,
        type,
        read_only,
        scale,
        precision,
        has_min,
        min_raw,
        has_max,
        max_raw,
        step_raw,
        default_raw,
        value_class,
        enum_values,
        enum_count
    };
    return desc;
}

constexpr RegisterCatalog kRegisterCatalog = {{{
    // =========================================================================
    // Live Data group (Read-only telemetry registers)
    // =========================================================================

    // Cell voltages (REG 0-15, Protocol Rev D page 18)
    make_descriptor(0, "cell1_voltage_mv", "Cell 1 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(1, "cell2_voltage_mv", "Cell 2 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(2, "cell3_voltage_mv", "Cell 3 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(3, "cell4_voltage_mv", "Cell 4 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(4, "cell5_voltage_mv", "Cell 5 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(5, "cell6_voltage_mv", "Cell 6 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(6, "cell7_voltage_mv", "Cell 7 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(7, "cell8_voltage_mv", "Cell 8 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(8, "cell9_voltage_mv", "Cell 9 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(9, "cell10_voltage_mv", "Cell 10 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(10, "cell11_voltage_mv", "Cell 11 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(11, "cell12_voltage_mv", "Cell 12 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(12, "cell13_voltage_mv", "Cell 13 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(13, "cell14_voltage_mv", "Cell 14 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(14, "cell15_voltage_mv", "Cell 15 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(15, "cell16_voltage_mv", "Cell 16 Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Individual cell voltage",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Pack voltage and current (REG 36, 38 - Protocol Rev D page 18)
    make_descriptor(36, "pack_voltage_v", "Pack Voltage", "V",
                    REG_GROUP_LIVE_DATA, "Total battery pack voltage",
                    TYPE_FLOAT, true, 1.0f, 1, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(38, "pack_current_a", "Pack Current", "A",
                    REG_GROUP_LIVE_DATA, "Battery pack current (+ charge, - discharge)",
                    TYPE_FLOAT, true, 1.0f, 2, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Min/Max cell voltages (REG 40, 41)
    make_descriptor(40, "min_cell_voltage_mv", "Min Cell Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Lowest cell voltage in pack",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(41, "max_cell_voltage_mv", "Max Cell Voltage", "mV",
                    REG_GROUP_LIVE_DATA, "Highest cell voltage in pack",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // External temperature sensors (REG 42, 43)
    make_descriptor(42, "ext_temp_sensor_1_decidegc", "External Temp Sensor 1", "°C",
                    REG_GROUP_LIVE_DATA, "External temperature sensor 1 (-32768 if disconnected)",
                    TYPE_INT16, true, 0.1f, 1, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(43, "ext_temp_sensor_2_decidegc", "External Temp Sensor 2", "°C",
                    REG_GROUP_LIVE_DATA, "External temperature sensor 2 (-32768 if disconnected)",
                    TYPE_INT16, true, 0.1f, 1, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // State of Health and Charge (REG 45, 46)
    make_descriptor(45, "state_of_health_raw", "State of Health", "%",
                    REG_GROUP_LIVE_DATA, "Battery state of health (0-50000, scale 0.002%)",
                    TYPE_UINT16, true, 0.002f, 1, true, 0, true, 50000, 1, 50000,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(46, "state_of_charge_raw", "State of Charge", "%",
                    REG_GROUP_LIVE_DATA, "Battery state of charge (UINT32, scale 0.000001%)",
                    TYPE_UINT32, true, 0.000001f, 2, true, 0, true, 100000000, 1, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Internal temperature (REG 48)
    make_descriptor(48, "internal_temperature_decidegc", "Internal Temperature", "°C",
                    REG_GROUP_LIVE_DATA, "BMS internal temperature",
                    TYPE_INT16, true, 0.1f, 1, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Online status (REG 50)
    make_descriptor(50, "online_status", "Online Status", "code",
                    REG_GROUP_LIVE_DATA, "BMS operational status (0x91=Charging, 0x92=Full, 0x93=Discharging, 0x96=Regen, 0x97=Idle, 0x9B=Fault)",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Balancing status (REG 51, 52)
    make_descriptor(51, "balancing_decision", "Balancing Decision", "bitmask",
                    REG_GROUP_LIVE_DATA, "Balancing decision bitmask (bit per cell)",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(52, "real_balancing", "Real Balancing", "bitmask",
                    REG_GROUP_LIVE_DATA, "Actual balancing status bitmask (bit per cell)",
                    TYPE_UINT16, true, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // =========================================================================
    // Statistics group (Read-only statistics registers)
    // =========================================================================

    // Total Distance (REG 100 - Protocol Rev D page 20)
    make_descriptor(100, "stats_total_distance_centikm", "Total Distance", "km",
                    REG_GROUP_STATISTICS, "Total distance traveled (UINT32, scale 0.01 km)",
                    TYPE_UINT32, true, 0.01f, 2, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // =========================================================================
    // Battery group
    make_descriptor(0x012C, "fully_charged_voltage_mv", "Fully Charged Voltage", "mV",
                    REG_GROUP_BATTERY, "Cell voltage when considered fully charged",
                    TYPE_UINT16, false, 1.0f, 0, true, 1200, true, 4500, 10, 3650,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x012D, "fully_discharged_voltage_mv", "Fully Discharged Voltage", "mV",
                    REG_GROUP_BATTERY, "Cell voltage considered fully discharged",
                    TYPE_UINT16, false, 1.0f, 0, true, 1000, true, 3500, 10, 3250,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x012F, "early_balancing_threshold_mv", "Early Balancing Threshold", "mV",
                    REG_GROUP_BATTERY, "Cell voltage threshold that enables balancing",
                    TYPE_UINT16, false, 1.0f, 0, true, 1000, true, 4500, 10, 3400,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0130, "charge_finished_current_ma", "Charge Finished Current", "mA",
                    REG_GROUP_BATTERY, "Current threshold signalling charge completion",
                    TYPE_UINT16, false, 1.0f, 0, true, 100, true, 5000, 10, 1000,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0131, "peak_discharge_current_a", "Peak Discharge Current Cutoff", "A",
                    REG_GROUP_BATTERY, "Instantaneous discharge protection limit",
                    TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 70,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0132, "battery_capacity_ah", "Battery Capacity", "Ah",
                    REG_GROUP_BATTERY, "Pack capacity used for SOC calculations",
                    TYPE_UINT16, false, 0.01f, 2, true, 10, true, 65500, 1, 31400,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0133, "cell_count", "Number of Series Cells", "cells",
                    REG_GROUP_BATTERY, "Configured number of series-connected cells",
                    TYPE_ENUM, false, 1.0f, 0, true, 4, true, 16, 1, 16,
                    VALUE_CLASS_ENUM, kCellCount.data(), static_cast<uint8_t>(kCellCount.size())),
    make_descriptor(0x0134, "allowed_disbalance_mv", "Allowed Cell Disbalance", "mV",
                    REG_GROUP_BATTERY, "Maximum per-cell delta before alarms",
                    TYPE_UINT16, false, 1.0f, 0, true, 15, true, 100, 1, 15,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Charger group
    make_descriptor(0x0136, "charger_startup_delay_s", "Charger Startup Delay", "s",
                    REG_GROUP_CHARGER, "Delay before enabling the charger",
                    TYPE_UINT16, false, 1.0f, 0, true, 5, true, 60, 1, 20,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0137, "charger_disable_delay_s", "Charger Disable Delay", "s",
                    REG_GROUP_CHARGER, "Delay before disabling charger after fault",
                    TYPE_UINT16, false, 1.0f, 0, true, 0, true, 60, 1, 5,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Safety group
    make_descriptor(0x013B, "overvoltage_cutoff_mv", "Over-voltage Cutoff", "mV",
                    REG_GROUP_SAFETY, "Cell voltage threshold to stop charging",
                    TYPE_UINT16, false, 1.0f, 0, true, 1200, true, 4500, 10, 3800,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x013C, "undervoltage_cutoff_mv", "Under-voltage Cutoff", "mV",
                    REG_GROUP_SAFETY, "Cell voltage threshold to stop discharging",
                    TYPE_UINT16, false, 1.0f, 0, true, 800, true, 3500, 10, 2800,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x013D, "discharge_overcurrent_a", "Discharge Over-current Cutoff", "A",
                    REG_GROUP_SAFETY, "Current limit for discharge protection",
                    TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 65,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x013E, "charge_overcurrent_a", "Charge Over-current Cutoff", "A",
                    REG_GROUP_SAFETY, "Current limit for charge protection",
                    TYPE_UINT16, false, 1.0f, 0, true, 1, true, 750, 1, 90,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x013F, "overheat_cutoff_c", "Overheat Cutoff", "°C",
                    REG_GROUP_SAFETY, "Temperature threshold to stop charging/discharging",
                    TYPE_UINT16, false, 1.0f, 0, true, 20, true, 90, 1, 60,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0140, "low_temp_charge_cutoff_c", "Low Temperature Charge Cutoff", "°C",
                    REG_GROUP_SAFETY, "Temperature below which charging is disabled",
                    TYPE_INT16, false, 1.0f, 0, true, -40, true, 10, 1, 0,
                    VALUE_CLASS_NUMERIC, nullptr, 0),

    // Advanced group
    make_descriptor(0x0141, "charge_restart_level_percent", "Charge Restart Level", "%",
                    REG_GROUP_ADVANCED, "SOC threshold to re-enable charging",
                    TYPE_UINT16, false, 1.0f, 0, true, 60, true, 95, 1, 80,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0142, "battery_max_cycles", "Battery Maximum Cycles Count", "cycles",
                    REG_GROUP_ADVANCED, "Total cycle counter limit",
                    TYPE_UINT16, false, 1.0f, 0, true, 10, true, 65000, 10, 5000,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0143, "state_of_health_permille", "State Of Health", "‰",
                    REG_GROUP_ADVANCED, "Settable SOH value",
                    TYPE_UINT16, false, 0.01f, 2, true, 0, true, 50000, 1, 100,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0148, "state_of_charge_permille", "State Of Charge", "‰",
                    REG_GROUP_ADVANCED, "Manual SOC override",
                    TYPE_UINT16, false, 0.01f, 2, true, 0, true, 50000, 1, 40,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x0149, "invert_ext_current_sensor", "Invert External Current Sensor", "flag",
                    REG_GROUP_ADVANCED, "Invert external shunt polarity",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kInvertCurrent.data(), static_cast<uint8_t>(kInvertCurrent.size())),

    // System group
    make_descriptor(0x014A, "charger_type", "Charger Type", "mode",
                    REG_GROUP_SYSTEM, "Defines charger behavior",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1,
                    VALUE_CLASS_ENUM, kChargerType.data(), static_cast<uint8_t>(kChargerType.size())),
    make_descriptor(0x014B, "load_switch_type", "Load Switch Type", "mode",
                    REG_GROUP_SYSTEM, "Output used for load switching",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kLoadSwitchType.data(), static_cast<uint8_t>(kLoadSwitchType.size())),
    make_descriptor(0x014C, "automatic_recovery_count", "Automatic Recovery Attempts", "count",
                    REG_GROUP_SYSTEM, "Number of automatic recovery tries",
                    TYPE_UINT16, false, 1.0f, 0, true, 1, true, 30, 1, 5,
                    VALUE_CLASS_NUMERIC, nullptr, 0),
    make_descriptor(0x014D, "charger_switch_type", "Charger Switch Type", "mode",
                    REG_GROUP_SYSTEM, "Output controlling the charger",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1,
                    VALUE_CLASS_ENUM, kChargerSwitchType.data(), static_cast<uint8_t>(kChargerSwitchType.size())),
    make_descriptor(0x014E, "ignition_source", "Ignition Source", "mode",
                    REG_GROUP_SYSTEM, "Input used to sense ignition",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kIgnitionSource.data(), static_cast<uint8_t>(kIgnitionSource.size())),
    make_descriptor(0x014F, "charger_detection_source", "Charger Detection Source", "mode",
                    REG_GROUP_SYSTEM, "Source used to detect presence of charger",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1,
                    VALUE_CLASS_ENUM, kChargerDetection.data(), static_cast<uint8_t>(kChargerDetection.size())),
    make_descriptor(0x0151, "precharge_pin", "Precharge Output", "mode",
                    REG_GROUP_SYSTEM, "Output used to precharge the contactor",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kPrechargePin.data(), static_cast<uint8_t>(kPrechargePin.size())),
    make_descriptor(0x0152, "precharge_duration", "Precharge Duration", "s",
                    REG_GROUP_SYSTEM, "Duration of precharge before closing main contactor",
                    TYPE_ENUM, false, 1.0f, 1, false, 0, false, 0, 0, 7,
                    VALUE_CLASS_ENUM, kPrechargeDuration.data(), static_cast<uint8_t>(kPrechargeDuration.size())),
    make_descriptor(0x0153, "temperature_sensor_type", "Temperature Sensor Type", "mode",
                    REG_GROUP_SYSTEM, "Defines type of connected temp sensors",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kTempSensorType.data(), static_cast<uint8_t>(kTempSensorType.size())),
    make_descriptor(0x0154, "operation_mode", "BMS Operation Mode", "mode",
                    REG_GROUP_SYSTEM, "Dual or single port operation",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kOperationMode.data(), static_cast<uint8_t>(kOperationMode.size())),
    make_descriptor(0x0155, "single_port_switch_type", "Single Port Switch Type", "mode",
                    REG_GROUP_SYSTEM, "Output used when operating in single-port mode",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kLoadSwitchType.data(), static_cast<uint8_t>(kLoadSwitchType.size())),
    make_descriptor(0x0156, "broadcast_interval", "Broadcast Interval", "mode",
                    REG_GROUP_SYSTEM, "UART broadcast period",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 0,
                    VALUE_CLASS_ENUM, kBroadcastInterval.data(), static_cast<uint8_t>(kBroadcastInterval.size())),
    make_descriptor(0x0157, "communication_protocol", "Communication Protocol", "mode",
                    REG_GROUP_SYSTEM, "Protocol used on UART port",
                    TYPE_ENUM, false, 1.0f, 0, false, 0, false, 0, 0, 1,
                    VALUE_CLASS_ENUM, kCommProtocol.data(), static_cast<uint8_t>(kCommProtocol.size()))
}};

const char *TAG = "tinybms_regs";

const register_descriptor_t *find_by_address(uint16_t address) {
    auto it = std::lower_bound(
        kRegisterCatalog.begin(), kRegisterCatalog.end(), address,
        [](const register_descriptor_t &desc, uint16_t addr) { return desc.address < addr; });
    if (it != kRegisterCatalog.end() && it->address == address) {
        return &(*it);
    }
    return nullptr;
}

}  // namespace

extern "C" {

const register_descriptor_t* tinybms_get_register_catalog(void)
{
    return kRegisterCatalog.data();
}

const register_descriptor_t* tinybms_get_register_by_address(uint16_t address)
{
    return find_by_address(address);
}

const register_descriptor_t* tinybms_get_register_by_key(const char *key)
{
    if (key == NULL) {
        return NULL;
    }

    std::string_view wanted_key(key);
    auto it = std::find_if(kRegisterCatalog.begin(), kRegisterCatalog.end(),
                           [wanted_key](const register_descriptor_t &desc) {
                               return wanted_key == desc.key;
                           });
    if (it != kRegisterCatalog.end()) {
        return &(*it);
    }
    return NULL;
}

float tinybms_raw_to_user(const register_descriptor_t *desc, uint16_t raw_value)
{
    if (desc == NULL) {
        return 0.0f;
    }

    if (desc->value_class == VALUE_CLASS_ENUM) {
        return static_cast<float>(raw_value);
    }

    float value = (desc->type == TYPE_INT16)
                      ? static_cast<float>(static_cast<int16_t>(raw_value))
                      : static_cast<float>(raw_value);

    value *= desc->scale;

    if (desc->precision > 0) {
        const float factor = std::pow(10.0f, static_cast<float>(desc->precision));
        value = std::round(value * factor) / factor;
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
        bool valid = std::any_of(desc->enum_values, desc->enum_values + desc->enum_count,
                                 [user_value](const enum_entry_t &entry) {
                                     return entry.value == static_cast<uint16_t>(user_value);
                                 });
        if (!valid) {
            ESP_LOGW(TAG, "Invalid enum value %d for register %s",
                     static_cast<int>(user_value), desc->key);
            return ESP_ERR_INVALID_ARG;
        }
        *raw_value = static_cast<uint16_t>(user_value);
        return ESP_OK;
    }

    float raw_float = user_value / desc->scale;

    if (desc->step_raw > 0) {
        const float base = desc->has_min ? static_cast<float>(desc->min_raw) : 0.0f;
        const float steps = std::round((raw_float - base) / desc->step_raw);
        raw_float = base + steps * desc->step_raw;
    }

    if (desc->has_min && raw_float < desc->min_raw) {
        ESP_LOGW(TAG, "Value %.2f below minimum for %s", raw_float, desc->key);
        raw_float = static_cast<float>(desc->min_raw);
    }
    if (desc->has_max && raw_float > desc->max_raw) {
        ESP_LOGW(TAG, "Value %.2f above maximum for %s", raw_float, desc->key);
        raw_float = static_cast<float>(desc->max_raw);
    }

    if (desc->type == TYPE_INT16) {
        *raw_value = static_cast<uint16_t>(static_cast<int16_t>(std::round(raw_float)));
    } else {
        *raw_value = static_cast<uint16_t>(std::round(raw_float));
    }

    return ESP_OK;
}

bool tinybms_validate_raw(const register_descriptor_t *desc, uint16_t raw_value)
{
    if (desc == NULL) {
        return false;
    }

    if (desc->value_class == VALUE_CLASS_ENUM) {
        return std::any_of(desc->enum_values, desc->enum_values + desc->enum_count,
                           [raw_value](const enum_entry_t &entry) {
                               return entry.value == raw_value;
                           });
    }

    const int32_t value = (desc->type == TYPE_INT16)
                              ? static_cast<int32_t>(static_cast<int16_t>(raw_value))
                              : static_cast<int32_t>(raw_value);

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

    const enum_entry_t *begin = desc->enum_values;
    const enum_entry_t *end = desc->enum_values + desc->enum_count;
    auto it = std::find_if(begin, end, [value](const enum_entry_t &entry) {
        return entry.value == value;
    });
    if (it != end) {
        return it->label;
    }
    return NULL;
}

const char* tinybms_get_group_name(register_group_t group)
{
    static const char *group_names[] = {
        "battery", "charger", "safety", "advanced", "system"
    };

#ifdef __cplusplus
    const size_t index = static_cast<size_t>(group);
    if (index >= static_cast<size_t>(register_group_t::Max)) {
        return "unknown";
    }
#else
    const size_t index = (size_t)group;
    if (index >= REG_GROUP_MAX) {
        return "unknown";
    }
#endif

    return group_names[index];
}

}  // extern "C"
