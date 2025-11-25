#ifndef WIDGET_VALUE_DISPLAY_H
#define WIDGET_VALUE_DISPLAY_H

#include "lvgl.h"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *label_title;
    lv_obj_t *label_value;
    lv_obj_t *label_unit;
} widget_value_display_t;

typedef struct {
    const char *title;        // Ex: "Tension"
    const char *unit;         // Ex: "V"
    const char *format;       // Ex: "%.2f" (pour float)
    lv_coord_t width;         // Largeur du widget (0 = auto)
    lv_color_t title_color;   // Couleur du titre
    lv_color_t value_color;   // Couleur de la valeur
    const lv_font_t *value_font; // Police de la valeur
} widget_value_config_t;

#define WIDGET_VALUE_DEFAULT_CONFIG { \
    .title = "Value", \
    .unit = "", \
    .format = "%.2f", \
    .width = 0, \
    .title_color = lv_color_hex(0xA0AEC0), \
    .value_color = lv_color_hex(0xF7FAFC), \
    .value_font = &lv_font_montserrat_24 \
}

/**
 * @brief Crée un widget Value Display
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_value_display_t* widget_value_create(
    lv_obj_t *parent,
    const widget_value_config_t *config);

/**
 * @brief Met à jour la valeur (float)
 * @param display Pointeur vers le widget
 * @param value Nouvelle valeur
 */
void widget_value_set_float(
    widget_value_display_t *display,
    float value);

/**
 * @brief Met à jour la valeur (int)
 * @param display Pointeur vers le widget
 * @param value Nouvelle valeur
 */
void widget_value_set_int(
    widget_value_display_t *display,
    int32_t value);

/**
 * @brief Met à jour la couleur de la valeur
 * @param display Pointeur vers le widget
 * @param color Nouvelle couleur
 */
void widget_value_set_color(
    widget_value_display_t *display,
    lv_color_t color);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_value_destroy(widget_value_display_t *display);

#endif // WIDGET_VALUE_DISPLAY_H
