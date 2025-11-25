#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include "history_data.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Instance globale des historiques
typedef struct {
    history_ring_buffer_t buf_1min;
    history_ring_buffer_t buf_1h;
    history_ring_buffer_t buf_24h;
    history_ring_buffer_t buf_7d;
    SemaphoreHandle_t mutex;
} history_manager_t;

// Périodes disponibles
typedef enum {
    HISTORY_1MIN,
    HISTORY_1H,
    HISTORY_24H,
    HISTORY_7D
} history_period_t;

/**
 * @brief Initialise le gestionnaire d'historique
 * @return ESP_OK si succès, ESP_ERR_NO_MEM si échec d'allocation
 */
esp_err_t history_manager_init(void);

/**
 * @brief Dé-initialise le gestionnaire et libère la mémoire
 */
void history_manager_deinit(void);

/**
 * @brief Ajoute un point de données (appelé par EventBus)
 * @param point Point de données à ajouter
 */
void history_add_point(const history_point_t *point);

/**
 * @brief Récupère les données pour affichage
 * @param period Période: HISTORY_1MIN, HISTORY_1H, HISTORY_24H, HISTORY_7D
 * @param points Buffer de sortie (doit être alloué par l'appelant)
 * @param max_points Taille max du buffer
 * @return Nombre de points copiés
 */
uint32_t history_get_points(
    history_period_t period,
    history_point_t *points,
    uint32_t max_points);

/**
 * @brief Exporte en CSV
 * @param period Période à exporter
 * @param filename Fichier de sortie (SPIFFS)
 * @return ESP_OK si succès
 */
esp_err_t history_export_csv(
    history_period_t period,
    const char *filename);

/**
 * @brief Sauvegarde persistante (appelé périodiquement)
 * @return ESP_OK si succès
 */
esp_err_t history_save_to_flash(void);

/**
 * @brief Charge les données depuis le flash
 * @return ESP_OK si succès
 */
esp_err_t history_load_from_flash(void);

#endif // HISTORY_MANAGER_H
