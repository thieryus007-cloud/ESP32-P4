/**
 * @file screen_tinybms_config.h
 * @brief TinyBMS Configuration Screen
 *
 * Displays and allows editing of TinyBMS register groups.
 * Organized in tabs: Battery, Charger, Safety, Advanced, System
 */

#ifndef SCREEN_TINYBMS_CONFIG_H
#define SCREEN_TINYBMS_CONFIG_H

#include "lvgl.h"
#include "tinybms_model.h"
#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create TinyBMS configuration screen
 *
 * @param parent Parent container (usually a tab)
 */
void screen_tinybms_config_create(lv_obj_t *parent);

/**
 * @brief Update configuration display
 *
 * Refreshes all register values from model cache.
 *
 * @param config Configuration snapshot
 */
void screen_tinybms_config_update(const tinybms_config_t *config);
void screen_tinybms_config_apply_register(const tinybms_register_update_t *update);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_TINYBMS_CONFIG_H
