/**
 * @file tinybms_client.c
 * @brief TinyBMS UART Client Implementation
 */

#include "tinybms_client.h"
#include "tinybms_protocol.h"
#include "event_types.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "tinybms_client";

// Module state
static struct {
    EventBus *bus;
    SemaphoreHandle_t uart_mutex;
    tinybms_state_t state;
    tinybms_stats_t stats;
    bool initialized;
} g_ctx = {0};

/**
 * @brief Initialize UART hardware
 */
static esp_err_t init_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = TINYBMS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "Configuring UART%d at %d baud", TINYBMS_UART_NUM, TINYBMS_UART_BAUD_RATE);

    esp_err_t ret = uart_param_config(TINYBMS_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(TINYBMS_UART_NUM,
                       TINYBMS_UART_TXD_PIN,
                       TINYBMS_UART_RXD_PIN,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install UART driver with RX buffer
    const int uart_buffer_size = 1024;
    ret = uart_driver_install(TINYBMS_UART_NUM,
                              uart_buffer_size,
                              0, // No TX buffer (blocking mode)
                              0, // No event queue
                              NULL,
                              0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "UART initialized on GPIO%d(TXD)/GPIO%d(RXD)",
             TINYBMS_UART_TXD_PIN, TINYBMS_UART_RXD_PIN);

    return ESP_OK;
}

/**
 * @brief Low-level register read (no mutex)
 */
static esp_err_t read_register_internal(uint16_t address, uint16_t *value)
{
    uint8_t tx_frame[TINYBMS_READ_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;

    // Build request frame
    esp_err_t ret = tinybms_build_read_frame(tx_frame, address);
    if (ret != ESP_OK) {
        return ret;
    }

    // Flush UART buffers
    uart_flush(TINYBMS_UART_NUM);

    // Send request
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_READ_FRAME_LEN);
    if (written != TINYBMS_READ_FRAME_LEN) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    // Wait for TX complete
    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(100));

    // Read response with timeout
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);

    while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
        int len = uart_read_bytes(TINYBMS_UART_NUM,
                                  &rx_buffer[rx_len],
                                  sizeof(rx_buffer) - rx_len,
                                  pdMS_TO_TICKS(50));

        if (len > 0) {
            rx_len += len;

            // Try to extract frame
            const uint8_t *frame_start;
            size_t frame_len;
            ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

            if (ret == ESP_OK) {
                // Valid frame received, parse it
                ret = tinybms_parse_read_response(frame_start, frame_len, value);
                if (ret == ESP_OK) {
                    ESP_LOGD(TAG, "Read 0x%04X from register 0x%04X", *value, address);
                    g_ctx.stats.reads_ok++;
                    return ESP_OK;
                }
            } else if (ret == ESP_ERR_INVALID_CRC) {
                g_ctx.stats.crc_errors++;
                ESP_LOGW(TAG, "CRC error on read response");
                return ESP_ERR_INVALID_CRC;
            }
        }
    }

    // Timeout
    g_ctx.stats.timeouts++;
    ESP_LOGW(TAG, "Timeout reading register 0x%04X", address);
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Low-level register write (no mutex)
 */
static esp_err_t write_register_internal(uint16_t address, uint16_t value)
{
    uint8_t tx_frame[TINYBMS_WRITE_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;

    // Build write frame
    esp_err_t ret = tinybms_build_write_frame(tx_frame, address, value);
    if (ret != ESP_OK) {
        return ret;
    }

    // Flush UART buffers
    uart_flush(TINYBMS_UART_NUM);

    // Send write command
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_WRITE_FRAME_LEN);
    if (written != TINYBMS_WRITE_FRAME_LEN) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    // Wait for TX complete
    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(100));

    // Wait for ACK/NACK
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);

    while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
        int len = uart_read_bytes(TINYBMS_UART_NUM,
                                  &rx_buffer[rx_len],
                                  sizeof(rx_buffer) - rx_len,
                                  pdMS_TO_TICKS(50));

        if (len > 0) {
            rx_len += len;

            // Try to extract frame
            const uint8_t *frame_start;
            size_t frame_len;
            ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

            if (ret == ESP_OK) {
                // Valid frame received, check ACK/NACK
                bool is_ack;
                uint8_t error_code;
                ret = tinybms_parse_ack(frame_start, frame_len, &is_ack, &error_code);

                if (ret == ESP_OK) {
                    if (is_ack) {
                        ESP_LOGD(TAG, "Write ACK for register 0x%04X = 0x%04X", address, value);
                        g_ctx.stats.writes_ok++;
                        return ESP_OK;
                    } else {
                        ESP_LOGW(TAG, "Write NACK for register 0x%04X (error: 0x%02X)",
                                 address, error_code);
                        g_ctx.stats.nacks++;
                        return ESP_FAIL;
                    }
                }
            } else if (ret == ESP_ERR_INVALID_CRC) {
                g_ctx.stats.crc_errors++;
                return ESP_ERR_INVALID_CRC;
            }
        }
    }

    // Timeout
    g_ctx.stats.timeouts++;
    ESP_LOGW(TAG, "Timeout waiting for write ACK (register 0x%04X)", address);
    return ESP_ERR_TIMEOUT;
}

// Public API implementations

esp_err_t tinybms_client_init(EventBus *bus)
{
    if (g_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (bus == NULL) {
        ESP_LOGE(TAG, "EventBus is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing TinyBMS client");

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.bus = bus;
    g_ctx.state = TINYBMS_STATE_DISCONNECTED;

    // Create mutex for UART access
    g_ctx.uart_mutex = xSemaphoreCreateMutex();
    if (g_ctx.uart_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create UART mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize UART
    esp_err_t ret = init_uart();
    if (ret != ESP_OK) {
        vSemaphoreDelete(g_ctx.uart_mutex);
        return ret;
    }

    g_ctx.initialized = true;
    ESP_LOGI(TAG, "TinyBMS client initialized");

    return ESP_OK;
}

esp_err_t tinybms_client_start(void)
{
    if (!g_ctx.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting TinyBMS client");
    g_ctx.state = TINYBMS_STATE_CONNECTING;

    // Try to read a register to test connection
    uint16_t test_value;
    esp_err_t ret = tinybms_read_register(0x012C, &test_value); // Read fully_charged_voltage_mv

    if (ret == ESP_OK) {
        g_ctx.state = TINYBMS_STATE_CONNECTED;
        ESP_LOGI(TAG, "TinyBMS connected (test read: 0x%04X)", test_value);

        // Publish connected event
        if (g_ctx.bus) {
            event_bus_publish(g_ctx.bus, EVENT_TINYBMS_CONNECTED, NULL, 0);
        }
    } else {
        g_ctx.state = TINYBMS_STATE_ERROR;
        ESP_LOGW(TAG, "TinyBMS connection failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t tinybms_read_register(uint16_t address, uint16_t *value)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(g_ctx.uart_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take UART mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Try read with retry
    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; retry++) {
        ret = read_register_internal(address, value);
        if (ret == ESP_OK) {
            break;
        }

        if (retry < TINYBMS_RETRY_COUNT - 1) {
            ESP_LOGD(TAG, "Retry %d/%d for register 0x%04X",
                     retry + 1, TINYBMS_RETRY_COUNT, address);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    if (ret != ESP_OK) {
        g_ctx.stats.reads_failed++;
    }

    xSemaphoreGive(g_ctx.uart_mutex);
    return ret;
}

esp_err_t tinybms_write_register(uint16_t address, uint16_t value,
                                  uint16_t *verified_value)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex with timeout
    if (xSemaphoreTake(g_ctx.uart_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take UART mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_FAIL;

    // Try write with retry
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; retry++) {
        ret = write_register_internal(address, value);
        if (ret == ESP_OK) {
            break;
        }

        if (retry < TINYBMS_RETRY_COUNT - 1) {
            ESP_LOGD(TAG, "Retry %d/%d for write to 0x%04X",
                     retry + 1, TINYBMS_RETRY_COUNT, address);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Verify write by reading back
    if (ret == ESP_OK) {
        uint16_t readback;
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay before readback

        ret = read_register_internal(address, &readback);
        if (ret == ESP_OK) {
            if (readback == value) {
                ESP_LOGI(TAG, "Write verified: register 0x%04X = 0x%04X", address, value);
                if (verified_value != NULL) {
                    *verified_value = readback;
                }
            } else {
                ESP_LOGW(TAG, "Write verification failed: wrote 0x%04X, read 0x%04X",
                         value, readback);
                ret = ESP_FAIL;
            }
        }
    }

    if (ret != ESP_OK) {
        g_ctx.stats.writes_failed++;
    }

    xSemaphoreGive(g_ctx.uart_mutex);
    return ret;
}

esp_err_t tinybms_restart(void)
{
    ESP_LOGI(TAG, "Restarting TinyBMS...");
    return tinybms_write_register(TINYBMS_REG_SYSTEM_RESTART,
                                   TINYBMS_RESTART_VALUE,
                                   NULL);
}

tinybms_state_t tinybms_get_state(void)
{
    return g_ctx.state;
}

esp_err_t tinybms_get_stats(tinybms_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &g_ctx.stats, sizeof(tinybms_stats_t));
    return ESP_OK;
}

void tinybms_reset_stats(void)
{
    memset(&g_ctx.stats, 0, sizeof(tinybms_stats_t));
    ESP_LOGI(TAG, "Statistics reset");
}
