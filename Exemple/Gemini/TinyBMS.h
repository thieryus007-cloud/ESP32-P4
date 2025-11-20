#pragma once

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "TinyBMS_Defs.h"

class TinyBMS {
public:
    TinyBMS();
    ~TinyBMS();

    // Initialisation du driver UART et démarrage de la tâche
    bool begin();

    // Méthode thread-safe pour récupérer les données (pour LVGL)
    TinyBMSData getData();

    // Écriture d'un registre (ex: changer un paramètre depuis l'UI)
    bool writeRegister(uint16_t registerId, float value, float scale = 1.0f);

private:
    // Tâche FreeRTOS
    static void uartTaskEntry(void* pvParameters);
    void uartLoop();

    // Helpers Protocole
    void sendReadCommand(uint16_t startAddr, uint16_t count);
    bool sendWriteCommand(uint16_t addr, uint16_t value);
    uint16_t calculateCRC(const uint8_t* data, size_t length);
    
    // Parsing
    void processBuffer(const uint8_t* buffer, size_t length, uint16_t expectedStartAddr);
    void parseRegisters(const uint8_t* payload, size_t length, uint16_t startAddr);

    // Variables membres
    QueueHandle_t _uartQueue;
    TaskHandle_t _taskHandle;
    SemaphoreHandle_t _dataMutex; // Pour protéger l'accès aux données
    TinyBMSData _bmsData;         // Données actuelles
    
    // État interne pour le polling
    uint8_t _rxBuffer[1024];
};
