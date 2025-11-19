#ifndef SCREEN_HISTORY_H
#define SCREEN_HISTORY_H

#include "lvgl.h"
#include "event_bus.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_history_set_bus(event_bus_t *bus);
void screen_history_create(lv_obj_t *parent);
void screen_history_update(const history_snapshot_t *snapshot);
void screen_history_show_export(const history_export_result_t *result);

#ifdef __cplusplus
}

namespace gui {

class ScreenHistory {
public:
    ScreenHistory(event_bus_t *bus, lv_obj_t *parent)
    {
        set_bus(bus);
        create(parent);
    }

    void set_bus(event_bus_t *bus) { screen_history_set_bus(bus); }
    void create(lv_obj_t *parent) { screen_history_create(parent); }
    void update(const history_snapshot_t &snapshot) { screen_history_update(&snapshot); }
    void show_export(const history_export_result_t &result) { screen_history_show_export(&result); }
};

}  // namespace gui
#endif

#endif // SCREEN_HISTORY_H
