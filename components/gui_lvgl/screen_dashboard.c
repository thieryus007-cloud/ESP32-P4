// components/gui_lvgl/screen_dashboard.c

#include "screen_dashboard.h"

#include <stdio.h>

// Helpers de style pour les cartes du dashboard
static lv_obj_t *create_card(lv_obj_t *parent, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(48));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 8, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);

    return card;
}

// --- Pointeurs vers les widgets ---
static lv_obj_t *s_meter_soc = NULL;
static lv_meter_scale_t *s_meter_soc_scale = NULL;
static lv_meter_indicator_t *s_meter_soc_needle = NULL;
static lv_meter_indicator_t *s_meter_soh_needle = NULL;

static lv_obj_t *s_meter_temp = NULL;
static lv_meter_scale_t *s_meter_temp_scale = NULL;
static lv_meter_indicator_t *s_meter_temp_needle = NULL;

static lv_obj_t *s_chart_power = NULL;
static lv_chart_series_t *s_chart_series_power = NULL;
static lv_chart_series_t *s_chart_series_current = NULL;

static lv_obj_t *s_label_voltage = NULL;
static lv_obj_t *s_label_status_wifi = NULL;
static lv_obj_t *s_label_status_storage = NULL;
static lv_obj_t *s_label_status_errors = NULL;

// --- Helpers mises à jour ---
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

static lv_obj_t *create_meter_gauge(lv_obj_t *parent, const char *center_text,
                                    float min, float max,
                                    lv_meter_scale_t **out_scale,
                                    lv_meter_indicator_t **out_needle)
{
    lv_obj_t *meter = lv_meter_create(parent);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 160, 160);

    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 21, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, scale, 4, 4, 15, lv_palette_main(LV_PALETTE_GREY), 10);
    lv_meter_set_scale_range(meter, scale, min, max, 270, 135);

    // Zones colorées warning/critique
    lv_meter_indicator_t *indic_warn = lv_meter_add_scale_lines(meter, scale,
                                                               color_warn(), color_warn(), false, 10, 0);
    lv_meter_set_indicator_start_value(meter, indic_warn, (int) (max * 0.8f));
    lv_meter_set_indicator_end_value(meter, indic_warn, (int) (max * 0.9f));

    lv_meter_indicator_t *indic_alert = lv_meter_add_scale_lines(meter, scale,
                                                                color_error(), color_error(), false, 10, 0);
    lv_meter_set_indicator_start_value(meter, indic_alert, (int) (max * 0.9f));
    lv_meter_set_indicator_end_value(meter, indic_alert, (int) max);

    *out_needle = lv_meter_add_needle_line(meter, scale, 4,
                                           lv_palette_main(LV_PALETTE_BLUE), -15);

    if (out_scale) {
        *out_scale = scale;
    }

    lv_obj_t *center_label = lv_label_create(meter);
    lv_label_set_text(center_label, center_text);
    lv_obj_center(center_label);

    return meter;
}

static lv_obj_t *create_status_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_label_status_wifi    = lv_label_create(row);
    s_label_status_storage = lv_label_create(row);
    s_label_status_errors  = lv_label_create(row);

    set_status_label(s_label_status_wifi, "WiFi", color_neutral());
    set_status_label(s_label_status_storage, "Storage", color_neutral());
    set_status_label(s_label_status_errors, "Errors", color_neutral());

    return row;
}

void screen_dashboard_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    // Carte SOC/SOH
    lv_obj_t *card_soc = create_card(parent, "SOC / SOH");
    s_meter_soc = create_meter_gauge(card_soc, "SOC", 0, 100, &s_meter_soc_scale, &s_meter_soc_needle);
    if (s_meter_soc_scale) {
        s_meter_soh_needle = lv_meter_add_needle_line(s_meter_soc, s_meter_soc_scale, 3,
                                                     lv_palette_main(LV_PALETTE_ORANGE), 20);
    }

    // Carte température
    lv_obj_t *card_temp = create_card(parent, "Température");
    s_meter_temp = create_meter_gauge(card_temp, "°C", 0, 80, &s_meter_temp_scale, &s_meter_temp_needle);

    // Carte puissance / courant (chart)
    lv_obj_t *card_power = create_card(parent, "Puissance & Courant");
    s_chart_power = lv_chart_create(card_power);
    lv_obj_set_size(s_chart_power, LV_PCT(100), 160);
    lv_chart_set_type(s_chart_power, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(s_chart_power, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(s_chart_power, 20);
    lv_chart_set_range(s_chart_power, LV_CHART_AXIS_PRIMARY_Y, -5000, 5000);
    lv_chart_set_div_line_count(s_chart_power, 4, 6);

    s_chart_series_power = lv_chart_add_series(s_chart_power,
                                               lv_palette_main(LV_PALETTE_BLUE),
                                               LV_CHART_AXIS_PRIMARY_Y);
    s_chart_series_current = lv_chart_add_series(s_chart_power,
                                                 lv_palette_main(LV_PALETTE_GREEN),
                                                 LV_CHART_AXIS_PRIMARY_Y);

    // Carte tension + statuts système
    lv_obj_t *card_status = create_card(parent, "Statuts système");
    s_label_voltage = lv_label_create(card_status);
    lv_label_set_text(s_label_voltage, "--.- V");
    lv_obj_set_style_text_font(s_label_voltage, &lv_font_montserrat_22, 0);

    create_status_row(card_status);
}

void screen_dashboard_update_battery(const battery_status_t *status)
{
    if (!status) return;

    if (s_meter_soc && s_meter_soc_needle) {
        lv_meter_set_indicator_value(s_meter_soc, s_meter_soc_needle, (int) status->soc);
    }

    if (s_meter_soc && s_meter_soh_needle) {
        lv_meter_set_indicator_value(s_meter_soc, s_meter_soh_needle, (int) status->soh);
    }

    if (s_meter_temp && s_meter_temp_needle) {
        // Clamp entre 0 et 80 pour rester dans l'échelle du meter
        float temp = status->temperature;
        if (temp < 0) temp = 0;
        if (temp > 80) temp = 80;
        lv_meter_set_indicator_value(s_meter_temp, s_meter_temp_needle, (int) temp);
    }

    if (s_chart_power && s_chart_series_power && s_chart_series_current) {
        lv_chart_set_next_value(s_chart_power, s_chart_series_power, (lv_coord_t) status->power);
        lv_chart_set_next_value(s_chart_power, s_chart_series_current, (lv_coord_t) (status->current * 10));
    }

    if (s_label_voltage) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f V", status->voltage);
        lv_label_set_text(s_label_voltage, buf);
    }
}

void screen_dashboard_update_system(const system_status_t *status)
{
    if (!status) return;

    if (s_label_status_wifi) {
        set_status_label(s_label_status_wifi,
                         "WiFi",
                         status->wifi_connected ? color_ok() : color_error());
    }

    if (s_label_status_storage) {
        lv_color_t color = status->storage_ok ? color_ok() : color_error();
        set_status_label(s_label_status_storage, "Storage", color);
    }

    if (s_label_status_errors) {
        lv_color_t color = status->has_error ? color_warn() : color_ok();
        set_status_label(s_label_status_errors, "Errors", color);
    }
}
