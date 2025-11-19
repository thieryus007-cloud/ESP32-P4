// components/gui_lvgl/screen_power.cpp

#include "screen_power.h"

#include "gui_format.hpp"
#include "ui_i18n.h"

using gui::set_label_textf;

static lv_obj_t *s_label_pv       = NULL;
static lv_obj_t *s_label_batt     = NULL;
static lv_obj_t *s_label_flow     = NULL;
static lv_obj_t *s_label_load     = NULL;
static lv_obj_t *s_label_status   = NULL;

static battery_status_t s_last_batt;
static system_status_t s_last_sys;
static bool s_has_batt = false;
static bool s_has_sys = false;

extern "C" {

void screen_power_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 16, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // --- Première ligne : PV ---
    s_label_pv = lv_label_create(parent);
    lv_label_set_text(s_label_pv, ui_i18n("power.pv"));

    // --- Ligne centrale : Flow schema ---
    lv_obj_t *cont_flow = lv_obj_create(parent);
    lv_obj_remove_style_all(cont_flow);
    lv_obj_set_flex_flow(cont_flow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_flow,
                          LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(cont_flow, LV_PCT(100));

    // Batterie
    s_label_batt = lv_label_create(cont_flow);
    lv_label_set_text(s_label_batt, ui_i18n("power.battery"));

    // Flèche
    s_label_flow = lv_label_create(cont_flow);
    lv_label_set_text(s_label_flow, "--W");

    // Load/Grid
    s_label_load = lv_label_create(cont_flow);
    lv_label_set_text(s_label_load, ui_i18n("power.load"));

    // --- Ligne status global ---
    s_label_status = lv_label_create(parent);
    lv_label_set_text(s_label_status, ui_i18n("power.status.ok"));
}

void screen_power_update(const battery_status_t *status)
{
    if (!status) return;

    s_last_batt = *status;
    s_has_batt = true;

    // PV: pour l'instant N/A, à alimenter plus tard si JSON pv_power_w existe
    if (s_label_pv) {
        lv_label_set_text(s_label_pv, ui_i18n("power.pv"));
    }

    // Batterie : V, I, P
    if (s_label_batt) {
        set_label_textf(s_label_batt,
                        "{:s}  {:.1f} V / {:.1f} A",
                        ui_i18n("power.battery"),
                        status->voltage,
                        status->current);
    }

    // Flèche / Flow : signe de la puissance
    if (s_label_flow) {
        const char *arrow = ui_i18n("power.flow.default"); // par défaut, batterie vers load
        const char *dir   = ui_i18n("power.flow.dir_discharge");

        if (status->power < -1.0f) {
            // batterie en CHARGE (consomme), flux en sens inverse
            arrow = ui_i18n("power.flow.charge");
            dir   = ui_i18n("power.flow.dir_charge");
        }

        set_label_textf(s_label_flow, "{:s}  {:.0f} W  {:s}", arrow, status->power, dir);

        // Couleur de la flèche : vert en décharge (= alimente), bleu en charge
        lv_color_t c = (status->power >= 0.0f)
                       ? lv_palette_main(LV_PALETTE_GREEN)
                       : lv_palette_main(LV_PALETTE_BLUE);
        lv_obj_set_style_text_color(s_label_flow, c, 0);
    }
}

void screen_power_update_system(const system_status_t *status)
{
    if (!status || !s_label_status) return;

    s_last_sys = *status;
    s_has_sys = true;

    const char *txt = ui_i18n("power.status.ok");
    lv_color_t col  = lv_palette_main(LV_PALETTE_GREEN);

    if (!status->telemetry_expected) {
        txt = "Mode autonome";
        col = lv_palette_main(LV_PALETTE_BLUE);
    } else if (!status->wifi_connected || !status->storage_ok || status->has_error) {
        txt = ui_i18n("power.status.check");
        col = lv_palette_main(LV_PALETTE_RED);
    }

    lv_label_set_text(s_label_status, txt);
    lv_obj_set_style_text_color(s_label_status, col, 0);
}

void screen_power_refresh_texts(void)
{
    if (s_label_pv) {
        lv_label_set_text(s_label_pv, ui_i18n("power.pv"));
    }
    if (s_label_batt) {
        lv_label_set_text(s_label_batt, ui_i18n("power.battery"));
    }
    if (s_label_load) {
        lv_label_set_text(s_label_load, ui_i18n("power.load"));
    }

    if (s_has_batt) {
        screen_power_update(&s_last_batt);
    }
    if (s_has_sys) {
        screen_power_update_system(&s_last_sys);
    } else if (s_label_status) {
        lv_label_set_text(s_label_status, ui_i18n("power.status.ok"));
    }
}

}  // extern "C"
