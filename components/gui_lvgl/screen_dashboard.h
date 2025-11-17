// components/gui_lvgl/screen_dashboard.h

#pragma once

#include "event_types.h"
#include "lvgl.h"

/**
 * @brief Crée l'onglet "Dashboard" avec gauges et mini-graphiques.
 */
void screen_dashboard_create(lv_obj_t *parent);

/**
 * @brief Met à jour les jauges/graphes liés au statut batterie.
 */
void screen_dashboard_update_battery(const battery_status_t *status);

/**
 * @brief Met à jour les indicateurs système (WiFi / stockage / erreurs).
 */
void screen_dashboard_update_system(const system_status_t *status);
void screen_dashboard_refresh_texts(void);
