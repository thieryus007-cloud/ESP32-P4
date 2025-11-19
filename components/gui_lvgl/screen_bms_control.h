#pragma once

#include "lvgl.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_bms_control_create(lv_obj_t *parent);
void screen_bms_control_update_cvl(const cvl_limits_event_t *limits);

#ifdef __cplusplus
}

namespace gui {

class ScreenBmsControl {
public:
    explicit ScreenBmsControl(lv_obj_t *parent) { screen_bms_control_create(parent); }

    void update_cvl(const cvl_limits_event_t &limits) { screen_bms_control_update_cvl(&limits); }
};

}  // namespace gui
#endif
