#ifndef SCREEN_HISTORY_H
#define SCREEN_HISTORY_H

#include "lvgl.h"
#include "event_bus.h"
#include "event_types.h"

void screen_history_set_bus(event_bus_t *bus);
void screen_history_create(lv_obj_t *parent);
void screen_history_update(const history_snapshot_t *snapshot);
void screen_history_show_export(const history_export_result_t *result);

#endif // SCREEN_HISTORY_H
