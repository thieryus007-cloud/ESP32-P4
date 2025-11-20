/*
 * tinybms_client.h
 * Interface publique pour le driver TinyBMS
 */

#ifndef TINYBMS_CLIENT_H
#define TINYBMS_CLIENT_H

#include "esp_err.h"
#include "tinybms_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structures de données décodées
typedef struct {
    float pack_voltage;
    float pack_current;
    uint16_t min_cell_v;
    uint16_t max_cell_v;
    uint16_t online_status;
    int16_t temp_internal;
    int16_t temp_ext1;
    int16_t temp_ext2;
    uint32_t soc; // Reg 46 (0.000001% resolution) [cite: 661]
    uint32_t lifetime_counter;
} tinybms_status_t;

typedef struct {
    float speed_kmh;
    uint32_t dist_left_km; // Note: PDF says UINT32 in logic, float in some descriptions. Handling as UINT32 based on Reg 44 logic
    uint32_t time_left_s;
} tinybms_calc_data_t;

typedef struct {
    uint8_t hw_version;
    uint8_t hw_changes;
    uint8_t fw_public;
    uint16_t fw_internal;
} tinybms_version_t;

// --- Core Functions ---
esp_err_t tinybms_init(int uart_num, int tx_pin, int rx_pin);

// --- Read Commands (Live Data) ---
esp_err_t tinybms_read_pack_voltage(float *voltage); // Cmd 0x14
esp_err_t tinybms_read_pack_current(float *current); // Cmd 0x15
esp_err_t tinybms_read_cell_voltages(uint16_t *cells, uint8_t max_count, uint8_t *count_out); // Cmd 0x1C
esp_err_t tinybms_read_temperatures(int16_t *internal, int16_t *ext1, int16_t *ext2); // Cmd 0x1B
esp_err_t tinybms_read_calc_values(tinybms_calc_data_t *data); // Cmd 0x20
esp_err_t tinybms_read_online_status(uint16_t *status); // Cmd 0x18
esp_err_t tinybms_read_soc(uint32_t *soc); // Cmd 0x1A

// --- Control Commands ---
esp_err_t tinybms_reset(tinybms_reset_opt_t option); // Cmd 0x02

// --- Advanced Register Access ---
// Write Single Register (Proprietary 0x0D)
esp_err_t tinybms_write_reg(uint16_t reg_addr, uint16_t value); 

// Write Multiple Registers (Proprietary 0x0B)
esp_err_t tinybms_write_reg_block(uint16_t start_addr, const uint16_t *data, uint8_t count);

// Read Register Block (Proprietary 0x07)
esp_err_t tinybms_read_reg_block(uint16_t start_addr, uint8_t count, uint16_t *data_out);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_CLIENT_H
