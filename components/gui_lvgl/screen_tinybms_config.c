/**
 * @file screen_tinybms_config.c
 * @brief TinyBMS Configuration Screen (editable register catalog)
 */

#include "screen_tinybms_config.h"
#include "tinybms_model.h"
#include "tinybms_registers.h"
#include "event_types.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "scr_tbms_config";

typedef struct {
    const register_descriptor_t *desc;
    lv_obj_t *input;
    lv_obj_t *hint;
} register_widget_t;

static register_widget_t s_widgets[TINYBMS_REGISTER_COUNT];
static lv_obj_t *s_container = NULL;

static const char* group_title(register_group_t group)
{
    switch (group) {
        case REG_GROUP_BATTERY: return "Battery";
        case REG_GROUP_CHARGER: return "Charger";
        case REG_GROUP_SAFETY: return "Safety";
        case REG_GROUP_ADVANCED: return "Advanced";
        case REG_GROUP_SYSTEM: return "System";
        default: return "Other";
    }
}

static void set_hint(register_widget_t *widget, const char *text, bool ok)
{
    if (!widget || !widget->hint) {
        return;
    }

    lv_label_set_text(widget->hint, text);
    lv_obj_set_style_text_color(widget->hint,
                                ok ? lv_color_hex(0x80FF80) : lv_color_hex(0xFF7070),
                                0);
}

static void populate_input(register_widget_t *widget, float user_value)
{
    if (!widget || !widget->desc || !widget->input) {
        return;
    }

    char buf[48];
    if (widget->desc->value_class == VALUE_CLASS_ENUM) {
        for (int i = 0; i < widget->desc->enum_count; i++) {
            if ((uint16_t)user_value == widget->desc->enum_values[i].value) {
                lv_dropdown_set_selected(widget->input, i);
                break;
            }
        }
    } else {
        snprintf(buf, sizeof(buf), "%.*f",
                 widget->desc->precision,
                 user_value);
        lv_textarea_set_text(widget->input, buf);
    }
}

static bool validate_user_value(const register_descriptor_t *desc,
                                float user_value,
                                char *err_buf,
                                size_t err_len)
{
    if (desc->value_class == VALUE_CLASS_ENUM) {
        for (int i = 0; i < desc->enum_count; i++) {
            if (desc->enum_values[i].value == (uint16_t)user_value) {
                return true;
            }
        }
        snprintf(err_buf, err_len, "Invalid option");
        return false;
    }

    float raw_float = user_value / desc->scale;
    if (desc->has_min && raw_float < desc->min_raw) {
        snprintf(err_buf, err_len, "Min %.0f%s",
                 desc->min_raw * desc->scale,
                 desc->unit);
        return false;
    }
    if (desc->has_max && raw_float > desc->max_raw) {
        snprintf(err_buf, err_len, "Max %.0f%s",
                 desc->max_raw * desc->scale,
                 desc->unit);
        return false;
    }
    return true;
}

static void try_write(register_widget_t *widget, float user_value)
{
    char err[64] = {0};
    if (!validate_user_value(widget->desc, user_value, err, sizeof(err))) {
        set_hint(widget, err, false);
        return;
    }

    esp_err_t ret = tinybms_model_write_register(widget->desc->address, user_value);
    if (ret == ESP_OK) {
        set_hint(widget, "Written", true);
    } else {
        snprintf(err, sizeof(err), "Write failed: %s", esp_err_to_name(ret));
        set_hint(widget, err, false);
    }
}

static void on_text_ready(lv_event_t *e)
{
    register_widget_t *widget = (register_widget_t *)lv_event_get_user_data(e);
    if (!widget || !widget->input) {
        return;
    }

    const char *text = lv_textarea_get_text(widget->input);
    if (text == NULL || text[0] == '\0') {
        return;
    }

    float value = strtof(text, NULL);
    try_write(widget, value);
}

static void on_dropdown_changed(lv_event_t *e)
{
    register_widget_t *widget = (register_widget_t *)lv_event_get_user_data(e);
    if (!widget || !widget->desc || widget->desc->value_class != VALUE_CLASS_ENUM) {
        return;
    }

    uint16_t selected_index = lv_dropdown_get_selected(widget->input);
    if (selected_index < widget->desc->enum_count) {
        uint16_t value = widget->desc->enum_values[selected_index].value;
        try_write(widget, (float)value);
    }
}

static void on_quick_read(lv_event_t *e)
{
    (void)e;
    tinybms_model_read_all();
}

static void on_quick_restart(lv_event_t *e)
{
    (void)e;
    tinybms_restart();
}

static lv_obj_t* create_section_header(lv_obj_t *parent, const char *text)
{
    lv_obj_t *header = lv_label_create(parent);
    lv_label_set_text(header, text);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(header, lv_color_hex(0x00AAFF), 0);
    lv_obj_set_style_pad_top(header, 8, 0);
    lv_obj_set_style_pad_bottom(header, 4, 0);
    return header;
}

static void create_register_row(lv_obj_t *parent,
                                register_widget_t *widget,
                                const register_descriptor_t *desc)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(96));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(row, 6, 0);

    lv_obj_t *title_row = lv_obj_create(row);
    lv_obj_remove_style_all(title_row);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(title_row);
    lv_label_set_text(label, desc->label);

    lv_obj_t *unit = lv_label_create(title_row);
    lv_label_set_text(unit, desc->unit);
    lv_obj_set_style_text_color(unit, lv_color_hex(0xA0A0A0), 0);

    lv_obj_t *input_row = lv_obj_create(row);
    lv_obj_remove_style_all(input_row);
    lv_obj_set_width(input_row, LV_PCT(100));
    lv_obj_set_flex_flow(input_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(input_row, 8, 0);

    if (desc->value_class == VALUE_CLASS_ENUM) {
        widget->input = lv_dropdown_create(input_row);
        lv_obj_set_width(widget->input, 190);
        char options[256] = {0};
        for (int i = 0; i < desc->enum_count; i++) {
            size_t used = strlen(options);
            size_t remaining = sizeof(options) - used;
            if (remaining > 1) {
                strncat(options, desc->enum_values[i].label, remaining - 1);
                used = strlen(options);
                remaining = sizeof(options) - used;
            }
            if (i < desc->enum_count - 1 && remaining > 1) {
                strncat(options, "\n", remaining - 1);
            }
        }
        lv_dropdown_set_options(widget->input, options);
        lv_obj_add_event_cb(widget->input, on_dropdown_changed, LV_EVENT_VALUE_CHANGED, widget);
    } else {
        widget->input = lv_textarea_create(input_row);
        lv_textarea_set_one_line(widget->input, true);
        lv_textarea_set_max_length(widget->input, 16);
        lv_textarea_set_accepted_chars(widget->input, "0123456789.-");
        lv_obj_set_width(widget->input, 160);
        lv_obj_add_event_cb(widget->input, on_text_ready, LV_EVENT_READY, widget);
        lv_obj_add_event_cb(widget->input, on_text_ready, LV_EVENT_DEFOCUSED, widget);
    }

    char hint[96];
    if (desc->has_min || desc->has_max) {
        char min_buf[24] = {0};
        char max_buf[24] = {0};
        if (desc->has_min) {
            snprintf(min_buf, sizeof(min_buf), "min %.0f", desc->min_raw * desc->scale);
        }
        if (desc->has_max) {
            snprintf(max_buf, sizeof(max_buf), "max %.0f", desc->max_raw * desc->scale);
        }
        if (desc->has_min && desc->has_max) {
            snprintf(hint, sizeof(hint), "%s / %s %s", min_buf, max_buf, desc->unit);
        } else if (desc->has_min) {
            snprintf(hint, sizeof(hint), "%s %s", min_buf, desc->unit);
        } else {
            snprintf(hint, sizeof(hint), "%s %s", max_buf, desc->unit);
        }
    } else {
        snprintf(hint, sizeof(hint), "default %u", desc->default_raw);
    }

    widget->hint = lv_label_create(row);
    lv_label_set_text(widget->hint, hint);
    lv_obj_set_style_text_color(widget->hint, lv_color_hex(0x808080), 0);
}

void screen_tinybms_config_create(lv_obj_t *parent)
{
    s_container = lv_obj_create(parent);
    lv_obj_set_size(s_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_container, 10, 0);
    lv_obj_set_scrollbar_mode(s_container, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *title = lv_label_create(s_container);
    lv_label_set_text(title, "TinyBMS Configuration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    lv_obj_t *quick = lv_obj_create(s_container);
    lv_obj_set_width(quick, LV_PCT(95));
    lv_obj_set_flex_flow(quick, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(quick, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(quick, 6, 0);

    lv_obj_t *lbl = lv_label_create(quick);
    lv_label_set_text(lbl, "Shortcuts: read all / restart TinyBMS");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xA0E0FF), 0);

    lv_obj_t *btn_read = lv_btn_create(quick);
    lv_obj_set_size(btn_read, 120, 32);
    lv_obj_add_event_cb(btn_read, on_quick_read, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_read), "Read all");

    lv_obj_t *btn_restart = lv_btn_create(quick);
    lv_obj_set_size(btn_restart, 120, 32);
    lv_obj_set_style_bg_color(btn_restart, lv_color_hex(0xFF5555), 0);
    lv_obj_add_event_cb(btn_restart, on_quick_restart, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_restart), "Restart");

    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    register_group_t current_group = REG_GROUP_MAX;
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        const register_descriptor_t *desc = &catalog[i];
        s_widgets[i].desc = desc;

        if (desc->group != current_group) {
            current_group = desc->group;
            create_section_header(s_container, group_title(current_group));
        }

        create_register_row(s_container, &s_widgets[i], desc);
    }

    ESP_LOGI(TAG, "TinyBMS config screen created with %d registers", TINYBMS_REGISTER_COUNT);
}

static register_widget_t* find_widget_by_address(uint16_t address)
{
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        if (s_widgets[i].desc && s_widgets[i].desc->address == address) {
            return &s_widgets[i];
        }
    }
    return NULL;
}

void screen_tinybms_config_apply_register(const tinybms_register_update_t *update)
{
    if (update == NULL) {
        return;
    }

    register_widget_t *widget = find_widget_by_address(update->address);
    if (widget != NULL) {
        populate_input(widget, update->user_value);
        char hint[48];
        snprintf(hint, sizeof(hint), "Last read %.2f %s",
                 update->user_value, widget->desc->unit);
        lv_label_set_text(widget->hint, hint);
        lv_obj_set_style_text_color(widget->hint, lv_color_hex(0xB0B0B0), 0);
    }
}

void screen_tinybms_config_update(const tinybms_config_t *config)
{
    (void)config;
    if (!s_container) {
        return;
    }

    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        float user_val;
        if (tinybms_model_get_cached(catalog[i].address, &user_val) == ESP_OK) {
            populate_input(&s_widgets[i], user_val);
        }
    }
}

