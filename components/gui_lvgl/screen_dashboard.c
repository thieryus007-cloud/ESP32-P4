// components/gui_lvgl/screen_dashboard.c

#include "screen_dashboard.h"

#include <float.h>
#include <math.h>
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

static lv_obj_t *s_card_cells = NULL;
static lv_obj_t *s_cell_bars[16];
static lv_obj_t *s_cell_labels[16];
static lv_obj_t *s_cell_range_label = NULL;

static lv_obj_t *s_label_voltage = NULL;
static lv_obj_t *s_label_status_wifi = NULL;
static lv_obj_t *s_label_status_storage = NULL;
static lv_obj_t *s_label_status_errors = NULL;

// --- Helpers mises à jour ---
static lv_color_t color_ok(void)      { return lv_palette_main(LV_PALETTE_TEAL); }
static lv_color_t color_min(void)     { return lv_palette_main(LV_PALETTE_BLUE); }
static lv_color_t color_max(void)     { return lv_palette_main(LV_PALETTE_GREEN); }
static lv_color_t color_error(void)   { return lv_palette_main(LV_PALETTE_RED); }
static lv_color_t color_warn(void)    { return lv_palette_main(LV_PALETTE_YELLOW); }
static lv_color_t color_neutral(void) { return lv_palette_main(LV_PALETTE_GREY); }
static lv_color_t color_bal(void)     { return lv_palette_main(LV_PALETTE_ORANGE); }

static const int DEFAULT_UNDERVOLTAGE_MV = 2800;
static const int DEFAULT_OVERVOLTAGE_MV  = 3800;

static void set_status_label(lv_obj_t *label, const char *text, lv_color_t color)
{
    if (!label) return;
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
}

static void compute_cell_axis_limits(const pack_stats_t *stats,
                                     int *out_min_mv,
                                     int *out_max_mv)
{
    int min_mv = (int) (DEFAULT_UNDERVOLTAGE_MV * 0.9f);
    int max_mv = (int) (DEFAULT_OVERVOLTAGE_MV * 1.1f);

    if (stats) {
        if (stats->cell_min > 0.0f) {
            int candidate = (int) floorf(stats->cell_min * 0.95f);
            if (candidate < min_mv) {
                min_mv = candidate;
            }
        }

        if (stats->cell_max > 0.0f) {
            int candidate = (int) ceilf(stats->cell_max * 1.05f);
            if (candidate > max_mv) {
                max_mv = candidate;
            }
        }
    }

    if (max_mv <= min_mv) {
        max_mv = min_mv + 100; // garde-fou pour lv_bar
    }

    if (out_min_mv) *out_min_mv = min_mv;
    if (out_max_mv) *out_max_mv = max_mv;
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

static lv_obj_t *create_legend_item(lv_obj_t *parent, lv_color_t color, const char *text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);

    return row;
}

static void create_cells_chart(lv_obj_t *parent)
{
    s_card_cells = create_card(parent, "Cellules (1-16)");
    lv_obj_set_width(s_card_cells, LV_PCT(100));

    s_cell_range_label = lv_label_create(s_card_cells);
    lv_label_set_text(s_cell_range_label, "Plage: 2.50-4.20 V • UV 2.80 • OV 3.80");

    // Légende des couleurs (proche de l'exemple web)
    lv_obj_t *legend = lv_obj_create(s_card_cells);
    lv_obj_remove_style_all(legend);
    lv_obj_set_width(legend, LV_PCT(100));
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(legend,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(legend, 12, 0);
    lv_obj_set_style_pad_row(legend, 6, 0);

    create_legend_item(legend, color_ok(), "Normal");
    create_legend_item(legend, color_min(), "Min");
    create_legend_item(legend, color_max(), "Max");
    create_legend_item(legend, color_bal(), "Balancing");
    create_legend_item(legend, color_error(), "UV/OV (alerte)");

    lv_obj_t *row = lv_obj_create(s_card_cells);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row, 6, 0);
    lv_obj_set_style_pad_column(row, 8, 0);

    for (uint8_t i = 0; i < 16; ++i) {
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, 20, 120);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *bar = lv_bar_create(col);
        lv_obj_set_size(bar, 16, 90);
        lv_bar_set_range(bar, 2500, 4200);
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_GREY), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);

        lv_obj_t *lbl = lv_label_create(col);
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "C%02u", (unsigned)(i + 1));
        lv_label_set_text(lbl, tmp);

        s_cell_bars[i] = bar;
        s_cell_labels[i] = lbl;
    }
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

    // Carte tensions cellules 1–16
    create_cells_chart(parent);
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

void screen_dashboard_update_cells(const pack_stats_t *stats)
{
    if (!stats || !s_card_cells) {
        return;
    }

    uint8_t count = stats->cell_count;
    if (count > 16) count = 16;

    // Recalcule min / max / moyenne sur le snapshot reçu
    float min_mv = FLT_MAX;
    float max_mv = -FLT_MAX;
    float sum_mv = 0.0f;
    int idx_min = -1, idx_max = -1;
    int valid = 0;

    for (uint8_t i = 0; i < count; ++i) {
        float mv = stats->cells[i];
        if (mv <= 0.0f) continue;

        sum_mv += mv;
        valid++;

        if (mv < min_mv) {
            min_mv = mv;
            idx_min = i;
        }
        if (mv > max_mv) {
            max_mv = mv;
            idx_max = i;
        }
    }

    float avg_mv = (valid > 0) ? (sum_mv / (float) valid) : 0.0f;

    pack_stats_t derived_stats = *stats;
    if (idx_min >= 0) {
        derived_stats.cell_min = min_mv;
    }
    if (idx_max >= 0) {
        derived_stats.cell_max = max_mv;
    }

    int axis_min_mv = 0;
    int axis_max_mv = 0;
    compute_cell_axis_limits(&derived_stats, &axis_min_mv, &axis_max_mv);

    if (s_cell_range_label) {
        char buf[96];
        float delta_mv = (idx_min >= 0 && idx_max >= 0) ? (max_mv - min_mv) : 0.0f;
        snprintf(buf, sizeof(buf),
                 "Plage: %.2f-%.2f V • UV %.2f • OV %.2f • Δ %.0f mV",
                 axis_min_mv / 1000.0f,
                 axis_max_mv / 1000.0f,
                 DEFAULT_UNDERVOLTAGE_MV / 1000.0f,
                 DEFAULT_OVERVOLTAGE_MV / 1000.0f,
                 delta_mv);
        lv_label_set_text(s_cell_range_label, buf);
    }

    for (uint8_t i = 0; i < 16; ++i) {
        lv_obj_t *bar = s_cell_bars[i];
        lv_obj_t *lbl = s_cell_labels[i];
        if (!bar || !lbl) continue;

        lv_bar_set_range(bar, axis_min_mv, axis_max_mv);

        if (i < count && stats->cells[i] > 0.0f) {
            float mv = stats->cells[i];
            lv_bar_set_value(bar, (int) mv, LV_ANIM_OFF);

            bool is_min = (i == idx_min);
            bool is_max = (i == idx_max);
            bool is_bal = stats->balancing[i];
            bool under_uv = (mv < DEFAULT_UNDERVOLTAGE_MV);
            bool over_ov = (mv > DEFAULT_OVERVOLTAGE_MV);

            lv_color_t col = color_ok();
            if (under_uv || over_ov) {
                col = color_error();
            } else if (is_bal) {
                col = color_bal();
            } else if (is_max) {
                col = color_max();
            } else if (is_min) {
                col = color_min();
            }
            lv_obj_set_style_bg_color(bar, col, LV_PART_INDICATOR);

            float diff_mv = mv - avg_mv;
            if (fabsf(diff_mv) < 0.5f) {
                diff_mv = 0.0f;
            }

            char tmp[32];
            const char *flag = "";
            if (under_uv || over_ov) {
                flag = " !";
            } else if (is_bal) {
                flag = " *";
            } else if (is_max) {
                flag = " ↑";
            } else if (is_min) {
                flag = " ↓";
            }
            snprintf(tmp, sizeof(tmp), "C%02u%s\n%.3f V (%+.0f mV)",
                     (unsigned) (i + 1), flag, mv / 1000.0f, diff_mv);
            lv_label_set_text(lbl, tmp);
        } else {
            lv_bar_set_value(bar, axis_min_mv, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar, color_neutral(), LV_PART_INDICATOR);

            char tmp[16];
            snprintf(tmp, sizeof(tmp), "C%02u", (unsigned)(i + 1));
            lv_label_set_text(lbl, tmp);
        }
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
