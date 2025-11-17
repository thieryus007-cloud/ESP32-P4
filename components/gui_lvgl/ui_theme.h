// components/gui_lvgl/ui_theme.h
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_THEME_MODE_AUTO = 0,
    UI_THEME_MODE_LIGHT,
    UI_THEME_MODE_DARK,
} ui_theme_mode_t;

/**
 * @brief Initialise la gestion des thèmes (clair / sombre / auto).
 */
void ui_theme_init(lv_display_t *disp);

/**
 * @brief Crée le menu rapide pour changer le thème.
 */
lv_obj_t *ui_theme_create_quick_menu(lv_obj_t *parent);

/**
 * @brief Applique un mode de thème et met à jour les widgets associés.
 */
void ui_theme_set_mode(ui_theme_mode_t mode);

/**
 * @brief Retourne le mode de thème actuel.
 */
ui_theme_mode_t ui_theme_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif // UI_THEME_H
