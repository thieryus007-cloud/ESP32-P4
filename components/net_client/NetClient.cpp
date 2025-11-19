// components/net_client/NetClient.cpp
#include "NetClient.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "remote_event_adapter.h" // Supposé compatible C++ ou extern "C"

#include <cstring>
#include <algorithm>

static const char* TAG = "NetClient";

// Constants
namespace {
    constexpr int MAX_RETRY = 5;
    constexpr int FAILOVER_THRESHOLD = CONFIG_HMI_WIFI_FAILOVER_THRESHOLD;
    constexpr size_t WS_BUF_HIGH = 4096;
    constexpr size_t WS_BUF_LOW = 1024;
    constexpr size_t HTTP_RESP_MAX = 8192;
}

NetClient::NetClient(event_bus_t* bus) : m_bus(bus) {
    // Initialisation par défaut de l'état
    m_netStatus = {};
    m_netStatus.storage_ok = true;
    m_netStatus.operation_mode = HMI_MODE_CONNECTED_S3;
    m_netStatus.telemetry_expected = true;
    m_netStatus.network_state = NETWORK_STATE_NOT_CONFIGURED;

    // Abonnement bus (via wrapper statique)
    if (m_bus) {
        event_bus_subscribe(m_bus, EVENT_TINYBMS_ALERT_TRIGGERED, tinybmsAlertHandlerWrapper, this);
        event_bus_subscribe(m_bus, EVENT_TINYBMS_ALERT_RECOVERED, tinybmsAlertHandlerWrapper, this);
    }
}

NetClient::~NetClient() {
    stop();
    // Pas besoin de free le mutex ou les vecteurs, C++ gère ça.
}

// --- Public API ---

void NetClient::start() {
    ESP_LOGI(TAG, "Starting NetClient...");
    initWiFi();
    // Note: WebSockets démarrent après l'acquisition IP dans onWifiEvent
}

void NetClient::stop() {
    ESP_LOGI(TAG, "Stopping NetClient...");
    stopWebSockets();
    stopWiFi();
    
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_netStatus.wifi_connected = false;
    m_netStatus.server_reachable = false;
    m_netStatus.network_state = NETWORK_STATE_NOT_CONFIGURED;
    
    // On ne publie pas ici pour éviter un appel bloquant potentiel dans le destructeur,
    // mais si c'est appelé explicitement, on pourrait.
}

void NetClient::setOperationMode(hmi_operation_mode_t mode, bool telemetryExpected) {
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_netStatus.operation_mode = mode;
        m_netStatus.telemetry_expected = telemetryExpected;

        if (!telemetryExpected) {
            m_netStatus.wifi_connected = false;
            m_netStatus.server_reachable = false;
            m_netStatus.network_state = NETWORK_STATE_NOT_CONFIGURED;
            m_netStatus.has_error = false;
        }
    } // Mutex relâché ici
    publishSystemStatus();
}

// --- WiFi Logic ---

void NetClient::initWiFi() {
    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Enregistrement avec 'this' comme user_data
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
        &NetClient::wifiEventHandlerWrapper, this, &m_wifiHandlerInstance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
        &NetClient::wifiEventHandlerWrapper, this, &m_ipHandlerInstance));

    wifi_config_t wifi_config = {};
    std::strncpy((char*)wifi_config.sta.ssid, Config::WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy((char*)wifi_config.sta.password, Config::WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void NetClient::stopWiFi() {
    if (m_wifiHandlerInstance) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, m_wifiHandlerInstance);
        m_wifiHandlerInstance = nullptr;
    }
    if (m_ipHandlerInstance) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, m_ipHandlerInstance);
        m_ipHandlerInstance = nullptr;
    }
    esp_wifi_stop();
}

void NetClient::wifiEventHandlerWrapper(void* arg, esp_event_base_t base, int32_t id, void* data) {
    // On recupère l'instance C++ depuis arg
    auto* client = static_cast<NetClient*>(arg);
    client->onWifiEvent(base, id, data);
}

void NetClient::onWifiEvent(esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        updateNetworkState(NETWORK_STATE_CONNECTING);
    } 
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (m_retryCount < MAX_RETRY) {
            esp_wifi_connect();
            m_retryCount++;
            ESP_LOGW(TAG, "WiFi Retry %d/%d", m_retryCount, MAX_RETRY);
            updateNetworkState(NETWORK_STATE_CONNECTING);
        } else {
            updateNetworkState(NETWORK_STATE_ERROR);
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_netStatus.wifi_connected = false;
            }
            processFailover();
        }
    } 
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        m_retryCount = 0;
        m_failSequences = 0;
        m_failoverTriggered = false;

        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_netStatus.wifi_connected = true;
        }
        updateNetworkState(NETWORK_STATE_ACTIVE);
        
        // Démarrage WS une fois IP ok
        initWebSockets();
    }
}

// --- WebSocket Logic ---

void NetClient::initWebSockets() {
    // Helper lambda pour configurer un client
    auto create_client = [this](const char* endpoint, WsChannel channel, size_t rx_size, size_t tx_size) {
        char uri[128];
        snprintf(uri, sizeof(uri), "ws://%s:%d%s", Config::BRIDGE_HOST, Config::BRIDGE_PORT, endpoint);
        
        esp_websocket_client_config_t cfg = {};
        cfg.uri = uri;
        cfg.buffer_size = rx_size;
        cfg.buffer_size_tx = tx_size;
        cfg.disable_auto_reconnect = false;
        cfg.reconnect_timeout_ms = 5000;

        auto handle = esp_websocket_client_init(&cfg);
        
        // On passe 'this' ET le channel. Astuce : on peut packer le channel dans l'arg 
        // ou créer une petite struct wrapper. Ici, on suppose que le channel est déduit 
        // par le handle ou on utilise une petite structure dédiée si besoin.
        // Simplification : on passe le channel casté en void* car on a déjà 'this' via Wrapper statique 
        // MAIS le wrapper statique a besoin de l'instance. 
        // Solution propre : utiliser une struct context.
        
        return handle;
    };
    
    // Pour simplifier l'exemple, je gère l'event wrapper WS de manière légèrement différente :
    // Le wrapper WS aura besoin de savoir QUEL client C++ appeler. 
    // Dans un système strictement C++, on utiliserait std::bind, mais IDF veut un function pointer.
    
    // Solution robuste : On enregistre le wrapper et on passe un context struct alloué
    struct WsContext { NetClient* self; WsChannel channel; };
    
    // ... Configuration des clients (similaire au code C mais encapsulé) ...
    // Ici je conserve la logique globale pour ne pas surcharger l'exemple, 
    // mais m_wsTelemetry = create_client(...)
}

void NetClient::stopWebSockets() {
    auto stop_and_destroy = [](esp_websocket_client_handle_t& handle) {
        if (handle) {
            esp_websocket_client_stop(handle);
            esp_websocket_client_destroy(handle);
            handle = nullptr;
        }
    };
    stop_and_destroy(m_wsTelemetry);
    stop_and_destroy(m_wsEvents);
    stop_and_destroy(m_wsAlerts);
}

bool NetClient::sendCommandWS(const std::string& data) {
    if (!m_wsEvents || !esp_websocket_client_is_connected(m_wsEvents)) return false;
    
    // Envoi C++ string data
    int res = esp_websocket_client_send_text(m_wsEvents, data.c_str(), data.length(), pdMS_TO_TICKS(200));
    return (res >= 0);
}

// --- HTTP Logic ---

bool NetClient::sendHttpRequest(const std::string& path, const std::string& method, const std::string& body) {
    // Utilisation std::string évite les erreurs de pointeurs NULL
    
    // Setup config
    std::string url = std::string("http://") + Config::BRIDGE_HOST + ":" + std::to_string(Config::BRIDGE_PORT) + path;
    
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.timeout_ms = 5000;
    
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    // RAII pour cleanup HTTP client
    // On utilise un unique_ptr avec un deleter custom pour garantir le cleanup
    auto client_deleter = [](esp_http_client_handle_t c) { esp_http_client_cleanup(c); };
    std::unique_ptr<esp_http_client, decltype(client_deleter)> client_guard(client, client_deleter);

    if (method == "POST") esp_http_client_set_method(client, HTTP_METHOD_POST);
    else if (method == "PUT") esp_http_client_set_method(client, HTTP_METHOD_PUT);
    else esp_http_client_set_method(client, HTTP_METHOD_GET);

    if (!body.empty()) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body.c_str(), body.length());
    }

    esp_err_t err = esp_http_client_perform(client);
    bool success = false;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        success = (status >= 200 && status < 300);
        // Gestion lecture réponse... (similaire au C mais avec std::vector<char>)
    }
    
    return success;
}

// --- Internal Utils ---

void NetClient::updateNetworkState(network_state_t newState) {
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_netStatus.network_state = newState;
    }
    publishSystemStatus();
}

void NetClient::publishSystemStatus() {
    if (!m_bus) return;

    system_status_t statusCopy;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        // Logique de calcul 'has_error' ici...
        statusCopy = m_netStatus;
    }
    
    event_t evt = {};
    evt.type = EVENT_SYSTEM_STATUS_UPDATED;
    evt.data = &statusCopy;
    evt.data_size = sizeof(statusCopy);
    event_bus_publish(m_bus, &evt);
}

void NetClient::processFailover() {
    m_failSequences++;
    if (CONFIG_HMI_WIFI_FAILOVER_ENABLED && !m_failoverTriggered && 
        m_failSequences >= FAILOVER_THRESHOLD) {
        
        m_failoverTriggered = true;
        // Publier event failover...
    }
}

// Wrapper pour l'alerte TinyBMS
void NetClient::tinybmsAlertHandlerWrapper(event_bus_t* bus, const event_t* event, void* ctx) {
    auto* self = static_cast<NetClient*>(ctx);
    if (event && event->data) {
        self->onTinyBmsAlert(static_cast<const tinybms_alert_event_t*>(event->data));
    }
}

void NetClient::onTinyBmsAlert(const tinybms_alert_event_t* payload) {
    // Logique d'envoi d'alerte...
    // Utilisation de std::string pour construire le JSON proprement
    // Ou mieux, utiliser une lib JSON.
}
