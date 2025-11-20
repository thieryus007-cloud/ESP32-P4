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
 * @brief Build a read register frame (Command 0x09 - Read Individual Registers)
 *
 * Frame format (7 bytes) selon Section 1.1.3 du protocole TinyBMS:
 * Byte1  Byte2  Byte3  Byte4      Byte5      Byte6    Byte7
 * [0xAA] [0x09] [PL]   [Addr:LSB] [Addr:MSB] [CRC:LSB] [CRC:MSB]
 *
 * Pour lire 1 registre: PL = 0x02 (2 bytes d'adresse)
 */
esp_err_t tinybms_build_read_frame(uint8_t *frame, uint16_t address)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_READ_INDIVIDUAL;         // Byte2: Command 0x09
    frame[2] = 0x02;                                // Byte3: PL = 2 (longueur payload = 2 octets d'adresse)
    frame[3] = address & 0xFF;                      // Byte4: Address low byte
    frame[4] = (address >> 8) & 0xFF;               // Byte5: Address high byte

    // Calculate CRC on first 5 bytes (preamble + cmd + PL + addr)
    uint16_t crc = tinybms_crc16(frame, 5);
    frame[5] = crc & 0xFF;                          // Byte6: CRC low byte
    frame[6] = (crc >> 8) & 0xFF;                   // Byte7: CRC high byte

    return ESP_OK;
}

/**
 * @brief Build a write register frame (Command 0x0D - Write Individual Registers)
 *
 * Frame format (9 bytes) selon Section 1.1.5 du protocole TinyBMS:
 * Byte1  Byte2  Byte3  Byte4      Byte5      Byte6      Byte7      Byte8    Byte9
 * [0xAA] [0x0D] [PL]   [Addr:LSB] [Addr:MSB] [Data:LSB] [Data:MSB] [CRC:LSB][CRC:MSB]
 *
 * Pour écrire 1 registre: PL = 0x04 (2 bytes addr + 2 bytes data)
 */
esp_err_t tinybms_build_write_frame(uint8_t *frame, uint16_t address, uint16_t value)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_WRITE_INDIVIDUAL;        // Byte2: Command 0x0D
    frame[2] = 0x04;                                // Byte3: PL = 4 (2 bytes addr + 2 bytes data)
    frame[3] = address & 0xFF;                      // Byte4: Address low byte
    frame[4] = (address >> 8) & 0xFF;               // Byte5: Address high byte
    frame[5] = value & 0xFF;                        // Byte6: Value low byte
    frame[6] = (value >> 8) & 0xFF;                 // Byte7: Value high byte

    // Calculate CRC on first 7 bytes (preamble + cmd + PL + addr + value)
    uint16_t crc = tinybms_crc16(frame, 7);
    frame[7] = crc & 0xFF;                          // Byte8: CRC low byte
    frame[8] = (crc >> 8) & 0xFF;                   // Byte9: CRC high byte

    return ESP_OK;
}

/**
 * @brief Build a reset command frame (Command 0x02 - Reset BMS)
 *
 * Frame format (6 bytes) selon Section 1.1.8 du protocole TinyBMS Rev D:
 * Byte1  Byte2  Byte3  Byte4   Byte5    Byte6
 * [0xAA] [0x02] [PL]   [OPTION][CRC:LSB][CRC:MSB]
 *
 * Pour reset BMS: PL = 0x01 (1 byte option), OPTION = 0x05 (Reset BMS)
 */
esp_err_t tinybms_build_reset_frame(uint8_t *frame)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_RESET;                   // Byte2: Command 0x02
    frame[2] = 0x01;                                // Byte3: PL = 1 (1 byte option)
    frame[3] = TINYBMS_RESET_OPTION_BMS;            // Byte4: Option 0x05 (Reset BMS)

    // Calculate CRC on first 4 bytes (preamble + cmd + PL + option)
    uint16_t crc = tinybms_crc16(frame, 4);
    frame[4] = crc & 0xFF;                          // Byte5: CRC low byte
    frame[5] = (crc >> 8) & 0xFF;                   // Byte6: CRC high byte

    return ESP_OK;
}

/**
 * @brief Build a read block frame (Command 0x07 - Proprietary)
 *
 * Frame format (8 bytes):
 * [0xAA] [0x07] [PL] [Start:LSB] [Start:MSB] [Count] [CRC:LSB] [CRC:MSB]
 * PL = 3 (2 bytes addr + 1 byte count)
 */
esp_err_t tinybms_build_read_block_frame(uint8_t *frame, uint16_t start_address, uint8_t count)
{
    if (frame == NULL || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_READ_BLOCK;              // Byte2: Command 0x07
    frame[2] = 0x03;                                // Byte3: PL = 3
    frame[3] = start_address & 0xFF;                // Byte4: Start address LSB
    frame[4] = (start_address >> 8) & 0xFF;         // Byte5: Start address MSB
    frame[5] = count;                               // Byte6: Register count

    // Calculate CRC on first 6 bytes
    uint16_t crc = tinybms_crc16(frame, 6);
    frame[6] = crc & 0xFF;                          // Byte7: CRC LSB
    frame[7] = (crc >> 8) & 0xFF;                   // Byte8: CRC MSB

    return ESP_OK;
}

/**
 * @brief Build a write block frame (Command 0x0B - Proprietary)
 *
 * Frame format (variable):
 * [0xAA] [0x0B] [PL] [Start:LSB] [Start:MSB] [Count] [Data...] [CRC:LSB] [CRC:MSB]
 * PL = 3 + count*2
 */
esp_err_t tinybms_build_write_block_frame(uint8_t *frame, uint16_t start_address,
                                          const uint16_t *values, uint8_t count)
{
    if (frame == NULL || values == NULL || count == 0 || count > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t payload_len = 3 + (count * 2);

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_WRITE_BLOCK;             // Byte2: Command 0x0B
    frame[2] = payload_len;                         // Byte3: PL
    frame[3] = start_address & 0xFF;                // Byte4: Start address LSB
    frame[4] = (start_address >> 8) & 0xFF;         // Byte5: Start address MSB
    frame[5] = count;                               // Byte6: Register count

    // Add register values (little-endian)
    for (uint8_t i = 0; i < count; i++) {
        uint8_t offset = 6 + (i * 2);
        frame[offset] = values[i] & 0xFF;           // Data LSB
        frame[offset + 1] = (values[i] >> 8) & 0xFF; // Data MSB
    }

    // Calculate CRC
    size_t crc_offset = 6 + (count * 2);
    uint16_t crc = tinybms_crc16(frame, crc_offset);
    frame[crc_offset] = crc & 0xFF;                 // CRC LSB
    frame[crc_offset + 1] = (crc >> 8) & 0xFF;      // CRC MSB

    return ESP_OK;
}

/**
 * @brief Build a MODBUS read frame (Command 0x03)
 *
 * Frame format (9 bytes):
 * [0xAA] [0x03] [PL] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [CRC:LSB] [CRC:MSB]
 * PL = 4 (2 bytes addr + 2 bytes quantity)
 */
esp_err_t tinybms_build_modbus_read_frame(uint8_t *frame, uint16_t start_address, uint16_t quantity)
{
    if (frame == NULL || quantity == 0 || quantity > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_MODBUS_READ;             // Byte2: Command 0x03
    frame[2] = 0x04;                                // Byte3: PL = 4
    frame[3] = start_address & 0xFF;                // Byte4: Start address LSB
    frame[4] = (start_address >> 8) & 0xFF;         // Byte5: Start address MSB
    frame[5] = quantity & 0xFF;                     // Byte6: Quantity LSB
    frame[6] = (quantity >> 8) & 0xFF;              // Byte7: Quantity MSB

    // Calculate CRC on first 7 bytes
    uint16_t crc = tinybms_crc16(frame, 7);
    frame[7] = crc & 0xFF;                          // Byte8: CRC LSB
    frame[8] = (crc >> 8) & 0xFF;                   // Byte9: CRC MSB

    return ESP_OK;
}

/**
 * @brief Build a MODBUS write frame (Command 0x10)
 *
 * Frame format (variable):
 * [0xAA] [0x10] [PL] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [ByteCount] [Data...] [CRC:LSB] [CRC:MSB]
 * PL = 5 + byte_count
 */
esp_err_t tinybms_build_modbus_write_frame(uint8_t *frame, uint16_t start_address,
                                           const uint16_t *values, uint16_t quantity)
{
    if (frame == NULL || values == NULL || quantity == 0 || quantity > 123) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t byte_count = quantity * 2;
    uint8_t payload_len = 5 + byte_count;

    frame[0] = TINYBMS_PREAMBLE;                    // Byte1: Preamble 0xAA
    frame[1] = TINYBMS_CMD_MODBUS_WRITE;            // Byte2: Command 0x10
    frame[2] = payload_len;                         // Byte3: PL
    frame[3] = start_address & 0xFF;                // Byte4: Start address LSB
    frame[4] = (start_address >> 8) & 0xFF;         // Byte5: Start address MSB
    frame[5] = quantity & 0xFF;                     // Byte6: Quantity LSB
    frame[6] = (quantity >> 8) & 0xFF;              // Byte7: Quantity MSB
    frame[7] = byte_count;                          // Byte8: Byte count

    // Add register values (big-endian for MODBUS)
    for (uint16_t i = 0; i < quantity; i++) {
        uint16_t offset = 8 + (i * 2);
        frame[offset] = (values[i] >> 8) & 0xFF;    // Data MSB (MODBUS uses big-endian)
        frame[offset + 1] = values[i] & 0xFF;       // Data LSB
    }

    // Calculate CRC
    size_t crc_offset = 8 + byte_count;
    uint16_t crc = tinybms_crc16(frame, crc_offset);
    frame[crc_offset] = crc & 0xFF;                 // CRC LSB
    frame[crc_offset + 1] = (crc >> 8) & 0xFF;      // CRC MSB

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
 * @brief Parse a read response frame (Command 0x09 response)
 *
 * Response format (9 bytes pour 1 registre) selon Section 1.1.3:
 * Byte1  Byte2  Byte3  Byte4      Byte5      Byte6     Byte7     Byte8    Byte9
 * [0xAA] [0x09] [PL]   [Addr:LSB] [Addr:MSB] [Data:LSB][Data:MSB][CRC:LSB][CRC:MSB]
 */
esp_err_t tinybms_parse_read_response(const uint8_t *frame, size_t frame_len,
                                       uint16_t *value)
{
    if (frame == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Minimum length: 9 bytes (0xAA + 0x09 + PL + Addr(2) + Data(2) + CRC(2))
    if (frame_len < 9) {
        ESP_LOGE(TAG, "Invalid read response length: %zu (expected >=9)", frame_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE) {
        ESP_LOGE(TAG, "Invalid preamble: 0x%02X", frame[0]);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[1] != TINYBMS_CMD_READ_INDIVIDUAL) {
        ESP_LOGE(TAG, "Invalid command: 0x%02X (expected 0x09)", frame[1]);
        return ESP_ERR_INVALID_ARG;
    }

    // Byte3 = PL (payload length)
    uint8_t payload_len = frame[2];

    // Pour 1 registre: PL devrait être >= 4 (2 bytes addr + 2 bytes data)
    if (payload_len < 4) {
        ESP_LOGE(TAG, "Invalid payload length: %d", payload_len);
        return ESP_ERR_INVALID_ARG;
    }

    // Extract address echo (Byte4-5)
    // uint16_t addr_echo = frame[3] | (frame[4] << 8);

    // Extract value (little-endian) from Byte6-7
    *value = frame[5] | (frame[6] << 8);

    return ESP_OK;
}

/**
 * @brief Parse a read block response frame (Command 0x07 response)
 *
 * Response format (variable):
 * [0xAA] [0x07] [PL] [Start:LSB] [Start:MSB] [Data...] [CRC:LSB] [CRC:MSB]
 */
esp_err_t tinybms_parse_read_block_response(const uint8_t *frame, size_t frame_len,
                                            uint16_t *values, uint8_t max_count,
                                            uint8_t *actual_count)
{
    if (frame == NULL || values == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len < 8) {
        ESP_LOGE(TAG, "Invalid read block response length: %zu", frame_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE || frame[1] != TINYBMS_CMD_READ_BLOCK) {
        ESP_LOGE(TAG, "Invalid read block response header");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t payload_len = frame[2];
    uint8_t data_bytes = payload_len - 2; // Minus 2 bytes for start address
    uint8_t register_count = data_bytes / 2;

    if (register_count > max_count) {
        ESP_LOGW(TAG, "Response contains more registers (%d) than buffer can hold (%d)",
                 register_count, max_count);
        register_count = max_count;
    }

    // Extract values (little-endian)
    for (uint8_t i = 0; i < register_count; i++) {
        uint8_t offset = 5 + (i * 2); // Skip header (3) + start addr (2)
        values[i] = frame[offset] | (frame[offset + 1] << 8);
    }

    *actual_count = register_count;
    return ESP_OK;
}

/**
 * @brief Parse a MODBUS read response frame (Command 0x03 response)
 *
 * Response format (variable):
 * [0xAA] [0x03] [PL] [ByteCount] [Data...] [CRC:LSB] [CRC:MSB]
 */
esp_err_t tinybms_parse_modbus_read_response(const uint8_t *frame, size_t frame_len,
                                             uint16_t *values, uint16_t max_count,
                                             uint16_t *actual_count)
{
    if (frame == NULL || values == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len < 6) {
        ESP_LOGE(TAG, "Invalid MODBUS read response length: %zu", frame_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE || frame[1] != TINYBMS_CMD_MODBUS_READ) {
        ESP_LOGE(TAG, "Invalid MODBUS read response header");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t byte_count = frame[3];
    uint16_t register_count = byte_count / 2;

    if (register_count > max_count) {
        ESP_LOGW(TAG, "Response contains more registers (%d) than buffer can hold (%d)",
                 register_count, max_count);
        register_count = max_count;
    }

    // Extract values (big-endian for MODBUS)
    for (uint16_t i = 0; i < register_count; i++) {
        uint8_t offset = 4 + (i * 2); // Skip header (3) + byte count (1)
        values[i] = (frame[offset] << 8) | frame[offset + 1]; // Big-endian
    }

    *actual_count = register_count;
    return ESP_OK;
}

/**
 * @brief Parse an ACK/NACK response
 *
 * ACK format (5 bytes) selon Section 1.1.1:
 * Byte1  Byte2  Byte3  Byte4    Byte5
 * [0xAA] [0x01] [CMD]  [CRC:LSB][CRC:MSB]
 *
 * NACK format (6 bytes):
 * Byte1  Byte2  Byte3  Byte4  Byte5    Byte6
 * [0xAA] [0x00] [CMD]  [ERROR][CRC:LSB][CRC:MSB]
 */
esp_err_t tinybms_parse_ack(const uint8_t *frame, size_t frame_len,
                             bool *is_ack, uint8_t *error_code)
{
    if (frame == NULL || is_ack == NULL || error_code == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame_len < 5) {
        ESP_LOGE(TAG, "ACK/NACK frame too short: %zu", frame_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (frame[0] != TINYBMS_PREAMBLE) {
        ESP_LOGE(TAG, "Invalid preamble in ACK/NACK: 0x%02X", frame[0]);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t response_type = frame[1];

    if (response_type == TINYBMS_RESP_ACK) {
        // ACK: Byte2 = 0x01
        *is_ack = true;
        *error_code = 0;
        ESP_LOGD(TAG, "Received ACK for command 0x%02X", frame[2]);
    } else if (response_type == TINYBMS_RESP_NACK) {
        // NACK: Byte2 = 0x00
        *is_ack = false;
        // Error code is in Byte4
        *error_code = (frame_len >= 6) ? frame[3] : 0xFF;
        ESP_LOGW(TAG, "Received NACK for command 0x%02X, error: 0x%02X",
                 frame[2], *error_code);
    } else {
        ESP_LOGE(TAG, "Unknown response type: 0x%02X", response_type);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
