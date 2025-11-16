// components/gui_lvgl/screen_home.h
#ifndef SCREEN_HOME_H
#define SCREEN_HOME_H

#include "lvgl.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_home_create(lv_obj_t *parent);

void screen_home_update_battery(const battery_status_t *status);
void screen_home_update_system(const system_status_t *status);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_HOME_H
