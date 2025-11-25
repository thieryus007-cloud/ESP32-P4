#ifndef WIDGET_KEYBOARD_H
#define WIDGET_KEYBOARD_H

#include "lvgl.h"

typedef enum {
    KEYBOARD_MODE_TEXT,      // Clavier texte complet (AZERTY/QWERTY)
    KEYBOARD_MODE_NUMBER,    // Clavier numérique avec décimales
    KEYBOARD_MODE_SPECIAL,   // Clavier avec caractères spéciaux
    KEYBOARD_MODE_HEX        // Clavier hexadécimal (0-9, A-F)
} keyboard_mode_t;

typedef struct {
    lv_obj_t *keyboard;      // Objet clavier LVGL
    lv_obj_t *textarea;      // Textarea associé (optionnel)
    lv_obj_t *container;     // Container pour gestion du positionnement
    keyboard_mode_t mode;    // Mode actuel du clavier
    bool is_visible;         // État de visibilité
} widget_keyboard_t;

typedef struct {
    keyboard_mode_t mode;    // Mode initial (défaut: TEXT)
    bool auto_hide;          // Masquer automatiquement après validation
    lv_coord_t height;       // Hauteur du clavier (0 = auto)
    const char *ok_text;     // Texte du bouton OK (NULL = "OK")
    const char *close_text;  // Texte du bouton fermer (NULL = "Fermer")
} widget_keyboard_config_t;

#define WIDGET_KEYBOARD_DEFAULT_CONFIG { \
    .mode = KEYBOARD_MODE_TEXT, \
    .auto_hide = true, \
    .height = 0, \
    .ok_text = NULL, \
    .close_text = NULL \
}

/**
 * @brief Crée un widget clavier virtuel
 * @param parent Objet parent LVGL
 * @param config Configuration (NULL pour défaut)
 * @return Pointeur vers le widget créé
 */
widget_keyboard_t* widget_keyboard_create(
    lv_obj_t *parent,
    const widget_keyboard_config_t *config);

/**
 * @brief Attache le clavier à un textarea
 * @param keyboard Pointeur vers le widget
 * @param textarea Textarea à associer
 */
void widget_keyboard_set_textarea(
    widget_keyboard_t *keyboard,
    lv_obj_t *textarea);

/**
 * @brief Change le mode du clavier
 * @param keyboard Pointeur vers le widget
 * @param mode Nouveau mode
 */
void widget_keyboard_set_mode(
    widget_keyboard_t *keyboard,
    keyboard_mode_t mode);

/**
 * @brief Affiche le clavier
 * @param keyboard Pointeur vers le widget
 */
void widget_keyboard_show(widget_keyboard_t *keyboard);

/**
 * @brief Masque le clavier
 * @param keyboard Pointeur vers le widget
 */
void widget_keyboard_hide(widget_keyboard_t *keyboard);

/**
 * @brief Bascule la visibilité du clavier
 * @param keyboard Pointeur vers le widget
 */
void widget_keyboard_toggle(widget_keyboard_t *keyboard);

/**
 * @brief Définit un callback appelé lorsque l'utilisateur appuie sur OK
 * @param keyboard Pointeur vers le widget
 * @param callback Fonction de callback
 * @param user_data Données utilisateur passées au callback
 */
void widget_keyboard_set_ok_callback(
    widget_keyboard_t *keyboard,
    lv_event_cb_t callback,
    void *user_data);

/**
 * @brief Détruit le widget et libère la mémoire
 */
void widget_keyboard_destroy(widget_keyboard_t *keyboard);

#endif // WIDGET_KEYBOARD_H
