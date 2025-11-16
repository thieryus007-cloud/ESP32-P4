// components/gui_lvgl/screen_home.c

#include "screen_home.h"

#include "lvgl.h"
#include <stdio.h>

// Pointeurs vers les widgets pour les mettre à jour
static lv_obj_t *s_label_soc        = NULL;
static lv_obj_t *s_label_voltage    = NULL;
static lv_obj_t *s_label_current    = NULL;
static lv_obj_t *s_label_power      = NULL;
static lv_obj_t *s_label_temp       = NULL;

static lv_obj_t *s_label_status_bms   = NULL;
static lv_obj_t *s_label_status_can   = NULL;
static lv_obj_t *s_label_status_mqtt  = NULL;
static lv_obj_t *s_label_status_wifi  = NULL;
static lv_obj_t *s_label_status_bal   = NULL;  // badge BAL
static lv_obj_t *s_label_status_alm   = NULL;  // 🔔 badge ALM

// Helpers pour couleur d'état
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

void screen_home_create(lv_obj_t *parent)
{
    // Layout global : colonne avec marges
    lv_obj_set_style_pad_all(parent, 8, 0);

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // --- Ligne SOC (gros) ---
    lv_obj_t *row_soc = lv_obj_create(cont);
    lv_obj_remove_style_all(row_soc);
    lv_obj_set_width(row_soc, LV_PCT(100));
    lv_obj_set_flex_flow(row_soc, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_soc,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label_soc_title = lv_label_create(row_soc);
    lv_label_set_text(label_soc_title, "SOC");

    s_label_soc = lv_label_create(row_soc);
    lv_obj_set_style_text_font(s_label_soc, &lv_font_montserrat_32, 0);
    lv_label_set_text(s_label_soc, "-- %");

    // --- Ligne tensions / courants / puissance / température ---
    lv_obj_t *row_values = lv_obj_create(cont);
    lv_obj_remove_style_all(row_values);
    lv_obj_set_width(row_values, LV_PCT(100));
    lv_obj_set_flex_flow(row_values, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_values,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *col_left = lv_obj_create(row_values);
    lv_obj_remove_style_all(col_left);
    lv_obj_set_flex_flow(col_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_left,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *col_right = lv_obj_create(row_values);
    lv_obj_remove_style_all(col_right);
    lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_right,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);

    // Voltage
    lv_obj_t *label_v_title = lv_label_create(col_left);
    lv_label_set_text(label_v_title, "Voltage");

    s_label_voltage = lv_label_create(col_right);
    lv_label_set_text(s_label_voltage, "--.- V");

    // Current
    lv_obj_t *label_i_title = lv_label_create(col_left);
    lv_label_set_text(label_i_title, "Courant");

    s_label_current = lv_label_create(col_right);
    lv_label_set_text(s_label_current, "--.- A");

    // Power
    lv_obj_t *label_p_title = lv_label_create(col_left);
    lv_label_set_text(label_p_title, "Puissance");

    s_label_power = lv_label_create(col_right);
    lv_label_set_text(s_label_power, "---- W");

    // Température
    lv_obj_t *label_t_title = lv_label_create(col_left);
    lv_label_set_text(label_t_title, "Temp");

    s_label_temp = lv_label_create(col_right);
    lv_label_set_text(s_label_temp, "--.- °C");

    // --- Ligne statuts (BMS / CAN / MQTT / WiFi / BAL / ALM) ---
    lv_obj_t *row_status = lv_obj_create(cont);
    lv_obj_remove_style_all(row_status);
    lv_obj_set_width(row_status, LV_PCT(100));
    lv_obj_set_flex_flow(row_status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_status,
                          LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_label_status_bms  = lv_label_create(row_status);
    s_label_status_can  = lv_label_create(row_status);
    s_label_status_mqtt = lv_label_create(row_status);
    s_label_status_wifi = lv_label_create(row_status);
    s_label_status_bal  = lv_label_create(row_status);
    s_label_status_alm  = lv_label_create(row_status);  // nouveau

    set_status_label(s_label_status_bms,  "BMS",  color_neutral());
    set_status_label(s_label_status_can,  "CAN",  color_neutral());
    set_status_label(s_label_status_mqtt, "MQTT", color_neutral());
    set_status_label(s_label_status_wifi, "WiFi", color_neutral());
    set_status_label(s_label_status_bal,  "BAL",  color_neutral());
    set_status_label(s_label_status_alm,  "ALM",  color_neutral());
}

void screen_home_update_battery(const battery_status_t *status)
{
    if (!status) return;

    char buf[64];

    if (s_label_soc) {
        snprintf(buf, sizeof(buf), "%.1f %%", status->soc);
        lv_label_set_text(s_label_soc, buf);
    }
    if (s_label_voltage) {
        snprintf(buf, sizeof(buf), "%.2f V", status->voltage);
        lv_label_set_text(s_label_voltage, buf);
    }
    if (s_label_current) {
        snprintf(buf, sizeof(buf), "%.2f A", status->current);
        lv_label_set_text(s_label_current, buf);
    }
    if (s_label_power) {
        snprintf(buf, sizeof(buf), "%.0f W", status->power);
        lv_label_set_text(s_label_power, buf);
    }
    if (s_label_temp) {
        snprintf(buf, sizeof(buf), "%.1f °C", status->temperature);
        lv_label_set_text(s_label_temp, buf);
    }

    // Couleurs BMS/CAN/MQTT basées sur les flags du status batterie
    if (s_label_status_bms) {
        set_status_label(s_label_status_bms,
                         "BMS",
                         status->bms_ok ? color_ok() : color_error());
    }
    if (s_label_status_can) {
        set_status_label(s_label_status_can,
                         "CAN",
                         status->can_ok ? color_ok() : color_error());
    }
    if (s_label_status_mqtt) {
        set_status_label(s_label_status_mqtt,
                         "MQTT",
                         status->mqtt_ok ? color_ok() : color_error());
    }
}

void screen_home_update_system(const system_status_t *status)
{
    if (!status) return;

    // WiFi / storage / erreurs globales -> on les reflète sur "WiFi"
    if (s_label_status_wifi) {
        lv_color_t c = color_neutral();
        const char *text = "WiFi";

        if (!status->wifi_connected) {
            c = color_error();
        } else if (!status->server_reachable || !status->storage_ok) {
            c = color_warn();
        } else if (status->has_error) {
            c = color_error();
        } else {
            c = color_ok();
        }
        set_status_label(s_label_status_wifi, text, c);
    }

    // 🔔 Badge ALM : rouge si has_error, neutre sinon
    if (s_label_status_alm) {
        if (status->has_error) {
            set_status_label(s_label_status_alm, "ALM", color_error());
        } else {
            set_status_label(s_label_status_alm, "ALM", color_neutral());
        }
    }
}

/**
 * @brief Badge global "BAL" sur Home :
 *        - BAL : orange si au moins une cellule en balancing
 *        - BAL : gris sinon
 */
void screen_home_update_balancing(const pack_stats_t *stats)
{
    if (!s_label_status_bal) return;

    if (!stats || stats->cell_count == 0) {
        set_status_label(s_label_status_bal, "BAL", color_neutral());
        return;
    }

    bool any_balancing = false;
    uint8_t count = stats->cell_count;
    if (count > PACK_MAX_CELLS) count = PACK_MAX_CELLS;

    for (uint8_t i = 0; i < count; ++i) {
        if (stats->balancing[i]) {
            any_balancing = true;
            break;
        }
    }

    if (any_balancing) {
        set_status_label(s_label_status_bal, "BAL",
                         lv_palette_main(LV_PALETTE_ORANGE));
    } else {
        set_status_label(s_label_status_bal, "BAL", color_neutral());
    }
}
