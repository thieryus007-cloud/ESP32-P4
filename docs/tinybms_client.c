/*
 * tinybms_client.c
 * Implémentation du driver TinyBMS UART
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "tinybms_client.h"

static const char *TAG = "TINYBMS";
static int g_uart_num = UART_NUM_1;

#define RX_BUF_SIZE 256
#define TX_BUF_SIZE 256
#define TIMEOUT_MS  500

// ---------------------------------------------------------
// CRC Calculation [cite: 272, 305]
// ---------------------------------------------------------
static uint16_t tinybms_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    uint16_t poly = 0x8005; // x16 + x15 + x2 + 1

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // Reversed poly representation for LSB processing
            } else {
                crc >>= 1;
            }
        }
    }
    // TinyBMS uses standard MODBUS CRC, which usually requires swapping bytes at end?
    // The code example in PDF [cite: 308] returns 'crcword'.
    // However, in the frame, it is sent LSB then MSB.
    return crc;
}

// ---------------------------------------------------------
// Low Level Transport
// ---------------------------------------------------------
static esp_err_t send_command_generic(uint8_t cmd, const uint8_t *payload, uint8_t payload_len, uint8_t *rx_buf, size_t *rx_len) {
    uint8_t tx_buf[TX_BUF_SIZE];
    uint16_t idx = 0;

    // 1. Header
    tx_buf[idx++] = TINYBMS_START_BYTE;
    tx_buf[idx++] = cmd;
    
    // 2. Payload (if any)
    // NOTE: For command 0x0D (Write Individual), Payload Length byte is not explicitly in the table count 
    // but implied in generic commands. However, some commands like 0x07 send PL byte.
    // We will follow the specific structure per command wrapper, but this generic function
    // assumes the caller formatted the payload content correctly, EXCEPT CRC.
    
    if (payload && payload_len > 0) {
        memcpy(&tx_buf[idx], payload, payload_len);
        idx += payload_len;
    }

    // 3. CRC
    uint16_t crc = tinybms_crc16(tx_buf, idx);
    tx_buf[idx++] = crc & 0xFF;        // CRC LSB
    tx_buf[idx++] = (crc >> 8) & 0xFF; // CRC MSB

    // 4. Send
    uart_flush(g_uart_num);
    uart_write_bytes(g_uart_num, (const char *)tx_buf, idx);

    // 5. Receive
    if (rx_buf != NULL && rx_len != NULL) {
        int len = uart_read_bytes(g_uart_num, rx_buf, RX_BUF_SIZE, pdMS_TO_TICKS(TIMEOUT_MS));
        if (len < 4) { // Min frame: AA CMD CRC_L CRC_H
            ESP_LOGE(TAG, "RX Timeout or Short Frame: %d", len);
            return ESP_ERR_TIMEOUT;
        }

        // Validate Header
        if (rx_buf[0] != TINYBMS_START_BYTE) return ESP_FAIL;
        
        // Validate CRC
        uint16_t recv_crc = rx_buf[len-2] | (rx_buf[len-1] << 8);
        uint16_t calc_crc = tinybms_crc16(rx_buf, len-2);
        
        if (recv_crc != calc_crc) {
            ESP_LOGE(TAG, "CRC Error: Recv %04X vs Calc %04X", recv_crc, calc_crc);
            return ESP_ERR_INVALID_CRC;
        }

        // Check Error Response (0x00 in byte 2) [cite: 49, 66]
        if (rx_buf[1] == 0x00) {
             ESP_LOGW(TAG, "BMS Error Code: 0x%02X for Cmd 0x%02X", rx_buf[3], rx_buf[2]);
             return ESP_FAIL;
        }

        *rx_len = len;
    }

    return ESP_OK;
}

// ---------------------------------------------------------
// Implementation - Commands
// ---------------------------------------------------------

esp_err_t tinybms_read_pack_voltage(float *voltage) {
    uint8_t rx[32];
    size_t rx_len;
    
    // Cmd 0x14 [cite: 164]
    if (send_command_generic(TINYBMS_CMD_READ_PACK_VOLTAGE, NULL, 0, rx, &rx_len) != ESP_OK) return ESP_FAIL;
    
    // Resp: AA 14 DATA_LSB ... DATA_MSB CRC CRC [cite: 165]
    // Data at index 2, 3, 4, 5
    uint32_t raw;
    memcpy(&raw, &rx[2], 4); // Little Endian copy
    *voltage = (float)raw / 1000000.0f; // Check standard float conversion? 
    // Actually PDF says [FLOAT] format directly. Usually IEEE754.
    memcpy(voltage, &rx[2], 4);
    
    return ESP_OK;
}

esp_err_t tinybms_read_pack_current(float *current) {
    uint8_t rx[32];
    size_t rx_len;
    if (send_command_generic(TINYBMS_CMD_READ_PACK_CURRENT, NULL, 0, rx, &rx_len) != ESP_OK) return ESP_FAIL;
    memcpy(current, &rx[2], 4);
    return ESP_OK;
}

esp_err_t tinybms_read_cell_voltages(uint16_t *cells, uint8_t max_count, uint8_t *count_out) {
    uint8_t rx[128];
    size_t rx_len;

    // Cmd 0x1C [cite: 221]
    if (send_command_generic(TINYBMS_CMD_READ_CELL_VOLTAGES, NULL, 0, rx, &rx_len) != ESP_OK) return ESP_FAIL;

    // Resp: AA 1C PL DATA1_LSB DATA1_MSB ... 
    uint8_t pl = rx[2]; // Payload Length
    uint8_t cell_cnt = pl / 2;
    
    if (cell_cnt > max_count) cell_cnt = max_count;
    *count_out = cell_cnt;

    for (int i = 0; i < cell_cnt; i++) {
        int idx = 3 + (i * 2);
        cells[i] = rx[idx] | (rx[idx+1] << 8);
    }
    return ESP_OK;
}

esp_err_t tinybms_read_temperatures(int16_t *internal, int16_t *ext1, int16_t *ext2) {
    uint8_t rx[32];
    size_t rx_len;

    // Cmd 0x1B [cite: 208]
    if (send_command_generic(TINYBMS_CMD_READ_TEMPS, NULL, 0, rx, &rx_len) != ESP_OK) return ESP_FAIL;
    
    // Resp: AA 1B PL(6) INT(2) EXT1(2) EXT2(2) CRC [cite: 210]
    // internal at 3,4; ext1 at 5,6; ext2 at 7,8
    *internal = (int16_t)(rx[3] | (rx[4] << 8));
    *ext1     = (int16_t)(rx[5] | (rx[6] << 8));
    *ext2     = (int16_t)(rx[7] | (rx[8] << 8));

    return ESP_OK;
}

esp_err_t tinybms_reset(tinybms_reset_opt_t option) {
    // Cmd 0x02, Option Byte [cite: 134]
    uint8_t payload[1] = { (uint8_t)option };
    uint8_t rx[16];
    size_t rx_len;
    
    // Note: Payload is just OPTION, CRC follows
    return send_command_generic(TINYBMS_CMD_RESET_CLEAR, payload, 1, rx, &rx_len);
}

esp_err_t tinybms_write_reg(uint16_t reg_addr, uint16_t value) {
    // Cmd 0x0D 
    // Structure: AA 0D PL(05) ADDR_LSB ADDR_MSB DATA_MSB DATA_LSB
    // WARNING: PDF  shows DATA MSB first for Write Individual!
    
    uint8_t payload[5];
    payload[0] = 0x05; // PL
    payload[1] = reg_addr & 0xFF;      // ADDR LSB
    payload[2] = (reg_addr >> 8) & 0xFF; // ADDR MSB
    payload[3] = (value >> 8) & 0xFF;    // DATA MSB
    payload[4] = value & 0xFF;         // DATA LSB
    
    uint8_t rx[16];
    size_t rx_len;
    return send_command_generic(TINYBMS_CMD_WRITE_REG_INDIVIDUAL, payload, 5, rx, &rx_len);
}

esp_err_t tinybms_init(int uart_num, int tx_pin, int rx_pin) {
    g_uart_num = uart_num;
    uart_config_t uart_config = {
        .baud_rate = 115200, // [cite: 44]
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(g_uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(g_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(g_uart_num, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    return ESP_OK;
}
