#include "screen_can_status.h"
#include "include/ui_theme.hpp"
#include <cstdio>

static gui::ScreenCanStatus* s_can = nullptr;

extern "C" void screen_can_status_create(lv_obj_t *parent) {
    if (!s_can) s_can = new gui::ScreenCanStatus(parent);
}

extern "C" void screen_can_status_update(const cvl_limits_event_t *limits) {
    if (s_can && limits) s_can->update(*limits);
}

namespace gui {

ScreenCanStatus::ScreenCanStatus(lv_obj_t* parent) {
    root = lv_obj_create(parent);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    theme::apply_screen_style(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_pad_all(root, 15);
    
    build_ui();
}

void ScreenCanStatus::build_ui() {
    static lv_style_t style_card;
    theme::init_card_style(&style_card);

    // Titre
    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, "VICTRON BMS-CAN LIMITS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, theme::color_text_sec(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // --- Ligne des Limites (CVL / CCL / DCL) ---
    lv_obj_t* grid = lv_obj_create(root);
    lv_obj_add_style(grid, &style_card, 0);
    lv_obj_set_size(grid, LV_PCT(100), 140);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto create_limit_box = [&](const char* name, lv_obj_t** lbl_val, lv_color_t color) {
        lv_obj_t* box = lv_obj_create(grid);
        lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(box, 0, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t* t = lv_label_create(box);
        lv_label_set_text(t, name);
        theme::apply_title_style(t);

        *lbl_val = lv_label_create(box);
        lv_label_set_text(*lbl_val, "--");
        lv_obj_set_style_text_font(*lbl_val, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(*lbl_val, color, 0);
    };

    create_limit_box("CVL (V)", &label_cvl, theme::color_primary());
    create_limit_box("CCL (A)", &label_ccl, theme::color_good());
    create_limit_box("DCL (A)", &label_dcl, theme::color_warn());

    // --- Section Status / Flags ---
    lv_obj_t* status_card = lv_obj_create(root);
    lv_obj_add_style(status_card, &style_card, 0);
    lv_obj_set_size(status_card, LV_PCT(100), 100);
    lv_obj_set_flex_flow(status_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_card, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // LED helper
    auto create_flag = [&](const char* txt, lv_obj_t** led_out) {
        lv_obj_t* cont = lv_obj_create(status_card);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        *led_out = lv_led_create(cont);
        lv_led_set_color(*led_out, theme::color_crit());
        lv_led_off(*led_out);
        
        lv_obj_t* l = lv_label_create(cont);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    };

    create_flag("Imbalance Hold", &led_imbalance);
    create_flag("Cell Protect", &led_cell_prot);

    // Texte État Machine
    label_state_txt = lv_label_create(status_card);
    lv_label_set_text(label_state_txt, "STATE: INIT");
}

void ScreenCanStatus::update(const cvl_limits_event_t& limits) {
    // Update Values
    lv_label_set_text_fmt(label_cvl, "%.1f", limits.cvl_voltage_v);
    lv_label_set_text_fmt(label_ccl, "%.0f", limits.ccl_current_a);
    lv_label_set_text_fmt(label_dcl, "%.0f", limits.dcl_current_a);

    // Update Flags
    if(limits.imbalance_hold_active) lv_led_on(led_imbalance);
    else lv_led_off(led_imbalance);

    if(limits.cell_protection_active) lv_led_on(led_cell_prot);
    else lv_led_off(led_cell_prot);

    // State debug text
    lv_label_set_text_fmt(label_state_txt, "STATE: %d", limits.cvl_state);
}

} // namespace gui
