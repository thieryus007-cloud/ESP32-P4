/**
 * @file screen_tinybms_config.c
 * @brief TinyBMS Configuration Screen Implementation
 */

#include "screen_tinybms_config.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "scr_tbms_config";

// UI elements for key configuration values
static struct {
    // Battery group
    lv_obj_t *label_cell_count;
    lv_obj_t *label_capacity;
    lv_obj_t *label_fully_charged_v;
    lv_obj_t *label_fully_discharged_v;

    // Safety group
    lv_obj_t *label_overvoltage_cutoff;
    lv_obj_t *label_undervoltage_cutoff;
    lv_obj_t *label_discharge_oc;
    lv_obj_t *label_charge_oc;
    lv_obj_t *label_overheat;

    // Advanced group
    lv_obj_t *label_charge_restart;
    lv_obj_t *label_soc;
    lv_obj_t *label_soh;
} ui = {0};

/**
 * @brief Create a configuration row
 */
static lv_obj_t* create_config_row(lv_obj_t *parent, const char *label_text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(95), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 5, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xB0B0B0), 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_color(value, lv_color_hex(0xFFFFFF), 0);

    return value;
}

/**
 * @brief Create a section header
 */
static void create_section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *header = lv_label_create(parent);
    lv_label_set_text(header, text);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_pad_top(header, 10, 0);
}

void screen_tinybms_config_create(lv_obj_t *parent)
{
    // Main scrollable container
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 10, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_AUTO);

    // Title
    lv_obj_t *title = lv_label_create(container);
    lv_label_set_text(title, "TinyBMS Configuration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    // Battery section
    create_section_header(container, "Battery Settings");
    ui.label_cell_count = create_config_row(container, "Cell Count");
    ui.label_capacity = create_config_row(container, "Capacity");
    ui.label_fully_charged_v = create_config_row(container, "Fully Charged");
    ui.label_fully_discharged_v = create_config_row(container, "Fully Discharged");

    // Safety section
    create_section_header(container, "Safety Limits");
    ui.label_overvoltage_cutoff = create_config_row(container, "Overvoltage Cutoff");
    ui.label_undervoltage_cutoff = create_config_row(container, "Undervoltage Cutoff");
    ui.label_discharge_oc = create_config_row(container, "Discharge OC");
    ui.label_charge_oc = create_config_row(container, "Charge OC");
    ui.label_overheat = create_config_row(container, "Overheat Cutoff");

    // Advanced section
    create_section_header(container, "Advanced");
    ui.label_charge_restart = create_config_row(container, "Charge Restart Level");
    ui.label_soc = create_config_row(container, "State of Charge");
    ui.label_soh = create_config_row(container, "State of Health");

    // Note label
    lv_obj_t *note = lv_label_create(container);
    lv_label_set_text(note, "Tap 'Read All' on Status tab to refresh values");
    lv_obj_set_style_text_color(note, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(note, 20, 0);

    ESP_LOGI(TAG, "TinyBMS config screen created");
}

void screen_tinybms_config_update(const tinybms_config_t *config)
{
    if (config == NULL || ui.label_cell_count == NULL) {
        return;
    }

    // Wrap in lv_async_call for thread safety
    typedef struct {
        tinybms_config_t config;
        lv_obj_t *label_cell_count;
        lv_obj_t *label_capacity;
        lv_obj_t *label_fully_charged_v;
        lv_obj_t *label_fully_discharged_v;
        lv_obj_t *label_overvoltage_cutoff;
        lv_obj_t *label_undervoltage_cutoff;
        lv_obj_t *label_discharge_oc;
        lv_obj_t *label_charge_oc;
        lv_obj_t *label_overheat;
        lv_obj_t *label_charge_restart;
        lv_obj_t *label_soc;
        lv_obj_t *label_soh;
    } update_ctx_t;

    update_ctx_t *ctx = malloc(sizeof(update_ctx_t));
    if (ctx == NULL) {
        return;
    }

    ctx->config = *config;
    ctx->label_cell_count = ui.label_cell_count;
    ctx->label_capacity = ui.label_capacity;
    ctx->label_fully_charged_v = ui.label_fully_charged_v;
    ctx->label_fully_discharged_v = ui.label_fully_discharged_v;
    ctx->label_overvoltage_cutoff = ui.label_overvoltage_cutoff;
    ctx->label_undervoltage_cutoff = ui.label_undervoltage_cutoff;
    ctx->label_discharge_oc = ui.label_discharge_oc;
    ctx->label_charge_oc = ui.label_charge_oc;
    ctx->label_overheat = ui.label_overheat;
    ctx->label_charge_restart = ui.label_charge_restart;
    ctx->label_soc = ui.label_soc;
    ctx->label_soh = ui.label_soh;

    lv_async_call([](void *arg) {
        update_ctx_t *c = (update_ctx_t *)arg;
        char buf[32];

        // Battery settings
        snprintf(buf, sizeof(buf), "%u cells", c->config.cell_count);
        lv_label_set_text(c->label_cell_count, buf);

        snprintf(buf, sizeof(buf), "%.2f Ah", c->config.battery_capacity_ah);
        lv_label_set_text(c->label_capacity, buf);

        snprintf(buf, sizeof(buf), "%u mV", c->config.fully_charged_voltage_mv);
        lv_label_set_text(c->label_fully_charged_v, buf);

        snprintf(buf, sizeof(buf), "%u mV", c->config.fully_discharged_voltage_mv);
        lv_label_set_text(c->label_fully_discharged_v, buf);

        // Safety limits
        snprintf(buf, sizeof(buf), "%u mV", c->config.overvoltage_cutoff_mv);
        lv_label_set_text(c->label_overvoltage_cutoff, buf);

        snprintf(buf, sizeof(buf), "%u mV", c->config.undervoltage_cutoff_mv);
        lv_label_set_text(c->label_undervoltage_cutoff, buf);

        snprintf(buf, sizeof(buf), "%u A", c->config.discharge_overcurrent_a);
        lv_label_set_text(c->label_discharge_oc, buf);

        snprintf(buf, sizeof(buf), "%u A", c->config.charge_overcurrent_a);
        lv_label_set_text(c->label_charge_oc, buf);

        snprintf(buf, sizeof(buf), "%u °C", c->config.overheat_cutoff_c);
        lv_label_set_text(c->label_overheat, buf);

        // Advanced
        snprintf(buf, sizeof(buf), "%u %%", c->config.charge_restart_level_percent);
        lv_label_set_text(c->label_charge_restart, buf);

        snprintf(buf, sizeof(buf), "%.1f %%", c->config.state_of_charge_permille / 10.0f);
        lv_label_set_text(c->label_soc, buf);

        snprintf(buf, sizeof(buf), "%.1f %%", c->config.state_of_health_permille / 10.0f);
        lv_label_set_text(c->label_soh, buf);

        // Invalidate all labels
        lv_obj_invalidate(c->label_cell_count);
        lv_obj_invalidate(c->label_capacity);
        lv_obj_invalidate(c->label_fully_charged_v);
        lv_obj_invalidate(c->label_fully_discharged_v);
        lv_obj_invalidate(c->label_overvoltage_cutoff);
        lv_obj_invalidate(c->label_undervoltage_cutoff);
        lv_obj_invalidate(c->label_discharge_oc);
        lv_obj_invalidate(c->label_charge_oc);
        lv_obj_invalidate(c->label_overheat);
        lv_obj_invalidate(c->label_charge_restart);
        lv_obj_invalidate(c->label_soc);
        lv_obj_invalidate(c->label_soh);

        free(c);
    }, ctx);
}
