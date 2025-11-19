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

namespace gui {

class ScreenAlerts {
public:
    ScreenAlerts(event_bus_t *bus, lv_obj_t *parent)
    {
        set_bus(bus);
        create(parent);
    }

    void set_bus(event_bus_t *bus) { screen_alerts_set_bus(bus); }
    void create(lv_obj_t *parent) { screen_alerts_create(parent); }

    void update_active(const alert_list_t &list) { screen_alerts_update_active(&list); }
    void update_history(const alert_list_t &list) { screen_alerts_update_history(&list); }
    void apply_filters(const alert_filters_t &filters) { screen_alerts_apply_filters(&filters); }
};

}  // namespace gui
#endif

#endif // SCREEN_ALERTS_H
