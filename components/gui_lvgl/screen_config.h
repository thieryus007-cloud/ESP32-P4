// components/gui_lvgl/screen_config.h
#ifndef SCREEN_CONFIG_H
#define SCREEN_CONFIG_H

#include "lvgl.h"
#include "event_types.h"
#include "event_bus.h"

#ifdef __cplusplus
#include <functional>
#include <utility>

namespace screen_config {
using SaveCallback = std::function<void(const hmi_config_t &, bool mqtt_only)>;
using ReloadCallback = std::function<void(bool include_mqtt)>;

void set_save_callback(SaveCallback cb);
void set_reload_callback(ReloadCallback cb);
}  // namespace screen_config

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

namespace gui {

class ScreenConfig {
public:
    ScreenConfig(event_bus_t *bus, lv_obj_t *parent)
    {
        set_bus(bus);
        create(parent);
    }

    void set_bus(event_bus_t *bus) { screen_config_set_bus(bus); }
    void create(lv_obj_t *parent) { screen_config_create(parent); }
    void apply(const hmi_config_t &config) { screen_config_apply(&config); }
    void show_result(const cmd_result_t &result) { screen_config_show_result(&result); }
    void set_loading(bool loading, const char *message) { screen_config_set_loading(loading, message); }
    void refresh_texts() { screen_config_refresh_texts(); }

    void set_save_callback(screen_config::SaveCallback cb)
    {
        screen_config::set_save_callback(std::move(cb));
    }

    void set_reload_callback(screen_config::ReloadCallback cb)
    {
        screen_config::set_reload_callback(std::move(cb));
    }
};

}  // namespace gui
#endif

#endif // SCREEN_CONFIG_H
