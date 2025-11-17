// components/telemetry_model/telemetry_model.h

#ifndef TELEMETRY_MODEL_H
#define TELEMETRY_MODEL_H

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le modèle de télémétrie local.
 *
 * S'abonne aux événements TinyBMS et publie battery_status_t / pack_stats_t
 * dérivés des registres. Tolère l'absence de flux S3.
 */
esp_err_t telemetry_model_init(event_bus_t *bus);

/**
 * @brief Démarre les tâches éventuelles du modèle (poll TinyBMS).
 */
esp_err_t telemetry_model_start(void);

#ifdef __cplusplus
}
#endif

#endif // TELEMETRY_MODEL_H
