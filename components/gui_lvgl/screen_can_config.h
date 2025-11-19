#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_can_config_create(lv_obj_t *parent);

#ifdef __cplusplus
}

namespace gui {

class ScreenCanConfig {
public:
    explicit ScreenCanConfig(lv_obj_t *parent) { screen_can_config_create(parent); }
};

}  // namespace gui
#endif
