// components/gui_lvgl/ui_notifications.h
#ifndef UI_NOTIFICATIONS_H
#define UI_NOTIFICATIONS_H

#include "lvgl.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise les notifications (toasts) et les indicateurs de chargement.
 */
void ui_notifications_init(event_bus_t *bus);

/**
 * @brief Accroche les widgets sur une couche (par défaut lv_layer_top()).
 */
void ui_notifications_attach(lv_obj_t *layer);

#ifdef __cplusplus
}
#endif

#endif // UI_NOTIFICATIONS_H
