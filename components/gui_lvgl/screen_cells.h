#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "event_types.h"

// API C pour l'intégration existante
void screen_cells_create(lv_obj_t *parent);
void screen_cells_update(const pack_stats_t *stats);

#ifdef __cplusplus
}

namespace gui {
    class ScreenCells {
    public:
        explicit ScreenCells(lv_obj_t* parent);
        void update(const pack_stats_t& stats);

    private:
        lv_obj_t* root;
        
        // Indicateurs numériques (Header)
        lv_obj_t* label_delta;
        lv_obj_t* label_min;
        lv_obj_t* label_max;

        // Graphique principal
        lv_obj_t* chart_container;
        lv_obj_t* chart_cells;
        lv_chart_series_t* ser_volts;
        lv_chart_series_t* ser_balance; // Série secondaire pour visualiser l'équilibrage

        // Widget liste détaillée (Optionnel, pour debug précis)
        lv_obj_t* list_container;

        void build_ui();
        void update_header_colors(float delta_mv);
    };
}
#endif
