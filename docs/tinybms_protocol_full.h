/*
 * tinybms_protocol_full.h
 * Transcription EXHAUSTIVE du document "Tiny BMS Communication Protocols Rev D"
 * Sources: Pages 1 à 27
 */

#ifndef TINYBMS_PROTOCOL_FULL_H
#define TINYBMS_PROTOCOL_FULL_H

#include <stdint.h>

// ==============================================================================
// CHAPITRE 1: UART COMMUNICATION COMMANDS [cite: 46]
// ==============================================================================
#define TINYBMS_UART_START_BYTE             0xAA
#define TINYBMS_CRC_POLY                    0x8005 // x16 + x15 + x2 + 1 [cite: 272]

// --- 1.1 Commandes de Base ---
#define CMD_ACK                             0x01 // [cite: 51]
#define CMD_NACK                            0x00 // [cite: 49]
#define CMD_RESET_CLEAR                     0x02 // Reset & Clear [cite: 134]
#define CMD_READ_REG_BLOCK_MODBUS           0x03 // Read Block (Modbus) [cite: 110]
#define CMD_READ_REG_BLOCK_PROP             0x07 // Read Block (Proprietary) [cite: 55]
#define CMD_READ_REG_INDIVIDUAL             0x09 // Read Individual [cite: 71]
#define CMD_WRITE_REG_BLOCK_PROP            0x0B // Write Block (Proprietary) [cite: 84]
#define CMD_WRITE_REG_INDIVIDUAL            0x0D // Write Individual [cite: 93]
#define CMD_WRITE_REG_BLOCK_MODBUS          0x10 // Write Block (Modbus) [cite: 118]

// --- 1.1 Commandes Événements ---
#define CMD_READ_EVENTS_NEWEST              0x11 // [cite: 140]
#define CMD_READ_EVENTS_ALL                 0x12 // [cite: 151]

// --- 1.1 Commandes "Live Data" (Accès direct) ---
#define CMD_READ_PACK_VOLTAGE               0x14 // Reg 36 [cite: 162]
#define CMD_READ_PACK_CURRENT               0x15 // Reg 38 [cite: 169]
#define CMD_READ_MAX_CELL_VOLTAGE           0x16 // Reg 41 [cite: 175]
#define CMD_READ_MIN_CELL_VOLTAGE           0x17 // Reg 40 [cite: 179]
#define CMD_READ_ONLINE_STATUS              0x18 // Reg 50 [cite: 184]
#define CMD_READ_LIFETIME_COUNTER           0x19 // Reg 32 [cite: 193]
#define CMD_READ_SOC                        0x1A // Reg 46 [cite: 201]
#define CMD_READ_TEMPS                      0x1B // Reg 48, 42, 43 [cite: 207]
#define CMD_READ_CELL_VOLTAGES              0x1C // Toutes les cellules [cite: 220]
#define CMD_READ_SETTINGS_VALUES            0x1D // Min/Max/Def/Cur [cite: 226]
#define CMD_READ_VERSION                    0x1E // [cite: 234]
#define CMD_READ_VERSION_EXT                0x1F // [cite: 242]
#define CMD_READ_CALC_VALUES                0x20 // Speed, Dist, Time [cite: 256]

// ==============================================================================
// CHAPITRE 3: REGISTERS MAP [cite: 656]
// ==============================================================================

// --- 3.1 Live Data (Lecture Seule) [cite: 661] ---
#define REG_CELL_VOLTAGE_1                  0
#define REG_CELL_VOLTAGE_2                  1
#define REG_CELL_VOLTAGE_3                  2
#define REG_CELL_VOLTAGE_4                  3
#define REG_CELL_VOLTAGE_5                  4
#define REG_CELL_VOLTAGE_6                  5
#define REG_CELL_VOLTAGE_7                  6
#define REG_CELL_VOLTAGE_8                  7
#define REG_CELL_VOLTAGE_9                  8
#define REG_CELL_VOLTAGE_10                 9
#define REG_CELL_VOLTAGE_11                 10
#define REG_CELL_VOLTAGE_12                 11
#define REG_CELL_VOLTAGE_13                 12
#define REG_CELL_VOLTAGE_14                 13
#define REG_CELL_VOLTAGE_15                 14
#define REG_CELL_VOLTAGE_16                 15
// 16-31 Reserved
#define REG_LIFETIME_COUNTER                32  // UINT32, Res 1s
// 33 Reserved
#define REG_ESTIMATED_TIME_LEFT             34  // UINT32, Res 1s
// 35 Reserved
#define REG_PACK_VOLTAGE                    36  // FLOAT, Res 1V -> Attention PDF dit FLOAT ici mais Code 0x14 dit FLOAT
#define REG_PACK_CURRENT                    38  // FLOAT, Res 1A
#define REG_MIN_CELL_VOLTAGE                40  // UINT16, Res 1mV
#define REG_MAX_CELL_VOLTAGE                41  // UINT16, Res 1mV
#define REG_TEMP_EXT_SENSOR_1               42  // INT16, Res 0.1 C
#define REG_TEMP_EXT_SENSOR_2               43  // INT16, Res 0.1 C
#define REG_DISTANCE_LEFT                   44  // UINT16, Res 1km
#define REG_STATE_OF_HEALTH                 45  // UINT16, 0-50000 (0.002%)
#define REG_STATE_OF_CHARGE                 46  // UINT32, Res 0.000001%
#define REG_INTERNAL_TEMP                   48  // INT16, Res 0.1 C
// 49 Reserved
#define REG_ONLINE_STATUS                   50  // UINT16 (Codes ci-dessous)
#define REG_BALANCING_DECISION              51  // UINT16 (Bitmask)
#define REG_REAL_BALANCING                  52  // UINT16 (Bitmask)
#define REG_NUM_DETECTED_CELLS              53  // UINT16
#define REG_SPEED                           54  // FLOAT km/h

// Codes Status (Reg 50) [cite: 190]
#define STATUS_CHARGING                     0x91
#define STATUS_FULLY_CHARGED                0x92
#define STATUS_DISCHARGING                  0x93
#define STATUS_REGENERATION                 0x96
#define STATUS_IDLE                         0x97
#define STATUS_FAULT                        0x9B

// --- 3.2 Statistics Data [cite: 668] ---
#define REG_STATS_TOTAL_DISTANCE            100 // UINT32, 0.01km (occupies registers 100-101)
#define REG_STATS_MAX_DISCHARGE_CUR         102 // UINT16, 100mA
#define REG_STATS_MAX_CHARGE_CUR            103 // UINT16, 100mA
#define REG_STATS_MAX_CELL_DIFF             104 // UINT16, 0.1mV
#define REG_STATS_UV_COUNT                  105 // UINT16
#define REG_STATS_OV_COUNT                  106 // UINT16
#define REG_STATS_OC_DISCHARGE_COUNT        107 // UINT16
#define REG_STATS_OC_CHARGE_COUNT           108 // UINT16
#define REG_STATS_OVERHEAT_COUNT            109 // UINT16
#define REG_STATS_CHARGING_COUNT            111 // UINT16
#define REG_STATS_FULL_CHARGE_COUNT         112 // UINT16
#define REG_STATS_MIN_PACK_TEMP             113 // INT8
#define REG_STATS_MAX_PACK_TEMP             114 // INT8
#define REG_STATS_LAST_RESET_EVENT          115 // UINT8 (Codes reset)
#define REG_STATS_LAST_WAKEUP_EVENT         116 // UINT8 (Codes wakeup) - NOTE: Table has layout issues in PDF, verifying...
#define REG_STATS_LAST_CLEARED_TS           116 // UINT32

// --- 3.4 Settings (Read/Write) [cite: 674] ---
#define REG_CFG_FULLY_CHARGED_V             300 // UINT16 mV
#define REG_CFG_FULLY_DISCHARGED_V          301 // UINT16 mV
#define REG_CFG_EARLY_BALANCING_THR         303 // UINT16 mV
#define REG_CFG_CHARGE_FINISHED_CUR         304 // UINT16 mA
#define REG_CFG_PEAK_DISCHARGE_CUT          305 // UINT16 A
#define REG_CFG_BATTERY_CAPACITY            306 // UINT16 0.01Ah
#define REG_CFG_SERIES_CELLS                307 // UINT16
#define REG_CFG_ALLOWED_DISBALANCE          308 // UINT16 mV
#define REG_CFG_CHARGER_STARTUP_DELAY       310 // UINT16 s
#define REG_CFG_CHARGER_DISABLE_DELAY       311 // UINT16 s
#define REG_CFG_PULSES_PER_UNIT             312 // UINT32
#define REG_CFG_DISTANCE_UNIT_NAME          314 // UINT16 (Enum)
#define REG_CFG_OV_CUTOFF                   315 // UINT16 mV
#define REG_CFG_UV_CUTOFF                   316 // UINT16 mV
#define REG_CFG_DISCHARGE_OC_CUTOFF         317 // UINT16 A
#define REG_CFG_CHARGE_OC_CUTOFF            318 // UINT16 A
#define REG_CFG_OVERHEAT_CUTOFF             319 // INT16 C
#define REG_CFG_LOW_TEMP_CUTOFF             320 // INT16 C
#define REG_CFG_CHARGE_RESTART_LEVEL        321 // UINT16 %
#define REG_CFG_MAX_CYCLES                  322 // UINT16
#define REG_CFG_MANUAL_SOH                  323 // UINT16
#define REG_CFG_MANUAL_SOC                  328 // UINT16
// Configuration Flags (Reg 329-343 contain complex bitmasks) [cite: 680]
#define REG_CFG_FLAGS_INV_CURR              329
#define REG_CFG_CHARGER_TYPE                330
#define REG_CFG_LOAD_SWITCH_TYPE            331
#define REG_CFG_AUTO_RECOVERY               332
#define REG_CFG_CHARGER_SWITCH_TYPE         333
#define REG_CFG_IGNITION                    334
#define REG_CFG_CHARGER_DETECTION           335
#define REG_CFG_SPEED_SENSOR                336
#define REG_CFG_PRECHARGE_PIN               337
#define REG_CFG_PRECHARGE_DURATION          338
#define REG_CFG_TEMP_SENSOR_TYPE            339
#define REG_CFG_OP_MODE                     340
#define REG_CFG_SINGLE_PORT_SWITCH          341
#define REG_CFG_BROADCAST_TIME              342
#define REG_CFG_PROTOCOL                    343

// ==============================================================================
// CHAPITRE 4: FAULT CODES [cite: 695]
// ==============================================================================
#define FAULT_UV_CUTOFF                     0x02
#define FAULT_OV_CUTOFF                     0x03
#define FAULT_OT_CUTOFF                     0x04
#define FAULT_DISCHARGE_OC                  0x05
#define FAULT_CHARGE_OC                     0x06
#define FAULT_REGEN_OC                      0x07
#define FAULT_LOW_TEMP                      0x0A
#define FAULT_CHARGER_SWITCH                0x0B
#define FAULT_LOAD_SWITCH                   0x0C
#define FAULT_SINGLE_PORT_SWITCH            0x0D
#define FAULT_EXT_CURRENT_SENSOR_DISC       0x0E
#define FAULT_EXT_CURRENT_SENSOR_CONN       0x0F

// Warnings [cite: 697]
#define WARN_FULLY_DISCHARGED               0x31
#define WARN_LOW_TEMP_CHARGE                0x32
#define WARN_CHG_DONE_HIGH_VOLT             0x38
#define WARN_CHG_DONE_LOW_VOLT              0x39

#endif // TINYBMS_PROTOCOL_FULL_H
