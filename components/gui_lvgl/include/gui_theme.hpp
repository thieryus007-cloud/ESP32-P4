#pragma once
#include "lvgl.h"

namespace gui {
namespace theme {

    // Couleurs Victron Energy
    inline lv_color_t color_bg() { return lv_color_hex(0x1e2c3b); }       // Bleu foncé fond
    inline lv_color_t color_primary() { return lv_color_hex(0x3498db); }  // Bleu cyan actif
    inline lv_color_t color_text() { return lv_color_hex(0xffffff); }     // Blanc
    inline lv_color_t color_good() { return lv_color_hex(0x2ecc71); }     // Vert
    inline lv_color_t color_warn() { return lv_color_hex(0xf1c40f); }     // Jaune
    inline lv_color_t color_crit() { return lv_color_hex(0xe74c3c); }     // Rouge

    // Helper pour créer un style de base
    inline void apply_base_style(lv_obj_t* obj) {
        lv_obj_set_style_bg_color(obj, color_bg(), LV_PART_MAIN);
        lv_obj_set_style_text_color(obj, color_text(), LV_PART_MAIN);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    }

} // namespace theme
} // namespace gui
