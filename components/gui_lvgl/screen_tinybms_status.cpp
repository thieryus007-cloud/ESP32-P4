/**
 * @file screen_tinybms_status.cpp
 * @brief TinyBMS Status Screen Implementation
 */

#include "screen_tinybms_status.h"
#include "tinybms_client.h"
#include "tinybms_model.h"
#include "event_bus.h"
#include "event_types.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "scr_tbms_status";

// UI elements
static struct {
    lv_obj_t *label_status;
    lv_obj_t *label_stats_reads;
    lv_obj_t *label_stats_writes;
    lv_obj_t *label_stats_errors;
    lv_obj_t *btn_read_all;
    lv_obj_t *btn_restart;
    lv_obj_t *log_container;
} ui = {0};

/**
 * @brief Button event handler for Read All
 */
static void btn_read_all_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Read All button clicked");

        // Disable button during read
        lv_obj_add_state(ui.btn_read_all, LV_STATE_DISABLED);
        lv_obj_invalidate(ui.btn_read_all);

        // Start read in background (this will take time)
        // In a real implementation, this should be done in a task
        esp_err_t ret = tinybms_model_read_all();

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Read all registers completed successfully");
        } else {
            ESP_LOGE(TAG, "Read all registers failed: %s", esp_err_to_name(ret));
        }

        // Re-enable button
        lv_obj_clear_state(ui.btn_read_all, LV_STATE_DISABLED);
        lv_obj_invalidate(ui.btn_read_all);
    }
}

/**
 * @brief Button event handler for Restart
 */
static void btn_restart_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Restart button clicked");

        esp_err_t ret = tinybms_restart();

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "TinyBMS restart command sent");
        } else {
            ESP_LOGE(TAG, "TinyBMS restart failed: %s", esp_err_to_name(ret));
        }
    }
}

void screen_tinybms_status_create(lv_obj_t *parent)
{
    // Main container
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 10, 0);

    // Title
    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, "TinyBMS UART Status");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    // Connection status
    lv_obj_t *status_container = lv_obj_create(container);
    lv_obj_set_size(status_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(status_container, 10, 0);

    lv_obj_t *label = lv_label_create(status_container);
    lv_label_set_text(label, "Connection:");
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);

    ui.label_status = lv_label_create(status_container);
    lv_label_set_text(ui.label_status, "Unknown");
    lv_obj_align(ui.label_status, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(ui.label_status, lv_color_hex(0x808080), 0);

    // Statistics section
    lv_obj_t *stats_title = lv_label_create(container);
    lv_label_set_text(stats_title, "Communication Statistics");
    lv_obj_set_style_text_font(stats_title, &lv_font_montserrat_16, 0);

    lv_obj_t *stats_container = lv_obj_create(container);
    lv_obj_set_size(stats_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(stats_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(stats_container, 10, 0);

    ui.label_stats_reads = lv_label_create(stats_container);
    lv_label_set_text(ui.label_stats_reads, "Reads: 0 OK / 0 Failed");

    ui.label_stats_writes = lv_label_create(stats_container);
    lv_label_set_text(ui.label_stats_writes, "Writes: 0 OK / 0 Failed");

    ui.label_stats_errors = lv_label_create(stats_container);
    lv_label_set_text(ui.label_stats_errors, "Errors: 0 CRC / 0 Timeout");

    // Control buttons
    lv_obj_t *btn_container = lv_obj_create(container);
    lv_obj_set_size(btn_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn_container, 10, 0);

    lv_obj_t *btn_title = lv_label_create(btn_container);
    lv_label_set_text(btn_title, "Shortcuts");
    lv_obj_set_style_text_font(btn_title, &lv_font_montserrat_14, 0);
    lv_obj_set_flex_grow(btn_title, 1);

    // Read All button
    ui.btn_read_all = lv_btn_create(btn_container);
    lv_obj_set_size(ui.btn_read_all, 150, 50);
    lv_obj_add_event_cb(ui.btn_read_all, btn_read_all_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(ui.btn_read_all);
    lv_label_set_text(btn_label, "Read All");
    lv_obj_center(btn_label);

    // Restart button
    ui.btn_restart = lv_btn_create(btn_container);
    lv_obj_set_size(ui.btn_restart, 150, 50);
    lv_obj_set_style_bg_color(ui.btn_restart, lv_color_hex(0xFF4444), 0);
    lv_obj_add_event_cb(ui.btn_restart, btn_restart_cb, LV_EVENT_CLICKED, NULL);

    btn_label = lv_label_create(ui.btn_restart);
    lv_label_set_text(btn_label, "Restart BMS");
    lv_obj_center(btn_label);

    // UART log
    lv_obj_t *log_title = lv_label_create(container);
    lv_label_set_text(log_title, "UART activity (latest)");
    lv_obj_set_style_text_font(log_title, &lv_font_montserrat_16, 0);

    ui.log_container = lv_obj_create(container);
    lv_obj_set_size(ui.log_container, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(ui.log_container, 10, 0);
    lv_obj_set_flex_flow(ui.log_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(ui.log_container, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < 6; i++) {
        lv_obj_t *log_line = lv_label_create(ui.log_container);
        lv_label_set_text(log_line, "Waiting for UART activity...");
        lv_obj_set_style_text_color(log_line, lv_color_hex(0x808080), 0);
    }

    ESP_LOGI(TAG, "TinyBMS status screen created");
}

void screen_tinybms_status_update_connection(bool connected)
{
    if (ui.label_status == NULL) {
        return;
    }

    // Wrap in lv_async_call for thread safety
    typedef struct {
        lv_obj_t *label;
        bool connected;
    } update_ctx_t;

    // [MOD] Allocation C++ avec new au lieu de malloc
    update_ctx_t *ctx = new (std::nothrow) update_ctx_t;
    if (ctx == NULL) {
        return;
    }

    ctx->label = ui.label_status;
    ctx->connected = connected;

    lv_async_call([](void *arg) {
        update_ctx_t *c = (update_ctx_t *)arg;

        if (c->connected) {
            lv_label_set_text(c->label, "Connected");
            lv_obj_set_style_text_color(c->label, lv_color_hex(0x00FF00), 0);
        } else {
            lv_label_set_text(c->label, "Disconnected");
            lv_obj_set_style_text_color(c->label, lv_color_hex(0xFF0000), 0);
        }

        lv_obj_invalidate(c->label);
        delete c; // [MOD] delete au lieu de free
    }, ctx);
}

void screen_tinybms_status_update_stats(const tinybms_stats_t *stats)
{
    if (ui.label_stats_reads == NULL || stats == NULL) {
        return;
    }

    // Wrap in lv_async_call for thread safety
    typedef struct {
        lv_obj_t *label_reads;
        lv_obj_t *label_writes;
        lv_obj_t *label_errors;
        tinybms_stats_t stats;
    } stats_ctx_t;

    // [MOD] Allocation C++ avec new au lieu de malloc
    stats_ctx_t *ctx = new (std::nothrow) stats_ctx_t;
    if (ctx == NULL) {
        return;
    }

    ctx->label_reads = ui.label_stats_reads;
    ctx->label_writes = ui.label_stats_writes;
    ctx->label_errors = ui.label_stats_errors;
    ctx->stats = *stats;

    lv_async_call([](void *arg) {
        stats_ctx_t *c = (stats_ctx_t *)arg;

        char buf[64];

        snprintf(buf, sizeof(buf), "Reads: %lu OK / %lu Failed (avg %lums)",
                 (unsigned long) c->stats.reads_ok,
                 (unsigned long) c->stats.reads_failed,
                 (unsigned long) c->stats.avg_latency_ms);
        lv_label_set_text(c->label_reads, buf);

        snprintf(buf, sizeof(buf), "Writes: %lu OK / %lu Failed (queue max %lu)",
                 (unsigned long) c->stats.writes_ok,
                 (unsigned long) c->stats.writes_failed,
                 (unsigned long) c->stats.queue_depth_max);
        lv_label_set_text(c->label_writes, buf);

        snprintf(buf, sizeof(buf), "Errors: %lu CRC / %lu Timeout / %lu NACK / %lu Retry",
                 (unsigned long) c->stats.crc_errors,
                 (unsigned long) c->stats.timeouts,
                 (unsigned long) c->stats.nacks,
                 (unsigned long) c->stats.retries);
        lv_label_set_text(c->label_errors, buf);

        lv_obj_invalidate(c->label_reads);
        lv_obj_invalidate(c->label_writes);
        lv_obj_invalidate(c->label_errors);

        delete c; // [MOD] delete au lieu de free
    }, ctx);
}

void screen_tinybms_status_append_log(const tinybms_uart_log_entry_t *entry)
{
    if (ui.log_container == NULL || entry == NULL) {
        return;
    }

    lv_obj_t *line = lv_label_create(ui.log_container);
    lv_label_set_text(line, entry->message);
    lv_obj_move_to_index(line, 0);

    if (entry->success) {
        lv_obj_set_style_text_color(line, lv_color_hex(0x80FF80), 0);
    } else {
        lv_obj_set_style_text_color(line, lv_color_hex(0xFF7070), 0);
    }

    uint32_t child_count = lv_obj_get_child_cnt(ui.log_container);
    while (child_count > 6) {
        lv_obj_t *last = lv_obj_get_child(ui.log_container, child_count - 1);
        lv_obj_delete(last);
        child_count = lv_obj_get_child_cnt(ui.log_container);
    }

    lv_obj_invalidate(ui.log_container);
}
