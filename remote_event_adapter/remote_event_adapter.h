// components/remote_event_adapter/remote_event_adapter.h
#ifndef REMOTE_EVENT_ADAPTER_H
#define REMOTE_EVENT_ADAPTER_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialiser l'adapter JSON <-> EventBus
 *
 * - S'abonne aux events USER_INPUT_* de la GUI
 * - Prépare les structures / queues pour recevoir du JSON venant de net_client
 */
void remote_event_adapter_init(event_bus_t *bus);

/**
 * @brief Démarrer l'adapter
 *
 * - Crée la task qui :
 *   - lit les messages JSON bruts (via queue alimentée par net_client)
 *   - parse les JSON
 *   - publie EVENT_REMOTE_* sur l'EventBus
 */
void remote_event_adapter_start(void);

/**
 * @brief Point d'entrée depuis net_client pour laisser l'adapter traiter un JSON
 *
 * @param json     buffer JSON (null-terminated)
 * @param length   longueur (optionnel, peut être 0 si null-terminated)
 *
 * @note  net_client appelle cette fonction quand il reçoit des messages sur
 *        /ws/telemetry ou /ws/events. L'adapter décode et publie des events.
 */
void remote_event_adapter_on_telemetry_json(const char *json, size_t length);
void remote_event_adapter_on_event_json(const char *json, size_t length);

#ifdef __cplusplus
}
#endif

#endif // REMOTE_EVENT_ADAPTER_H
