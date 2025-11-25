#ifndef WIDGET_GAUGE_CIRCULAR_H
#define WIDGET_GAUGE_CIRCULAR_H

#include "lvgl.h"

/**
 * @brief Widget de jauge circulaire avec aiguille
 *
 * Ce widget crée un cadran circulaire complet (360°) avec:
 * - Échelle graduée personnalisable
 * - Aiguille animée
 * - Label central pour la valeur
 * - Support de valeurs négatives
 */

typedef struct {
    lv_obj_t *container;        // Conteneur principal
    lv_obj_t *arc_bg;          // Arc de fond (échelle)
    lv_obj_t *needle;          // Aiguille
    lv_obj_t *center_dot;      // Point central
    lv_obj_t *label_value;     // Label valeur centrale
    lv_obj_t *label_unit;      // Label unité
    lv_obj_t *label_title;     // Label titre (en haut)
    float current_value;       // Valeur actuelle
    float min_value;           // Valeur minimale de l'échelle
    float max_value;           // Valeur maximale de l'échelle
    lv_anim_t anim;            // Animation de l'aiguille
} widget_gauge_circular_t;

typedef struct {
    lv_coord_t size;           // Diamètre du cadran (défaut: 200)
    float min_value;           // Valeur min (ex: -5000)
    float max_value;           // Valeur max (ex: 5000)
    const char *title;         // Titre (NULL = pas de titre)
    const char *unit;          // Unité (ex: "V", "A")
    const char *format;        // Format d'affichage (ex: "%.1f")
    lv_color_t needle_color;   // Couleur de l'aiguille
    lv_color_t scale_color;    // Couleur de l'échelle
    uint16_t needle_width;     // Largeur de l'aiguille (défaut: 3)
    uint16_t needle_length;    // Longueur de l'aiguille en % (défaut: 70)
    bool animate;              // Activer les animations
    uint16_t anim_duration;    // Durée animation ms (défaut: 500)
} widget_gauge_circular_config_t;

#define WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG { \
    .size = 200, \
    .min_value = 0, \
    .max_value = 100, \
    .title = NULL, \
    .unit = "", \
    .format = "%.0f", \
    .needle_color = lv_color_hex(0x4299E1), \
    .scale_color = lv_color_hex(0x4A5568), \
    .needle_width = 3, \
    .needle_length = 70, \
    .animate = true, \
    .anim_duration = 500 \
}

/**
 * @brief Crée un widget de jauge circulaire
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_gauge_circular_t* widget_gauge_circular_create(
    lv_obj_t *parent,
    const widget_gauge_circular_config_t *config);

/**
 * @brief Met à jour la valeur de la jauge
 * @param gauge Pointeur vers le widget
 * @param value Nouvelle valeur
 */
void widget_gauge_circular_set_value(
    widget_gauge_circular_t *gauge,
    float value);

/**
 * @brief Met à jour le titre de la jauge
 * @param gauge Pointeur vers le widget
 * @param title Nouveau titre
 */
void widget_gauge_circular_set_title(
    widget_gauge_circular_t *gauge,
    const char *title);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_gauge_circular_destroy(widget_gauge_circular_t *gauge);

#endif // WIDGET_GAUGE_CIRCULAR_H
