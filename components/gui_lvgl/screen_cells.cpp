#include "screen_cells.h"

#include <algorithm>
#include <cstdio>

#include "gui_format.hpp"
#include "pack_stats_utils.hpp"

static lv_obj_t *s_label_min       = NULL;
static lv_obj_t *s_label_max       = NULL;
static lv_obj_t *s_label_delta     = NULL;
static lv_obj_t *s_label_avg       = NULL;
static lv_obj_t *s_label_bal_start = NULL;
static lv_obj_t *s_label_bal_stop  = NULL;

// On s'aligne sur PACK_MAX_CELLS de event_types.h
#define MAX_CELLS PACK_MAX_CELLS

static lv_obj_t *s_cell_bars[MAX_CELLS];
static lv_obj_t *s_cell_labels[MAX_CELLS];

static float s_last_min_mv = 0.0f;
static float s_last_max_mv = 0.0f;

using gui::set_label_textf;

extern "C" {

void screen_cells_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // --- Ligne stats principales ---
    lv_obj_t *cont_stats = lv_obj_create(parent);
    lv_obj_set_width(cont_stats, LV_PCT(100));
    lv_obj_set_flex_flow(cont_stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_stats,
                          LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_label_min   = lv_label_create(cont_stats);
    s_label_max   = lv_label_create(cont_stats);
    s_label_delta = lv_label_create(cont_stats);
    s_label_avg   = lv_label_create(cont_stats);

    lv_label_set_text(s_label_min,   "Min: -- mV");
    lv_label_set_text(s_label_max,   "Max: -- mV");
    lv_label_set_text(s_label_delta, "Δ: -- mV");
    lv_label_set_text(s_label_avg,   "Avg: -- mV");

    // --- Ligne seuils balancing ---
    lv_obj_t *cont_bal = lv_obj_create(parent);
    lv_obj_set_width(cont_bal, LV_PCT(100));
    lv_obj_set_flex_flow(cont_bal, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_bal,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_label_bal_start = lv_label_create(cont_bal);
    s_label_bal_stop  = lv_label_create(cont_bal);

    lv_label_set_text(s_label_bal_start, "Bal start: -- mV");
    lv_label_set_text(s_label_bal_stop,  "Bal stop: -- mV");

    // --- Conteneur scrollable pour les barres de cellules ---
    lv_obj_t *cont_cells = lv_obj_create(parent);
    lv_obj_set_size(cont_cells, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont_cells, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_cells,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(cont_cells, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(cont_cells, LV_SCROLLBAR_MODE_AUTO);

    // On pré-crée les 32 barres avec labels en dessous
    for (int i = 0; i < MAX_CELLS; ++i) {
        lv_obj_t *col = lv_obj_create(cont_cells);
        lv_obj_remove_style_all(col);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        // Barre
        lv_obj_t *bar = lv_bar_create(col);
        lv_obj_set_size(bar, 20, 120);  // largeur x hauteur
        lv_bar_set_range(bar, 0, 1000); // 0–1000 = 0–100% (échelle normalisée)
        lv_bar_set_value(bar, 0, LV_ANIM_OFF);

        // Label "Cxx"
        lv_obj_t *lbl = lv_label_create(col);
        char tmp[16];
        std::snprintf(tmp, sizeof(tmp), "C%02d", i + 1);
        lv_label_set_text(lbl, tmp);

        s_cell_bars[i]   = bar;
        s_cell_labels[i] = lbl;
    }
}

void screen_cells_update_pack(const battery_status_t *status)
{
    (void) status;
    // Pour l'instant, on ne se sert pas de battery_status_t ici.
    // Tu pourras l'utiliser plus tard pour définir une plage dynamique.
}

void screen_cells_update_cells(const pack_stats_t *stats)
{
    if (!stats) return;

    const auto cell_values   = gui::cell_values(*stats);
    const auto balancing     = gui::balancing_states(*stats);
    const auto extrema       = gui::compute_extrema(cell_values);

    // Stats globales
    if (extrema.has_cells) {
        set_label_textf(s_label_min,   "Min: {:.1f} mV", extrema.min);
        set_label_textf(s_label_max,   "Max: {:.1f} mV", extrema.max);
        set_label_textf(s_label_delta, "Δ: {:.1f} mV",   extrema.delta);
        set_label_textf(s_label_avg,   "Avg: {:.1f} mV", extrema.avg);
    } else {
        set_label_textf(s_label_min,   "Min: -- mV");
        set_label_textf(s_label_max,   "Max: -- mV");
        set_label_textf(s_label_delta, "Δ: -- mV");
        set_label_textf(s_label_avg,   "Avg: -- mV");
    }

    // Seuils balancing (si fournis)
    if (stats->bal_start_mv > 0.0f) {
        set_label_textf(s_label_bal_start, "Bal start: {:.1f} mV", stats->bal_start_mv);
    } else {
        set_label_textf(s_label_bal_start, "Bal start: -- mV");
    }

    if (stats->bal_stop_mv > 0.0f) {
        set_label_textf(s_label_bal_stop, "Bal stop: {:.1f} mV", stats->bal_stop_mv);
    } else {
        set_label_textf(s_label_bal_stop, "Bal stop: -- mV");
    }

    s_last_min_mv = extrema.has_cells ? extrema.min : 0.0f;
    s_last_max_mv = extrema.has_cells ? extrema.max : 0.0f;

    // Plage pour normaliser les barres
    float min_mv = extrema.has_cells ? extrema.min : 2800.0f;
    float max_mv = extrema.has_cells ? extrema.max : 3600.0f;
    if (max_mv <= min_mv || max_mv <= 0.0f) {
        // Plage par défaut 2800–3600 mV
        min_mv = 2800.0f;
        max_mv = 3600.0f;
    }
    float range = max_mv - min_mv;
    if (range <= 0.0f) {
        range = 1.0f;
    }

    // Mettre à jour chaque barre
    const uint8_t count = static_cast<uint8_t>(cell_values.size());

    for (uint8_t i = 0; i < MAX_CELLS; ++i) {
        lv_obj_t *bar = s_cell_bars[i];
        lv_obj_t *lbl = s_cell_labels[i];
        if (!bar || !lbl) continue;

        if (i < count) {
            float mv = cell_values[i];
            bool  balancing_active = (i < balancing.size()) ? balancing[i] : false;

            // Normalise entre 0–1000
            float norm = (mv - min_mv) / range;
            norm       = std::clamp(norm, 0.0f, 1.0f);
            int val = (int)(norm * 1000.0f);
            lv_bar_set_value(bar, val, LV_ANIM_OFF);

            // Couleur de la barre :
            // - rouge : cell_min
            // - vert : cell_max
            // - orange : balancing actif
            // - bleu : normal
            lv_color_t col = lv_palette_main(LV_PALETTE_BLUE);

            if (extrema.has_cells && mv == extrema.min) {
                col = lv_palette_main(LV_PALETTE_RED);
            }
            if (extrema.has_cells && mv == extrema.max) {
                col = lv_palette_main(LV_PALETTE_GREEN);
            }
            if (balancing_active) {
                col = lv_palette_main(LV_PALETTE_ORANGE);
            }

            lv_obj_set_style_bg_color(bar, col, LV_PART_INDICATOR);

            // Label : on ajoute une étoile si balancing actif
            char tmp[16];
            if (balancing_active) {
                std::snprintf(tmp, sizeof(tmp), "C%02u*", static_cast<unsigned>(i + 1));
            } else {
                std::snprintf(tmp, sizeof(tmp), "C%02u", static_cast<unsigned>(i + 1));
            }
            lv_label_set_text(lbl, tmp);

        } else {
            // Cellule non utilisée : barre vide + label Cxx
            lv_bar_set_value(bar, 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(bar,
                                      lv_palette_main(LV_PALETTE_GREY),
                                      LV_PART_INDICATOR);

            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "C%02u", static_cast<unsigned>(i + 1));
            lv_label_set_text(lbl, tmp);
        }
    }
}

}  // extern "C"
