// components/gui_lvgl/screen_home.h
#pragma once

#include <memory>

#include "event_types.h"
#include "lvgl.h"

#ifdef __cplusplus

namespace gui {

class ScreenHome {
public:
    ~ScreenHome();

    void update_battery(const battery_status_t &status);
    void update_system(const system_status_t &status);
    void update_balancing(const pack_stats_t *stats);
    void refresh_texts();

private:
    explicit ScreenHome(lv_obj_t *parent);

    class Impl;
    std::unique_ptr<Impl> impl_;

    friend std::unique_ptr<ScreenHome> create_screen_home(lv_obj_t *parent);
};

std::unique_ptr<ScreenHome> create_screen_home(lv_obj_t *parent);

}  // namespace gui

#endif  // __cplusplus
