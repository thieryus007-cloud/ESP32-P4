/**
 * @file tinybms_client.c
 * @brief TinyBMS UART Client Implementation
 */

#include "tinybms_client.h"
#include "tinybms_protocol.h"
#include "event_types.h"
#include "event_bus.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <sys/param.h>
#include <limits.h>
#include <stdio.h>

static const char *TAG = "tinybms_client";

#define TINYBMS_CLIENT_QUEUE_DEPTH 10

typedef enum {
    TINYBMS_REQ_READ = 0,
    TINYBMS_REQ_WRITE,
} tinybms_request_type_t;

typedef struct {
    tinybms_request_type_t type;
    uint16_t address;
    uint16_t value;
    uint16_t *out_value;
    uint16_t *verified_value;
    TaskHandle_t requester;
    uint64_t enqueue_us;
} tinybms_request_t;

// Module state
static struct {
    event_bus_t *bus;
    tinybms_state_t state;
    tinybms_stats_t stats;
    bool initialized;
    QueueHandle_t request_queue;
    TaskHandle_t worker_task;
    uint64_t latency_acc_us;
    uint32_t latency_samples;
} g_ctx = {0};

static QueueHandle_t g_uart_evt_queue = NULL;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static void publish_stats_event(void);
static void tinybms_client_worker_task(void *arg);
static esp_err_t read_register_internal(uint16_t address, uint16_t *value);
static esp_err_t write_register_internal(uint16_t address, uint16_t value);
static esp_err_t send_reset_command_internal(void);
static esp_err_t read_block_internal(uint16_t start_address, uint8_t count, uint16_t *values);
static esp_err_t write_block_internal(uint16_t start_address, uint8_t count, const uint16_t *values);
static esp_err_t modbus_read_internal(uint16_t start_address, uint16_t quantity, uint16_t *values);
static esp_err_t modbus_write_internal(uint16_t start_address, uint16_t quantity, const uint16_t *values);

static inline void stats_increment(uint32_t *field)
{
    portENTER_CRITICAL(&s_stats_lock);
    (*field)++;
    portEXIT_CRITICAL(&s_stats_lock);
}

static void stats_record_latency(uint64_t start_us)
{
    uint64_t duration_us = esp_timer_get_time() - start_us;
    portENTER_CRITICAL(&s_stats_lock);
    g_ctx.latency_acc_us += duration_us;
    g_ctx.latency_samples++;
    if (g_ctx.latency_samples > 0) {
        g_ctx.stats.avg_latency_ms = (uint32_t) ((g_ctx.latency_acc_us / g_ctx.latency_samples) / 1000ULL);
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

static void stats_update_queue_depth(void)
{
    if (!g_ctx.request_queue) {
        return;
    }

    UBaseType_t depth = uxQueueMessagesWaiting(g_ctx.request_queue);
    portENTER_CRITICAL(&s_stats_lock);
    if (depth > g_ctx.stats.queue_depth_max) {
        g_ctx.stats.queue_depth_max = depth;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

static void stats_update_result(tinybms_request_type_t type, esp_err_t result)
{
    portENTER_CRITICAL(&s_stats_lock);
    if (type == TINYBMS_REQ_READ) {
        if (result == ESP_OK) {
            g_ctx.stats.reads_ok++;
        } else {
            g_ctx.stats.reads_failed++;
        }
    } else if (type == TINYBMS_REQ_WRITE) {
        if (result == ESP_OK) {
            g_ctx.stats.writes_ok++;
        } else {
            g_ctx.stats.writes_failed++;
        }
    }

    if (result == ESP_ERR_TIMEOUT) {
        g_ctx.stats.timeouts++;
    } else if (result == ESP_ERR_INVALID_CRC) {
        g_ctx.stats.crc_errors++;
    } else if (result == ESP_ERR_INVALID_RESPONSE) {
        g_ctx.stats.nacks++;
    }
    portEXIT_CRITICAL(&s_stats_lock);
}

static TickType_t request_timeout_ticks(void)
{
    uint32_t wait_ms = (TINYBMS_TIMEOUT_MS + 100) * (TINYBMS_RETRY_COUNT + 1);
    return pdMS_TO_TICKS(wait_ms);
}

static esp_err_t enqueue_request(tinybms_request_t *req)
{
    if (!g_ctx.request_queue || !req) {
        return ESP_ERR_INVALID_STATE;
    }

    req->enqueue_us = esp_timer_get_time();
    if (xQueueSend(g_ctx.request_queue, req, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    stats_update_queue_depth();
    return ESP_OK;
}

static void publish_uart_log(const char *action, uint16_t address, esp_err_t result,
                             const char *detail)
{
    if (g_ctx.bus == NULL) {
        return;
    }

    tinybms_uart_log_entry_t entry = {0};
    strncpy(entry.action, action, sizeof(entry.action) - 1);
    entry.address = address;
    entry.result = (int)result;
    entry.success = (result == ESP_OK);

    const char *status = esp_err_to_name(result);
    snprintf(entry.message, sizeof(entry.message), "%s 0x%04X: %s%s%s",
             action,
             address,
             status,
             (detail && detail[0]) ? " - " : "",
             (detail && detail[0]) ? detail : "");

    event_t evt = {
        .type = EVENT_TINYBMS_UART_LOG,
        .data = &entry,
        .data_size = sizeof(entry),
    };
    event_bus_publish(g_ctx.bus, &evt);
}

static void publish_stats_event(void)
{
    if (!g_ctx.bus) {
        return;
    }

    tinybms_stats_t snapshot;
    portENTER_CRITICAL(&s_stats_lock);
    snapshot = g_ctx.stats;
    portEXIT_CRITICAL(&s_stats_lock);

    tinybms_stats_event_t stats_evt = {
        .stats = snapshot,
        .timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000ULL),
    };

    event_t evt = {
        .type = EVENT_TINYBMS_STATS_UPDATED,
        .data = &stats_evt,
        .data_size = sizeof(stats_evt),
    };
    event_bus_publish(g_ctx.bus, &evt);
}

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

    // Install UART driver with RX/TX buffers and event queue
    const int rx_buffer_size = 2 * TINYBMS_MAX_FRAME_LEN;
    const int tx_buffer_size = TINYBMS_MAX_FRAME_LEN;
    ret = uart_driver_install(TINYBMS_UART_NUM,
                              rx_buffer_size,
                              tx_buffer_size,
                              8, // Event queue depth
                              &g_uart_evt_queue,
                              0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Tune RX/TX policies to reduce blocking
    uart_set_rx_timeout(TINYBMS_UART_NUM, 8);
    uart_set_rx_full_threshold(TINYBMS_UART_NUM, TINYBMS_MAX_FRAME_LEN / 2);
    uart_set_tx_fifo_full_threshold(TINYBMS_UART_NUM, UART_FIFO_LEN / 2);

    uart_flush_input(TINYBMS_UART_NUM);

    ESP_LOGI(TAG, "UART initialized on GPIO%d(TXD)/GPIO%d(RXD)",
             TINYBMS_UART_TXD_PIN, TINYBMS_UART_RXD_PIN);

    return ESP_OK;
}

static esp_err_t perform_read_with_retry(uint16_t address, uint16_t *value)
{
    esp_err_t ret = ESP_FAIL;
    uint16_t local_value = 0;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = read_register_internal(address, &local_value);
        if (ret == ESP_OK) {
            if (value) {
                *value = local_value;
            }
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    return ret;
}

static esp_err_t perform_write_with_retry(uint16_t address, uint16_t value)
{
    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = write_register_internal(address, value);
        if (ret == ESP_OK) {
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    return ret;
}

static esp_err_t verify_write(uint16_t address, uint16_t expected, uint16_t *readback_out)
{
    vTaskDelay(pdMS_TO_TICKS(50));
    uint16_t readback = 0;
    esp_err_t ret = perform_read_with_retry(address, &readback);
    if (ret != ESP_OK) {
        return ret;
    }

    if (readback != expected) {
        ESP_LOGW(TAG, "Write verification mismatch: wrote 0x%04X read 0x%04X", expected, readback);
        return ESP_FAIL;
    }

    if (readback_out) {
        *readback_out = readback;
    }
    return ESP_OK;
}

static void tinybms_client_worker_task(void *arg)
{
    (void) arg;
    tinybms_request_t req;

    while (1) {
        if (xQueueReceive(g_ctx.request_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t ret = ESP_FAIL;
        char detail[64];
        detail[0] = '\0';

        if (req.type == TINYBMS_REQ_READ) {
            ret = perform_read_with_retry(req.address, req.out_value);
            if (ret == ESP_OK && req.out_value) {
                snprintf(detail, sizeof(detail), "value=0x%04X", *req.out_value);
            }
            publish_uart_log("read", req.address, ret, detail);
        } else if (req.type == TINYBMS_REQ_WRITE) {
            ret = perform_write_with_retry(req.address, req.value);
            if (ret == ESP_OK) {
                ret = verify_write(req.address, req.value, req.verified_value);
                if (ret == ESP_OK) {
                    snprintf(detail, sizeof(detail), "written=0x%04X", req.value);
                }
            }
            publish_uart_log("write", req.address, ret, detail);
        }

        stats_update_result(req.type, ret);
        stats_record_latency(req.enqueue_us);
        publish_stats_event();

        if (req.requester) {
            xTaskNotify(req.requester, (uint32_t) ret, eSetValueWithOverwrite);
        }
    }
}

/**
 * @brief Low-level register read (no mutex)
 */
static esp_err_t read_register_internal(uint16_t address, uint16_t *value)
{
    uint8_t tx_frame[TINYBMS_READ_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5; // preamble + len + cmd + CRC

    // Build request frame
    esp_err_t ret = tinybms_build_read_frame(tx_frame, address);
    if (ret != ESP_OK) {
        return ret;
    }

    // Send request
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_READ_FRAME_LEN);
    if (written != TINYBMS_READ_FRAME_LEN) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    // Wait for TX complete (non-blocking thanks to TX buffer)
    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Read response with timeout
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);

    TickType_t deadline = start_time + timeout_ticks;
    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM,
                                              &rx_buffer[rx_len],
                                              to_read,
                                              0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        // Try to extract frame only when enough data is available
        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            ret = tinybms_parse_read_response(frame_start, frame_len, value);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "Read 0x%04X from register 0x%04X", *value, address);
                return ESP_OK;
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGW(TAG, "CRC error on read response");
            uart_flush_input(TINYBMS_UART_NUM);
            rx_len = 0;
            return ESP_ERR_INVALID_CRC;
        }
    }

    // Timeout
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
    const size_t min_frame_len = 5; // preamble + len + cmd + CRC

    // Build write frame
    esp_err_t ret = tinybms_build_write_frame(tx_frame, address, value);
    if (ret != ESP_OK) {
        return ret;
    }

    // Send write command
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_WRITE_FRAME_LEN);
    if (written != TINYBMS_WRITE_FRAME_LEN) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for ACK/NACK
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);

    TickType_t deadline = start_time + timeout_ticks;
    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM,
                                              &rx_buffer[rx_len],
                                              to_read,
                                              0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected during write, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            bool is_ack;
            uint8_t error_code;
            ret = tinybms_parse_ack(frame_start, frame_len, &is_ack, &error_code);

            if (ret == ESP_OK) {
                if (is_ack) {
                    ESP_LOGD(TAG, "Write ACK for register 0x%04X = 0x%04X", address, value);
                    return ESP_OK;
                } else {
                    ESP_LOGW(TAG, "Write NACK for register 0x%04X (error: 0x%02X)",
                             address, error_code);
                    return ESP_ERR_INVALID_RESPONSE;
                }
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            uart_flush_input(TINYBMS_UART_NUM);
            rx_len = 0;
            return ESP_ERR_INVALID_CRC;
        }
    }

    // Timeout
    ESP_LOGW(TAG, "Timeout waiting for write ACK (register 0x%04X)", address);
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Send reset command (Command 0x02 with option 0x05)
 *
 * Conforme à la section 1.1.8 du protocole TinyBMS Rev D
 */
static esp_err_t send_reset_command_internal(void)
{
    uint8_t tx_frame[TINYBMS_RESET_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5; // preamble + len + cmd + CRC

    // Build reset frame
    esp_err_t ret = tinybms_build_reset_frame(tx_frame);
    if (ret != ESP_OK) {
        return ret;
    }

    // Send reset command
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_RESET_FRAME_LEN);
    if (written != TINYBMS_RESET_FRAME_LEN) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for ACK/NACK
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);

    TickType_t deadline = start_time + timeout_ticks;
    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM,
                                              &rx_buffer[rx_len],
                                              to_read,
                                              0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected during reset, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            bool is_ack;
            uint8_t error_code;
            ret = tinybms_parse_ack(frame_start, frame_len, &is_ack, &error_code);

            if (ret == ESP_OK) {
                if (is_ack) {
                    ESP_LOGI(TAG, "Reset command ACK received");
                    return ESP_OK;
                } else {
                    ESP_LOGW(TAG, "Reset command NACK (error: 0x%02X)", error_code);
                    return ESP_ERR_INVALID_RESPONSE;
                }
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            uart_flush_input(TINYBMS_UART_NUM);
            rx_len = 0;
            return ESP_ERR_INVALID_CRC;
        }
    }

    // Timeout
    ESP_LOGW(TAG, "Timeout waiting for reset ACK");
    return ESP_ERR_TIMEOUT;
}

// Public API implementations

esp_err_t tinybms_client_init(event_bus_t *bus)
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

    // Initialize UART
    esp_err_t ret = init_uart();
    if (ret != ESP_OK) {
        return ret;
    }

    g_ctx.request_queue = xQueueCreate(TINYBMS_CLIENT_QUEUE_DEPTH, sizeof(tinybms_request_t));
    if (g_ctx.request_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TinyBMS request queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ok = xTaskCreate(tinybms_client_worker_task,
                                     "tinybms_io",
                                     4096,
                                     NULL,
                                     6,
                                     &g_ctx.worker_task);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TinyBMS worker task");
        vQueueDelete(g_ctx.request_queue);
        g_ctx.request_queue = NULL;
        return ESP_FAIL;
    }

    g_ctx.initialized = true;
    ESP_LOGI(TAG, "TinyBMS client initialized");

    publish_stats_event();

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
            event_t evt = {
                .type = EVENT_TINYBMS_CONNECTED,
                .data = NULL,
            };
            event_bus_publish(g_ctx.bus, &evt);
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

    tinybms_request_t req = {
        .type = TINYBMS_REQ_READ,
        .address = address,
        .out_value = value,
        .verified_value = NULL,
        .requester = xTaskGetCurrentTaskHandle(),
    };

    esp_err_t err = enqueue_request(&req);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t notify_value = 0;
    if (xTaskNotifyWait(0, ULONG_MAX, &notify_value, request_timeout_ticks()) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return (esp_err_t) notify_value;
}

esp_err_t tinybms_write_register(uint16_t address, uint16_t value,
                                  uint16_t *verified_value)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    tinybms_request_t req = {
        .type = TINYBMS_REQ_WRITE,
        .address = address,
        .value = value,
        .out_value = NULL,
        .verified_value = verified_value,
        .requester = xTaskGetCurrentTaskHandle(),
    };

    esp_err_t err = enqueue_request(&req);
    if (err != ESP_OK) {
        return err;
    }

    uint32_t notify_value = 0;
    if (xTaskNotifyWait(0, ULONG_MAX, &notify_value, request_timeout_ticks()) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return (esp_err_t) notify_value;
}

esp_err_t tinybms_restart(void)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Restarting TinyBMS using Command 0x02...");
    esp_err_t ret = send_reset_command_internal();

    // Log the reset command result
    publish_uart_log("reset", 0x0002, ret, "Command 0x02 Option 0x05");

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Reset command sent successfully");
    } else {
        ESP_LOGW(TAG, "Reset command failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Low-level read block operation (Command 0x07)
 */
static esp_err_t read_block_internal(uint16_t start_address, uint8_t count, uint16_t *values)
{
    if (count == 0 || count > 255 || values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_frame[8]; // Fixed size for read block request
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5;

    // Build read block frame
    esp_err_t ret = tinybms_build_read_block_frame(tx_frame, start_address, count);
    if (ret != ESP_OK) {
        return ret;
    }

    // Send request
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, 8);
    if (written != 8) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for response
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);
    TickType_t deadline = start_time + timeout_ticks;

    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        // Try to extract frame
        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            uint8_t actual_count = 0;
            ret = tinybms_parse_read_block_response(frame_start, frame_len, values, count, &actual_count);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "Read block: %d registers from 0x%04X", actual_count, start_address);
                return ESP_OK;
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGW(TAG, "CRC error on read block response");
            uart_flush_input(TINYBMS_UART_NUM);
            return ESP_ERR_INVALID_CRC;
        }
    }

    ESP_LOGW(TAG, "Timeout reading block from 0x%04X", start_address);
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Low-level write block operation (Command 0x0B)
 */
static esp_err_t write_block_internal(uint16_t start_address, uint8_t count, const uint16_t *values)
{
    if (count == 0 || count > 125 || values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_frame[TINYBMS_MAX_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5;

    // Build write block frame
    esp_err_t ret = tinybms_build_write_block_frame(tx_frame, start_address, values, count);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calculate frame size: 3 (header) + 3 (start_addr + count) + count*2 (data) + 2 (CRC)
    size_t frame_size = 8 + (count * 2);

    // Send write command
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, frame_size);
    if (written != (int)frame_size) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for ACK/NACK
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);
    TickType_t deadline = start_time + timeout_ticks;

    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected during write block, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            bool is_ack;
            uint8_t error_code;
            ret = tinybms_parse_ack(frame_start, frame_len, &is_ack, &error_code);

            if (ret == ESP_OK) {
                if (is_ack) {
                    ESP_LOGD(TAG, "Write block ACK for %d registers at 0x%04X", count, start_address);
                    return ESP_OK;
                } else {
                    ESP_LOGW(TAG, "Write block NACK (error: 0x%02X)", error_code);
                    return ESP_ERR_INVALID_RESPONSE;
                }
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            uart_flush_input(TINYBMS_UART_NUM);
            return ESP_ERR_INVALID_CRC;
        }
    }

    ESP_LOGW(TAG, "Timeout waiting for write block ACK");
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Low-level MODBUS read operation (Command 0x03)
 */
static esp_err_t modbus_read_internal(uint16_t start_address, uint16_t quantity, uint16_t *values)
{
    if (quantity == 0 || quantity > 125 || values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_frame[9]; // Fixed size for MODBUS read request
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5;

    // Build MODBUS read frame
    esp_err_t ret = tinybms_build_modbus_read_frame(tx_frame, start_address, quantity);
    if (ret != ESP_OK) {
        return ret;
    }

    // Send request
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, 9);
    if (written != 9) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for response
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);
    TickType_t deadline = start_time + timeout_ticks;

    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        // Try to extract frame
        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            uint16_t actual_count = 0;
            ret = tinybms_parse_modbus_read_response(frame_start, frame_len, values, quantity, &actual_count);
            if (ret == ESP_OK) {
                ESP_LOGD(TAG, "MODBUS read: %d registers from 0x%04X", actual_count, start_address);
                return ESP_OK;
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGW(TAG, "CRC error on MODBUS read response");
            uart_flush_input(TINYBMS_UART_NUM);
            return ESP_ERR_INVALID_CRC;
        }
    }

    ESP_LOGW(TAG, "Timeout on MODBUS read from 0x%04X", start_address);
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Low-level MODBUS write operation (Command 0x10)
 */
static esp_err_t modbus_write_internal(uint16_t start_address, uint16_t quantity, const uint16_t *values)
{
    if (quantity == 0 || quantity > 123 || values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_frame[TINYBMS_MAX_FRAME_LEN];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5;

    // Build MODBUS write frame
    esp_err_t ret = tinybms_build_modbus_write_frame(tx_frame, start_address, values, quantity);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calculate frame size: 3 (header) + 5 (addr + qty + byte_count) + quantity*2 (data) + 2 (CRC)
    size_t frame_size = 10 + (quantity * 2);

    // Send write command
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, frame_size);
    if (written != (int)frame_size) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    // Wait for ACK/NACK
    TickType_t start_time = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS);
    TickType_t deadline = start_time + timeout_ticks;

    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected during MODBUS write, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            bool is_ack;
            uint8_t error_code;
            ret = tinybms_parse_ack(frame_start, frame_len, &is_ack, &error_code);

            if (ret == ESP_OK) {
                if (is_ack) {
                    ESP_LOGD(TAG, "MODBUS write ACK for %d registers at 0x%04X", quantity, start_address);
                    return ESP_OK;
                } else {
                    ESP_LOGW(TAG, "MODBUS write NACK (error: 0x%02X)", error_code);
                    return ESP_ERR_INVALID_RESPONSE;
                }
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            uart_flush_input(TINYBMS_UART_NUM);
            return ESP_ERR_INVALID_CRC;
        }
    }

    ESP_LOGW(TAG, "Timeout waiting for MODBUS write ACK");
    return ESP_ERR_TIMEOUT;
}

esp_err_t tinybms_read_block(uint16_t start_address, uint8_t count, uint16_t *values)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (values == NULL || count == 0 || count > 255) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Reading %d registers from 0x%04X (Command 0x07)", count, start_address);

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = read_block_internal(start_address, count, values);
        if (ret == ESP_OK) {
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    char detail[64] = {0};
    if (ret == ESP_OK) {
        snprintf(detail, sizeof(detail), "count=%d", count);
    }
    publish_uart_log("read_block", start_address, ret, detail);
    publish_stats_event();

    return ret;
}

esp_err_t tinybms_write_block(uint16_t start_address, uint8_t count, const uint16_t *values)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (values == NULL || count == 0 || count > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Writing %d registers to 0x%04X (Command 0x0B)", count, start_address);

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = write_block_internal(start_address, count, values);
        if (ret == ESP_OK) {
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    char detail[64] = {0};
    if (ret == ESP_OK) {
        snprintf(detail, sizeof(detail), "count=%d", count);
    }
    publish_uart_log("write_block", start_address, ret, detail);
    publish_stats_event();

    return ret;
}

esp_err_t tinybms_modbus_read(uint16_t start_address, uint16_t quantity, uint16_t *values)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (values == NULL || quantity == 0 || quantity > 125) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "MODBUS read: %d registers from 0x%04X (Command 0x03)", quantity, start_address);

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = modbus_read_internal(start_address, quantity, values);
        if (ret == ESP_OK) {
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    char detail[64] = {0};
    if (ret == ESP_OK) {
        snprintf(detail, sizeof(detail), "quantity=%d", quantity);
    }
    publish_uart_log("modbus_read", start_address, ret, detail);
    publish_stats_event();

    return ret;
}

esp_err_t tinybms_modbus_write(uint16_t start_address, uint16_t quantity, const uint16_t *values)
{
    if (!g_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (values == NULL || quantity == 0 || quantity > 123) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "MODBUS write: %d registers to 0x%04X (Command 0x10)", quantity, start_address);

    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < TINYBMS_RETRY_COUNT; ++retry) {
        ret = modbus_write_internal(start_address, quantity, values);
        if (ret == ESP_OK) {
            break;
        }
        if (retry < TINYBMS_RETRY_COUNT - 1) {
            stats_increment(&g_ctx.stats.retries);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    char detail[64] = {0};
    if (ret == ESP_OK) {
        snprintf(detail, sizeof(detail), "quantity=%d", quantity);
    }
    publish_uart_log("modbus_write", start_address, ret, detail);
    publish_stats_event();

    return ret;
}

/**
 * @brief Generic internal function for simple command/response pattern
 */
static esp_err_t simple_command_internal(uint8_t command, uint8_t *response_frame,
                                         size_t *response_len, TickType_t timeout_ticks)
{
    uint8_t tx_frame[5];
    uint8_t rx_buffer[TINYBMS_MAX_FRAME_LEN];
    size_t rx_len = 0;
    const size_t min_frame_len = 5;

    esp_err_t ret = tinybms_build_simple_command_frame(tx_frame, command);
    if (ret != ESP_OK) {
        return ret;
    }

    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, 5);
    if (written != 5) {
        ESP_LOGE(TAG, "UART write failed: %d bytes", written);
        return ESP_FAIL;
    }

    uart_wait_tx_done(TINYBMS_UART_NUM, pdMS_TO_TICKS(20));

    TickType_t start_time = xTaskGetTickCount();
    TickType_t deadline = start_time + timeout_ticks;

    while (xTaskGetTickCount() < deadline) {
        uart_event_t event = {0};
        TickType_t remaining = deadline - xTaskGetTickCount();
        if (remaining == 0) {
            break;
        }

        if (g_uart_evt_queue && xQueueReceive(g_uart_evt_queue, &event, remaining) == pdTRUE) {
            if (event.type == UART_DATA || event.type == UART_PATTERN_DET) {
                size_t available = 0;
                uart_get_buffered_data_len(TINYBMS_UART_NUM, &available);
                if (available > 0 && rx_len < sizeof(rx_buffer)) {
                    size_t to_read = MIN(available, sizeof(rx_buffer) - rx_len);
                    int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
                    if (len > 0) {
                        rx_len += (size_t)len;
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                ESP_LOGW(TAG, "UART overflow detected, flushing input");
                uart_flush_input(TINYBMS_UART_NUM);
                rx_len = 0;
                continue;
            }
        }

        size_t buffered_len = 0;
        uart_get_buffered_data_len(TINYBMS_UART_NUM, &buffered_len);
        if (buffered_len > 0 && rx_len < sizeof(rx_buffer)) {
            size_t to_read = MIN(buffered_len, sizeof(rx_buffer) - rx_len);
            int len = uart_read_bytes(TINYBMS_UART_NUM, &rx_buffer[rx_len], to_read, 0);
            if (len > 0) {
                rx_len += (size_t)len;
            }
        }

        if (rx_len < min_frame_len) {
            continue;
        }

        const uint8_t *frame_start;
        size_t frame_len;
        ret = tinybms_extract_frame(rx_buffer, rx_len, &frame_start, &frame_len);

        if (ret == ESP_OK) {
            if (frame_start[1] == command) {
                memcpy(response_frame, frame_start, frame_len);
                *response_len = frame_len;
                return ESP_OK;
            }
        } else if (ret == ESP_ERR_INVALID_CRC) {
            uart_flush_input(TINYBMS_UART_NUM);
            return ESP_ERR_INVALID_CRC;
        }
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t tinybms_read_newest_events(uint16_t *events, uint8_t max_count, uint8_t *actual_count)
{
    if (!g_ctx.initialized || events == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading newest events (Command 0x11)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_NEWEST_EVENTS, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_NEWEST_EVENTS,
                                                 events, max_count, actual_count);
    }

    publish_uart_log("read_newest_events", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_all_events(uint16_t *events, uint8_t max_count, uint8_t *actual_count)
{
    if (!g_ctx.initialized || events == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading all events (Command 0x12)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_ALL_EVENTS, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_ALL_EVENTS,
                                                 events, max_count, actual_count);
    }

    publish_uart_log("read_all_events", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_pack_voltage(uint16_t *voltage)
{
    if (!g_ctx.initialized || voltage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading pack voltage (Command 0x14)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_PACK_VOLTAGE, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_PACK_VOLTAGE, voltage);
    }

    publish_uart_log("read_pack_voltage", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_pack_current(int16_t *current)
{
    if (!g_ctx.initialized || current == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading pack current (Command 0x15)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_PACK_CURRENT, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_int16_response(response, response_len,
                                                  TINYBMS_CMD_READ_PACK_CURRENT, current);
    }

    publish_uart_log("read_pack_current", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_max_cell_voltage(uint16_t *max_voltage)
{
    if (!g_ctx.initialized || max_voltage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading max cell voltage (Command 0x16)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_MAX_CELL_V, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_MAX_CELL_V, max_voltage);
    }

    publish_uart_log("read_max_cell_voltage", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_min_cell_voltage(uint16_t *min_voltage)
{
    if (!g_ctx.initialized || min_voltage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading min cell voltage (Command 0x17)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_MIN_CELL_V, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_MIN_CELL_V, min_voltage);
    }

    publish_uart_log("read_min_cell_voltage", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_online_status(uint16_t *status)
{
    if (!g_ctx.initialized || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading online status (Command 0x18)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_ONLINE_STATUS, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_ONLINE_STATUS, status);
    }

    publish_uart_log("read_online_status", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_lifetime_counter(uint16_t *lifetime)
{
    if (!g_ctx.initialized || lifetime == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading lifetime counter (Command 0x19)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_LIFETIME, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_LIFETIME, lifetime);
    }

    publish_uart_log("read_lifetime_counter", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_estimated_soc(uint16_t *soc)
{
    if (!g_ctx.initialized || soc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading estimated SOC (Command 0x1A)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_SOC, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_simple_uint16_response(response, response_len,
                                                   TINYBMS_CMD_READ_SOC, soc);
    }

    publish_uart_log("read_estimated_soc", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_temperatures(uint16_t *temperatures, uint8_t max_count, uint8_t *actual_count)
{
    if (!g_ctx.initialized || temperatures == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading temperatures (Command 0x1B)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_TEMPERATURES, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_TEMPERATURES,
                                                 temperatures, max_count, actual_count);
    }

    publish_uart_log("read_temperatures", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_cell_voltages(uint16_t *voltages, uint8_t max_count, uint8_t *actual_count)
{
    if (!g_ctx.initialized || voltages == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading cell voltages (Command 0x1C)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_CELL_VOLTAGES, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_CELL_VOLTAGES,
                                                 voltages, max_count, actual_count);
    }

    publish_uart_log("read_cell_voltages", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_settings_values(uint16_t *settings, uint8_t max_count, uint8_t *actual_count)
{
    if (!g_ctx.initialized || settings == NULL || actual_count == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading settings values (Command 0x1D)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_SETTINGS, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_SETTINGS,
                                                 settings, max_count, actual_count);
    }

    publish_uart_log("read_settings_values", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_version(uint8_t *major, uint8_t *minor, uint8_t *patch)
{
    if (!g_ctx.initialized || major == NULL || minor == NULL || patch == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading version (Command 0x1E)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_VERSION, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_version_response(response, response_len,
                                            TINYBMS_CMD_READ_VERSION,
                                            major, minor, patch);
    }

    publish_uart_log("read_version", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_extended_version(uint8_t *major, uint8_t *minor, uint8_t *patch)
{
    if (!g_ctx.initialized || major == NULL || minor == NULL || patch == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading extended version (Command 0x1F)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_EXT_VERSION, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        ret = tinybms_parse_version_response(response, response_len,
                                            TINYBMS_CMD_READ_EXT_VERSION,
                                            major, minor, patch);
    }

    publish_uart_log("read_extended_version", 0, ret, NULL);
    return ret;
}

esp_err_t tinybms_read_speed_distance(uint16_t *speed, uint16_t *distance)
{
    if (!g_ctx.initialized || speed == NULL || distance == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "Reading speed/distance (Command 0x20)");

    uint8_t response[TINYBMS_MAX_FRAME_LEN];
    size_t response_len = 0;
    esp_err_t ret = simple_command_internal(TINYBMS_CMD_READ_SPEED_DISTANCE, response,
                                            &response_len, pdMS_TO_TICKS(TINYBMS_TIMEOUT_MS));
    if (ret == ESP_OK) {
        // Response contains 2 values: speed and distance
        uint16_t values[2];
        uint8_t count;
        ret = tinybms_parse_multi_value_response(response, response_len,
                                                 TINYBMS_CMD_READ_SPEED_DISTANCE,
                                                 values, 2, &count);
        if (ret == ESP_OK && count >= 2) {
            *speed = values[0];
            *distance = values[1];
        } else {
            ret = ESP_ERR_INVALID_RESPONSE;
        }
    }

    publish_uart_log("read_speed_distance", 0, ret, NULL);
    return ret;
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

    portENTER_CRITICAL(&s_stats_lock);
    memcpy(stats, &g_ctx.stats, sizeof(tinybms_stats_t));
    portEXIT_CRITICAL(&s_stats_lock);
    return ESP_OK;
}

void tinybms_reset_stats(void)
{
    portENTER_CRITICAL(&s_stats_lock);
    memset(&g_ctx.stats, 0, sizeof(tinybms_stats_t));
    g_ctx.latency_acc_us = 0;
    g_ctx.latency_samples = 0;
    portEXIT_CRITICAL(&s_stats_lock);
    ESP_LOGI(TAG, "Statistics reset");
    publish_stats_event();
}
