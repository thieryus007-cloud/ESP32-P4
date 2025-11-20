#pragma once
#include "lvgl.h"

namespace gui {
namespace theme {

    // --- Palette Victron Energy ---
    // Fond général (Bleu très foncé)
    inline lv_color_t color_bg()        { return lv_color_hex(0x1e2c3b); }
    // Panneaux / Cartes (Bleu gris)
    inline lv_color_t color_panel()     { return lv_color_hex(0x2c3e50); }
    // Couleur Primaire (Bleu Cyan Victron)
    inline lv_color_t color_primary()   { return lv_color_hex(0x3498db); }
    // Texte principal (Blanc)
    inline lv_color_t color_text()      { return lv_color_hex(0xffffff); }
    // Texte secondaire (Gris clair)
    inline lv_color_t color_text_sec()  { return lv_color_hex(0xbdc3c7); }
    
    // État Système
    inline lv_color_t color_good()      { return lv_color_hex(0x2ecc71); } // Vert
    inline lv_color_t color_warn()      { return lv_color_hex(0xf1c40f); } // Jaune/Orange
    inline lv_color_t color_crit()      { return lv_color_hex(0xe74c3c); } // Rouge

    // --- Helpers de Style ---
    
    // Applique le fond d'écran global
    inline void apply_screen_style(lv_obj_t* obj) {
        lv_obj_set_style_bg_color(obj, color_bg(), LV_PART_MAIN);
        lv_obj_set_style_text_color(obj, color_text(), LV_PART_MAIN);
    }

    // Crée un style pour une "Card" (conteneur arrondi)
    inline void init_card_style(lv_style_t* style) {
        lv_style_init(style);
        lv_style_set_bg_color(style, color_panel());
        lv_style_set_radius(style, 8);
        lv_style_set_border_width(style, 0);
        lv_style_set_pad_all(style, 10);
        lv_style_set_shadow_width(style, 20);
        lv_style_set_shadow_color(style, lv_color_black());
        lv_style_set_shadow_opa(style, LV_OPA_40);
    }

    // Titre des cartes
    inline void apply_title_style(lv_obj_t* label) {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(label, color_primary(), 0);
    }

} // namespace theme
} // namespace gui
