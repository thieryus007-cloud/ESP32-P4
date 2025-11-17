// components/network/network_publisher.h

#ifndef NETWORK_PUBLISHER_H
#define NETWORK_PUBLISHER_H

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le publisher réseau TinyBMS.
 *
 * Souscrit aux événements de télémétrie et surveille l'état réseau afin de
 * décider quand expédier les points via MQTT/HTTP ou les mettre en tampon.
 *
 * @param bus EventBus global
 * @return ESP_OK si succès
 */
esp_err_t network_publisher_init(event_bus_t *bus);

/**
 * @brief Démarre la tâche périodique d'envoi.
 *
 * La cadence est contrôlée par menuconfig (CONFIG_NETWORK_TELEMETRY_PERIOD_MS)
 * et peut être désactivée via CONFIG_NETWORK_TELEMETRY_PUBLISHER_ENABLED.
 */
esp_err_t network_publisher_start(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_PUBLISHER_H
