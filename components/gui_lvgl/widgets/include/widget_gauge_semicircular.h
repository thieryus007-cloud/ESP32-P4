#ifndef WIDGET_GAUGE_SEMICIRCULAR_H
#define WIDGET_GAUGE_SEMICIRCULAR_H

#include "lvgl.h"

/**
 * @brief Widget de jauge semi-circulaire avec multi-aiguilles
 *
 * Ce widget crée un cadran semi-circulaire (180°) avec:
 * - Échelle graduée personnalisable
 * - Support de plusieurs aiguilles (jusqu'à 4)
 * - Labels pour chaque aiguille avec nom et valeur
 * - Dégradé de couleur sur l'arc
 * - Animations fluides
 */

#define MAX_NEEDLES 4

typedef struct {
    lv_color_t color;         // Couleur de l'aiguille
    float value;              // Valeur actuelle
    const char *name;         // Nom (ex: "S1", "S2", "Int")
    bool visible;             // Visibilité
} gauge_needle_t;

typedef struct {
    lv_obj_t *container;      // Conteneur principal
    lv_obj_t *arc;            // Arc de fond (échelle)
    lv_obj_t *needles[MAX_NEEDLES];  // Aiguilles (lines)
    lv_obj_t *needle_labels[MAX_NEEDLES]; // Labels des aiguilles
    lv_obj_t *label_title;    // Titre
    gauge_needle_t needle_data[MAX_NEEDLES]; // Données des aiguilles
    uint8_t needle_count;     // Nombre d'aiguilles actives
    float min_value;          // Valeur minimale
    float max_value;          // Valeur maximale
    lv_anim_t anims[MAX_NEEDLES]; // Animations
} widget_gauge_semicircular_t;

typedef struct {
    lv_coord_t width;         // Largeur (défaut: 280)
    lv_coord_t height;        // Hauteur (défaut: 180)
    float min_value;          // Valeur min (ex: 0)
    float max_value;          // Valeur max (ex: 100)
    const char *title;        // Titre (NULL = pas de titre)
    const char *unit;         // Unité (ex: "°C", "%")
    uint16_t arc_width;       // Largeur de l'arc (défaut: 12)
    lv_color_t arc_color_start; // Couleur début du dégradé
    lv_color_t arc_color_end;   // Couleur fin du dégradé
    bool show_gradient;       // Afficher le dégradé
    bool animate;             // Activer les animations
    uint16_t anim_duration;   // Durée animation ms (défaut: 500)
} widget_gauge_semicircular_config_t;

#define WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG { \
    .width = 280, \
    .height = 180, \
    .min_value = 0, \
    .max_value = 100, \
    .title = NULL, \
    .unit = "", \
    .arc_width = 12, \
    .arc_color_start = lv_color_hex(0x4299E1), \
    .arc_color_end = lv_color_hex(0x38A169), \
    .show_gradient = true, \
    .animate = true, \
    .anim_duration = 500 \
}

/**
 * @brief Crée un widget de jauge semi-circulaire
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_gauge_semicircular_t* widget_gauge_semicircular_create(
    lv_obj_t *parent,
    const widget_gauge_semicircular_config_t *config);

/**
 * @brief Ajoute une aiguille à la jauge
 * @param gauge Pointeur vers le widget
 * @param name Nom de l'aiguille (ex: "S1", "S2")
 * @param color Couleur de l'aiguille
 * @param initial_value Valeur initiale
 * @return Index de l'aiguille (0-3) ou -1 si erreur
 */
int widget_gauge_semicircular_add_needle(
    widget_gauge_semicircular_t *gauge,
    const char *name,
    lv_color_t color,
    float initial_value);

/**
 * @brief Met à jour la valeur d'une aiguille
 * @param gauge Pointeur vers le widget
 * @param needle_index Index de l'aiguille (0-3)
 * @param value Nouvelle valeur
 */
void widget_gauge_semicircular_set_needle_value(
    widget_gauge_semicircular_t *gauge,
    uint8_t needle_index,
    float value);

/**
 * @brief Met à jour le titre
 * @param gauge Pointeur vers le widget
 * @param title Nouveau titre
 */
void widget_gauge_semicircular_set_title(
    widget_gauge_semicircular_t *gauge,
    const char *title);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_gauge_semicircular_destroy(widget_gauge_semicircular_t *gauge);

#endif // WIDGET_GAUGE_SEMICIRCULAR_H
