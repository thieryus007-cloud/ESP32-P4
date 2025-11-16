// components/gui_lvgl/screen_cells.h
#ifndef SCREEN_CELLS_H
#define SCREEN_CELLS_H

#include "lvgl.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void screen_cells_create(lv_obj_t *parent);

/**
 * @brief Mise à jour des infos pack globales (tension, delta, etc.)
 *        utilisée pour fixer les bornes d'affichage.
 */
void screen_cells_update_pack(const battery_status_t *status);

/**
 * @brief Mise à jour des tensions de cellules + stats
 */
void screen_cells_update_cells(const pack_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_CELLS_H
