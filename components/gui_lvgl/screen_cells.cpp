#include "screen_cells.h"
#include "include/ui_theme.hpp"
#include <cstdio>

// Instance globale
static gui::ScreenCells* s_cells = nullptr;

extern "C" void screen_cells_create(lv_obj_t *parent) {
    if (!s_cells) s_cells = new gui::ScreenCells(parent);
}

extern "C" void screen_cells_update(const pack_stats_t *stats) {
    if (s_cells && stats) s_cells->update(*stats);
}

namespace gui {

ScreenCells::ScreenCells(lv_obj_t* parent) {
    root = lv_obj_create(parent);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    theme::apply_screen_style(root);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_pad_all(root, 10);
    
    build_ui();
}

void ScreenCells::build_ui() {
    static lv_style_t style_panel;
    theme::init_card_style(&style_panel);

    // --- 1. Header Stat (Min/Max/Delta) ---
    lv_obj_t* header = lv_obj_create(root);
    lv_obj_add_style(header, &style_panel, 0);
    lv_obj_set_size(header, LV_PCT(100), 80);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Helper pour créer des blocs de stats
    auto create_stat_block = [&](const char* title, lv_obj_t** label_out) {
        lv_obj_t* cont = lv_obj_create(header);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        lv_obj_t* t = lv_label_create(cont);
        lv_label_set_text(t, title);
        theme::apply_title_style(t);
        
        *label_out = lv_label_create(cont);
        lv_label_set_text(*label_out, "-.-- V");
        lv_obj_set_style_text_font(*label_out, &lv_font_montserrat_20, 0);
    };

    create_stat_block("MIN", &label_min);
    create_stat_block("MAX", &label_max);
    create_stat_block("DELTA", &label_delta);

    // --- 2. Graphique à Barres ---
    chart_container = lv_obj_create(root);
    lv_obj_add_style(chart_container, &style_panel, 0);
    lv_obj_set_size(chart_container, LV_PCT(100), LV_PCT(75)); // Reste de la hauteur
    
    chart_cells = lv_chart_create(chart_container);
    lv_obj_set_size(chart_cells, LV_PCT(100), LV_PCT(100));
    lv_chart_set_type(chart_cells, LV_CHART_TYPE_BAR);
    lv_chart_set_range(chart_cells, LV_CHART_AXIS_PRIMARY_Y, 2500, 3650); // Zoom mV défaut
    lv_chart_set_div_line_count(chart_cells, 5, 0);
    lv_chart_set_point_count(chart_cells, PACK_MAX_CELLS); // 32 points max

    // Style du chart
    lv_obj_set_style_bg_color(chart_cells, theme::color_panel(), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart_cells, 0, LV_PART_MAIN);
    
    // Séries
    ser_volts = lv_chart_add_series(chart_cells, theme::color_good(), LV_CHART_AXIS_PRIMARY_Y);
    ser_balance = lv_chart_add_series(chart_cells, theme::color_primary(), LV_CHART_AXIS_PRIMARY_Y);
}

void ScreenCells::update(const pack_stats_t& stats) {
    // Mise à jour Textes
    lv_label_set_text_fmt(label_min, "%.3f V", stats.cell_min / 1000.0f);
    lv_label_set_text_fmt(label_max, "%.3f V", stats.cell_max / 1000.0f);
    lv_label_set_text_fmt(label_delta, "%.0f mV", stats.cell_delta);
    
    update_header_colors(stats.cell_delta);

    // Mise à jour Chart
    // Ajustement dynamique de l'échelle Y pour zoomer sur les différences
    // On ajoute une marge de +/- 50mV autour du min/max réel
    lv_coord_t y_min = (lv_coord_t)(stats.cell_min - 50);
    lv_coord_t y_max = (lv_coord_t)(stats.cell_max + 50);
    // Garde-fou pour rester lisible
    if(y_min < 2000) y_min = 2000; 
    if(y_max > 4500) y_max = 4500;
    
    lv_chart_set_range(chart_cells, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
    
    // Nombre de points effectifs (ex: si seulement 16 cellules détectées)
    uint16_t count = (stats.cell_count > 0 && stats.cell_count <= PACK_MAX_CELLS) ? stats.cell_count : PACK_MAX_CELLS;
    lv_chart_set_point_count(chart_cells, count);

    for(uint16_t i=0; i < count; i++) {
        // Valeur principale (Tension)
        lv_chart_set_value_by_id(chart_cells, ser_volts, i, (lv_coord_t)stats.cells[i]);
        
        // Indication visuelle du Balancing : 
        // Si balancing actif, on met une petite barre témoin ou on change la couleur (compliqué avec LVGL standard).
        // Ici, astuce : on utilise la série 2 pour marquer le "top" si balancing
        if(stats.balancing[i]) {
            lv_chart_set_value_by_id(chart_cells, ser_balance, i, (lv_coord_t)stats.cells[i]);
        } else {
            lv_chart_set_value_by_id(chart_cells, ser_balance, i, 0); // Cache la barre
        }
    }
    lv_chart_refresh(chart_cells);
}

void ScreenCells::update_header_colors(float delta_mv) {
    if(delta_mv > 50) {
        lv_obj_set_style_text_color(label_delta, theme::color_crit(), 0);
    } else if (delta_mv > 20) {
        lv_obj_set_style_text_color(label_delta, theme::color_warn(), 0);
    } else {
        lv_obj_set_style_text_color(label_delta, theme::color_good(), 0);
    }
}

} // namespace gui
