#pragma once
#include <cstdint>
#include <vector>

// Configuration du port UART
#define TINY_BMS_UART_NUM      UART_NUM_1
#define TINY_BMS_TX_PIN        17 // À adapter selon votre board ESP32-P4
#define TINY_BMS_RX_PIN        18 // À adapter
#define TINY_BMS_BAUD_RATE     115200

// Mapping des registres importants (Basé sur la documentation)
enum TinyRegister : uint16_t {
    // Live Data
    REG_CELL_1_VOLTAGE = 0,
    REG_LIFETIME_COUNTER = 32,
    REG_PACK_VOLTAGE = 36,       // FLOAT
    REG_PACK_CURRENT = 38,       // FLOAT
    REG_MIN_CELL_VOLTAGE = 40,
    REG_MAX_CELL_VOLTAGE = 41,
    REG_SOC = 46,                // UINT32 (High Res)
    REG_INTERNAL_TEMP = 48,
    REG_BMS_STATUS = 50,
    
    // Settings
    REG_FULLY_CHARGED_VOLTAGE = 300,
    REG_FULLY_DISCHARGED_VOLTAGE = 301,
    REG_OVER_VOLTAGE_CUTOFF = 315,
    REG_UNDER_VOLTAGE_CUTOFF = 316,
    REG_DISCHARGE_OVER_CURRENT = 317,
    
    // Version
    REG_HARDWARE_VERSION = 500
};

// Structure pour stocker l'état du BMS (Data Binding pour LVGL)
struct TinyBMSData {
    float cellVoltages[16];
    float packVoltage;
    float packCurrent;
    float soc;
    float internalTemp;
    float extTemp1;
    float extTemp2;
    uint16_t minCellVoltage;
    uint16_t maxCellVoltage;
    uint16_t status;
    
    // Constructeur pour initialiser à 0
    TinyBMSData() : packVoltage(0), packCurrent(0), soc(0), internalTemp(0), status(0) {
        for(int i=0; i<16; i++) cellVoltages[i] = 0;
    }
};
