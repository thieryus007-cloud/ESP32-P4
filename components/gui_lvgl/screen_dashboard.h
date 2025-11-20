#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "event_types.h" // Votre fichier existant

// API C pour compatibilité si nécessaire
void screen_dashboard_create(lv_obj_t *parent);
void screen_dashboard_update_battery(const battery_status_t *status);
void screen_dashboard_update_system(const system_status_t *status);

#ifdef __cplusplus
}

// API C++ interne
namespace gui {
    class ScreenDashboard {
    public:
        explicit ScreenDashboard(lv_obj_t* parent);
        void update_battery(const battery_status_t& status);
        void update_system(const system_status_t& status);

    private:
        lv_obj_t* root;
        // Widgets SOC
        lv_obj_t* arc_soc;
        lv_obj_t* label_soc;
        
        // Widgets Power
        lv_obj_t* label_power;
        lv_obj_t* label_voltage;
        lv_obj_t* label_current;
        lv_obj_t* arc_power_flow; // Animation flux

        // Widgets Status
        lv_obj_t* led_wifi;
        lv_obj_t* led_mqtt;
        lv_obj_t* led_can;

        void build_ui();
        void update_soc_visuals(float soc);
    };
}
#endif
