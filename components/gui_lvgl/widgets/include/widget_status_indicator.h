#ifndef WIDGET_STATUS_INDICATOR_H
#define WIDGET_STATUS_INDICATOR_H

#include "lvgl.h"

typedef enum {
    STATUS_INACTIVE = 0,   // Gris - Non configuré/inactif
    STATUS_ERROR,          // Rouge - Erreur/Échec
    STATUS_WARNING,        // Orange - Avertissement
    STATUS_OK              // Vert - Fonctionnel
} status_state_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *led;
    lv_obj_t *label;
    status_state_t state;
} widget_status_indicator_t;

typedef struct {
    lv_coord_t led_size;       // Taille LED (défaut: 12)
    const char *label_text;    // Texte du label
    bool horizontal;           // true = horizontal, false = vertical
    lv_coord_t spacing;        // Espacement LED-label (défaut: 8)
} widget_status_config_t;

#define WIDGET_STATUS_DEFAULT_CONFIG { \
    .led_size = 12, \
    .label_text = "Status", \
    .horizontal = true, \
    .spacing = 8 \
}

/**
 * @brief Crée un widget Status Indicator
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_status_indicator_t* widget_status_create(
    lv_obj_t *parent,
    const widget_status_config_t *config);

/**
 * @brief Met à jour l'état du status
 * @param indicator Pointeur vers le widget
 * @param state Nouvel état
 */
void widget_status_set_state(
    widget_status_indicator_t *indicator,
    status_state_t state);

/**
 * @brief Met à jour le texte du label
 * @param indicator Pointeur vers le widget
 * @param text Nouveau texte
 */
void widget_status_set_label(
    widget_status_indicator_t *indicator,
    const char *text);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_status_destroy(widget_status_indicator_t *indicator);

#endif // WIDGET_STATUS_INDICATOR_H
