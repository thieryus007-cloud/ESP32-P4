// components/gui_lvgl/screen_alerts.h
#ifndef SCREEN_ALERTS_H
#define SCREEN_ALERTS_H

#include "lvgl.h"
#include "event_types.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_alerts_set_bus(event_bus_t *bus);
void screen_alerts_create(lv_obj_t *parent);
void screen_alerts_update_active(const alert_list_t *list);
void screen_alerts_update_history(const alert_list_t *list);
void screen_alerts_apply_filters(const alert_filters_t *filters);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_ALERTS_H
