/**
 * @file dashboard_gauges_example.c
 * @brief Exemple de tableau de bord BMS avec jauges circulaires et semi-circulaires
 *
 * Cet exemple reproduit l'interface dashboard montrée dans l'image de référence avec:
 * - BATTERY STATUS: Jauge SOC et SOH
 * - BATTERY MONITOR: Voltage, Current, Power
 * - TEMPERATURES: Multi-capteurs (S1, S2, Int)
 */

#include "widget_gauge_circular.h"
#include "widget_gauge_semicircular.h"
#include "lvgl.h"

// Widgets globaux pour le dashboard
static struct {
    // Section BATTERY STATUS
    widget_gauge_semicircular_t *gauge_soc;

    // Section BATTERY MONITOR
    widget_gauge_circular_t *gauge_voltage;
    widget_gauge_circular_t *gauge_current;
    lv_obj_t *label_power;

    // Section TEMPERATURES
    widget_gauge_semicircular_t *gauge_temps;
    int needle_s2;  // Index aiguille S2
    int needle_s1;  // Index aiguille S1
    int needle_int; // Index aiguille Int
} dashboard;

/**
 * Crée la section "BATTERY STATUS"
 * Affiche SOC (State of Charge) et SOH (State of Health)
 */
static void create_battery_status_section(lv_obj_t *parent) {
    // Container pour la section
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_set_size(section, 300, 280);
    lv_obj_set_style_bg_color(section, lv_color_hex(0x1A202C), 0);
    lv_obj_set_style_border_width(section, 1, 0);
    lv_obj_set_style_border_color(section, lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_radius(section, 10, 0);

    // Titre de la section
    lv_obj_t *title = lv_label_create(section);
    lv_label_set_text(title, "BATTERY STATUS");
    lv_obj_set_style_text_color(title, lv_color_hex(0xA0AEC0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    // Créer la jauge SOC/SOH
    widget_gauge_semicircular_config_t config = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
    config.width = 260;
    config.height = 150;
    config.min_value = 20;  // Commence à 20% comme dans l'image
    config.max_value = 100;
    config.unit = "%";
    config.arc_color_start = lv_color_hex(0x4299E1);  // Bleu
    config.arc_color_end = lv_color_hex(0x38A169);    // Vert
    config.show_gradient = true;

    dashboard.gauge_soc = widget_gauge_semicircular_create(section, &config);
    lv_obj_align(dashboard.gauge_soc->container, LV_ALIGN_CENTER, 0, 10);

    // Ajouter l'aiguille SOC (verte)
    widget_gauge_semicircular_add_needle(dashboard.gauge_soc, "SOC",
                                         lv_color_hex(0x38A169), 80);

    // Ajouter l'aiguille SOH (cyan)
    widget_gauge_semicircular_add_needle(dashboard.gauge_soc, "SOH",
                                         lv_color_hex(0x4299E1), 95);
}

/**
 * Crée la section "BATTERY MONITOR"
 * Affiche Voltage, Current et Power
 */
static void create_battery_monitor_section(lv_obj_t *parent) {
    // Container pour la section
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_set_size(section, 480, 280);
    lv_obj_set_style_bg_color(section, lv_color_hex(0x1A202C), 0);
    lv_obj_set_style_border_width(section, 1, 0);
    lv_obj_set_style_border_color(section, lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_radius(section, 10, 0);

    // Titre de la section
    lv_obj_t *title = lv_label_create(section);
    lv_label_set_text(title, "BATTERY MONITOR");
    lv_obj_set_style_text_color(title, lv_color_hex(0xA0AEC0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    // Container pour les deux jauges
    lv_obj_t *gauges_cont = lv_obj_create(section);
    lv_obj_set_size(gauges_cont, 460, 210);
    lv_obj_align(gauges_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(gauges_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gauges_cont, 0, 0);
    lv_obj_set_style_pad_all(gauges_cont, 0, 0);
    lv_obj_set_flex_flow(gauges_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gauges_cont, LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Jauge VOLTAGE (gauche)
    widget_gauge_circular_config_t voltage_config = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
    voltage_config.size = 180;
    voltage_config.min_value = -5000;  // -5k comme dans l'image
    voltage_config.max_value = 5000;   // 5k
    voltage_config.title = NULL;  // Pas de titre sur la jauge elle-même
    voltage_config.unit = "V";
    voltage_config.format = "%.0f";
    voltage_config.needle_color = lv_color_hex(0x4299E1);  // Bleu
    voltage_config.needle_length = 60;

    dashboard.gauge_voltage = widget_gauge_circular_create(gauges_cont, &voltage_config);
    widget_gauge_circular_set_value(dashboard.gauge_voltage, 50);  // 50V

    // Jauge CURRENT (droite)
    widget_gauge_circular_config_t current_config = WIDGET_GAUGE_CIRCULAR_DEFAULT_CONFIG;
    current_config.size = 180;
    current_config.min_value = -120;
    current_config.max_value = 120;
    current_config.title = NULL;
    current_config.unit = "A";
    current_config.format = "%.0f";
    current_config.needle_color = lv_color_hex(0x38A169);  // Vert
    current_config.needle_length = 60;

    dashboard.gauge_current = widget_gauge_circular_create(gauges_cont, &current_config);
    widget_gauge_circular_set_value(dashboard.gauge_current, 0);  // 0A

    // Label POWER au centre
    dashboard.label_power = lv_label_create(section);
    lv_label_set_text(dashboard.label_power, "0 W");
    lv_obj_set_style_text_font(dashboard.label_power, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(dashboard.label_power, lv_color_white(), 0);
    lv_obj_align(dashboard.label_power, LV_ALIGN_CENTER, 0, 20);
}

/**
 * Crée la section "TEMPERATURES"
 * Affiche 3 capteurs: S2, S1, Int
 */
static void create_temperatures_section(lv_obj_t *parent) {
    // Container pour la section
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_set_size(section, 300, 280);
    lv_obj_set_style_bg_color(section, lv_color_hex(0x1A202C), 0);
    lv_obj_set_style_border_width(section, 1, 0);
    lv_obj_set_style_border_color(section, lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_radius(section, 10, 0);

    // Titre de la section
    lv_obj_t *title = lv_label_create(section);
    lv_label_set_text(title, "TEMPERATURES");
    lv_obj_set_style_text_color(title, lv_color_hex(0xA0AEC0), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    // Créer la jauge de températures
    widget_gauge_semicircular_config_t config = WIDGET_GAUGE_SEMICIRCULAR_DEFAULT_CONFIG;
    config.width = 260;
    config.height = 150;
    config.min_value = 0;
    config.max_value = 70;  // 0-70°C comme dans l'image
    config.unit = "°C";
    config.arc_color_start = lv_color_hex(0x4299E1);  // Bleu (froid)
    config.arc_color_end = lv_color_hex(0xED8936);    // Orange (chaud)
    config.show_gradient = true;

    dashboard.gauge_temps = widget_gauge_semicircular_create(section, &config);
    lv_obj_align(dashboard.gauge_temps->container, LV_ALIGN_CENTER, 0, 10);

    // Ajouter les 3 aiguilles pour les capteurs
    dashboard.needle_s2 = widget_gauge_semicircular_add_needle(
        dashboard.gauge_temps, "S2", lv_color_hex(0x00D9FF), 25);  // Cyan

    dashboard.needle_s1 = widget_gauge_semicircular_add_needle(
        dashboard.gauge_temps, "S1", lv_color_hex(0xFF1493), 40);  // Magenta

    dashboard.needle_int = widget_gauge_semicircular_add_needle(
        dashboard.gauge_temps, "Int", lv_color_hex(0xFFA500), 55);  // Orange
}

/**
 * Crée le dashboard complet avec les 3 sections
 */
void dashboard_gauges_create(lv_obj_t *parent) {
    // Container principal du dashboard
    lv_obj_t *dashboard_cont = lv_obj_create(parent);
    lv_obj_set_size(dashboard_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dashboard_cont, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_border_width(dashboard_cont, 0, 0);
    lv_obj_set_style_pad_all(dashboard_cont, 10, 0);
    lv_obj_set_flex_flow(dashboard_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dashboard_cont, LV_FLEX_ALIGN_SPACE_AROUND,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Créer les 3 sections
    create_battery_status_section(dashboard_cont);
    create_battery_monitor_section(dashboard_cont);
    create_temperatures_section(dashboard_cont);
}

/**
 * Met à jour les valeurs du dashboard
 * À appeler depuis vos événements de mise à jour des données BMS
 */
void dashboard_gauges_update(float soc, float soh,
                             float voltage, float current,
                             float temp_s2, float temp_s1, float temp_int) {
    // Mettre à jour SOC et SOH
    if (dashboard.gauge_soc) {
        widget_gauge_semicircular_set_needle_value(dashboard.gauge_soc, 0, soc);
        widget_gauge_semicircular_set_needle_value(dashboard.gauge_soc, 1, soh);
    }

    // Mettre à jour voltage et current
    if (dashboard.gauge_voltage) {
        widget_gauge_circular_set_value(dashboard.gauge_voltage, voltage);
    }
    if (dashboard.gauge_current) {
        widget_gauge_circular_set_value(dashboard.gauge_current, current);
    }

    // Calculer et afficher la puissance
    if (dashboard.label_power) {
        float power = voltage * current;
        char power_str[32];
        snprintf(power_str, sizeof(power_str), "%.0f W", power);
        lv_label_set_text(dashboard.label_power, power_str);
    }

    // Mettre à jour les températures
    if (dashboard.gauge_temps) {
        widget_gauge_semicircular_set_needle_value(dashboard.gauge_temps,
                                                   dashboard.needle_s2, temp_s2);
        widget_gauge_semicircular_set_needle_value(dashboard.gauge_temps,
                                                   dashboard.needle_s1, temp_s1);
        widget_gauge_semicircular_set_needle_value(dashboard.gauge_temps,
                                                   dashboard.needle_int, temp_int);
    }
}

/**
 * Exemple de simulation des données (pour test)
 */
void dashboard_gauges_simulate(void) {
    static float soc = 80.0f;
    static float voltage = 50.0f;
    static float current = 0.0f;

    // Simuler des variations
    soc += (rand() % 3 - 1) * 0.5f;  // +/- 0.5%
    voltage += (rand() % 3 - 1) * 0.2f;  // +/- 0.2V
    current += (rand() % 3 - 1) * 1.0f;  // +/- 1A

    // Limiter les valeurs
    if (soc < 20) soc = 20;
    if (soc > 100) soc = 100;
    if (voltage < 40) voltage = 40;
    if (voltage > 60) voltage = 60;
    if (current < -10) current = -10;
    if (current > 10) current = 10;

    // Températures simulées
    float temp_s2 = 25.0f + (rand() % 5);
    float temp_s1 = 40.0f + (rand() % 5);
    float temp_int = 55.0f + (rand() % 5);

    // Mettre à jour
    dashboard_gauges_update(soc, 95.0f, voltage, current,
                           temp_s2, temp_s1, temp_int);
}
