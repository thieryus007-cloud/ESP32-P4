#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H

#include "event_bus.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le module d'historique (ring buffer + EventBus)
 */
void history_model_init(event_bus_t *bus);

/**
 * @brief Démarre le module (placeholder si task dédiée nécessaire)
 */
void history_model_start(void);

/**
 * @brief Callback utilisé par le remote_event_adapter pour pousser
 *        un historique obtenu via HTTP.
 *
 * @param status_code Code HTTP retourné
 * @param body        Corps JSON (optionnel, peut être NULL)
 */
void history_model_on_remote_history(int status_code, const char *body);

#ifdef __cplusplus
}
#endif

#endif // HISTORY_MODEL_H
