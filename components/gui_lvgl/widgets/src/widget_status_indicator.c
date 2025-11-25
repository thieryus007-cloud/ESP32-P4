#include "widget_status_indicator.h"
#include <stdlib.h>
#include <string.h>

static lv_color_t get_state_color(status_state_t state) {
    switch (state) {
        case STATUS_OK:
            return lv_color_hex(0x38A169); // Vert
        case STATUS_WARNING:
            return lv_color_hex(0xED8936); // Orange
        case STATUS_ERROR:
            return lv_color_hex(0xE53E3E); // Rouge
        case STATUS_INACTIVE:
        default:
            return lv_color_hex(0x718096); // Gris
    }
}

widget_status_indicator_t* widget_status_create(
    lv_obj_t *parent,
    const widget_status_config_t *config) {

    // Configuration par défaut si NULL
    widget_status_config_t cfg = WIDGET_STATUS_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_status_indicator_t *indicator = lv_malloc(sizeof(widget_status_indicator_t));
    if (indicator == NULL) return NULL;

    // Container principal
    indicator->container = lv_obj_create(parent);
    lv_obj_set_size(indicator->container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(indicator->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(indicator->container, 0, 0);
    lv_obj_set_style_pad_all(indicator->container, 4, 0);
    lv_obj_clear_flag(indicator->container, LV_OBJ_FLAG_SCROLLABLE);

    // Layout horizontal ou vertical
    if (cfg.horizontal) {
        lv_obj_set_flex_flow(indicator->container, LV_FLEX_FLOW_ROW);
    } else {
        lv_obj_set_flex_flow(indicator->container, LV_FLEX_FLOW_COLUMN);
    }
    lv_obj_set_flex_align(indicator->container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(indicator->container, cfg.spacing, 0);
    lv_obj_set_style_pad_row(indicator->container, cfg.spacing, 0);

    // LED
    indicator->led = lv_led_create(indicator->container);
    lv_obj_set_size(indicator->led, cfg.led_size, cfg.led_size);
    lv_led_off(indicator->led);

    // Label
    indicator->label = lv_label_create(indicator->container);
    lv_label_set_text(indicator->label, cfg.label_text);

    // État initial
    indicator->state = STATUS_INACTIVE;
    widget_status_set_state(indicator, STATUS_INACTIVE);

    return indicator;
}

void widget_status_set_state(
    widget_status_indicator_t *indicator,
    status_state_t state) {

    if (indicator == NULL) return;

    indicator->state = state;
    lv_color_t color = get_state_color(state);

    // Mettre à jour la LED
    lv_led_set_color(indicator->led, color);
    if (state == STATUS_INACTIVE) {
        lv_led_off(indicator->led);
    } else {
        lv_led_on(indicator->led);
    }
}

void widget_status_set_label(
    widget_status_indicator_t *indicator,
    const char *text) {

    if (indicator == NULL || text == NULL) return;
    lv_label_set_text(indicator->label, text);
}

void widget_status_destroy(widget_status_indicator_t *indicator) {
    if (indicator == NULL) return;
    lv_obj_del(indicator->container);
    lv_free(indicator);
}
