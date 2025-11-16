// components/remote_event_adapter/remote_event_adapter.h
#ifndef REMOTE_EVENT_ADAPTER_H
#define REMOTE_EVENT_ADAPTER_H

#include <stddef.h>
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise l'adapter distant (référence vers l'EventBus).
 */
void remote_event_adapter_init(event_bus_t *bus);

/**
 * @brief Démarre l'adapter (si une task dédiée est ajoutée plus tard).
 */
void remote_event_adapter_start(void);

/**
 * @brief Callback appelé quand un JSON de /ws/telemetry est reçu.
 */
void remote_event_adapter_on_telemetry_json(const char *json, size_t length);

/**
 * @brief Callback appelé quand un JSON de /ws/events est reçu.
 */
void remote_event_adapter_on_event_json(const char *json, size_t length);

/**
 * @brief Callback appelé quand un JSON de statut MQTT est reçu.
 *
 *  Alignement logique avec SystemStatus.handleMqttStatus(status) :
 *  - status.enabled = false  → mqtt_ok = false
 *  - status.enabled = true & status.connected = true  → mqtt_ok = true
 *  - status.enabled = true & status.connected = false → mqtt_ok = false
 *
 *  JSON attendu (exemple) :
 *  {
 *    "enabled": true,
 *    "connected": false,
 *    "client_id": "tinybms-bridge",
 *    "last_error": "Connection refused"
 *  }
 */
void remote_event_adapter_on_mqtt_status_json(const char *json, size_t length);

#ifdef __cplusplus
}
#endif

#endif // REMOTE_EVENT_ADAPTER_H
