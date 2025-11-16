// components/gui_lvgl/gui_init.h
#ifndef GUI_INIT_H
#define GUI_INIT_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialiser la GUI LVGL
 *
 * - Initialise LVGL (via esp_lvgl_port + BSP Waveshare)
 * - Crée les écrans de base (home, battery, events...)
 * - S'abonne aux events nécessaires :
 *   - EVENT_BATTERY_STATUS_UPDATED
 *   - EVENT_SYSTEM_STATUS_UPDATED
 *   - EVENT_CONFIG_UPDATED
 */
void gui_init(event_bus_t *bus);

/**
 * @brief Démarrer la GUI
 *
 * Selon l'implémentation, ceci peut :
 * - Créer une task GUI si nécessaire
 * - Ou simplement activer certains timers LVGL
 *
 * Dans beaucoup de patterns esp_lvgl_port, la task LVGL est créée
 * dans la phase d'init, donc cette fonction peut être vide au début.
 */
void gui_start(void);

#ifdef __cplusplus
}
#endif

#endif // GUI_INIT_H
