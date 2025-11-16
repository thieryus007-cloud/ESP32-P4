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

#ifdef __cplusplus
}
#endif

#endif // SCREEN_POWER_H
