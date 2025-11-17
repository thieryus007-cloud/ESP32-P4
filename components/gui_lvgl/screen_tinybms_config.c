/**
 * @file screen_tinybms_config.c
 * @brief TinyBMS Configuration Screen (editable register catalog)
 */

#include "screen_tinybms_config.h"
#include "tinybms_model.h"
#include "tinybms_registers.h"
#include "event_types.h"
#include "esp_log.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "scr_tbms_config";

typedef struct {
    const register_descriptor_t *desc;
    lv_obj_t *row;
    lv_obj_t *input;
    lv_obj_t *hint;
    lv_obj_t *status_chip;
    lv_obj_t *label;
} register_widget_t;

static register_widget_t s_widgets[TINYBMS_REGISTER_COUNT];
static lv_obj_t *s_root = NULL;
static lv_obj_t *s_tabview = NULL;
static lv_obj_t *s_search_box = NULL;

typedef struct {
    register_group_t group;
    const char *title;
    lv_obj_t *tab;
    lv_obj_t *list;
} group_container_t;

static group_container_t s_groups[REG_GROUP_MAX];

static void set_hint(register_widget_t *widget, const char *text, bool ok)
{
    if (!widget || !widget->hint) {
        return;
    }

    lv_label_set_text(widget->hint, text);
    lv_obj_set_style_text_color(widget->hint,
                                ok ? lv_color_hex(0x80FF80) : lv_color_hex(0xFF7070),
                                0);

    if (widget->input) {
        lv_color_t border = ok ? lv_color_hex(0x35C759) : lv_color_hex(0xFF5555);
        lv_obj_set_style_border_width(widget->input, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(widget->input, border, LV_PART_MAIN);
    }
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

static void reset_input_style(register_widget_t *widget)
{
    if (widget && widget->input) {
        lv_obj_set_style_border_width(widget->input, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(widget->input, lv_color_hex(0x404040), LV_PART_MAIN);
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

    reset_input_style(widget);
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

    reset_input_style(widget);
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

    widget->label = lv_label_create(title_row);
    lv_label_set_text(widget->label, desc->label);

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

    widget->status_chip = lv_label_create(row);
    lv_label_set_text(widget->status_chip, "Last read: --");
    lv_obj_set_style_text_color(widget->status_chip, lv_color_hex(0xB0B0B0), 0);

    widget->row = row;
}

static const char* group_tab_title(register_group_t group)
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

static void on_tab_read_all(lv_event_t *e)
{
    (void)e;
    tinybms_model_read_all();
}

static void on_tab_restart(lv_event_t *e)
{
    (void)e;
    tinybms_restart();
}

static void on_tab_write_pending(lv_event_t *e)
{
    (void)e;
    // Future: batch write pending values. Placeholder for UI consistency.
}

static lv_obj_t *create_actions_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(bar, 4, 0);
    lv_obj_set_style_pad_bottom(bar, 8, 0);

    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, "Actions: read / write / restart");
    lv_obj_set_style_text_color(label, lv_color_hex(0x7DC8FF), 0);

    lv_obj_t *btn_row = lv_obj_create(bar);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(btn_row, 8, 0);

    lv_obj_t *btn_read = lv_btn_create(btn_row);
    lv_obj_set_size(btn_read, 110, 32);
    lv_label_set_text(lv_label_create(btn_read), "Read all");
    lv_obj_add_event_cb(btn_read, on_tab_read_all, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_write = lv_btn_create(btn_row);
    lv_obj_set_size(btn_write, 120, 32);
    lv_label_set_text(lv_label_create(btn_write), "Write pending");
    lv_obj_add_event_cb(btn_write, on_tab_write_pending, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(btn_write, LV_STATE_DISABLED);

    lv_obj_t *btn_restart = lv_btn_create(btn_row);
    lv_obj_set_size(btn_restart, 110, 32);
    lv_obj_set_style_bg_color(btn_restart, lv_color_hex(0xFF5555), 0);
    lv_label_set_text(lv_label_create(btn_restart), "Restart");
    lv_obj_add_event_cb(btn_restart, on_tab_restart, LV_EVENT_CLICKED, NULL);

    return bar;
}

static void create_group_tab(group_container_t *group)
{
    group->tab = lv_tabview_add_tab(s_tabview, group_tab_title(group->group));
    lv_obj_set_flex_flow(group->tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(group->tab, 8, 0);
    lv_obj_set_flex_align(group->tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_actions_bar(group->tab);

    group->list = lv_obj_create(group->tab);
    lv_obj_set_width(group->list, LV_PCT(100));
    lv_obj_set_height(group->list, LV_PCT(100));
    lv_obj_set_flex_flow(group->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group->list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(group->list, 6, 0);
    lv_obj_set_scrollbar_mode(group->list, LV_SCROLLBAR_MODE_AUTO);
}

static bool contains_case_insensitive(const char *haystack, const char *needle)
{
    if (!haystack || !needle || *needle == '\0') {
        return true;
    }

    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);
    if (n_len > h_len) {
        return false;
    }

    for (size_t i = 0; i <= h_len - n_len; i++) {
        size_t j = 0;
        for (; j < n_len; j++) {
            char h = (char) tolower((unsigned char)haystack[i + j]);
            char n = (char) tolower((unsigned char)needle[j]);
            if (h != n) {
                break;
            }
        }
        if (j == n_len) {
            return true;
        }
    }
    return false;
}

static void apply_search_filter(void)
{
    if (!s_search_box) {
        return;
    }

    const char *filter = lv_textarea_get_text(s_search_box);
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        register_widget_t *w = &s_widgets[i];
        if (!w->row || !w->label) {
            continue;
        }

        bool visible = contains_case_insensitive(lv_label_get_text(w->label), filter);
        if (visible) {
            lv_obj_clear_flag(w->row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(w->row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void on_search_changed(lv_event_t *e)
{
    (void)e;
    apply_search_filter();
}

void screen_tinybms_config_create(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(s_root, 10, 0);
    lv_obj_set_scrollbar_mode(s_root, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "TinyBMS Configuration");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    lv_obj_t *search_row = lv_obj_create(s_root);
    lv_obj_remove_style_all(search_row);
    lv_obj_set_width(search_row, LV_PCT(100));
    lv_obj_set_flex_flow(search_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(search_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(search_row, 8, 0);

    lv_obj_t *search_label = lv_label_create(search_row);
    lv_label_set_text(search_label, "Recherche registre");

    s_search_box = lv_textarea_create(search_row);
    lv_textarea_set_one_line(s_search_box, true);
    lv_textarea_set_placeholder_text(s_search_box, "Nom ou description...");
    lv_obj_set_width(s_search_box, 220);
    lv_obj_add_event_cb(s_search_box, on_search_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *quick = lv_obj_create(s_root);
    lv_obj_set_width(quick, LV_PCT(100));
    lv_obj_set_flex_flow(quick, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(quick, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(quick, 6, 0);

    lv_obj_t *lbl = lv_label_create(quick);
    lv_label_set_text(lbl, "Raccourcis globaux : lecture complète / redémarrage");
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

    s_tabview = lv_tabview_create(s_root, LV_DIR_TOP, 40);
    lv_obj_set_size(s_tabview, LV_PCT(100), LV_PCT(100));

    for (int g = 0; g < REG_GROUP_MAX; g++) {
        s_groups[g].group = (register_group_t)g;
        s_groups[g].title = group_tab_title((register_group_t)g);
        create_group_tab(&s_groups[g]);
    }

    const register_descriptor_t *catalog = tinybms_get_register_catalog();
    for (int i = 0; i < TINYBMS_REGISTER_COUNT; i++) {
        const register_descriptor_t *desc = &catalog[i];
        s_widgets[i].desc = desc;

        group_container_t *group = &s_groups[desc->group];
        create_register_row(group->list, &s_widgets[i], desc);
    }

    apply_search_filter();
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

        if (widget->status_chip) {
            lv_label_set_text(widget->status_chip, hint);
            lv_obj_set_style_text_color(widget->status_chip, lv_color_hex(0x80C080), 0);
        }
    }
}

void screen_tinybms_config_update(const tinybms_config_t *config)
{
    (void)config;
    if (!s_root) {
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

