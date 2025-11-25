#include "widget_cell_bar.h"
#include <stdlib.h>

widget_cell_bar_t* widget_cell_bar_create(
    lv_obj_t *parent,
    uint8_t cell_index,
    const widget_cell_bar_config_t *config) {

    // Configuration par défaut si NULL
    widget_cell_bar_config_t cfg = WIDGET_CELL_BAR_DEFAULT_CONFIG;
    if (config != NULL) {
        cfg = *config;
    }

    // Allocation mémoire
    widget_cell_bar_t *cell_bar = lv_malloc(sizeof(widget_cell_bar_t));
    if (cell_bar == NULL) return NULL;

    // Container principal
    cell_bar->container = lv_obj_create(parent);
    lv_obj_set_size(cell_bar->container, cfg.bar_width + 60, cfg.bar_height + 10);
    lv_obj_set_style_bg_opa(cell_bar->container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cell_bar->container, 0, 0);
    lv_obj_set_style_pad_all(cell_bar->container, 2, 0);
    lv_obj_clear_flag(cell_bar->container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(cell_bar->container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cell_bar->container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cell_bar->container, 8, 0);

    // Label index (ex: "C1")
    cell_bar->label_index = lv_label_create(cell_bar->container);
    lv_label_set_text_fmt(cell_bar->label_index, "C%d", cell_index);
    lv_obj_set_width(cell_bar->label_index, 30);

    // Barre de progression
    cell_bar->bar = lv_bar_create(cell_bar->container);
    lv_obj_set_size(cell_bar->bar, cfg.bar_width, cfg.bar_height);
    lv_bar_set_range(cell_bar->bar, cfg.min_voltage, cfg.max_voltage);
    lv_bar_set_value(cell_bar->bar, cfg.min_voltage, LV_ANIM_OFF);

    // Style de la barre
    lv_obj_set_style_bg_color(cell_bar->bar, lv_color_hex(0x2D3748), LV_PART_MAIN);
    lv_obj_set_style_bg_color(cell_bar->bar, lv_color_hex(0x38A169), LV_PART_INDICATOR);

    // Label valeur
    cell_bar->label_value = lv_label_create(cell_bar->container);
    lv_label_set_text(cell_bar->label_value, "-.--- V");
    lv_obj_set_style_text_font(cell_bar->label_value, &lv_font_montserrat_14, 0);

    // Icône d'équilibrage (initialement cachée)
    cell_bar->balancing_icon = lv_label_create(cell_bar->container);
    lv_label_set_text(cell_bar->balancing_icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_color(cell_bar->balancing_icon, lv_color_hex(0xED8936), 0);
    lv_obj_add_flag(cell_bar->balancing_icon, LV_OBJ_FLAG_HIDDEN);

    // Initialisation des valeurs
    cell_bar->cell_index = cell_index;
    cell_bar->voltage_mv = 0;
    cell_bar->is_balancing = false;

    // Stocker la config dans user_data pour utilisation ultérieure
    lv_obj_set_user_data(cell_bar->container, lv_malloc(sizeof(widget_cell_bar_config_t)));
    if (lv_obj_get_user_data(cell_bar->container)) {
        memcpy(lv_obj_get_user_data(cell_bar->container), &cfg, sizeof(widget_cell_bar_config_t));
    }

    return cell_bar;
}

void widget_cell_bar_set_voltage(
    widget_cell_bar_t *bar,
    uint16_t voltage_mv,
    bool is_balancing) {

    if (bar == NULL) return;

    bar->voltage_mv = voltage_mv;
    bar->is_balancing = is_balancing;

    // Récupérer la config depuis user_data
    widget_cell_bar_config_t *cfg = (widget_cell_bar_config_t *)lv_obj_get_user_data(bar->container);
    if (cfg == NULL) return;

    // Mettre à jour la barre
    lv_bar_set_value(bar->bar, voltage_mv, LV_ANIM_ON);

    // Mettre à jour le label valeur
    lv_label_set_text_fmt(bar->label_value, "%.3f V", voltage_mv / 1000.0f);

    // Mettre à jour la couleur selon les seuils
    lv_color_t color;
    if (voltage_mv < cfg->low_threshold) {
        color = lv_color_hex(0xE53E3E); // Rouge - Tension basse
    } else if (voltage_mv > cfg->high_threshold) {
        color = lv_color_hex(0xED8936); // Orange - Tension haute
    } else {
        color = lv_color_hex(0x38A169); // Vert - Normal
    }
    lv_obj_set_style_bg_color(bar->bar, color, LV_PART_INDICATOR);

    // Afficher/masquer l'icône d'équilibrage
    if (is_balancing) {
        lv_obj_clear_flag(bar->balancing_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar->balancing_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void widget_cell_bar_destroy(widget_cell_bar_t *bar) {
    if (bar == NULL) return;

    // Libérer la config stockée
    void *user_data = lv_obj_get_user_data(bar->container);
    if (user_data) {
        lv_free(user_data);
    }

    lv_obj_del(bar->container);
    lv_free(bar);
}
