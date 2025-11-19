#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <cstdio> // Pour FILE*

#include "event_bus.h"
#include "event_types.h"
// On suppose que NetClient.hpp est disponible suite à la refonte précédente
#include "NetClient.hpp" 
#include "cJSON.h"

class HistoryModel {
public:
    // Configuration statique
    static constexpr size_t CAPACITY = 2048; //
    static constexpr size_t EXPORT_BATCH_SIZE = 128; // Pour l'écriture fichier

    // Constructeur : Injection des dépendances
    HistoryModel(event_bus_t* bus, NetClient* netClient);
    ~HistoryModel();

    // Pas de copie
    HistoryModel(const HistoryModel&) = delete;
    HistoryModel& operator=(const HistoryModel&) = delete;

    // API Publique (principalement utilisée par le wrapper C)
    void start();
    
    // Callback réseau (réponse HTTP)
    void onRemoteHistoryResponse(int statusCode, const std::string& body);

private:
    // --- Types Internes ---
    struct RingBuffer {
        std::vector<history_sample_t> data;
        size_t head = 0;
        size_t count = 0;

        RingBuffer(size_t cap) : data(cap), head(0), count(0) {}
    };

    // --- Handlers d'événements (Implémentation) ---
    void onBatteryUpdate(const battery_status_t* status);
    void onHistoryRequest(const user_input_history_request_t* req);
    void onHistoryExport(const user_input_history_export_t* req);

    // --- Helpers ---
    void pushSample(const history_sample_t& sample);
    void publishSnapshot(history_range_t range, const std::vector<history_sample_t>& samples, bool fromBackend);
    void publishLocalSnapshot(history_range_t range);
    bool parseHistoryJson(const std::string& jsonBody);
    
    // Helper statique pour trampoline C
    static void eventHandlerWrapper(event_bus_t* bus, const event_t* event, void* ctx);

    // --- Membres ---
    event_bus_t* m_bus;
    NetClient* m_netClient;
    
    mutable std::mutex m_mutex; // Protège le RingBuffer
    RingBuffer m_ring;
    
    history_range_t m_lastRequestedRange = HISTORY_RANGE_LAST_HOUR;
};
