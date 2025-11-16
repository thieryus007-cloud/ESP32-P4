/**
 * @file screen_tinybms_status.h
 * @brief TinyBMS Status Screen
 *
 * Displays TinyBMS connection status, communication statistics,
 * and provides controls for reading all registers and restarting TinyBMS.
 */

#ifndef SCREEN_TINYBMS_STATUS_H
#define SCREEN_TINYBMS_STATUS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create TinyBMS status screen
 *
 * @param parent Parent container (usually a tab)
 */
void screen_tinybms_status_create(lv_obj_t *parent);

/**
 * @brief Update connection status
 *
 * @param connected true if TinyBMS is connected
 */
void screen_tinybms_status_update_connection(bool connected);

/**
 * @brief Update statistics display
 *
 * @param reads_ok Successful reads
 * @param reads_failed Failed reads
 * @param writes_ok Successful writes
 * @param writes_failed Failed writes
 * @param crc_errors CRC errors
 * @param timeouts Timeouts
 */
void screen_tinybms_status_update_stats(uint32_t reads_ok, uint32_t reads_failed,
                                         uint32_t writes_ok, uint32_t writes_failed,
                                         uint32_t crc_errors, uint32_t timeouts);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_TINYBMS_STATUS_H
