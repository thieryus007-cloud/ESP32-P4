/**
 * @file tinybms_protocol.c
 * @brief TinyBMS Binary Protocol Implementation
 */

#include "tinybms_protocol.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "tinybms_proto";

/**
 * @brief Calculate CRC16 (Modbus-like algorithm)
 *
 * Reference: Exemple/mac-local/src/serial.js lines 21-34
 */
uint16_t tinybms_crc16(const uint8_t *buffer, size_t length)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= buffer[i];

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = ((crc >> 1) ^ 0xA001) & 0xFFFF;
            } else {
                crc = (crc >> 1) & 0xFFFF;
            }
        }
    }

    return crc & 0xFFFF;
}

/**
 * @brief Build a read register frame
 *
 * Frame format (7 bytes):
 * [0xAA] [0x07] [0x01] [Addr_LO] [Addr_HI] [CRC_LO] [CRC_HI]
 */
esp_err_t tinybms_build_read_frame(uint8_t *frame, uint16_t address)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;              // Preamble
    frame[1] = 0x07;                          // Length field
    frame[2] = TINYBMS_CMD_READ;              // Command: Read
    frame[3] = address & 0xFF;                // Address low byte
    frame[4] = (address >> 8) & 0xFF;         // Address high byte

    // Calculate CRC on first 5 bytes
    uint16_t crc = tinybms_crc16(frame, 5);
    frame[5] = crc & 0xFF;                    // CRC low byte
    frame[6] = (crc >> 8) & 0xFF;             // CRC high byte

    return ESP_OK;
}

/**
 * @brief Build a write register frame
 *
 * Frame format (9 bytes):
 * [0xAA] [0x0D] [0x04] [Addr_LO] [Addr_HI] [Val_LO] [Val_HI] [CRC_LO] [CRC_HI]
 */
esp_err_t tinybms_build_write_frame(uint8_t *frame, uint16_t address, uint16_t value)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;              // Preamble
    frame[1] = 0x0D;                          // Length field
    frame[2] = TINYBMS_CMD_WRITE;             // Command: Write
    frame[3] = address & 0xFF;                // Address low byte
    frame[4] = (address >> 8) & 0xFF;         // Address high byte
    frame[5] = value & 0xFF;                  // Value low byte
    frame[6] = (value >> 8) & 0xFF;           // Value high byte

    // Calculate CRC on first 7 bytes
    uint16_t crc = tinybms_crc16(frame, 7);
    frame[7] = crc & 0xFF;                    // CRC low byte
    frame[8] = (crc >> 8) & 0xFF;             // CRC high byte

    return ESP_OK;
}

/**
 * @brief Extract a complete frame from receive buffer
 *
 * Reference: Exemple/mac-local/src/serial.js lines 70-103
 */
esp_err_t tinybms_extract_frame(const uint8_t *buffer, size_t buffer_len,
                                 const uint8_t **frame_start, size_t *frame_len)
{
    if (buffer == NULL || frame_start == NULL || frame_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Search for preamble
    const uint8_t *preamble_pos = NULL;
    for (size_t i = 0; i < buffer_len; i++) {
        if (buffer[i] == TINYBMS_PREAMBLE) {
            preamble_pos = &buffer[i];
            break;
        }
    }

    if (preamble_pos == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t remaining = buffer_len - (preamble_pos - buffer);

    // Need at least 5 bytes to get length field
    if (remaining < 5) {
        return ESP_ERR_NOT_FOUND; // Need more data
    }

    // Get payload length and calculate total frame length
    uint8_t payload_len = preamble_pos[2];
    size_t total_frame_len = 3 + payload_len + 2; // Header(3) + Payload + CRC(2)

    // Check if we have the complete frame
    if (remaining < total_frame_len) {
        return ESP_ERR_NOT_FOUND; // Incomplete frame
    }

    // Verify CRC
    uint16_t expected_crc = preamble_pos[total_frame_len - 2] |
                           (preamble_pos[total_frame_len - 1] << 8);
    uint16_t computed_crc = tinybms_crc16(preamble_pos, total_frame_len - 2);

    if (expected_crc != computed_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected 0x%04X, got 0x%04X",
                 expected_crc, computed_crc);
        return ESP_ERR_INVALID_CRC;
    }

    // Valid frame found
    *frame_start = preamble_pos;
    *frame_len = total_frame_len;

    return ESP_OK;
}

/**
 * @brief Parse a read response frame
 *
 * Response format (7 bytes):
 * [0xAA] [0x07] [0x02] [Val_LO] [Val_HI] [CRC_LO] [CRC_HI]
 */
esp_err_t tinybms_parse_read_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t *value)
{
    if (frame == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len != 7) {
        ESP_LOGE(TAG, "Invalid read response length: %d", frame_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE ||
        frame[1] != 0x07 ||
        frame[2] != TINYBMS_CMD_READ_RESPONSE) {
        ESP_LOGE(TAG, "Invalid read response header");
        return ESP_ERR_INVALID_ARG;
    }

    // Extract value (little-endian)
    *value = frame[3] | (frame[4] << 8);

    return ESP_OK;
}

/**
 * @brief Parse an ACK/NACK response
 */
esp_err_t tinybms_parse_ack(const uint8_t *frame, size_t frame_len,
                             bool *is_ack, uint8_t *error_code)
{
    if (frame == NULL || is_ack == NULL || error_code == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd = frame[2];

    if (cmd == TINYBMS_CMD_ACK) {
        *is_ack = true;
        *error_code = 0;
    } else if (cmd == TINYBMS_CMD_NACK) {
        *is_ack = false;
        // Error code is in payload[0] if available
        *error_code = (frame_len > 3) ? frame[3] : 0xFF;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
