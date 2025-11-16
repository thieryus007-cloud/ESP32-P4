/**
 * @file tinybms_protocol.h
 * @brief TinyBMS Binary Protocol Implementation
 *
 * Protocol Frame Format:
 * +----------+--------+---------+---------+----------+
 * | Preamble | Length | Command | Payload | CRC16-LE |
 * |   0xAA   |   1B   |   1B    |  var    |   2B     |
 * +----------+--------+---------+---------+----------+
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
#define TINYBMS_PREAMBLE           0xAA
#define TINYBMS_CMD_READ           0x01
#define TINYBMS_CMD_READ_RESPONSE  0x02
#define TINYBMS_CMD_WRITE          0x04
#define TINYBMS_CMD_ACK            0x01
#define TINYBMS_CMD_NACK           0x81

#define TINYBMS_READ_FRAME_LEN     7
#define TINYBMS_WRITE_FRAME_LEN    9
#define TINYBMS_MAX_FRAME_LEN      256

// Special addresses
#define TINYBMS_REG_SYSTEM_RESTART 0x0086
#define TINYBMS_RESTART_VALUE      0xA55A

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
 * @brief Parse a read response frame
 *
 * @param frame Frame buffer
 * @param frame_len Frame length
 * @param value Output: register value
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid frame
 */
esp_err_t tinybms_parse_read_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t *value);

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
