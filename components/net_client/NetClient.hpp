// components/net_client/include/NetClient.hpp
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "esp_event.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "event_bus.h"     // Votre module C existant
#include "event_types.h"   // Vos types existants

// Configuration constants moved to strict types inside a namespace or static class members
namespace Config {
    constexpr auto WIFI_SSID = CONFIG_HMI_WIFI_SSID;
    constexpr auto WIFI_PASS = CONFIG_HMI_WIFI_PASSWORD;
    constexpr auto BRIDGE_HOST = CONFIG_HMI_BRIDGE_HOST;
    constexpr int  BRIDGE_PORT = CONFIG_HMI_BRIDGE_PORT;
}

class NetClient {
public:
    // Constructeur / Destructeur
    explicit NetClient(event_bus_t* bus);
    ~NetClient();

    // Interdiction de copie (Singleton par instance unique)
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    // API Publique
    void start();
    void stop();
    
    void setOperationMode(hmi_operation_mode_t mode, bool telemetryExpected);
    bool sendCommandWS(const std::string& data);
    bool sendHttpRequest(const std::string& path, const std::string& method, const std::string& body);

private:
    // Types internes
    enum class WsChannel { Telemetry, Events, Alerts };
    
    // Helpers internes
    void initWiFi();
    void stopWiFi();
    void initWebSockets();
    void stopWebSockets();
    
    void updateNetworkState(network_state_t newState);
    void publishSystemStatus();
    void processFailover();

    // Trampolines pour les callbacks C (ESP-IDF require C signatures)
    static void wifiEventHandlerWrapper(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void websocketEventHandlerWrapper(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void tinybmsAlertHandlerWrapper(event_bus_t* bus, const event_t* event, void* ctx);

    // Handlers C++ réels
    void onWifiEvent(esp_event_base_t base, int32_t id, void* data);
    void onWebsocketEvent(WsChannel channel, const esp_websocket_event_data_t* data);
    void onTinyBmsAlert(const tinybms_alert_event_t* payload);

    // Membres
    event_bus_t* m_bus;
    
    // État protégé par Mutex
    mutable std::mutex m_stateMutex; 
    system_status_t m_netStatus;
    
    // Gestion WiFi
    esp_event_handler_instance_t m_wifiHandlerInstance = nullptr;
    esp_event_handler_instance_t m_ipHandlerInstance = nullptr;
    int m_retryCount = 0;
    int m_failSequences = 0;
    bool m_failoverTriggered = false;

    // WebSockets handles
    esp_websocket_client_handle_t m_wsTelemetry = nullptr;
    esp_websocket_client_handle_t m_wsEvents = nullptr;
    esp_websocket_client_handle_t m_wsAlerts = nullptr;
};
