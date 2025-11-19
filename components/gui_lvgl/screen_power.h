// components/gui_lvgl/screen_power.h
#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#include "lvgl.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_power_create(lv_obj_t *parent);

void screen_power_update(const battery_status_t *status);
void screen_power_update_system(const system_status_t *status);
void screen_power_refresh_texts(void);
#ifdef __cplusplus
}

namespace gui {

class ScreenPower {
public:
    explicit ScreenPower(lv_obj_t *parent) { screen_power_create(parent); }

    void update(const battery_status_t &status) { screen_power_update(&status); }
    void update_system(const system_status_t &status) { screen_power_update_system(&status); }
    void refresh_texts() { screen_power_refresh_texts(); }
};

}  // namespace gui

#endif

#endif // SCREEN_POWER_H
