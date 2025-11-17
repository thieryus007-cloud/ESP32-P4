// main/operation_mode.h
#ifndef OPERATION_MODE_H
#define OPERATION_MODE_H

#include "event_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le mode de fonctionnement et charge la valeur persistée.
 */
esp_err_t operation_mode_init(void);

/**
 * @brief Récupère le mode courant en mémoire.
 */
hmi_operation_mode_t operation_mode_get(void);

/**
 * @brief Définit et persiste le mode de fonctionnement.
 */
esp_err_t operation_mode_set(hmi_operation_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // OPERATION_MODE_H
