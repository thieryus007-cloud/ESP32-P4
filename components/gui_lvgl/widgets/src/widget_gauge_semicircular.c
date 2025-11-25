#include "widget_gauge_semicircular.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Structure pour passer les données à l'animation
typedef struct {
    widget_gauge_semicircular_t *gauge;
    uint8_t needle_index;
} needle_anim_data_t;

// Callback d'animation pour une aiguille
static void needle_anim_cb(void *var, int32_t v) {
    needle_anim_data_t *data = (needle_anim_data_t *)var;
    widget_gauge_semicircular_t *gauge = data->gauge;
    uint8_t idx = data->needle_index;

    if (idx >= gauge->needle_count) return;

    // Calculer l'angle de l'aiguille (180° arc, de gauche à droite)
    // v est entre 0 et 1000
    float angle_deg = (v / 1000.0f) * 180.0f;  // 0-180°
    float angle_rad = (angle_deg + 180.0f) * M_PI / 180.0f; // Convertir pour starts à gauche

    // Calculer la position de l'aiguille
    widget_gauge_semicircular_config_t *cfg =
        (widget_gauge_semicircular_config_t *)lv_obj_get_user_data(gauge->container);
    if (!cfg) return;

    lv_coord_t center_x = cfg->width / 2;
    lv_coord_t center_y = cfg->height;
    lv_coord_t needle_len = (cfg->height - 20); // Longueur de l'aiguille

    // Points de la ligne
    static lv_point_t line_points[MAX_NEEDLES][2];
    line_points[idx][0].x = center_x;
    line_points[idx][0].y = center_y;
    line_points[idx][1].x = center_x + (lv_coord_t)(needle_len * cos(angle_rad));
    line_points[idx][1].y = center_y + (lv_coord_t)(needle_len * sin(angle_rad));

    lv_line_set_points(gauge->needles[idx], line_points[idx], 2);
}

widget_gauge_semicircular_t* widget_gauge_semicircular_create(
    lv_obj_t *parent,
    const widget_gauge_semicircular_config_t *config) {

    // Configuration par défaut si NULL
    widget_gauge_semicircular_config_t cfg = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_gauge_semicircular_t *gauge = lv_malloc(sizeof(widget_gauge_semicircular_t));
    if (gauge == NULL) return NULL;

    memset(gauge, 0, sizeof(widget_gauge_semicircular_t));
    gauge->min_value = cfg.min_value;
    gauge->max_value = cfg.max_value;
    gauge->needle_count = 0;

    // Container principal
    gauge->container = lv_obj_create(parent);
    lv_obj_set_size(gauge->container, cfg.width, cfg.height + 50); // +50 pour titre et labels
    lv_obj_set_style_bg_opa(gauge->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauge->container, 0, 0);
    lv_obj_set_style_pad_all(gauge->container, 0, 0);
    lv_obj_clear_flag(gauge->container, LV_OBJ_FLAG_SCROLLABLE);

    // Titre (optionnel)
    if (cfg.title != NULL) {
        gauge->label_title = lv_label_create(gauge->container);
        lv_label_set_text(gauge->label_title, cfg.title);
        lv_obj_set_style_text_color(gauge->label_title, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_text_font(gauge->label_title, &lv_font_montserrat_16, 0);
        lv_obj_align(gauge->label_title, LV_ALIGN_TOP_MID, 0, 0);
    } else {
        gauge->label_title = NULL;
    }

    // Arc semi-circulaire (fond)
    gauge->arc = lv_arc_create(gauge->container);
    lv_obj_set_size(gauge->arc, cfg.width - 20, (cfg.height - 10) * 2); // *2 pour le cercle complet
    if (cfg.title != NULL) {
        lv_obj_align(gauge->arc, LV_ALIGN_TOP_MID, 0, 25);
    } else {
        lv_obj_align(gauge->arc, LV_ALIGN_TOP_MID, 0, 5);
    }

    // Configurer pour montrer seulement la moitié supérieure
    lv_arc_set_bg_angles(gauge->arc, 180, 360);  // Demi-cercle supérieur
    lv_arc_set_range(gauge->arc, 0, 100);
    lv_arc_set_value(gauge->arc, 100);  // Remplir l'arc de fond
    lv_obj_remove_style(gauge->arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(gauge->arc, LV_OBJ_FLAG_CLICKABLE);

    // Style de l'arc
    lv_obj_set_style_arc_width(gauge->arc, cfg.arc_width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge->arc, lv_color_hex(0x2D3748), LV_PART_MAIN);

    if (cfg.show_gradient) {
        lv_obj_set_style_arc_width(gauge->arc, cfg.arc_width, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(gauge->arc, cfg.arc_color_start, LV_PART_INDICATOR);
        // Note: LVGL ne supporte pas les dégradés natifs sur les arcs
        // On utilise une couleur unique ou on crée plusieurs arcs superposés
    } else {
        lv_obj_set_style_arc_width(gauge->arc, 0, LV_PART_INDICATOR);
    }

    // Initialiser les aiguilles à NULL
    for (int i = 0; i < MAX_NEEDLES; i++) {
        gauge->needles[i] = NULL;
        gauge->needle_labels[i] = NULL;
    }

    // Stocker la config dans user_data
    widget_gauge_semicircular_config_t *cfg_copy = lv_malloc(sizeof(widget_gauge_semicircular_config_t));
    if (cfg_copy) {
        *cfg_copy = cfg;
        lv_obj_set_user_data(gauge->container, cfg_copy);
    }

    return gauge;
}

int widget_gauge_semicircular_add_needle(
    widget_gauge_semicircular_t *gauge,
    const char *name,
    lv_color_t color,
    float initial_value) {

    if (gauge == NULL || gauge->needle_count >= MAX_NEEDLES) {
        return -1;
    }

    uint8_t idx = gauge->needle_count;

    // Récupérer la config
    widget_gauge_semicircular_config_t *cfg =
        (widget_gauge_semicircular_config_t *)lv_obj_get_user_data(gauge->container);
    if (!cfg) return -1;

    // Créer l'aiguille (ligne)
    gauge->needles[idx] = lv_line_create(gauge->container);

    // Points initiaux (horizontal gauche)
    static lv_point_t line_points[MAX_NEEDLES][2];
    lv_coord_t center_x = cfg->width / 2;
    lv_coord_t center_y = cfg->height;
    if (cfg->title != NULL) {
        center_y += 25;
    } else {
        center_y += 5;
    }

    line_points[idx][0].x = center_x;
    line_points[idx][0].y = center_y;
    line_points[idx][1].x = center_x - (cfg->height - 20);
    line_points[idx][1].y = center_y;

    lv_line_set_points(gauge->needles[idx], line_points[idx], 2);
    lv_obj_align(gauge->needles[idx], LV_ALIGN_TOP_LEFT, 0, 0);

    // Style de l'aiguille
    lv_obj_set_style_line_width(gauge->needles[idx], 3, 0);
    lv_obj_set_style_line_color(gauge->needles[idx], color, 0);
    lv_obj_set_style_line_rounded(gauge->needles[idx], true, 0);

    // Créer le label pour l'aiguille
    gauge->needle_labels[idx] = lv_label_create(gauge->container);
    char label_text[32];
    snprintf(label_text, sizeof(label_text), "%s %.0f%s", name, initial_value, cfg->unit);
    lv_label_set_text(gauge->needle_labels[idx], label_text);
    lv_obj_set_style_text_color(gauge->needle_labels[idx], color, 0);
    lv_obj_set_style_text_font(gauge->needle_labels[idx], &lv_font_montserrat_14, 0);

    // Positionner le label en bas
    lv_coord_t label_x = 10 + (idx * 80);
    lv_obj_align(gauge->needle_labels[idx], LV_ALIGN_BOTTOM_LEFT, label_x, 0);

    // Stocker les données
    gauge->needle_data[idx].color = color;
    gauge->needle_data[idx].value = initial_value;
    gauge->needle_data[idx].name = name;
    gauge->needle_data[idx].visible = true;

    gauge->needle_count++;

    // Mettre à jour la valeur (pour positionner l'aiguille)
    widget_gauge_semicircular_set_needle_value(gauge, idx, initial_value);

    return idx;
}

void widget_gauge_semicircular_set_needle_value(
    widget_gauge_semicircular_t *gauge,
    uint8_t needle_index,
    float value) {

    if (gauge == NULL || needle_index >= gauge->needle_count) {
        return;
    }

    // Limiter la valeur
    if (value < gauge->min_value) value = gauge->min_value;
    if (value > gauge->max_value) value = gauge->max_value;

    // Récupérer la config
    widget_gauge_semicircular_config_t *cfg =
        (widget_gauge_semicircular_config_t *)lv_obj_get_user_data(gauge->container);
    if (!cfg) return;

    // Mettre à jour le label
    char label_text[32];
    snprintf(label_text, sizeof(label_text), "%s %.0f%s",
             gauge->needle_data[needle_index].name, value, cfg->unit);
    lv_label_set_text(gauge->needle_labels[needle_index], label_text);

    // Calculer l'angle normalisé (0-1000)
    float normalized = (value - gauge->min_value) / (gauge->max_value - gauge->min_value);
    int32_t angle_value = (int32_t)(normalized * 1000);

    if (cfg->animate) {
        // Animation
        float old_normalized = (gauge->needle_data[needle_index].value - gauge->min_value) /
                              (gauge->max_value - gauge->min_value);
        int32_t old_angle = (int32_t)(old_normalized * 1000);

        // Données pour le callback
        static needle_anim_data_t anim_data[MAX_NEEDLES];
        anim_data[needle_index].gauge = gauge;
        anim_data[needle_index].needle_index = needle_index;

        lv_anim_init(&gauge->anims[needle_index]);
        lv_anim_set_var(&gauge->anims[needle_index], &anim_data[needle_index]);
        lv_anim_set_exec_cb(&gauge->anims[needle_index], needle_anim_cb);
        lv_anim_set_values(&gauge->anims[needle_index], old_angle, angle_value);
        lv_anim_set_time(&gauge->anims[needle_index], cfg->anim_duration);
        lv_anim_set_path_cb(&gauge->anims[needle_index], lv_anim_path_ease_out);
        lv_anim_start(&gauge->anims[needle_index]);
    } else {
        // Mise à jour directe
        static needle_anim_data_t anim_data[MAX_NEEDLES];
        anim_data[needle_index].gauge = gauge;
        anim_data[needle_index].needle_index = needle_index;
        needle_anim_cb(&anim_data[needle_index], angle_value);
    }

    gauge->needle_data[needle_index].value = value;
}

void widget_gauge_semicircular_set_title(
    widget_gauge_semicircular_t *gauge,
    const char *title) {

    if (gauge == NULL) return;

    if (gauge->label_title == NULL && title != NULL) {
        gauge->label_title = lv_label_create(gauge->container);
        lv_obj_set_style_text_color(gauge->label_title, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_text_font(gauge->label_title, &lv_font_montserrat_16, 0);
        lv_obj_align(gauge->label_title, LV_ALIGN_TOP_MID, 0, 0);
    }

    if (gauge->label_title != NULL) {
        lv_label_set_text(gauge->label_title, title);
    }
}

void widget_gauge_semicircular_destroy(widget_gauge_semicircular_t *gauge) {
    if (gauge == NULL) return;

    // Arrêter toutes les animations
    for (int i = 0; i < MAX_NEEDLES; i++) {
        lv_anim_del(&gauge->anims[i], NULL);
    }

    // Libérer la config
    void *user_data = lv_obj_get_user_data(gauge->container);
    if (user_data) {
        lv_free(user_data);
    }

    // Supprimer le container
    lv_obj_del(gauge->container);

    // Libérer la structure
    lv_free(gauge);
}
