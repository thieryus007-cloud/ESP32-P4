// components/gui_lvgl/screen_dashboard.h

#pragma once

#include "event_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Met à jour les panneaux dépendant des statistiques pack/cellules.
 */
void screen_dashboard_update_cells(const pack_stats_t *stats);

void screen_dashboard_refresh_texts(void);

#ifdef __cplusplus
}

namespace gui {

class ScreenDashboard {
public:
    explicit ScreenDashboard(lv_obj_t *parent) { screen_dashboard_create(parent); }

    void update_battery(const battery_status_t &status) { screen_dashboard_update_battery(&status); }
    void update_system(const system_status_t &status) { screen_dashboard_update_system(&status); }
    void update_cells(const pack_stats_t &stats) { screen_dashboard_update_cells(&stats); }
    void refresh_texts() { screen_dashboard_refresh_texts(); }
};

}  // namespace gui

#endif  // __cplusplus
