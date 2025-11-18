#include "diagnostic_logger.h"

#include "event_types.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

#define DIAG_LOG_NAMESPACE      "diaglog"
#define DIAG_LOG_STORAGE_KEY    "ring_v1"
#define DIAG_LOG_MAX_ENTRIES    64
#define DIAG_LOG_MAX_PAYLOAD    96
#define DIAG_LOG_COMPRESSION_RLE 1

typedef struct {
    uint64_t timestamp_ms;
    uint32_t sequence;
    uint16_t stored_len;
    uint16_t original_len;
    uint8_t  source;
    uint8_t  compression; // 0 = none, 1 = RLE
    uint8_t  reserved[2];
    uint8_t  payload[DIAG_LOG_MAX_PAYLOAD];
} diag_log_entry_t;

typedef struct {
    uint32_t head;
    uint32_t count;
    uint32_t next_sequence;
    uint32_t dropped;
    bool     healthy;
    diag_log_entry_t entries[DIAG_LOG_MAX_ENTRIES];
} diag_log_ring_t;

static const char *TAG = "diag_logger";
static diag_log_ring_t s_ring = {
    .healthy = true,
};
static event_bus_t *s_bus = NULL;

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static size_t rle_compress(const uint8_t *input, size_t len, uint8_t *out, size_t out_max)
{
    size_t i = 0;
    size_t out_len = 0;

    while (i < len) {
        uint8_t byte = input[i];
        size_t run = 1;
        while ((i + run) < len && input[i + run] == byte && run < 255) {
            run++;
        }

        if ((out_len + 2) > out_max) {
            return 0;
        }

        out[out_len++] = byte;
        out[out_len++] = (uint8_t) run;
        i += run;
    }

    return out_len;
}

static void persist_ring(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(DIAG_LOG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        s_ring.healthy = false;
        s_ring.dropped++;
        return;
    }

    err = nvs_set_blob(handle, DIAG_LOG_STORAGE_KEY, &s_ring, sizeof(s_ring));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist diag logs: %s", esp_err_to_name(err));
        s_ring.healthy = false;
        s_ring.dropped++;
    } else {
        s_ring.healthy = true;
    }
}

static void load_ring(void)
{
    nvs_handle_t handle;
    size_t size = sizeof(s_ring);
    esp_err_t err = nvs_open(DIAG_LOG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing diag log storage (%s)", esp_err_to_name(err));
        memset(&s_ring, 0, sizeof(s_ring));
        s_ring.healthy = true;
        return;
    }

    err = nvs_get_blob(handle, DIAG_LOG_STORAGE_KEY, &s_ring, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(s_ring)) {
        ESP_LOGW(TAG, "Diag log storage reset (%s, size=%zu)", esp_err_to_name(err), size);
        memset(&s_ring, 0, sizeof(s_ring));
        s_ring.healthy = true;
    }
}

static void append_entry(diag_log_source_t source, const char *message)
{
    if (!message || !s_bus) {
        return;
    }

    size_t msg_len = strnlen(message, DIAG_LOG_MAX_PAYLOAD);
    if (msg_len == DIAG_LOG_MAX_PAYLOAD) {
        s_ring.dropped++;
        s_ring.healthy = false;
        ESP_LOGW(TAG, "Diag log dropped (message too long)");
        return;
    }

    uint8_t compressed[DIAG_LOG_MAX_PAYLOAD];
    size_t compressed_len = rle_compress((const uint8_t *) message, msg_len, compressed, sizeof(compressed));

    diag_log_entry_t entry = {
        .timestamp_ms = now_ms(),
        .sequence = s_ring.next_sequence++,
        .source = (uint8_t) source,
        .original_len = (uint16_t) msg_len,
        .compression = 0,
    };

    if (compressed_len > 0 && compressed_len < msg_len) {
        entry.stored_len = (uint16_t) compressed_len;
        entry.compression = DIAG_LOG_COMPRESSION_RLE;
        memcpy(entry.payload, compressed, compressed_len);
    } else {
        entry.stored_len = (uint16_t) msg_len;
        memcpy(entry.payload, message, msg_len);
    }

    uint32_t idx = s_ring.head % DIAG_LOG_MAX_ENTRIES;
    s_ring.entries[idx] = entry;
    s_ring.head = (s_ring.head + 1) % DIAG_LOG_MAX_ENTRIES;
    if (s_ring.count < DIAG_LOG_MAX_ENTRIES) {
        s_ring.count++;
    }

    persist_ring();
}

static void handle_uart_log(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_uart_log_entry_t *entry = (const tinybms_uart_log_entry_t *) event->data;
    char message[DIAG_LOG_MAX_PAYLOAD];
    snprintf(message, sizeof(message), "UART action=%s addr=0x%04X res=%d msg=%s",
             entry->action,
             entry->address,
             entry->result,
             entry->message);

    append_entry(DIAG_LOG_SOURCE_UART, message);
}

static void handle_register_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_register_update_t *update = (const tinybms_register_update_t *) event->data;
    char message[DIAG_LOG_MAX_PAYLOAD];
    snprintf(message, sizeof(message), "Reg %s=%.3f (0x%04X)",
             update->key,
             update->user_value,
             update->raw_value);

    append_entry(DIAG_LOG_SOURCE_RS485, message);
}

static void handle_stats_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_stats_event_t *stats_evt = (const tinybms_stats_event_t *) event->data;
    char message[DIAG_LOG_MAX_PAYLOAD];
    snprintf(message,
             sizeof(message),
             "stats ok_r=%lu ok_w=%lu crc=%lu timeouts=%lu nacks=%lu retries=%lu",
             (unsigned long) stats_evt->stats.reads_ok,
             (unsigned long) stats_evt->stats.writes_ok,
             (unsigned long) stats_evt->stats.crc_errors,
             (unsigned long) stats_evt->stats.timeouts,
             (unsigned long) stats_evt->stats.nacks,
             (unsigned long) stats_evt->stats.retries);

    append_entry(DIAG_LOG_SOURCE_UART, message);
}

esp_err_t diagnostic_logger_init(event_bus_t *bus)
{
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    s_bus = bus;
    load_ring();

    event_bus_subscribe(bus, EVENT_TINYBMS_UART_LOG, handle_uart_log, NULL);
    event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, handle_register_update, NULL);
    event_bus_subscribe(bus, EVENT_TINYBMS_STATS_UPDATED, handle_stats_update, NULL);

    ESP_LOGI(TAG, "Diagnostic logger ready (entries=%u, dropped=%u)",
             s_ring.count,
             s_ring.dropped);
    return ESP_OK;
}

diag_logger_status_t diagnostic_logger_get_status(void)
{
    diag_logger_status_t status = {
        .dropped = s_ring.dropped,
        .healthy = s_ring.healthy,
    };

    event_bus_queue_metrics_t metrics = {0};
    if (event_bus_get_queue_metrics(s_bus, &metrics)) {
        status.event_queue_capacity = metrics.queue_capacity;
        status.event_queue_depth    = metrics.messages_waiting;
        status.event_queue_drops    = metrics.dropped_events;
        status.event_queue_ready    = true;
    }
    return status;
}

diag_logger_ring_info_t diagnostic_logger_get_ring_info(void)
{
    diag_logger_ring_info_t info = {
        .used = s_ring.count,
        .capacity = DIAG_LOG_MAX_ENTRIES,
        .dropped = s_ring.dropped,
        .healthy = s_ring.healthy,
    };
    return info;
}
