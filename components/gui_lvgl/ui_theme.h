// components/gui_lvgl/ui_theme.h
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"
#include "event_bus.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_THEME_MODE_AUTO = 0,
    UI_THEME_MODE_LIGHT,
    UI_THEME_MODE_DARK,
} ui_theme_mode_t;

// Palette de couleurs complète
typedef struct {
    // Backgrounds
    lv_color_t bg_primary;      // Fond principal
    lv_color_t bg_secondary;    // Fond secondaire (cartes)
    lv_color_t bg_tertiary;     // Fond tertiaire (inputs)

    // Texte
    lv_color_t text_primary;    // Texte principal
    lv_color_t text_secondary;  // Texte secondaire
    lv_color_t text_disabled;   // Texte désactivé

    // Bordures
    lv_color_t border_default;
    lv_color_t border_focus;

    // Accents
    lv_color_t accent_primary;  // Bleu
    lv_color_t accent_success;  // Vert
    lv_color_t accent_warning;  // Orange
    lv_color_t accent_error;    // Rouge

    // États spécifiques BMS
    lv_color_t charging;        // En charge
    lv_color_t discharging;     // En décharge
    lv_color_t balancing;       // Équilibrage
    lv_color_t idle;            // Repos
} theme_palette_t;

/**
 * @brief Initialise la gestion des thèmes (clair / sombre / auto).
 * Charge le thème sauvegardé depuis NVS.
 */
void ui_theme_init(lv_display_t *disp);

/**
 * @brief Crée le menu rapide pour changer le thème.
 */
lv_obj_t *ui_theme_create_quick_menu(lv_obj_t *parent);

/**
 * @brief Applique un mode de thème et met à jour les widgets associés.
 * Sauvegarde le choix en NVS.
 */
void ui_theme_set_mode(ui_theme_mode_t mode);

/**
 * @brief Retourne le mode de thème actuel.
 */
ui_theme_mode_t ui_theme_get_mode(void);

/**
 * @brief Active/désactive le basculement automatique selon l'heure
 * @param enable true pour activer
 * @param dark_hour Heure de passage au mode sombre (0-23)
 * @param light_hour Heure de passage au mode clair (0-23)
 */
void ui_theme_set_auto(bool enable, uint8_t dark_hour, uint8_t light_hour);

/**
 * @brief Récupère la palette de couleurs actuelle
 * @return Pointeur vers la palette (ne pas libérer)
 */
const theme_palette_t* ui_theme_get_palette(void);

/**
 * @brief Bascule entre les modes (pour bouton toggle)
 */
void ui_theme_toggle(void);

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H
