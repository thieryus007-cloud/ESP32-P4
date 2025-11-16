// components/gui_lvgl/screen_config.c

#include "screen_config.h"

void screen_config_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 16, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Configuration HMI / BMS");

    lv_obj_t *info = lv_label_create(parent);
    lv_label_set_text(info,
        "Cet onglet affichera / permettra de modifier la configuration\n"
        "via les endpoints REST existants (/api/config, /api/registers, ...).\n"
        "A implémenter dans une phase suivante.");
}
