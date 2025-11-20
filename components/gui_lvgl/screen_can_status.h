#pragma once

#include "lvgl.h"
#include "can_victron.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_can_status_create(lv_obj_t *parent);
void screen_can_status_update(const can_victron_status_t *status);

#ifdef __cplusplus
}

namespace gui {

class ScreenCanStatus {
public:
    explicit ScreenCanStatus(lv_obj_t *parent) { screen_can_status_create(parent); }

    void update(const can_victron_status_t &status) { screen_can_status_update(&status); }
};

}  // namespace gui
#endif
