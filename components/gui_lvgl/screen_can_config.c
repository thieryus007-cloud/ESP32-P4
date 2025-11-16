// components/gui_lvgl/screen_can_config.c

#include "screen_can_config.h"
#include "lvgl.h"
#include <stdio.h>

void screen_can_config_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 8, 0);

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // Titre
    lv_obj_t *label_title = lv_label_create(cont);
    lv_label_set_text(label_title, "CAN Configuration");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, 0);

    // Section: GPIO
    lv_obj_t *label_section_gpio = lv_label_create(cont);
    lv_label_set_text(label_section_gpio, "GPIO Configuration:");
    lv_obj_set_style_text_font(label_section_gpio, &lv_font_montserrat_16, 0);

    lv_obj_t *row1 = lv_obj_create(cont);
    lv_obj_remove_style_all(row1);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label1_title = lv_label_create(row1);
    lv_label_set_text(label1_title, "TX GPIO:");

    lv_obj_t *label1_value = lv_label_create(row1);
    lv_label_set_text(label1_value, "22");

    lv_obj_t *row2 = lv_obj_create(cont);
    lv_obj_remove_style_all(row2);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label2_title = lv_label_create(row2);
    lv_label_set_text(label2_title, "RX GPIO:");

    lv_obj_t *label2_value = lv_label_create(row2);
    lv_label_set_text(label2_value, "21");

    // Séparateur
    lv_obj_t *sep1 = lv_obj_create(cont);
    lv_obj_set_height(sep1, 1);
    lv_obj_set_width(sep1, LV_PCT(100));
    lv_obj_set_style_bg_color(sep1, lv_palette_main(LV_PALETTE_GREY), 0);

    // Section: Protocole
    lv_obj_t *label_section_protocol = lv_label_create(cont);
    lv_label_set_text(label_section_protocol, "Protocol Settings:");
    lv_obj_set_style_text_font(label_section_protocol, &lv_font_montserrat_16, 0);

    lv_obj_t *row3 = lv_obj_create(cont);
    lv_obj_remove_style_all(row3);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label3_title = lv_label_create(row3);
    lv_label_set_text(label3_title, "Speed:");

    lv_obj_t *label3_value = lv_label_create(row3);
    lv_label_set_text(label3_value, "500 kbps");

    lv_obj_t *row4 = lv_obj_create(cont);
    lv_obj_remove_style_all(row4);
    lv_obj_set_width(row4, LV_PCT(100));
    lv_obj_set_flex_flow(row4, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row4,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label4_title = lv_label_create(row4);
    lv_label_set_text(label4_title, "Protocol:");

    lv_obj_t *label4_value = lv_label_create(row4);
    lv_label_set_text(label4_value, "Victron Energy CAN");

    // Séparateur
    lv_obj_t *sep2 = lv_obj_create(cont);
    lv_obj_set_height(sep2, 1);
    lv_obj_set_width(sep2, LV_PCT(100));
    lv_obj_set_style_bg_color(sep2, lv_palette_main(LV_PALETTE_GREY), 0);

    // Section: Keepalive
    lv_obj_t *label_section_keepalive = lv_label_create(cont);
    lv_label_set_text(label_section_keepalive, "Keepalive Settings:");
    lv_obj_set_style_text_font(label_section_keepalive, &lv_font_montserrat_16, 0);

    lv_obj_t *row5 = lv_obj_create(cont);
    lv_obj_remove_style_all(row5);
    lv_obj_set_width(row5, LV_PCT(100));
    lv_obj_set_flex_flow(row5, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row5,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label5_title = lv_label_create(row5);
    lv_label_set_text(label5_title, "Interval:");

    lv_obj_t *label5_value = lv_label_create(row5);
    lv_label_set_text(label5_value, "1000 ms");

    lv_obj_t *row6 = lv_obj_create(cont);
    lv_obj_remove_style_all(row6);
    lv_obj_set_width(row6, LV_PCT(100));
    lv_obj_set_flex_flow(row6, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row6,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label6_title = lv_label_create(row6);
    lv_label_set_text(label6_title, "Timeout:");

    lv_obj_t *label6_value = lv_label_create(row6);
    lv_label_set_text(label6_value, "5000 ms");

    lv_obj_t *row7 = lv_obj_create(cont);
    lv_obj_remove_style_all(row7);
    lv_obj_set_width(row7, LV_PCT(100));
    lv_obj_set_flex_flow(row7, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row7,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label7_title = lv_label_create(row7);
    lv_label_set_text(label7_title, "Retry Interval:");

    lv_obj_t *label7_value = lv_label_create(row7);
    lv_label_set_text(label7_value, "2000 ms");

    // Séparateur
    lv_obj_t *sep3 = lv_obj_create(cont);
    lv_obj_set_height(sep3, 1);
    lv_obj_set_width(sep3, LV_PCT(100));
    lv_obj_set_style_bg_color(sep3, lv_palette_main(LV_PALETTE_GREY), 0);

    // Section: Identité
    lv_obj_t *label_section_identity = lv_label_create(cont);
    lv_label_set_text(label_section_identity, "Battery Identity:");
    lv_obj_set_style_text_font(label_section_identity, &lv_font_montserrat_16, 0);

    lv_obj_t *row8 = lv_obj_create(cont);
    lv_obj_remove_style_all(row8);
    lv_obj_set_width(row8, LV_PCT(100));
    lv_obj_set_flex_flow(row8, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row8,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label8_title = lv_label_create(row8);
    lv_label_set_text(label8_title, "Manufacturer:");

    lv_obj_t *label8_value = lv_label_create(row8);
    lv_label_set_text(label8_value, "Enepaq");

    lv_obj_t *row9 = lv_obj_create(cont);
    lv_obj_remove_style_all(row9);
    lv_obj_set_width(row9, LV_PCT(100));
    lv_obj_set_flex_flow(row9, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row9,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label9_title = lv_label_create(row9);
    lv_label_set_text(label9_title, "Battery Name:");

    lv_obj_t *label9_value = lv_label_create(row9);
    lv_label_set_text(label9_value, "ESP32-P4-BMS");

    lv_obj_t *row10 = lv_obj_create(cont);
    lv_obj_remove_style_all(row10);
    lv_obj_set_width(row10, LV_PCT(100));
    lv_obj_set_flex_flow(row10, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row10,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label10_title = lv_label_create(row10);
    lv_label_set_text(label10_title, "Family:");

    lv_obj_t *label10_value = lv_label_create(row10);
    lv_label_set_text(label10_value, "LiFePO4");
}
