/*
 * tinybms_client.hpp
 * Classe principale C++ gérant la communication et l'EventBus
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "tinybms_defs.h"
#include "esp_event.h" // Supposé être utilisé pour l'EventBus

// Structure interne pour stocker l'état complet
struct TinyBMS_Data {
    float voltage;
    float current;
    float soc;
    uint16_t cells[16];
    uint8_t cell_count;
    int16_t temp_internal;
    int16_t temp_ext1;
    int16_t temp_ext2;
    uint16_t status_code; // Reg 50
    float speed_kmh;
    uint32_t distance_left;
    uint32_t time_left;
};

class TinyBMSClient {
public:
    // Singleton pattern pour accès facile
    static TinyBMSClient& getInstance();

    // Initialisation (GPIOs, UART)
    bool init(uart_port_t uart_num, int tx_pin, int rx_pin);

    // Démarrer la tâche de fond (Le moteur)
    void start();

    // --- Commandes Publiques (Appelables depuis GUI) ---
    bool writeRegister(uint16_t reg_addr, uint16_t value); // Commande 0x0D
    bool resetBMS(TinyBmsResetOpt option);                 // Commande 0x02
    bool readAllConfig();                                  // Pour l'écran Config

    // Accesseur thread-safe aux données
    TinyBMS_Data getSnapshot();

private:
    TinyBMSClient(); // Constructeur privé
    
    // Tâche principale (La boucle infinie)
    static void workerTask(void* arg);
    void run();

    // Méthodes de communication bas niveau
    bool sendCommand(uint8_t cmd, const uint8_t* payload, size_t len);
    bool readResponse(uint8_t cmd, uint8_t* buffer, size_t max_len, size_t& out_len);
    uint16_t calculateCRC(const uint8_t* data, size_t len);

    // Parsing des réponses complexes
    void parseCellVoltages(const uint8_t* data, size_t len);
    void parseTemps(const uint8_t* data, size_t len);
    void parseCalcValues(const uint8_t* data, size_t len);

    // Membres
    uart_port_t _uart_num;
    TaskHandle_t _taskHandle;
    SemaphoreHandle_t _mutex; // Thread-safety
    TinyBMS_Data _cachedData;
    bool _isConnected;
};
