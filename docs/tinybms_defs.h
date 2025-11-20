/*
 * tinybms_defs.h
 * Définitions complètes basées sur TinyBMS Communication Protocols Rev D
 */
#pragma once

#include <stdint.h>

#define TINYBMS_START_BYTE                  0xAA
#define TINYBMS_PROTOCOL_CRC_POLY           0x8005

// --- Commandes UART ---
#define TINYBMS_CMD_RESET_CLEAR             0x02 //
#define TINYBMS_CMD_READ_REG_BLOCK          0x07
#define TINYBMS_CMD_READ_REG_INDIVIDUAL     0x09
#define TINYBMS_CMD_WRITE_REG_BLOCK         0x0B
#define TINYBMS_CMD_WRITE_REG_INDIVIDUAL    0x0D //
#define TINYBMS_CMD_READ_EVENTS_NEWEST      0x11 //
#define TINYBMS_CMD_READ_EVENTS_ALL         0x12
#define TINYBMS_CMD_READ_PACK_VOLTAGE       0x14 //
#define TINYBMS_CMD_READ_PACK_CURRENT       0x15 //
#define TINYBMS_CMD_READ_MAX_CELL_V         0x16
#define TINYBMS_CMD_READ_MIN_CELL_V         0x17
#define TINYBMS_CMD_READ_ONLINE_STATUS      0x18 //
#define TINYBMS_CMD_READ_LIFETIME_CNTR      0x19
#define TINYBMS_CMD_READ_SOC                0x1A //
#define TINYBMS_CMD_READ_TEMPS              0x1B //
#define TINYBMS_CMD_READ_CELL_VOLTAGES      0x1C //
#define TINYBMS_CMD_READ_SETTINGS_INFO      0x1D
#define TINYBMS_CMD_READ_VERSION            0x1E
#define TINYBMS_CMD_READ_VERSION_EXT        0x1F
#define TINYBMS_CMD_READ_CALC_VALUES        0x20 //

// --- Registres Clés (Mapping Chapitre 3) ---
#define TINYBMS_REG_LIFETIME_COUNTER        32
#define TINYBMS_REG_PACK_VOLTAGE            36
#define TINYBMS_REG_PACK_CURRENT            38
#define TINYBMS_REG_SOC                     46
#define TINYBMS_REG_ONLINE_STATUS           50

// --- Configuration (W) ---
#define TINYBMS_REG_CFG_FULLY_CHARGED_V     300
#define TINYBMS_REG_CFG_FULLY_DISCHARGED_V  301
#define TINYBMS_REG_CFG_OVER_VOLTAGE_CUT    315
#define TINYBMS_REG_CFG_UNDER_VOLTAGE_CUT   316
#define TINYBMS_REG_CFG_DIS_OC_CUT          317
#define TINYBMS_REG_CFG_CHG_OC_CUT          318

// --- Options Reset ---
enum TinyBmsResetOpt {
    RESET_CLEAR_EVENTS = 0x01, //
    RESET_CLEAR_STATS  = 0x02, //
    RESET_REBOOT       = 0x05  //
};
