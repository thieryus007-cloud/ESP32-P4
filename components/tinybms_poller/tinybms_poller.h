/**
 * @file tinybms_poller.h
 * @brief TinyBMS Periodic Poller - Automatic Live Data Refresh
 *
 * Polls TinyBMS live data registers periodically to keep the cache fresh
 * and trigger UI updates. Similar to the web interface polling strategy.
 *
 * Architecture:
 *   tinybms_poller (task) → tinybms_model_read_register() → EVENT_TINYBMS_REGISTER_UPDATED → GUI/CAN/MQTT
 */

#ifndef TINYBMS_POLLER_H
#define TINYBMS_POLLER_H

#include "esp_err.h"
#include "event_bus.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Polling configuration
 */
typedef struct {
    uint32_t live_data_period_ms;      ///< Period for live data polling (default: 2000ms)
    uint32_t config_data_period_ms;    ///< Period for config data polling (default: 30000ms)
    uint32_t inter_register_delay_ms;  ///< Delay between individual register reads (default: 50ms)
    bool enable_live_data;             ///< Enable live data polling (default: true)
    bool enable_config_data;           ///< Enable config data polling (default: false)
} tinybms_poller_config_t;

/**
 * @brief Get default polling configuration
 *
 * @return Default configuration
 */
tinybms_poller_config_t tinybms_poller_get_default_config(void);

/**
 * @brief Initialize TinyBMS poller
 *
 * Sets up the polling task but does not start it.
 *
 * @param bus Pointer to EventBus for publishing events
 * @param config Polling configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_init(event_bus_t *bus, const tinybms_poller_config_t *config);

/**
 * @brief Start TinyBMS poller
 *
 * Begins periodic polling of TinyBMS registers.
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_start(void);

/**
 * @brief Stop TinyBMS poller
 *
 * Stops the polling task.
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_stop(void);

/**
 * @brief Trigger an immediate poll cycle
 *
 * Forces an immediate poll of all registers without waiting for the next period.
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_trigger_now(void);

/**
 * @brief Update polling configuration
 *
 * @param config New configuration
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_set_config(const tinybms_poller_config_t *config);

/**
 * @brief Get current polling configuration
 *
 * @param config Output: current configuration
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_get_config(tinybms_poller_config_t *config);

/**
 * @brief Get poller statistics
 *
 * @param total_polls Output: total poll cycles completed
 * @param successful_reads Output: total successful register reads
 * @param failed_reads Output: total failed register reads
 * @return ESP_OK on success
 */
esp_err_t tinybms_poller_get_stats(uint32_t *total_polls, uint32_t *successful_reads,
                                    uint32_t *failed_reads);

/**
 * @brief Reset poller statistics
 */
void tinybms_poller_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_POLLER_H
