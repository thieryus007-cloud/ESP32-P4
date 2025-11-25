#include "widget_soc_gauge.h"
#include <stdlib.h>

// Callback animation
static void soc_anim_cb(void *var, int32_t v) {
    widget_soc_gauge_t *gauge = (widget_soc_gauge_t *)var;
    gauge->current_value = v;
    lv_arc_set_value(gauge->arc, v);
    lv_label_set_text_fmt(gauge->label_value, "%d%%", v);

    // Mise à jour couleur selon seuils
    lv_color_t color;
    if (v < 20) {
        color = lv_color_hex(0xE53E3E); // Rouge
    } else if (v < 80) {
        color = lv_color_hex(0xED8936); // Orange
    } else {
        color = lv_color_hex(0x38A169); // Vert
    }
    lv_obj_set_style_arc_color(gauge->arc, color, LV_PART_INDICATOR);
}

widget_soc_gauge_t* widget_soc_gauge_create(
    lv_obj_t *parent,
    const widget_soc_gauge_config_t *config) {

    // Configuration par défaut si NULL
    widget_soc_gauge_config_t cfg = WIDGET_SOC_GAUGE_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_soc_gauge_t *gauge = lv_malloc(sizeof(widget_soc_gauge_t));
    if (gauge == NULL) return NULL;

    // Container principal
    gauge->container = lv_obj_create(parent);
    lv_obj_set_size(gauge->container, cfg.width, cfg.height);
    lv_obj_set_style_bg_opa(gauge->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge->container, 0, 0);
    lv_obj_clear_flag(gauge->container, LV_OBJ_FLAG_SCROLLABLE);

    // Arc de progression
    gauge->arc = lv_arc_create(gauge->container);
    lv_obj_set_size(gauge->arc, cfg.width - 20, cfg.height - 20);
    lv_obj_center(gauge->arc);
    lv_arc_set_rotation(gauge->arc, 135);
    lv_arc_set_bg_angles(gauge->arc, 0, 270);
    lv_arc_set_range(gauge->arc, 0, 100);
    lv_arc_set_value(gauge->arc, 0);
    lv_obj_remove_style(gauge->arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(gauge->arc, LV_OBJ_FLAG_CLICKABLE);

    // Style arc
    lv_obj_set_style_arc_width(gauge->arc, cfg.arc_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge->arc, cfg.color_bg, LV_PART_MAIN);
    lv_obj_set_style_arc_width(gauge->arc, cfg.arc_width, LV_PART_INDICATOR);

    // Label valeur centrale
    gauge->label_value = lv_label_create(gauge->container);
    lv_label_set_text(gauge->label_value, "0%");
    lv_obj_set_style_text_font(gauge->label_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(gauge->label_value, lv_color_white(), 0);
    lv_obj_align(gauge->label_value, LV_ALIGN_CENTER, 0, -10);

    // Label unité
    gauge->label_unit = lv_label_create(gauge->container);
    lv_label_set_text(gauge->label_unit, "SOC");
    lv_obj_set_style_text_font(gauge->label_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(gauge->label_unit, lv_color_hex(0xA0AEC0), 0);
    lv_obj_align(gauge->label_unit, LV_ALIGN_CENTER, 0, 30);

    // Label tendance (optionnel)
    if (cfg.show_trend) {
        gauge->label_trend = lv_label_create(gauge->container);
        lv_label_set_text(gauge->label_trend, LV_SYMBOL_MINUS);
        lv_obj_set_style_text_font(gauge->label_trend, &lv_font_montserrat_20, 0);
        lv_obj_align(gauge->label_trend, LV_ALIGN_CENTER, 0, 55);
    } else {
        gauge->label_trend = NULL;
    }

    gauge->current_value = 0;
    gauge->target_value = 0;

    return gauge;
}

void widget_soc_gauge_set_value(
    widget_soc_gauge_t *gauge,
    int16_t value,
    int8_t trend) {

    if (gauge == NULL) return;
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    gauge->target_value = value;

    // Animation de la valeur
    lv_anim_init(&gauge->anim);
    lv_anim_set_var(&gauge->anim, gauge);
    lv_anim_set_exec_cb(&gauge->anim, soc_anim_cb);
    lv_anim_set_values(&gauge->anim, gauge->current_value, value);
    lv_anim_set_time(&gauge->anim, 500);
    lv_anim_set_path_cb(&gauge->anim, lv_anim_path_ease_out);
    lv_anim_start(&gauge->anim);

    // Mise à jour tendance
    if (gauge->label_trend != NULL) {
        const char *icon;
        lv_color_t color;
        if (trend > 0) {
            icon = LV_SYMBOL_UP;
            color = lv_color_hex(0x38A169); // Vert
        } else if (trend < 0) {
            icon = LV_SYMBOL_DOWN;
            color = lv_color_hex(0xED8936); // Orange
        } else {
            icon = LV_SYMBOL_MINUS;
            color = lv_color_hex(0xA0AEC0); // Gris
        }
        lv_label_set_text(gauge->label_trend, icon);
        lv_obj_set_style_text_color(gauge->label_trend, color, 0);
    }
}

void widget_soc_gauge_destroy(widget_soc_gauge_t *gauge) {
    if (gauge == NULL) return;
    lv_anim_del(gauge, NULL);
    lv_obj_del(gauge->container);
    lv_free(gauge);
}
