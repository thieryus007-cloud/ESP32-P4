/*
 * tinybms_client.cpp
 * Implémentation complète "Voiture" : Tâche, EventBus, Logique Métier.
 */

#include "tinybms_client.hpp"
#include "esp_log.h"
#include <cstring>

// --- Configuration ---
#define TAG "TinyBMS"
#define UART_BUF_SIZE 512
#define TIMEOUT_MS 200
#define POLLING_INTERVAL_MS 500 // Mise à jour 2Hz

// --- EventBus (MOCK - À ADAPTER À TON PROJET EXACT) ---
// Je déclare ici les bases de l'EventBus comme vu dans ton architecture
ESP_EVENT_DECLARE_BASE(TINYBMS_EVENT);
enum {
    TINYBMS_EVENT_UPDATE,
    TINYBMS_EVENT_ERROR,
    TINYBMS_EVENT_CONNECTED
};

TinyBMSClient& TinyBMSClient::getInstance() {
    static TinyBMSClient instance;
    return instance;
}

TinyBMSClient::TinyBMSClient() : _uart_num(UART_NUM_1), _taskHandle(nullptr), _isConnected(false) {
    _mutex = xSemaphoreCreateMutex();
    memset(&_cachedData, 0, sizeof(TinyBMS_Data));
}

bool TinyBMSClient::init(uart_port_t uart_num, int tx_pin, int rx_pin) {
    _uart_num = uart_num;
    
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    if (uart_param_config(_uart_num, &uart_config) != ESP_OK) return false;
    if (uart_set_pin(_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
    if (uart_driver_install(_uart_num, UART_BUF_SIZE * 2, 0, 0, NULL, 0) != ESP_OK) return false;

    ESP_LOGI(TAG, "TinyBMS UART Initialized on pins TX:%d RX:%d", tx_pin, rx_pin);
    return true;
}

void TinyBMSClient::start() {
    if (!_taskHandle) {
        xTaskCreate(workerTask, "TinyBMS_Task", 4096, this, 5, &_taskHandle);
    }
}

// ---------------------------------------------------------
// Le Cœur du Système : La Boucle Principale
// ---------------------------------------------------------
void TinyBMSClient::workerTask(void* arg) {
    TinyBMSClient* client = static_cast<TinyBMSClient*>(arg);
    client->run();
}

void TinyBMSClient::run() {
    uint8_t rxBuf[256];
    size_t rxLen;
    
    ESP_LOGI(TAG, "Starting Polling Loop...");

    while (true) {
        // 1. Protéger l'accès UART avec un Mutex (Thread-Safe)
        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000))) {
            
            bool cycleSuccess = true;

            // --- A. Lecture Tension Pack (Cmd 0x14) ---
            if (sendCommand(TINYBMS_CMD_READ_PACK_VOLTAGE, NULL, 0)) {
                if (readResponse(TINYBMS_CMD_READ_PACK_VOLTAGE, rxBuf, sizeof(rxBuf), rxLen)) {
                    memcpy(&_cachedData.voltage, &rxBuf[2], 4); // Float direct
                } else cycleSuccess = false;
            }

            // --- B. Lecture Courant (Cmd 0x15) ---
            if (sendCommand(TINYBMS_CMD_READ_PACK_CURRENT, NULL, 0)) {
                if (readResponse(TINYBMS_CMD_READ_PACK_CURRENT, rxBuf, sizeof(rxBuf), rxLen)) {
                    memcpy(&_cachedData.current, &rxBuf[2], 4);
                }
            }

            // --- C. Lecture Cellules (Cmd 0x1C) ---
            if (sendCommand(TINYBMS_CMD_READ_CELL_VOLTAGES, NULL, 0)) {
                if (readResponse(TINYBMS_CMD_READ_CELL_VOLTAGES, rxBuf, sizeof(rxBuf), rxLen)) {
                    parseCellVoltages(rxBuf, rxLen);
                }
            }

            // --- D. Lecture Températures (Cmd 0x1B) ---
            if (sendCommand(TINYBMS_CMD_READ_TEMPS, NULL, 0)) {
                if (readResponse(TINYBMS_CMD_READ_TEMPS, rxBuf, sizeof(rxBuf), rxLen)) {
                    parseTemps(rxBuf, rxLen);
                }
            }

            // --- E. SOC (Cmd 0x1A) ---
            if (sendCommand(TINYBMS_CMD_READ_SOC, NULL, 0)) {
                if (readResponse(TINYBMS_CMD_READ_SOC, rxBuf, sizeof(rxBuf), rxLen)) {
                    uint32_t rawSoc;
                    memcpy(&rawSoc, &rxBuf[2], 4);
                    _cachedData.soc = (float)rawSoc / 1000000.0f; // Résolution 0.000001%
                }
            }
            
            // --- F. Status (Cmd 0x18) ---
            if (sendCommand(TINYBMS_CMD_READ_ONLINE_STATUS, NULL, 0)) {
                 if (readResponse(TINYBMS_CMD_READ_ONLINE_STATUS, rxBuf, sizeof(rxBuf), rxLen)) {
                    _cachedData.status_code = rxBuf[2] | (rxBuf[3] << 8);
                 }
            }

            xSemaphoreGive(_mutex);

            // 2. Publication EventBus (Connecter ici ton système d'événements)
            if (cycleSuccess) {
                if (!_isConnected) {
                    _isConnected = true;
                    ESP_LOGI(TAG, "TinyBMS Connected!");
                    // esp_event_post(TINYBMS_EVENT, TINYBMS_EVENT_CONNECTED, ...);
                }
                // esp_event_post(TINYBMS_EVENT, TINYBMS_EVENT_UPDATE, &_cachedData, sizeof(TinyBMS_Data), ...);
            } else {
                // Gestion d'erreur simple
                // ESP_LOGW(TAG, "Comms error");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(POLLING_INTERVAL_MS));
    }
}

// ---------------------------------------------------------
// Implémentation des Commandes Spécifiques
// ---------------------------------------------------------

bool TinyBMSClient::writeRegister(uint16_t reg_addr, uint16_t value) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    
    // Construction Payload pour Cmd 0x0D
    // Attention: PDF spécifie DATA MSB d'abord pour l'écriture !
    uint8_t payload[5];
    payload[0] = 0x05; // Payload length byte (supposé fixe pour un reg)
    payload[1] = reg_addr & 0xFF;       // ADDR LSB
    payload[2] = (reg_addr >> 8) & 0xFF; // ADDR MSB
    payload[3] = (value >> 8) & 0xFF;    // DATA MSB
    payload[4] = value & 0xFF;           // DATA LSB
    
    bool success = sendCommand(TINYBMS_CMD_WRITE_REG_INDIVIDUAL, payload, 5);
    if (success) {
        uint8_t rx[16];
        size_t rxLen;
        // On attend un ACK
        success = readResponse(TINYBMS_CMD_WRITE_REG_INDIVIDUAL, rx, sizeof(rx), rxLen);
    }
    
    xSemaphoreGive(_mutex);
    return success;
}

bool TinyBMSClient::resetBMS(TinyBmsResetOpt option) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    uint8_t payload = (uint8_t)option;
    
    // Cmd 0x02
    bool success = sendCommand(TINYBMS_CMD_RESET_CLEAR, &payload, 1);
    // Pas de lecture de réponse immédiate si c'est un reboot (0x05
