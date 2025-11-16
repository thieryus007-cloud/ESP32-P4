// components/gui_lvgl/screen_power.c

#include "screen_power.h"

#include <stdio.h>
#include <stdarg.h>

static lv_obj_t *s_label_pv       = NULL;
static lv_obj_t *s_label_batt     = NULL;
static lv_obj_t *s_label_flow     = NULL;
static lv_obj_t *s_label_load     = NULL;
static lv_obj_t *s_label_status   = NULL;

static void set_label_fmt(lv_obj_t *label, const char *fmt, ...)
{
    if (!label || !fmt) return;
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    lv_label_set_text(label, buf);
}

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
    lv_label_set_text(s_label_pv, "PV: N/A");

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
    lv_label_set_text(s_label_batt, "[Battery]");

    // Flèche
    s_label_flow = lv_label_create(cont_flow);
    lv_label_set_text(s_label_flow, "--W");

    // Load/Grid
    s_label_load = lv_label_create(cont_flow);
    lv_label_set_text(s_label_load, "[Load/Grid]");

    // --- Ligne status global ---
    s_label_status = lv_label_create(parent);
    lv_label_set_text(s_label_status, "Status: --");
}

void screen_power_update(const battery_status_t *status)
{
    if (!status) return;

    // PV: pour l'instant N/A, à alimenter plus tard si JSON pv_power_w existe
    if (s_label_pv) {
        lv_label_set_text(s_label_pv, "PV: N/A");
    }

    // Batterie : V, I, P
    if (s_label_batt) {
        set_label_fmt(s_label_batt,
                      "[Battery]  %.1f V / %.1f A",
                      status->voltage,
                      status->current);
    }

    // Flèche / Flow : signe de la puissance
    if (s_label_flow) {
        const char *arrow = "→"; // par défaut, batterie vers load
        const char *dir   = "to LOAD";

        if (status->power < -1.0f) {
            // batterie en CHARGE (consomme), flux en sens inverse
            arrow = "←";
            dir   = "from LOAD/GRID";
        }

        set_label_fmt(s_label_flow, "%s  %.0f W  %s", arrow, status->power, dir);

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

    const char *txt = "Status: OK";
    lv_color_t col  = lv_palette_main(LV_PALETTE_GREEN);

    if (!status->wifi_connected || !status->storage_ok || status->has_error) {
        txt = "Status: CHECK SYSTEM";
        col = lv_palette_main(LV_PALETTE_RED);
    }

    lv_label_set_text(s_label_status, txt);
    lv_obj_set_style_text_color(s_label_status, col, 0);
}
