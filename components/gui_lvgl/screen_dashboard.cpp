// screen_dashboard.cpp
#include "screen_dashboard.h"
#include "include/ui_theme.hpp" // Pointer vers le fichier créé en A
#include <cstdio>
#include <cmath>

// Instance globale statique pour les wrappers C
static gui::ScreenDashboard* s_dashboard = nullptr;

// --- Wrappers C (pour compatibilité avec votre code existant) ---
extern "C" void screen_dashboard_create(lv_obj_t *parent) {
    if (!s_dashboard) {
        s_dashboard = new gui::ScreenDashboard(parent);
    }
}

extern "C" void screen_dashboard_update_battery(const battery_status_t *status) {
    if (s_dashboard && status) {
        s_dashboard->update_battery(*status);
    }
}

extern "C" void screen_dashboard_update_system(const system_status_t *status) {
    if (s_dashboard && status) {
        s_dashboard->update_system(*status);
    }
}

// --- Implémentation C++ ---
namespace gui {

ScreenDashboard::ScreenDashboard(lv_obj_t* parent) {
    root = lv_obj_create(parent);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    theme::apply_screen_style(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN); // Stack vertical
    lv_obj_set_pad_all(root, 10);
    
    build_ui();
}

void ScreenDashboard::build_ui() {
    static lv_style_t style_card;
    theme::init_card_style(&style_card);

    // --- ZONE 1 : Header SOC (Gros Arc central) ---
    lv_obj_t* top_section = lv_obj_create(root);
    lv_obj_add_style(top_section, &style_card, 0);
    lv_obj_set_size(top_section, LV_PCT(100), 220); // Hauteur fixe pour l'arc
    
    arc_soc = lv_arc_create(top_section);
    lv_obj_set_size(arc_soc, 180, 180);
    lv_obj_center(arc_soc);
    lv_arc_set_rotation(arc_soc, 135);
    lv_arc_set_bg_angles(arc_soc, 0, 270);
    lv_arc_set_value(arc_soc, 0);
    lv_obj_remove_style(arc_soc, NULL, LV_PART_KNOB); // Pas de bouton
    lv_obj_clear_flag(arc_soc, LV_FLAG_CLICKABLE);
    // Arc épais
    lv_obj_set_style_arc_width(arc_soc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_soc, 20, LV_PART_INDICATOR);

    label_soc = lv_label_create(top_section);
    lv_obj_center(label_soc);
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_48, 0);
    lv_label_set_text(label_soc, "-- %");

    // --- ZONE 2 : Grid Métriques (U, I, P) ---
    lv_obj_t* metrics_grid = lv_obj_create(root);
    lv_obj_add_style(metrics_grid, &style_card, 0);
    lv_obj_set_size(metrics_grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(metrics_grid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(metrics_grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Voltage
    lv_obj_t* box_v = lv_obj_create(metrics_grid);
    lv_obj_set_size(box_v, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box_v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_v, 0, 0);
    lv_obj_set_flex_flow(box_v, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box_v, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* lbl_v_title = lv_label_create(box_v);
    lv_label_set_text(lbl_v_title, "VOLTAGE");
    theme::apply_title_style(lbl_v_title);
    
    label_voltage = lv_label_create(box_v);
    lv_label_set_text(label_voltage, "--.- V");
    lv_obj_set_style_text_font(label_voltage, &lv_font_montserrat_20, 0);

    // Puissance (Centre)
    lv_obj_t* box_p = lv_obj_create(metrics_grid);
    lv_obj_set_size(box_p, LV_PCT(35), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box_p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_p, 0, 0);
    lv_obj_set_flex_flow(box_p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box_p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl_p_title = lv_label_create(box_p);
    lv_label_set_text(lbl_p_title, "POWER");
    theme::apply_title_style(lbl_p_title);

    label_power = lv_label_create(box_p);
    lv_label_set_text(label_power, "0 W");
    lv_obj_set_style_text_font(label_power, &lv_font_montserrat_24, 0); // Plus gros

    // Courant
    lv_obj_t* box_i = lv_obj_create(metrics_grid);
    lv_obj_set_size(box_i, LV_PCT(30), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box_i, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box_i, 0, 0);
    lv_obj_set_flex_flow(box_i, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box_i, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl_i_title = lv_label_create(box_i);
    lv_label_set_text(lbl_i_title, "CURRENT");
    theme::apply_title_style(lbl_i_title);

    label_current = lv_label_create(box_i);
    lv_label_set_text(label_current, "--.- A");
    lv_obj_set_style_text_font(label_current, &lv_font_montserrat_20, 0);

    // --- ZONE 3 : Status LEDs ---
    lv_obj_t* status_bar = lv_obj_create(root);
    lv_obj_add_style(status_bar, &style_card, 0);
    lv_obj_set_size(status_bar, LV_PCT(100), 60);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Helper pour créer une "LED" avec texte
    auto create_status_led = [&](const char* txt) -> lv_obj_t* {
        lv_obj_t* cont = lv_obj_create(status_bar);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_size(cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t* led = lv_led_create(cont);
        lv_obj_set_size(led, 14, 14);
        lv_led_set_color(led, theme::color_primary());
        lv_led_off(led);

        lv_obj_t* l = lv_label_create(cont);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_left(l, 5, 0);
        
        return led;
    };

    led_wifi = create_status_led("WiFi");
    led_mqtt = create_status_led("MQTT");
    led_can  = create_status_led("CAN");
}

void ScreenDashboard::update_battery(const battery_status_t& status) {
    // Update SOC
    lv_arc_set_value(arc_soc, (int)status.soc);
    lv_label_set_text_fmt(label_soc, "%.0f %%", status.soc);
    update_soc_visuals(status.soc);

    // Update Metrics
    lv_label_set_text_fmt(label_voltage, "%.2f V", status.voltage);
    lv_label_set_text_fmt(label_current, "%.1f A", status.current);
    lv_label_set_text_fmt(label_power, "%.0f W", status.power);

    // Couleur dynamique Puissance
    if(status.power > 0) {
        lv_obj_set_style_text_color(label_power, theme::color_good(), 0); // Charge (Vert)
    } else if (status.power < 0) {
        lv_obj_set_style_text_color(label_power, theme::color_warn(), 0); // Décharge (Jaune/Orange)
    } else {
        lv_obj_set_style_text_color(label_power, theme::color_text(), 0); // Repos
    }
}

void ScreenDashboard::update_system(const system_status_t& status) {
    // WiFi
    if(status.wifi_connected) {
        lv_led_set_color(led_wifi, theme::color_good());
        lv_led_on(led_wifi);
    } else {
        lv_led_set_color(led_wifi, theme::color_crit());
        lv_led_on(led_wifi);
    }

    // MQTT (Basé sur server_reachable pour l'instant)
    if(status.server_reachable) {
        lv_led_set_color(led_mqtt, theme::color_good());
        lv_led_on(led_mqtt);
    } else {
        lv_led_off(led_mqtt);
    }
    
    // CAN (A adapter si on a l'info, sinon bleu par défaut)
     lv_led_set_color(led_can, theme::color_primary());
     lv_led_on(led_can);
}

void ScreenDashboard::update_soc_visuals(float soc) {
    lv_color_t c;
    if(soc < 20) c = theme::color_crit();
    else if(soc < 40) c = theme::color_warn();
    else c = theme::color_primary(); // Bleu Victron standard
    
    lv_obj_set_style_arc_color(arc_soc, c, LV_PART_INDICATOR);
}

} // namespace gui
  
