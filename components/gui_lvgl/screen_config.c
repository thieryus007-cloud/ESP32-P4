// components/gui_lvgl/screen_config.c

#include "screen_config.h"

#include <stdio.h>
#include <string.h>

static event_bus_t *s_bus = NULL;
static hmi_config_t s_local_config = { 0 };

static lv_obj_t *s_ta_ssid = NULL;
static lv_obj_t *s_ta_password = NULL;
static lv_obj_t *s_ta_static_ip = NULL;
static lv_obj_t *s_ta_mqtt_broker = NULL;
static lv_obj_t *s_ta_mqtt_pub = NULL;
static lv_obj_t *s_ta_mqtt_sub = NULL;
static lv_obj_t *s_ta_can_bitrate = NULL;
static lv_obj_t *s_ta_uart_baud = NULL;
static lv_obj_t *s_ta_uart_parity = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_spinner = NULL;

static lv_obj_t *create_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_width(cont, LV_PCT(100));
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_bg_color(cont, lv_palette_lighten(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_20, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    return cont;
}

static lv_obj_t *create_text_field(lv_obj_t *parent,
                                   const char *label,
                                   bool password)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);

    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, password);
    lv_obj_set_width(ta, LV_PCT(100));
    return ta;
}

static void set_status(const char *msg, lv_color_t color)
{
    if (s_status_label) {
        lv_label_set_text(s_status_label, msg ? msg : "");
        lv_obj_set_style_text_color(s_status_label, color, 0);
    }
}

void screen_config_set_bus(event_bus_t *bus)
{
    s_bus = bus;
}

void screen_config_set_loading(bool loading, const char *message)
{
    if (s_spinner) {
        if (loading) {
            lv_obj_clear_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }
    set_status(message ? message : "", loading ? lv_palette_main(LV_PALETTE_BLUE)
                                                : lv_palette_main(LV_PALETTE_GREY));
}

static bool is_valid_ip(const char *ip)
{
    if (!ip || strlen(ip) == 0) {
        return true; // champ optionnel
    }

    int segments = 0;
    const char *ptr = ip;
    while (*ptr) {
        if (segments >= 4) return false;
        int value = 0;
        int digits = 0;
        while (*ptr && *ptr != '.') {
            if (*ptr < '0' || *ptr > '9') return false;
            value = value * 10 + (*ptr - '0');
            digits++;
            ptr++;
        }
        if (digits == 0 || value > 255) return false;
        segments++;
        if (*ptr == '.') ptr++;
    }
    return segments == 4;
}

static bool read_form(hmi_config_t *out)
{
    if (!out) return false;

    memset(out, 0, sizeof(*out));
    const char *ssid = lv_textarea_get_text(s_ta_ssid);
    const char *pwd  = lv_textarea_get_text(s_ta_password);
    const char *ip   = lv_textarea_get_text(s_ta_static_ip);
    const char *broker = lv_textarea_get_text(s_ta_mqtt_broker);
    const char *topic_pub = lv_textarea_get_text(s_ta_mqtt_pub);
    const char *topic_sub = lv_textarea_get_text(s_ta_mqtt_sub);
    const char *can_bitrate = lv_textarea_get_text(s_ta_can_bitrate);
    const char *uart_baud = lv_textarea_get_text(s_ta_uart_baud);
    const char *uart_parity = lv_textarea_get_text(s_ta_uart_parity);

    if (!ssid || strlen(ssid) == 0) {
        set_status("SSID requis", lv_palette_main(LV_PALETTE_RED));
        return false;
    }
    if (!broker || strlen(broker) == 0) {
        set_status("Broker MQTT requis", lv_palette_main(LV_PALETTE_RED));
        return false;
    }
    if (!is_valid_ip(ip)) {
        set_status("IP statique invalide (xxx.xxx.xxx.xxx)", lv_palette_main(LV_PALETTE_RED));
        return false;
    }

    strncpy(out->wifi_ssid, ssid, sizeof(out->wifi_ssid) - 1);
    strncpy(out->wifi_password, pwd ? pwd : "", sizeof(out->wifi_password) - 1);
    strncpy(out->static_ip, ip ? ip : "", sizeof(out->static_ip) - 1);
    strncpy(out->mqtt_broker, broker, sizeof(out->mqtt_broker) - 1);
    strncpy(out->mqtt_topic_pub, topic_pub ? topic_pub : "", sizeof(out->mqtt_topic_pub) - 1);
    strncpy(out->mqtt_topic_sub, topic_sub ? topic_sub : "", sizeof(out->mqtt_topic_sub) - 1);
    strncpy(out->uart_parity, uart_parity ? uart_parity : "N", sizeof(out->uart_parity) - 1);

    out->can_bitrate = atoi(can_bitrate);
    out->uart_baudrate = atoi(uart_baud);

    if (out->can_bitrate <= 0) {
        set_status("Bitrate CAN invalide", lv_palette_main(LV_PALETTE_RED));
        return false;
    }
    if (out->uart_baudrate <= 0) {
        set_status("Baudrate UART invalide", lv_palette_main(LV_PALETTE_RED));
        return false;
    }

    return true;
}

static void fill_field(lv_obj_t *ta, const char *value)
{
    if (ta && value) {
        lv_textarea_set_text(ta, value);
    }
}

static void apply_local_to_fields(void)
{
    char buf[32];

    fill_field(s_ta_ssid, s_local_config.wifi_ssid);
    fill_field(s_ta_password, s_local_config.wifi_password);
    fill_field(s_ta_static_ip, s_local_config.static_ip);
    fill_field(s_ta_mqtt_broker, s_local_config.mqtt_broker);
    fill_field(s_ta_mqtt_pub, s_local_config.mqtt_topic_pub);
    fill_field(s_ta_mqtt_sub, s_local_config.mqtt_topic_sub);

    snprintf(buf, sizeof(buf), "%d", s_local_config.can_bitrate);
    fill_field(s_ta_can_bitrate, buf);
    snprintf(buf, sizeof(buf), "%d", s_local_config.uart_baudrate);
    fill_field(s_ta_uart_baud, buf);
    fill_field(s_ta_uart_parity, s_local_config.uart_parity);
}

static void on_reload_event(lv_event_t *e)
{
    (void) e;
    if (!s_bus) return;

    user_input_reload_config_t req = {
        .include_mqtt = true,
    };
    event_t evt = {
        .type = EVENT_USER_INPUT_RELOAD_CONFIG,
        .data = &req,
    };
    event_bus_publish(s_bus, &evt);
    screen_config_set_loading(true, "Chargement configuration...");
}

static void on_save_event(lv_event_t *e)
{
    (void) e;
    if (!s_bus) return;

    hmi_config_t cfg;
    if (!read_form(&cfg)) {
        return;
    }

    user_input_write_config_t req = {
        .config = cfg,
        .mqtt_only = false,
    };
    event_t evt = {
        .type = EVENT_USER_INPUT_WRITE_CONFIG,
        .data = &req,
    };
    event_bus_publish(s_bus, &evt);
    screen_config_set_loading(true, "Enregistrement...");
}

void screen_config_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Configuration HMI / BMS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    // WiFi / IP
    lv_obj_t *section_wifi = create_section(parent, "WiFi STA");
    s_ta_ssid = create_text_field(section_wifi, "SSID", false);
    s_ta_password = create_text_field(section_wifi, "Mot de passe", true);
    s_ta_static_ip = create_text_field(section_wifi, "IP statique (optionnel)", false);
    lv_textarea_set_placeholder_text(s_ta_static_ip, "192.168.1.50");

    // MQTT
    lv_obj_t *section_mqtt = create_section(parent, "MQTT");
    s_ta_mqtt_broker = create_text_field(section_mqtt, "Broker (host:port)", false);
    s_ta_mqtt_pub = create_text_field(section_mqtt, "Topic publication", false);
    s_ta_mqtt_sub = create_text_field(section_mqtt, "Topic souscription", false);

    // Bus
    lv_obj_t *section_bus = create_section(parent, "Bus CAN & UART");
    s_ta_can_bitrate = create_text_field(section_bus, "CAN bitrate (ex: 500000)", false);
    s_ta_uart_baud = create_text_field(section_bus, "UART baudrate", false);
    s_ta_uart_parity = create_text_field(section_bus, "UART parité (N/E/O)", false);

    // Boutons
    lv_obj_t *row_actions = lv_obj_create(parent);
    lv_obj_remove_style_all(row_actions);
    lv_obj_set_width(row_actions, LV_PCT(100));
    lv_obj_set_flex_flow(row_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_actions,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *btn_reload = lv_btn_create(row_actions);
    lv_obj_add_event_cb(btn_reload, on_reload_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_reload = lv_label_create(btn_reload);
    lv_label_set_text(lbl_reload, "Recharger");

    lv_obj_t *btn_save = lv_btn_create(row_actions);
    lv_obj_add_event_cb(btn_save, on_save_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Sauvegarder");

    // Statut + spinner
    lv_obj_t *row_status = lv_obj_create(parent);
    lv_obj_remove_style_all(row_status);
    lv_obj_set_width(row_status, LV_PCT(100));
    lv_obj_set_flex_flow(row_status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_status,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_spinner = lv_spinner_create(row_status, 1000, 60);
    lv_obj_set_size(s_spinner, 32, 32);
    lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);

    s_status_label = lv_label_create(row_status);
    lv_label_set_text(s_status_label, "Prêt");

    apply_local_to_fields();
}

void screen_config_apply(const hmi_config_t *config)
{
    if (!config) return;
    s_local_config = *config;
    apply_local_to_fields();
    screen_config_set_loading(false, "Configuration mise à jour");
}

void screen_config_show_result(const cmd_result_t *result)
{
    if (!result) return;
    if (result->success) {
        set_status(result->message, lv_palette_main(LV_PALETTE_GREEN));
    } else {
        set_status(result->message, lv_palette_main(LV_PALETTE_RED));
    }
    if (s_spinner) {
        lv_obj_add_flag(s_spinner, LV_OBJ_FLAG_HIDDEN);
    }
}

