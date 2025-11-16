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
 * @brief TinyBMS client statistics
 */
typedef struct {
    uint32_t reads_ok;
    uint32_t reads_failed;
    uint32_t writes_ok;
    uint32_t writes_failed;
    uint32_t crc_errors;
    uint32_t timeouts;
    uint32_t nacks;
} tinybms_stats_t;

/**
 * @brief Initialize TinyBMS UART client
 *
 * Sets up UART1 for RS485 communication with TinyBMS.
 * Creates mutex for thread-safe access.
 *
 * @param bus Pointer to EventBus for publishing events
 * @return ESP_OK on success
 */
esp_err_t tinybms_client_init(EventBus *bus);

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
 * Sends restart command (write 0xA55A to register 0x0086)
 *
 * @return ESP_OK on success
 */
esp_err_t tinybms_restart(void);

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
