/**
 * @file tinybms_client.h
 * @brief TinyBMS UART Client - High Level Interface
 *
 * Manages UART communication with TinyBMS via RS485 interface.
 * Provides thread-safe register read/write operations.
 *
 * Hardware:
 * - UART1
 * - GPIO27: RXD
 * - GPIO26: TXD
 * - 115200 baud, 8N1
 */

#ifndef TINYBMS_CLIENT_H
#define TINYBMS_CLIENT_H

#include "esp_err.h"
#include "event_bus.h"
#include "event_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// UART configuration
#define TINYBMS_UART_NUM           UART_NUM_1
#define TINYBMS_UART_RXD_PIN       27
#define TINYBMS_UART_TXD_PIN       26
#define TINYBMS_UART_BAUD_RATE     115200

// Transaction timeouts
#define TINYBMS_TIMEOUT_MS         750
#define TINYBMS_RETRY_COUNT        3

/**
 * @brief TinyBMS connection state
 */
typedef enum {
    TINYBMS_STATE_DISCONNECTED = 0,
    TINYBMS_STATE_CONNECTING,
    TINYBMS_STATE_CONNECTED,
    TINYBMS_STATE_ERROR
} tinybms_state_t;

/**
 * @brief Initialize TinyBMS UART client
 *
 * Sets up UART1 for RS485 communication with TinyBMS.
 * Creates mutex for thread-safe access.
 *
 * @param bus Pointer to EventBus for publishing events
 * @return ESP_OK on success
 */
esp_err_t tinybms_client_init(event_bus_t *bus);

/**
 * @brief Start TinyBMS client
 *
 * Begins communication and attempts to connect to TinyBMS.
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_client_start(void);

/**
 * @brief Read a register from TinyBMS
 *
 * Thread-safe synchronous read operation with retry logic.
 *
 * @param address Register address
 * @param value Output: register value
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout,
 *         ESP_ERR_INVALID_CRC on CRC error
 */
esp_err_t tinybms_read_register(uint16_t address, uint16_t *value);

/**
 * @brief Write a register to TinyBMS
 *
 * Thread-safe synchronous write operation with verification.
 * Writes value, waits for ACK, then reads back to verify.
 *
 * @param address Register address
 * @param value Value to write
 * @param verified_value Output: value read back after write (can be NULL)
 * @return ESP_OK on success and verification
 */
esp_err_t tinybms_write_register(uint16_t address, uint16_t value,
                                  uint16_t *verified_value);

/**
 * @brief Restart TinyBMS
 *
 * Sends reset command using Command 0x02 with option 0x05 (Reset BMS)
 * Conforme à la section 1.1.8 du protocole TinyBMS Rev D
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_restart(void);

/**
 * @brief Read multiple consecutive registers (Command 0x07 - Block Read)
 *
 * @param start_address Starting register address
 * @param count Number of registers to read (1-255)
 * @param values Output: array of values (must be pre-allocated)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_block(uint16_t start_address, uint8_t count, uint16_t *values);

/**
 * @brief Write multiple consecutive registers (Command 0x0B - Block Write)
 *
 * @param start_address Starting register address
 * @param count Number of registers to write (1-125)
 * @param values Array of values to write
 * @return ESP_OK on success
 */
esp_err_t tinybms_write_block(uint16_t start_address, uint8_t count, const uint16_t *values);

/**
 * @brief Read multiple registers using MODBUS protocol (Command 0x03)
 *
 * @param start_address Starting register address
 * @param quantity Number of registers to read (1-125)
 * @param values Output: array of values (must be pre-allocated)
 * @return ESP_OK on success
 */
esp_err_t tinybms_modbus_read(uint16_t start_address, uint16_t quantity, uint16_t *values);

/**
 * @brief Write multiple registers using MODBUS protocol (Command 0x10)
 *
 * @param start_address Starting register address
 * @param quantity Number of registers to write (1-123)
 * @param values Array of values to write
 * @return ESP_OK on success
 */
esp_err_t tinybms_modbus_write(uint16_t start_address, uint16_t quantity, const uint16_t *values);

/**
 * @brief Read newest events (Command 0x11)
 *
 * @param events Output: array to store events
 * @param max_count Maximum number of events
 * @param actual_count Output: actual number of events read
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_newest_events(uint16_t *events, uint8_t max_count, uint8_t *actual_count);

/**
 * @brief Read all events (Command 0x12)
 *
 * @param events Output: array to store events
 * @param max_count Maximum number of events
 * @param actual_count Output: actual number of events read
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_all_events(uint16_t *events, uint8_t max_count, uint8_t *actual_count);

/**
 * @brief Read battery pack voltage (Command 0x14)
 *
 * @param voltage Output: pack voltage (in mV or 0.01V units)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_pack_voltage(uint16_t *voltage);

/**
 * @brief Read battery pack current (Command 0x15)
 *
 * @param current Output: pack current (signed, in mA or 0.01A units)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_pack_current(int16_t *current);

/**
 * @brief Read maximum cell voltage (Command 0x16)
 *
 * @param max_voltage Output: max cell voltage (in mV)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_max_cell_voltage(uint16_t *max_voltage);

/**
 * @brief Read minimum cell voltage (Command 0x17)
 *
 * @param min_voltage Output: min cell voltage (in mV)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_min_cell_voltage(uint16_t *min_voltage);

/**
 * @brief Read online status (Command 0x18)
 *
 * @param status Output: online status flags
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_online_status(uint16_t *status);

/**
 * @brief Read lifetime counter (Command 0x19)
 *
 * @param lifetime Output: lifetime counter value
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_lifetime_counter(uint16_t *lifetime);

/**
 * @brief Read estimated state of charge (Command 0x1A)
 *
 * @param soc Output: SOC value (0-100% or 0-10000 for 0.01% units)
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_estimated_soc(uint16_t *soc);

/**
 * @brief Read temperatures (Command 0x1B)
 *
 * @param temperatures Output: array of temperature values
 * @param max_count Maximum number of temperature sensors
 * @param actual_count Output: actual number of temperatures read
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_temperatures(uint16_t *temperatures, uint8_t max_count, uint8_t *actual_count);

/**
 * @brief Read cell voltages (Command 0x1C)
 *
 * @param voltages Output: array of cell voltages (in mV)
 * @param max_count Maximum number of cells
 * @param actual_count Output: actual number of cells read
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_cell_voltages(uint16_t *voltages, uint8_t max_count, uint8_t *actual_count);

/**
 * @brief Read settings values (Command 0x1D)
 *
 * @param settings Output: array of settings values
 * @param max_count Maximum number of settings
 * @param actual_count Output: actual number of settings read
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_settings_values(uint16_t *settings, uint8_t max_count, uint8_t *actual_count);

/**
 * @brief Read firmware version (Command 0x1E)
 *
 * @param major Output: major version number
 * @param minor Output: minor version number
 * @param patch Output: patch version number
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_version(uint8_t *major, uint8_t *minor, uint8_t *patch);

/**
 * @brief Read extended firmware version (Command 0x1F)
 *
 * @param major Output: major version number
 * @param minor Output: minor version number
 * @param patch Output: patch version number
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_extended_version(uint8_t *major, uint8_t *minor, uint8_t *patch);

/**
 * @brief Read speed and distance data (Command 0x20)
 *
 * @param speed Output: speed value
 * @param distance Output: distance value
 * @return ESP_OK on success
 */
esp_err_t tinybms_read_speed_distance(uint16_t *speed, uint16_t *distance);

/**
 * @brief Get current connection state
 *
 * @return Current state
 */
tinybms_state_t tinybms_get_state(void);

/**
 * @brief Get client statistics
 *
 * @param stats Output: statistics structure
 * @return ESP_OK on success
 */
esp_err_t tinybms_get_stats(tinybms_stats_t *stats);

/**
 * @brief Reset statistics counters
 */
void tinybms_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_CLIENT_H
