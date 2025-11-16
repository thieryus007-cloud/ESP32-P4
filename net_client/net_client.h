// components/net_client/net_client.h
#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ssid;
    const char *password;
    const char *bridge_host;   // IP ou hostname du S3
    uint16_t    bridge_port;   // typiquement 80 ou 8080
} net_client_config_t;

/**
 * @brief Initialiser le client réseau (WiFi + WS/HTTP)
 *
 * @note  Ne lance pas encore les connexions, juste stocke la config,
 *        crée les queues, etc.
 */
void net_client_init(event_bus_t *bus);

/**
 * @brief Démarrer le client réseau
 *
 * - Connecte le WiFi
 * - Ouvre les WebSockets :
 *   - /ws/telemetry
 *   - /ws/events
 *   (plus tard /ws/uart, /ws/can si besoin)
 */
void net_client_start(void);

/**
 * @brief Envoyer un message texte sur le WebSocket "commandes"
 *
 * @param data   buffer JSON
 * @param len    taille
 *
 * @note  Cette fonction sera utilisée par remote_event_adapter
 *        pour envoyer des commandes utilisateur (set target SOC, etc.).
 */
bool net_client_send_command_ws(const char *data, size_t len);

/**
 * @brief Envoyer une requête HTTP POST/PUT/GET vers l'API REST du S3
 *
 * @param path   ex: "/api/config"
 * @param method "GET", "POST", "PUT"
 * @param body   JSON ou NULL
 * @param body_len taille du body
 *
 * @note  On pourra renvoyer la réponse via un EVENT_REMOTE_CMD_RESULT
 *        ou un EVENT_REMOTE_CONFIG_SNAPSHOT, selon le type de requête.
 */
bool net_client_send_http_request(const char *path,
                                  const char *method,
                                  const char *body,
                                  size_t body_len);

#ifdef __cplusplus
}
#endif

#endif // NET_CLIENT_H
