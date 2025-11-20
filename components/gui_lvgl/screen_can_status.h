#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "event_types.h"

// API C
void screen_can_status_create(lv_obj_t *parent);
void screen_can_status_update(const cvl_limits_event_t *limits);

#ifdef __cplusplus
}

namespace gui {
    class ScreenCanStatus {
    public:
        explicit ScreenCanStatus(lv_obj_t* parent);
        void update(const cvl_limits_event_t& limits);

    private:
        lv_obj_t* root;
        
        // Affichage principal "Gros Chiffres"
        lv_obj_t* label_cvl;
        lv_obj_t* label_ccl;
        lv_obj_t* label_dcl;
        
        // Status flags
        lv_obj_t* led_imbalance;
        lv_obj_t* led_cell_prot;
        lv_obj_t* label_state_txt;

        void build_ui();
    };
}
#endif
