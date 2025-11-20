/**
 * @file tinybms_protocol.h
 * @brief TinyBMS Binary Protocol Implementation
 *
 * Conforme aux spécifications TinyBMS Communication Protocols Revision D, 2025-07-04
 *
 * Protocol Frame Format (Read Individual Register - Command 0x09):
 * Request (7 bytes):
 * +----------+---------+----+----------+----------+---------+---------+
 * | Preamble | Command | PL | Addr:LSB | Addr:MSB | CRC:LSB | CRC:MSB |
 * |   0xAA   |  0x09   | 02 |    1B    |    1B    |   1B    |   1B    |
 * +----------+---------+----+----------+----------+---------+---------+
 *
 * Response (9 bytes):
 * +----------+---------+----+----------+----------+----------+----------+---------+---------+
 * | Preamble | Command | PL | Addr:LSB | Addr:MSB | Data:LSB | Data:MSB | CRC:LSB | CRC:MSB |
 * |   0xAA   |  0x09   | 04 |    1B    |    1B    |    1B    |    1B    |   1B    |   1B    |
 * +----------+---------+----+----------+----------+----------+----------+---------+---------+
 *
 * Write Individual Register - Command 0x0D:
 * Request (9 bytes):
 * +----------+---------+----+----------+----------+----------+----------+---------+---------+
 * | Preamble | Command | PL | Addr:LSB | Addr:MSB | Data:LSB | Data:MSB | CRC:LSB | CRC:MSB |
 * |   0xAA   |  0x0D   | 04 |    1B    |    1B    |    1B    |    1B    |   1B    |   1B    |
 * +----------+---------+----+----------+----------+----------+----------+---------+---------+
 */

#ifndef TINYBMS_PROTOCOL_H
#define TINYBMS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Protocol constants
#define TINYBMS_PREAMBLE                0xAA

// Command codes (conformes à la spécification TinyBMS Rev D)
#define TINYBMS_CMD_RESET               0x02  // Reset BMS command
#define TINYBMS_CMD_READ_BLOCK          0x07  // Read registers block (proprietary)
#define TINYBMS_CMD_READ_INDIVIDUAL     0x09  // Read individual registers
#define TINYBMS_CMD_WRITE_BLOCK         0x0B  // Write registers block (proprietary)
#define TINYBMS_CMD_WRITE_INDIVIDUAL    0x0D  // Write individual registers
#define TINYBMS_CMD_MODBUS_READ         0x03  // Read registers block (MODBUS)
#define TINYBMS_CMD_MODBUS_WRITE        0x10  // Write registers block (MODBUS)

// Reset command options (Command 0x02)
#define TINYBMS_RESET_OPTION_BMS        0x05  // Reset BMS option

// Response codes
#define TINYBMS_RESP_ACK                0x01  // Byte2 in ACK response
#define TINYBMS_RESP_NACK               0x00  // Byte2 in NACK response

// Frame lengths (using Read/Write Individual commands 0x09/0x0D)
#define TINYBMS_READ_FRAME_LEN          7     // 0xAA + 0x09 + PL + Addr(2) + CRC(2)
#define TINYBMS_WRITE_FRAME_LEN         9     // 0xAA + 0x0D + PL + Addr(2) + Data(2) + CRC(2)
#define TINYBMS_RESET_FRAME_LEN         6     // 0xAA + 0x02 + PL + Option + CRC(2)
#define TINYBMS_MAX_FRAME_LEN           256

/**
 * @brief Calculate CRC16 for TinyBMS protocol
 *
 * Uses Modbus-like CRC16 algorithm with 0xA001 polynomial
 *
 * @param buffer Data buffer
 * @param length Buffer length
 * @return CRC16 value (little-endian)
 */
uint16_t tinybms_crc16(const uint8_t *buffer, size_t length);

/**
 * @brief Build a read register frame
 *
 * @param frame Output buffer (must be >= 7 bytes)
 * @param address Register address
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_read_frame(uint8_t *frame, uint16_t address);

/**
 * @brief Build a write register frame
 *
 * @param frame Output buffer (must be >= 9 bytes)
 * @param address Register address
 * @param value Register value
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_write_frame(uint8_t *frame, uint16_t address, uint16_t value);

/**
 * @brief Build a reset command frame
 *
 * Builds a frame for Command 0x02 (Reset) with option 0x05 (Reset BMS)
 * Frame format: [0xAA] [0x02] [0x01] [0x05] [CRC_LSB] [CRC_MSB]
 *
 * @param frame Output buffer (must be >= 6 bytes)
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_reset_frame(uint8_t *frame);

/**
 * @brief Build a read block frame (Command 0x07 - Proprietary)
 *
 * Reads multiple consecutive registers
 * Frame format: [0xAA] [0x07] [PL] [Start:LSB] [Start:MSB] [Count] [CRC_LSB] [CRC_MSB]
 *
 * @param frame Output buffer (must be >= 8 bytes)
 * @param start_address Starting register address
 * @param count Number of registers to read (1-255)
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_read_block_frame(uint8_t *frame, uint16_t start_address, uint8_t count);

/**
 * @brief Build a write block frame (Command 0x0B - Proprietary)
 *
 * Writes multiple consecutive registers
 * Frame format: [0xAA] [0x0B] [PL] [Start:LSB] [Start:MSB] [Count] [Data...] [CRC_LSB] [CRC_MSB]
 *
 * @param frame Output buffer (must be >= 6 + count*2 bytes)
 * @param start_address Starting register address
 * @param values Array of values to write
 * @param count Number of registers to write (1-125)
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_write_block_frame(uint8_t *frame, uint16_t start_address,
                                          const uint16_t *values, uint8_t count);

/**
 * @brief Build a MODBUS read frame (Command 0x03)
 *
 * MODBUS-compatible read of multiple registers
 * Frame format: [0xAA] [0x03] [PL] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [CRC_LSB] [CRC_MSB]
 *
 * @param frame Output buffer (must be >= 9 bytes)
 * @param start_address Starting register address
 * @param quantity Number of registers to read (1-125)
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_modbus_read_frame(uint8_t *frame, uint16_t start_address, uint16_t quantity);

/**
 * @brief Build a MODBUS write frame (Command 0x10)
 *
 * MODBUS-compatible write of multiple registers
 * Frame format: [0xAA] [0x10] [PL] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [ByteCount] [Data...] [CRC_LSB] [CRC_MSB]
 *
 * @param frame Output buffer (must be >= 9 + count*2 bytes)
 * @param start_address Starting register address
 * @param values Array of values to write
 * @param quantity Number of registers to write (1-123)
 * @return ESP_OK on success
 */
esp_err_t tinybms_build_modbus_write_frame(uint8_t *frame, uint16_t start_address,
                                           const uint16_t *values, uint16_t quantity);

/**
 * @brief Extract a complete frame from receive buffer
 *
 * Searches for preamble, validates length and CRC
 *
 * @param buffer Input buffer
 * @param buffer_len Buffer length
 * @param frame_start Output: pointer to frame start (if found)
 * @param frame_len Output: frame length (if found)
 * @return ESP_OK if valid frame found, ESP_ERR_NOT_FOUND if no frame,
 *         ESP_ERR_INVALID_CRC on CRC error
 */
esp_err_t tinybms_extract_frame(const uint8_t *buffer, size_t buffer_len,
                                 const uint8_t **frame_start, size_t *frame_len);

/**
 * @brief Parse a read response frame (Command 0x09)
 *
 * @param frame Frame buffer
 * @param frame_len Frame length
 * @param value Output: register value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid frame
 */
esp_err_t tinybms_parse_read_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t *value);

/**
 * @brief Parse a read block response frame (Command 0x07)
 *
 * @param frame Frame buffer
 * @param frame_len Frame length
 * @param values Output: array of register values (must be pre-allocated)
 * @param max_count Maximum number of values that can be stored
 * @param actual_count Output: actual number of values read
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid frame
 */
esp_err_t tinybms_parse_read_block_response(const uint8_t *frame, size_t frame_len,
                                            uint16_t *values, uint8_t max_count,
                                            uint8_t *actual_count);

/**
 * @brief Parse a MODBUS read response frame (Command 0x03)
 *
 * @param frame Frame buffer
 * @param frame_len Frame length
 * @param values Output: array of register values (must be pre-allocated)
 * @param max_count Maximum number of values that can be stored
 * @param actual_count Output: actual number of values read
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid frame
 */
esp_err_t tinybms_parse_modbus_read_response(const uint8_t *frame, size_t frame_len,
                                             uint16_t *values, uint16_t max_count,
                                             uint16_t *actual_count);

/**
 * @brief Parse an ACK/NACK response
 *
 * @param frame Frame buffer
 * @param frame_len Frame length
 * @param is_ack Output: true if ACK, false if NACK
 * @param error_code Output: error code (if NACK)
 * @return ESP_OK on success
 */
esp_err_t tinybms_parse_ack(const uint8_t *frame, size_t frame_len,
                             bool *is_ack, uint8_t *error_code);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_PROTOCOL_H
