// components/gui_lvgl/screen_battery.h
#ifndef SCREEN_BATTERY_H
#define SCREEN_BATTERY_H

#include "lvgl.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_battery_create(lv_obj_t *parent);

/**
 * @brief Mise à jour des infos pack de base (via battery_status_t)
 *        - voltage, courant, puissance, SOC
 */
void screen_battery_update_pack_basic(const battery_status_t *status);

/**
 * @brief Mise à jour des statistiques pack + cellules détaillées
 *        (min/max/delta, liste des cellules, etc.)
 */
void screen_battery_update_pack_stats(const pack_stats_t *stats);

#ifdef __cplusplus
}

namespace gui {

class ScreenBattery {
public:
    explicit ScreenBattery(lv_obj_t *parent) { screen_battery_create(parent); }

    void update_pack_basic(const battery_status_t &status)
    {
        screen_battery_update_pack_basic(&status);
    }

    void update_pack_stats(const pack_stats_t &stats)
    {
        screen_battery_update_pack_stats(&stats);
    }
};

}  // namespace gui
#endif

#endif // SCREEN_BATTERY_H
