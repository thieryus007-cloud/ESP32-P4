// components/gui_lvgl/screen_battery.c

#include "screen_battery.h"

#include <stdio.h>
#include <stdarg.h>

#include "event_types.h"   // pour PACK_MAX_CELLS

// Widgets globaux de l'écran Pack
static lv_obj_t *s_label_pack_soc      = NULL;
static lv_obj_t *s_label_pack_voltage  = NULL;
static lv_obj_t *s_label_pack_current  = NULL;
static lv_obj_t *s_label_pack_power    = NULL;

static lv_obj_t *s_label_cell_min      = NULL;
static lv_obj_t *s_label_cell_max      = NULL;
static lv_obj_t *s_label_cell_delta    = NULL;
static lv_obj_t *s_label_cell_avg      = NULL;

static lv_obj_t *s_label_balancing     = NULL;   // 🔹 nouveau badge Balancing

static lv_obj_t *s_table_cells         = NULL;

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

void screen_battery_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // --- Section Pack résumé ---
    lv_obj_t *cont_summary = lv_obj_create(parent);
    lv_obj_set_width(cont_summary, LV_PCT(100));
    lv_obj_set_flex_flow(cont_summary, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_summary,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Colonne labels
    lv_obj_t *col_labels = lv_obj_create(cont_summary);
    lv_obj_remove_style_all(col_labels);
    lv_obj_set_flex_flow(col_labels, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_labels,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // Colonne valeurs
    lv_obj_t *col_values = lv_obj_create(cont_summary);
    lv_obj_remove_style_all(col_values);
    lv_obj_set_flex_flow(col_values, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_values,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl_soc_t = lv_label_create(col_labels);
    lv_label_set_text(lbl_soc_t, "SOC");

    s_label_pack_soc = lv_label_create(col_values);
    lv_label_set_text(s_label_pack_soc, "-- %");

    lv_obj_t *lbl_v_t = lv_label_create(col_labels);
    lv_label_set_text(lbl_v_t, "Pack V");

    s_label_pack_voltage = lv_label_create(col_values);
    lv_label_set_text(s_label_pack_voltage, "--.- V");

    lv_obj_t *lbl_i_t = lv_label_create(col_labels);
    lv_label_set_text(lbl_i_t, "Pack I");

    s_label_pack_current = lv_label_create(col_values);
    lv_label_set_text(s_label_pack_current, "--.- A");

    lv_obj_t *lbl_p_t = lv_label_create(col_labels);
    lv_label_set_text(lbl_p_t, "Pack P");

    s_label_pack_power = lv_label_create(col_values);
    lv_label_set_text(s_label_pack_power, "---- W");

    // --- Section stats cellules + badge balancing ---
    lv_obj_t *cont_stats = lv_obj_create(parent);
    lv_obj_set_width(cont_stats, LV_PCT(100));
    lv_obj_set_flex_flow(cont_stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_stats,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *col_stats_labels = lv_obj_create(cont_stats);
    lv_obj_remove_style_all(col_stats_labels);
    lv_obj_set_flex_flow(col_stats_labels, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_stats_labels,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *col_stats_values = lv_obj_create(cont_stats);
    lv_obj_remove_style_all(col_stats_values);
    lv_obj_set_flex_flow(col_stats_values, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_stats_values,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl_min_t = lv_label_create(col_stats_labels);
    lv_label_set_text(lbl_min_t, "Cell min");

    s_label_cell_min = lv_label_create(col_stats_values);
    lv_label_set_text(s_label_cell_min, "-- mV");

    lv_obj_t *lbl_max_t = lv_label_create(col_stats_labels);
    lv_label_set_text(lbl_max_t, "Cell max");

    s_label_cell_max = lv_label_create(col_stats_values);
    lv_label_set_text(s_label_cell_max, "-- mV");

    lv_obj_t *lbl_delta_t = lv_label_create(col_stats_labels);
    lv_label_set_text(lbl_delta_t, "Delta");

    s_label_cell_delta = lv_label_create(col_stats_values);
    lv_label_set_text(s_label_cell_delta, "-- mV");

    lv_obj_t *lbl_avg_t = lv_label_create(col_stats_labels);
    lv_label_set_text(lbl_avg_t, "Avg");

    s_label_cell_avg = lv_label_create(col_stats_values);
    lv_label_set_text(s_label_cell_avg, "-- mV");

    // 🔹 Badge Balancing
    s_label_balancing = lv_label_create(cont_stats);
    lv_label_set_text(s_label_balancing, "Balancing: OFF");
    lv_obj_set_style_text_color(s_label_balancing,
                                lv_palette_main(LV_PALETTE_GREY), 0);

    // --- Table des cellules ---
    s_table_cells = lv_table_create(parent);
    lv_obj_set_width(s_table_cells, LV_PCT(100));

    // Deux colonnes : "Cell #" et "Voltage"
    lv_table_set_col_cnt(s_table_cells, 2);
    lv_table_set_col_width(s_table_cells, 0, 80);
    lv_table_set_col_width(s_table_cells, 1, 100);

    lv_table_set_cell_value(s_table_cells, 0, 0, "Cell");
    lv_table_set_cell_value(s_table_cells, 0, 1, "Voltage");
}

void screen_battery_update_pack_basic(const battery_status_t *status)
{
    if (!status) return;

    if (s_label_pack_soc) {
        set_label_fmt(s_label_pack_soc, "%.1f %%", status->soc);
    }
    if (s_label_pack_voltage) {
        set_label_fmt(s_label_pack_voltage, "%.2f V", status->voltage);
    }
    if (s_label_pack_current) {
        set_label_fmt(s_label_pack_current, "%.2f A", status->current);
    }
    if (s_label_pack_power) {
        set_label_fmt(s_label_pack_power, "%.0f W", status->power);
    }
}

void screen_battery_update_pack_stats(const pack_stats_t *stats)
{
    if (!stats) return;

    // Stats min/max/delta/avg
    if (s_label_cell_min) {
        if (stats->cell_count > 0) {
            set_label_fmt(s_label_cell_min, "%.1f mV", stats->cell_min);
        } else {
            lv_label_set_text(s_label_cell_min, "-- mV");
        }
    }
    if (s_label_cell_max) {
        if (stats->cell_count > 0) {
            set_label_fmt(s_label_cell_max, "%.1f mV", stats->cell_max);
        } else {
            lv_label_set_text(s_label_cell_max, "-- mV");
        }
    }
    if (s_label_cell_delta) {
        if (stats->cell_count > 0) {
            set_label_fmt(s_label_cell_delta, "%.1f mV", stats->cell_delta);
        } else {
            lv_label_set_text(s_label_cell_delta, "-- mV");
        }
    }
    if (s_label_cell_avg) {
        if (stats->cell_count > 0) {
            set_label_fmt(s_label_cell_avg, "%.1f mV", stats->cell_avg);
        } else {
            lv_label_set_text(s_label_cell_avg, "-- mV");
        }
    }

    // 🔹 Badge Balancing : ON si au moins une cellule est en balancing
    if (s_label_balancing) {
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
            lv_label_set_text(s_label_balancing, "Balancing: ON");
            lv_obj_set_style_text_color(s_label_balancing,
                                        lv_palette_main(LV_PALETTE_ORANGE), 0);
        } else {
            lv_label_set_text(s_label_balancing, "Balancing: OFF");
            lv_obj_set_style_text_color(s_label_balancing,
                                        lv_palette_main(LV_PALETTE_GREY), 0);
        }
    }

    // Table cellules
    if (!s_table_cells) {
        return;
    }

    uint8_t rows = stats->cell_count;
    if (rows > PACK_MAX_CELLS) rows = PACK_MAX_CELLS;

    lv_table_set_row_cnt(s_table_cells, rows + 1); // +1 pour l'entête

    for (uint8_t i = 0; i < rows; ++i) {
        char buf_cell[16];
        char buf_volt[32];

        snprintf(buf_cell, sizeof(buf_cell), "%u", (unsigned)(i + 1));
        snprintf(buf_volt, sizeof(buf_volt), "%.1f mV", stats->cells[i]);

        lv_table_set_cell_value(s_table_cells, i + 1, 0, buf_cell);
        lv_table_set_cell_value(s_table_cells, i + 1, 1, buf_volt);
    }
}
