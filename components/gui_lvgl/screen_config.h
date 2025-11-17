// components/gui_lvgl/screen_config.h
#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include "lvgl.h"
#include "event_types.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_config_create(lv_obj_t *parent);
void screen_config_apply(const hmi_config_t *config);
void screen_config_show_result(const cmd_result_t *result);
void screen_config_set_loading(bool loading, const char *message);
void screen_config_set_bus(event_bus_t *bus);
void screen_config_refresh_texts(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CONFIG_H
