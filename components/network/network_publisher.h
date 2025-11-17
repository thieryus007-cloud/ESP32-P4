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

typedef struct {
    uint64_t last_sync_ms;        // timestamp du dernier publish HTTP/MQTT réussi
    uint32_t buffered_points;     // taille actuelle du tampon offline
    uint32_t buffer_capacity;     // capacité max du tampon offline
    uint32_t publish_errors;      // nombre de publish en erreur depuis le boot
    uint32_t published_points;    // nombre total de points envoyés
    uint32_t last_duration_ms;    // durée du dernier publish (build + envoi)
} network_publisher_metrics_t;

/**
 * @brief Démarre la tâche périodique d'envoi.
 *
 * La cadence est contrôlée par menuconfig (CONFIG_NETWORK_TELEMETRY_PERIOD_MS)
 * et peut être désactivée via CONFIG_NETWORK_TELEMETRY_PUBLISHER_ENABLED.
 */
esp_err_t network_publisher_start(void);

network_publisher_metrics_t network_publisher_get_metrics(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_PUBLISHER_H
