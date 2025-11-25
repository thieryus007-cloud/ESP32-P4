#include "widget_gauge_circular.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Callback d'animation pour l'aiguille
static void needle_anim_cb(void *var, int32_t v) {
    widget_gauge_circular_t *gauge = (widget_gauge_circular_t *)var;

    // Calculer l'angle de l'aiguille (0° = haut, sens horaire)
    // v est entre 0 et 1000 (pour précision)
    float angle_deg = (v / 1000.0f) * 360.0f - 90.0f; // -90 pour commencer à gauche

    // Mettre à jour la rotation de l'aiguille
    lv_obj_set_style_transform_angle(gauge->needle, (int16_t)(angle_deg * 10), 0);
}

widget_gauge_circular_t* widget_gauge_circular_create(
    lv_obj_t *parent,
    const widget_gauge_circular_config_t *config) {

    // Configuration par défaut si NULL
    widget_gauge_circular_config_t cfg = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_gauge_circular_t *gauge = lv_malloc(sizeof(widget_gauge_circular_t));
    if (gauge == NULL) return NULL;

    gauge->min_value = cfg.min_value;
    gauge->max_value = cfg.max_value;
    gauge->current_value = cfg.min_value;

    // Container principal
    gauge->container = lv_obj_create(parent);
    lv_obj_set_size(gauge->container, cfg.size, cfg.size + 40); // +40 pour le titre
    lv_obj_set_style_bg_opa(gauge->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge->container, 0, 0);
    lv_obj_set_style_pad_all(gauge->container, 0, 0);
    lv_obj_clear_flag(gauge->container, LV_OBJ_FLAG_SCROLLABLE);

    // Titre (optionnel)
    if (cfg.title != NULL) {
        gauge->label_title = lv_label_create(gauge->container);
        lv_label_set_text(gauge->label_title, cfg.title);
        lv_obj_set_style_text_color(gauge->label_title, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_text_font(gauge->label_title, &lv_font_montserrat_14, 0);
        lv_obj_align(gauge->label_title, LV_ALIGN_TOP_MID, 0, 0);
    } else {
        gauge->label_title = NULL;
    }

    // Arc de fond (échelle)
    gauge->arc_bg = lv_arc_create(gauge->container);
    lv_obj_set_size(gauge->arc_bg, cfg.size, cfg.size);
    if (cfg.title != NULL) {
        lv_obj_align(gauge->arc_bg, LV_ALIGN_CENTER, 0, 20);
    } else {
        lv_obj_center(gauge->arc_bg);
    }
    lv_arc_set_bg_angles(gauge->arc_bg, 0, 360);
    lv_arc_set_range(gauge->arc_bg, 0, 100);
    lv_arc_set_value(gauge->arc_bg, 0);
    lv_obj_remove_style(gauge->arc_bg, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(gauge->arc_bg, LV_OBJ_FLAG_CLICKABLE);

    // Style de l'arc
    lv_obj_set_style_arc_width(gauge->arc_bg, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge->arc_bg, cfg.scale_color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(gauge->arc_bg, 0, LV_PART_INDICATOR);

    // Créer l'aiguille comme une ligne
    gauge->needle = lv_line_create(gauge->container);

    // Points de la ligne (du centre vers le haut initialement)
    static lv_point_t line_points[2];
    lv_coord_t needle_len = (cfg.size / 2) * cfg.needle_length / 100;
    line_points[0].x = cfg.size / 2;
    line_points[0].y = cfg.size / 2;
    line_points[1].x = cfg.size / 2;
    line_points[1].y = (cfg.size / 2) - needle_len;

    lv_line_set_points(gauge->needle, line_points, 2);
    if (cfg.title != NULL) {
        lv_obj_align(gauge->needle, LV_ALIGN_CENTER, 0, 20);
    } else {
        lv_obj_center(gauge->needle);
    }

    // Style de l'aiguille
    lv_obj_set_style_line_width(gauge->needle, cfg.needle_width, 0);
    lv_obj_set_style_line_color(gauge->needle, cfg.needle_color, 0);
    lv_obj_set_style_line_rounded(gauge->needle, true, 0);

    // Activer la transformation pour la rotation
    lv_obj_set_style_transform_pivot_x(gauge->needle, cfg.size / 2, 0);
    lv_obj_set_style_transform_pivot_y(gauge->needle, cfg.size / 2, 0);

    // Point central
    gauge->center_dot = lv_obj_create(gauge->container);
    lv_obj_set_size(gauge->center_dot, 10, 10);
    if (cfg.title != NULL) {
        lv_obj_align(gauge->center_dot, LV_ALIGN_CENTER, 0, 20);
    } else {
        lv_obj_center(gauge->center_dot);
    }
    lv_obj_set_style_radius(gauge->center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gauge->center_dot, cfg.needle_color, 0);
    lv_obj_set_style_border_width(gauge->center_dot, 0, 0);

    // Label valeur centrale
    gauge->label_value = lv_label_create(gauge->container);
    lv_label_set_text(gauge->label_value, "0");
    lv_obj_set_style_text_font(gauge->label_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(gauge->label_value, lv_color_white(), 0);
    if (cfg.title != NULL) {
        lv_obj_align(gauge->label_value, LV_ALIGN_CENTER, 0, 10);
    } else {
        lv_obj_align(gauge->label_value, LV_ALIGN_CENTER, 0, -10);
    }

    // Label unité
    gauge->label_unit = lv_label_create(gauge->container);
    lv_label_set_text(gauge->label_unit, cfg.unit);
    lv_obj_set_style_text_font(gauge->label_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(gauge->label_unit, lv_color_hex(0xA0AEC0), 0);
    if (cfg.title != NULL) {
        lv_obj_align(gauge->label_unit, LV_ALIGN_CENTER, 0, 40);
    } else {
        lv_obj_align(gauge->label_unit, LV_ALIGN_CENTER, 0, 20);
    }

    // Stocker la config dans user_data
    widget_gauge_circular_config_t *cfg_copy = lv_malloc(sizeof(widget_gauge_circular_config_t));
    if (cfg_copy) {
        *cfg_copy = cfg;
        lv_obj_set_user_data(gauge->container, cfg_copy);
    }

    return gauge;
}

void widget_gauge_circular_set_value(
    widget_gauge_circular_t *gauge,
    float value) {

    if (gauge == NULL) return;

    // Limiter la valeur aux bornes
    if (value < gauge->min_value) value = gauge->min_value;
    if (value > gauge->max_value) value = gauge->max_value;

    // Récupérer la config
    widget_gauge_circular_config_t *cfg =
        (widget_gauge_circular_config_t *)lv_obj_get_user_data(gauge->container);

    // Mettre à jour le label
    char buf[32];
    if (cfg && cfg->format) {
        snprintf(buf, sizeof(buf), cfg->format, value);
    } else {
        snprintf(buf, sizeof(buf), "%.0f", value);
    }
    lv_label_set_text(gauge->label_value, buf);

    // Calculer l'angle (normaliser entre 0 et 1000)
    float normalized = (value - gauge->min_value) / (gauge->max_value - gauge->min_value);
    int32_t angle_value = (int32_t)(normalized * 1000);

    if (cfg && cfg->animate) {
        // Animation de l'aiguille
        int32_t current_angle = (int32_t)((gauge->current_value - gauge->min_value) /
                                          (gauge->max_value - gauge->min_value) * 1000);

        lv_anim_init(&gauge->anim);
        lv_anim_set_var(&gauge->anim, gauge);
        lv_anim_set_exec_cb(&gauge->anim, needle_anim_cb);
        lv_anim_set_values(&gauge->anim, current_angle, angle_value);
        lv_anim_set_time(&gauge->anim, cfg->anim_duration);
        lv_anim_set_path_cb(&gauge->anim, lv_anim_path_ease_out);
        lv_anim_start(&gauge->anim);
    } else {
        // Pas d'animation, mise à jour directe
        needle_anim_cb(gauge, angle_value);
    }

    gauge->current_value = value;
}

void widget_gauge_circular_set_title(
    widget_gauge_circular_t *gauge,
    const char *title) {

    if (gauge == NULL) return;

    if (gauge->label_title == NULL && title != NULL) {
        // Créer le label si nécessaire
        gauge->label_title = lv_label_create(gauge->container);
        lv_obj_set_style_text_color(gauge->label_title, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_text_font(gauge->label_title, &lv_font_montserrat_14, 0);
        lv_obj_align(gauge->label_title, LV_ALIGN_TOP_MID, 0, 0);
    }

    if (gauge->label_title != NULL) {
        lv_label_set_text(gauge->label_title, title);
    }
}

void widget_gauge_circular_destroy(widget_gauge_circular_t *gauge) {
    if (gauge == NULL) return;

    // Arrêter les animations
    lv_anim_del(gauge, NULL);

    // Libérer la config
    void *user_data = lv_obj_get_user_data(gauge->container);
    if (user_data) {
        lv_free(user_data);
    }

    // Supprimer le container (supprime tous les enfants)
    lv_obj_del(gauge->container);

    // Libérer la structure
    lv_free(gauge);
}
