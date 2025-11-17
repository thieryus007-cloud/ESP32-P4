// components/gui_lvgl/screen_home.c

#include "screen_home.h"

#include "lvgl.h"
#include <stdio.h>
#include "ui_i18n.h"

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

static lv_obj_t *s_label_soc_title    = NULL;
static lv_obj_t *s_label_voltage_title = NULL;
static lv_obj_t *s_label_current_title = NULL;
static lv_obj_t *s_label_power_title   = NULL;
static lv_obj_t *s_label_temp_title    = NULL;

static battery_status_t s_last_batt;
static bool s_has_batt = false;
static system_status_t s_last_sys;
static bool s_has_sys = false;
static pack_stats_t s_last_pack;
static bool s_has_pack = false;

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

static void apply_static_texts(void)
{
    ui_i18n_label_set_text(s_label_soc_title, "home.soc");
    ui_i18n_label_set_text(s_label_voltage_title, "home.voltage");
    ui_i18n_label_set_text(s_label_current_title, "home.current");
    ui_i18n_label_set_text(s_label_power_title, "home.power");
    ui_i18n_label_set_text(s_label_temp_title, "home.temperature");

    set_status_label(s_label_status_bms, ui_i18n("home.status.bms"), color_neutral());
    set_status_label(s_label_status_can, ui_i18n("home.status.can"), color_neutral());
    set_status_label(s_label_status_mqtt, ui_i18n("home.status.mqtt"), color_neutral());
    set_status_label(s_label_status_wifi, ui_i18n("home.status.wifi"), color_neutral());
    set_status_label(s_label_status_bal, ui_i18n("home.status.bal"), color_neutral());
    set_status_label(s_label_status_alm, ui_i18n("home.status.alm"), color_neutral());
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

    s_label_soc_title = lv_label_create(row_soc);
    ui_i18n_label_set_text(s_label_soc_title, "home.soc");

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
    s_label_voltage_title = lv_label_create(col_left);
    ui_i18n_label_set_text(s_label_voltage_title, "home.voltage");

    s_label_voltage = lv_label_create(col_right);
    lv_label_set_text(s_label_voltage, "--.- V");

    // Current
    s_label_current_title = lv_label_create(col_left);
    ui_i18n_label_set_text(s_label_current_title, "home.current");

    s_label_current = lv_label_create(col_right);
    lv_label_set_text(s_label_current, "--.- A");

    // Power
    s_label_power_title = lv_label_create(col_left);
    ui_i18n_label_set_text(s_label_power_title, "home.power");

    s_label_power = lv_label_create(col_right);
    lv_label_set_text(s_label_power, "---- W");

    // Température
    s_label_temp_title = lv_label_create(col_left);
    ui_i18n_label_set_text(s_label_temp_title, "home.temperature");

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

    apply_static_texts();
}

void screen_home_update_battery(const battery_status_t *status)
{
    if (!status) return;

    s_last_batt = *status;
    s_has_batt = true;

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
                         ui_i18n("home.status.bms"),
                         status->bms_ok ? color_ok() : color_error());
    }
    if (s_label_status_can) {
        set_status_label(s_label_status_can,
                         ui_i18n("home.status.can"),
                         status->can_ok ? color_ok() : color_error());
    }
    if (s_label_status_mqtt) {
        if (s_has_sys && !s_last_sys.telemetry_expected) {
            set_status_label(s_label_status_mqtt, "Autonome", lv_palette_main(LV_PALETTE_BLUE));
        } else {
            set_status_label(s_label_status_mqtt,
                             ui_i18n("home.status.mqtt"),
                             status->mqtt_ok ? color_ok() : color_error());
        }
    }
}

void screen_home_update_system(const system_status_t *status)
{
    if (!status) return;

    s_last_sys = *status;
    s_has_sys = true;

    // WiFi / storage / erreurs globales -> on les reflète sur "WiFi"
    if (s_label_status_wifi) {
        if (!status->telemetry_expected) {
            set_status_label(s_label_status_wifi, "Autonome", lv_palette_main(LV_PALETTE_BLUE));
        } else {
            lv_color_t c = color_neutral();
            const char *text = ui_i18n("home.status.wifi");

            if (status->network_state == NETWORK_STATE_NOT_CONFIGURED) {
                c = color_warn();
                text = "WiFi N/A";
            } else if (status->network_state != NETWORK_STATE_ACTIVE) {
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
    }

    // 🔔 Badge ALM : rouge si has_error, neutre sinon
    if (s_label_status_alm) {
        if (status->has_error) {
            set_status_label(s_label_status_alm, ui_i18n("home.status.alm"), color_error());
        } else {
            set_status_label(s_label_status_alm, ui_i18n("home.status.alm"), color_neutral());
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

    if (stats) {
        s_last_pack = *stats;
        s_has_pack = true;
    } else {
        s_has_pack = false;
    }

    if (!stats || stats->cell_count == 0) {
        set_status_label(s_label_status_bal, ui_i18n("home.status.bal"), color_neutral());
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
        set_status_label(s_label_status_bal, ui_i18n("home.status.bal"),
                         lv_palette_main(LV_PALETTE_ORANGE));
    } else {
        set_status_label(s_label_status_bal, ui_i18n("home.status.bal"), color_neutral());
    }
}

void screen_home_refresh_texts(void)
{
    apply_static_texts();

    if (s_has_batt) {
        screen_home_update_battery(&s_last_batt);
    }
    if (s_has_sys) {
        screen_home_update_system(&s_last_sys);
    }
    if (s_has_pack) {
        screen_home_update_balancing(&s_last_pack);
    } else {
        screen_home_update_balancing(NULL);
    }
}
