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
#include "event_types.h"

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
void screen_tinybms_status_update_stats(const tinybms_stats_t *stats);
void screen_tinybms_status_append_log(const tinybms_uart_log_entry_t *entry);

#ifdef __cplusplus
}

namespace gui {

class ScreenTinybmsStatus {
public:
    explicit ScreenTinybmsStatus(lv_obj_t *parent) { screen_tinybms_status_create(parent); }

    void update_connection(bool connected) { screen_tinybms_status_update_connection(connected); }
    void update_stats(const tinybms_stats_t &stats) { screen_tinybms_status_update_stats(&stats); }
    void append_log(const tinybms_uart_log_entry_t &entry) { screen_tinybms_status_append_log(&entry); }
};

}  // namespace gui
#endif

#endif // SCREEN_TINYBMS_STATUS_H
