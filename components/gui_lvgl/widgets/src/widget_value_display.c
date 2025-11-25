#include "widget_value_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

widget_value_display_t* widget_value_create(
    lv_obj_t *parent,
    const widget_value_config_t *config) {

    // Configuration par défaut si NULL
    widget_value_config_t cfg = WIDGET_VALUE_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_value_display_t *display = lv_malloc(sizeof(widget_value_display_t));
    if (display == NULL) return NULL;

    // Container principal
    display->container = lv_obj_create(parent);
    if (cfg.width > 0) {
        lv_obj_set_width(display->container, cfg.width);
    } else {
        lv_obj_set_width(display->container, LV_SIZE_CONTENT);
    }
    lv_obj_set_height(display->container, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(display->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(display->container, 0, 0);
    lv_obj_set_style_pad_all(display->container, 4, 0);
    lv_obj_clear_flag(display->container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(display->container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(display->container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    // Label titre
    display->label_title = lv_label_create(display->container);
    lv_label_set_text(display->label_title, cfg.title);
    lv_obj_set_style_text_color(display->label_title, cfg.title_color, 0);
    lv_obj_set_style_text_font(display->label_title, &lv_font_montserrat_14, 0);

    // Container pour valeur + unité
    lv_obj_t *value_container = lv_obj_create(display->container);
    lv_obj_set_size(value_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(value_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(value_container, 0, 0);
    lv_obj_set_style_pad_all(value_container, 0, 0);
    lv_obj_clear_flag(value_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(value_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(value_container, 4, 0);

    // Label valeur
    display->label_value = lv_label_create(value_container);
    lv_label_set_text(display->label_value, "--");
    lv_obj_set_style_text_color(display->label_value, cfg.value_color, 0);
    lv_obj_set_style_text_font(display->label_value, cfg.value_font, 0);

    // Label unité
    display->label_unit = lv_label_create(value_container);
    lv_label_set_text(display->label_unit, cfg.unit);
    lv_obj_set_style_text_color(display->label_unit, cfg.title_color, 0);
    lv_obj_set_style_text_font(display->label_unit, &lv_font_montserrat_16, 0);

    // Stocker le format dans user_data
    size_t format_len = strlen(cfg.format) + 1;
    char *format_copy = lv_malloc(format_len);
    if (format_copy) {
        memcpy(format_copy, cfg.format, format_len);
        lv_obj_set_user_data(display->container, format_copy);
    }

    return display;
}

void widget_value_set_float(
    widget_value_display_t *display,
    float value) {

    if (display == NULL) return;

    // Récupérer le format
    const char *format = (const char *)lv_obj_get_user_data(display->container);
    if (format == NULL) {
        format = "%.2f";
    }

    // Créer le buffer de formatage
    char buffer[32];
    snprintf(buffer, sizeof(buffer), format, value);

    lv_label_set_text(display->label_value, buffer);
}

void widget_value_set_int(
    widget_value_display_t *display,
    int32_t value) {

    if (display == NULL) return;

    // Récupérer le format
    const char *format = (const char *)lv_obj_get_user_data(display->container);
    if (format == NULL) {
        format = "%d";
    }

    // Créer le buffer de formatage
    char buffer[32];
    snprintf(buffer, sizeof(buffer), format, value);

    lv_label_set_text(display->label_value, buffer);
}

void widget_value_set_color(
    widget_value_display_t *display,
    lv_color_t color) {

    if (display == NULL) return;
    lv_obj_set_style_text_color(display->label_value, color, 0);
}

void widget_value_destroy(widget_value_display_t *display) {
    if (display == NULL) return;

    // Libérer le format stocké
    void *user_data = lv_obj_get_user_data(display->container);
    if (user_data) {
        lv_free(user_data);
    }

    lv_obj_del(display->container);
    lv_free(display);
}
