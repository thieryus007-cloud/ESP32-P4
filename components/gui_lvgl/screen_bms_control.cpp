// components/gui_lvgl/screen_bms_control.c

#include "screen_bms_control.h"
#include "cvl_types.h"
#include "lvgl.h"
#include <stdio.h>

// Pointeurs vers les widgets
static lv_obj_t *s_label_cvl_state = NULL;
static lv_obj_t *s_label_cvl_voltage = NULL;
static lv_obj_t *s_label_ccl_current = NULL;
static lv_obj_t *s_label_dcl_current = NULL;
static lv_obj_t *s_label_imbalance_hold = NULL;
static lv_obj_t *s_label_cell_protection = NULL;

// Helpers couleur
static lv_color_t color_ok(void)      { return lv_palette_main(LV_PALETTE_GREEN); }
static lv_color_t color_warn(void)    { return lv_palette_main(LV_PALETTE_YELLOW); }
static lv_color_t color_error(void)   { return lv_palette_main(LV_PALETTE_RED); }
static lv_color_t color_neutral(void) { return lv_palette_main(LV_PALETTE_GREY); }

static void set_status_label(lv_obj_t *label, const char *text, lv_color_t color)
{
    if (!label) return;
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
}

void screen_bms_control_create(lv_obj_t *parent)
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
    lv_label_set_text(label_title, "BMS Control (CVL)");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, 0);

    // Section: CVL State
    lv_obj_t *label_section_state = lv_label_create(cont);
    lv_label_set_text(label_section_state, "Charge Voltage Limit State:");
    lv_obj_set_style_text_font(label_section_state, &lv_font_montserrat_16, 0);

    lv_obj_t *row1 = lv_obj_create(cont);
    lv_obj_remove_style_all(row1);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label1_title = lv_label_create(row1);
    lv_label_set_text(label1_title, "State:");

    s_label_cvl_state = lv_label_create(row1);
    lv_label_set_text(s_label_cvl_state, "BULK");
    lv_obj_set_style_text_font(s_label_cvl_state, &lv_font_montserrat_20, 0);

    // Séparateur
    lv_obj_t *sep1 = lv_obj_create(cont);
    lv_obj_set_height(sep1, 1);
    lv_obj_set_width(sep1, LV_PCT(100));
    lv_obj_set_style_bg_color(sep1, lv_palette_main(LV_PALETTE_GREY), 0);

    // Section: Limites
    lv_obj_t *label_section_limits = lv_label_create(cont);
    lv_label_set_text(label_section_limits, "Charge/Discharge Limits:");
    lv_obj_set_style_text_font(label_section_limits, &lv_font_montserrat_16, 0);

    lv_obj_t *row2 = lv_obj_create(cont);
    lv_obj_remove_style_all(row2);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label2_title = lv_label_create(row2);
    lv_label_set_text(label2_title, "CVL (Charge Voltage):");

    s_label_cvl_voltage = lv_label_create(row2);
    lv_label_set_text(s_label_cvl_voltage, "--.- V");
    lv_obj_set_style_text_font(s_label_cvl_voltage, &lv_font_montserrat_18, 0);

    lv_obj_t *row3 = lv_obj_create(cont);
    lv_obj_remove_style_all(row3);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label3_title = lv_label_create(row3);
    lv_label_set_text(label3_title, "CCL (Charge Current):");

    s_label_ccl_current = lv_label_create(row3);
    lv_label_set_text(s_label_ccl_current, "--.- A");
    lv_obj_set_style_text_font(s_label_ccl_current, &lv_font_montserrat_18, 0);

    lv_obj_t *row4 = lv_obj_create(cont);
    lv_obj_remove_style_all(row4);
    lv_obj_set_width(row4, LV_PCT(100));
    lv_obj_set_flex_flow(row4, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row4,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label4_title = lv_label_create(row4);
    lv_label_set_text(label4_title, "DCL (Discharge Current):");

    s_label_dcl_current = lv_label_create(row4);
    lv_label_set_text(s_label_dcl_current, "--.- A");
    lv_obj_set_style_text_font(s_label_dcl_current, &lv_font_montserrat_18, 0);

    // Séparateur
    lv_obj_t *sep2 = lv_obj_create(cont);
    lv_obj_set_height(sep2, 1);
    lv_obj_set_width(sep2, LV_PCT(100));
    lv_obj_set_style_bg_color(sep2, lv_palette_main(LV_PALETTE_GREY), 0);

    // Section: Protections
    lv_obj_t *label_section_protection = lv_label_create(cont);
    lv_label_set_text(label_section_protection, "Protection Status:");
    lv_obj_set_style_text_font(label_section_protection, &lv_font_montserrat_16, 0);

    lv_obj_t *row5 = lv_obj_create(cont);
    lv_obj_remove_style_all(row5);
    lv_obj_set_width(row5, LV_PCT(100));
    lv_obj_set_flex_flow(row5, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row5,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label5_title = lv_label_create(row5);
    lv_label_set_text(label5_title, "Imbalance Hold:");

    s_label_imbalance_hold = lv_label_create(row5);
    lv_label_set_text(s_label_imbalance_hold, "INACTIVE");
    lv_obj_set_style_text_color(s_label_imbalance_hold, color_ok(), 0);

    lv_obj_t *row6 = lv_obj_create(cont);
    lv_obj_remove_style_all(row6);
    lv_obj_set_width(row6, LV_PCT(100));
    lv_obj_set_flex_flow(row6, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row6,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label6_title = lv_label_create(row6);
    lv_label_set_text(label6_title, "Cell Protection:");

    s_label_cell_protection = lv_label_create(row6);
    lv_label_set_text(s_label_cell_protection, "INACTIVE");
    lv_obj_set_style_text_color(s_label_cell_protection, color_ok(), 0);

    // Séparateur
    lv_obj_t *sep3 = lv_obj_create(cont);
    lv_obj_set_height(sep3, 1);
    lv_obj_set_width(sep3, LV_PCT(100));
    lv_obj_set_style_bg_color(sep3, lv_palette_main(LV_PALETTE_GREY), 0);

    // Info CVL States
    lv_obj_t *label_info = lv_label_create(cont);
    lv_label_set_text(label_info,
        "CVL States:\n"
        "BULK: Rapid charging (SOC < 90%)\n"
        "TRANSITION: Moving to float (90-95%)\n"
        "FLOAT_APPROACH: Nearing float (95-98%)\n"
        "FLOAT: Maintenance charge (SOC > 98%)\n"
        "IMBALANCE_HOLD: Cell balance protection\n"
        "SUSTAIN: Low SOC maintenance (< 5%)");
    lv_label_set_long_mode(label_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_info, LV_PCT(95));
    lv_obj_set_style_text_color(label_info, lv_palette_main(LV_PALETTE_GREY), 0);
}

void screen_bms_control_update_cvl(const cvl_limits_event_t *limits)
{
    if (!limits) return;

    char buf[64];

    // CVL State
    const char *state_str = "UNKNOWN";
    lv_color_t state_color = color_neutral();

    switch (limits->cvl_state) {
        case CVL_STATE_BULK:
            state_str = "BULK";
            state_color = color_ok();
            break;
        case CVL_STATE_TRANSITION:
            state_str = "TRANSITION";
            state_color = color_ok();
            break;
        case CVL_STATE_FLOAT_APPROACH:
            state_str = "FLOAT_APPROACH";
            state_color = color_ok();
            break;
        case CVL_STATE_FLOAT:
            state_str = "FLOAT";
            state_color = lv_palette_main(LV_PALETTE_BLUE);
            break;
        case CVL_STATE_IMBALANCE_HOLD:
            state_str = "IMBALANCE_HOLD";
            state_color = color_warn();
            break;
        case CVL_STATE_SUSTAIN:
            state_str = "SUSTAIN";
            state_color = color_warn();
            break;
        default:
            break;
    }
    set_status_label(s_label_cvl_state, state_str, state_color);

    // Limites
    snprintf(buf, sizeof(buf), "%.2f V", limits->cvl_voltage_v);
    lv_label_set_text(s_label_cvl_voltage, buf);

    snprintf(buf, sizeof(buf), "%.1f A", limits->ccl_current_a);
    lv_label_set_text(s_label_ccl_current, buf);

    snprintf(buf, sizeof(buf), "%.1f A", limits->dcl_current_a);
    lv_label_set_text(s_label_dcl_current, buf);

    // Protections
    if (limits->imbalance_hold_active) {
        set_status_label(s_label_imbalance_hold, "ACTIVE", color_warn());
    } else {
        set_status_label(s_label_imbalance_hold, "INACTIVE", color_ok());
    }

    if (limits->cell_protection_active) {
        set_status_label(s_label_cell_protection, "ACTIVE", color_error());
    } else {
        set_status_label(s_label_cell_protection, "INACTIVE", color_ok());
    }
}
