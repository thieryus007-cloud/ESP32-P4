#ifndef WIDGET_CELL_BAR_H
#define WIDGET_CELL_BAR_H

#include "lvgl.h"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *bar;
    lv_obj_t *label_value;
    lv_obj_t *label_index;
    lv_obj_t *balancing_icon;
    uint8_t cell_index;
    uint16_t voltage_mv;
    bool is_balancing;
} widget_cell_bar_t;

typedef struct {
    uint16_t min_voltage;    // Tension min (ex: 2800 mV)
    uint16_t max_voltage;    // Tension max (ex: 4200 mV)
    uint16_t low_threshold;  // Seuil bas (ex: 3000 mV)
    uint16_t high_threshold; // Seuil haut (ex: 4100 mV)
    lv_coord_t bar_width;    // Largeur de la barre (défaut: 200)
    lv_coord_t bar_height;   // Hauteur de la barre (défaut: 30)
} widget_cell_bar_config_t;

#define WIDGET_CELL_BAR_DEFAULT_CONFIG { \
    .min_voltage = 2800, \
    .max_voltage = 4200, \
    .low_threshold = 3000, \
    .high_threshold = 4100, \
    .bar_width = 200, \
    .bar_height = 30 \
}

/**
 * @brief Crée un widget Cell Bar
 * @param parent Objet parent LVGL
 * @param cell_index Index de la cellule (affiché)
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_cell_bar_t* widget_cell_bar_create(
    lv_obj_t *parent,
    uint8_t cell_index,
    const widget_cell_bar_config_t *config);

/**
 * @brief Met à jour la tension et l'état d'équilibrage
 * @param bar Pointeur vers le widget
 * @param voltage_mv Tension en millivolts
 * @param is_balancing État d'équilibrage
 */
void widget_cell_bar_set_voltage(
    widget_cell_bar_t *bar,
    uint16_t voltage_mv,
    bool is_balancing);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_cell_bar_destroy(widget_cell_bar_t *bar);

#endif // WIDGET_CELL_BAR_H
