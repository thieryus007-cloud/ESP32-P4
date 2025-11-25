#ifndef WIDGET_SOC_GAUGE_H
#define WIDGET_SOC_GAUGE_H

#include "lvgl.h"

typedef struct {
    lv_obj_t *container;      // Conteneur principal
    lv_obj_t *arc;            // Arc de progression
    lv_obj_t *label_value;    // Label valeur centrale (ex: "85%")
    lv_obj_t *label_unit;     // Label unité ("SOC")
    lv_obj_t *label_trend;    // Indicateur tendance (flèche)
    int16_t current_value;    // Valeur actuelle 0-100
    int16_t target_value;     // Valeur cible pour animation
    lv_anim_t anim;           // Animation en cours
} widget_soc_gauge_t;

typedef struct {
    lv_coord_t width;         // Largeur du widget (défaut: 200)
    lv_coord_t height;        // Hauteur du widget (défaut: 200)
    uint16_t arc_width;       // Épaisseur de l'arc (défaut: 15)
    lv_color_t color_low;     // Couleur 0-20% (rouge)
    lv_color_t color_medium;  // Couleur 20-80% (orange)
    lv_color_t color_high;    // Couleur 80-100% (vert)
    lv_color_t color_bg;      // Couleur fond arc
    bool show_trend;          // Afficher indicateur tendance
    bool animate;             // Activer animations
    uint16_t anim_duration;   // Durée animation ms (défaut: 500)
} widget_soc_gauge_config_t;

// Valeurs par défaut
#define WIDGET_SOC_GAUGE_DEFAULT_CONFIG { \
    .width = 200, \
    .height = 200, \
    .arc_width = 15, \
    .color_low = lv_color_hex(0xE53E3E), \
    .color_medium = lv_color_hex(0xED8936), \
    .color_high = lv_color_hex(0x38A169), \
    .color_bg = lv_color_hex(0x2D3748), \
    .show_trend = true, \
    .animate = true, \
    .anim_duration = 500 \
}

/**
 * @brief Crée un widget SOC Gauge
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_soc_gauge_t* widget_soc_gauge_create(
    lv_obj_t *parent,
    const widget_soc_gauge_config_t *config);

/**
 * @brief Met à jour la valeur SOC
 * @param gauge Pointeur vers le widget
 * @param value Nouvelle valeur 0-100
 * @param trend Direction: -1 (décharge), 0 (stable), 1 (charge)
 */
void widget_soc_gauge_set_value(
    widget_soc_gauge_t *gauge,
    int16_t value,
    int8_t trend);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_soc_gauge_destroy(widget_soc_gauge_t *gauge);

#endif // WIDGET_SOC_GAUGE_H
