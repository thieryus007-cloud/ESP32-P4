// components/gui_lvgl/screen_config.cpp

#include "screen_config.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "ui_i18n.h"

namespace {

bool is_valid_ipv4(std::string_view ip)
{
    if (ip.empty()) {
        return false;
    }

    int segments = 0;
    size_t start = 0;
    while (start < ip.size()) {
        if (segments >= 4) {
            return false;
        }
        const size_t end = ip.find('.', start);
        const auto part  = ip.substr(start, (end == std::string_view::npos) ? ip.size() - start : end - start);
        if (part.empty() || part.size() > 3) {
            return false;
        }
        int value = 0;
        for (char c : part) {
            if (c < '0' || c > '9') {
                return false;
            }
            value = value * 10 + (c - '0');
        }
        if (value > 255) {
            return false;
        }
        segments++;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return segments == 4;
}

bool is_valid_ipv6(std::string_view ip)
{
    if (ip.empty()) {
        return false;
    }

    int segments          = 0;
    bool saw_double_colon = false;
    size_t i              = 0;

    while (i < ip.size()) {
        if (ip[i] == ':') {
            if (i + 1 < ip.size() && ip[i + 1] == ':') {
                if (saw_double_colon) {
                    return false;
                }
                saw_double_colon = true;
                i += 2;
                continue;
            }
            if (i == 0 || ip[i - 1] == ':') {
                return false;
            }
            ++i;
            continue;
        }

        int digits = 0;
        while (i < ip.size() && ip[i] != ':') {
            if (!std::isxdigit(static_cast<unsigned char>(ip[i]))) {
                return false;
            }
            ++i;
            ++digits;
            if (digits > 4) {
                return false;
            }
        }

        ++segments;
        if (segments > 8) {
            return false;
        }
    }

    if (!saw_double_colon && segments != 8) {
        return false;
    }
    if (saw_double_colon && segments > 8) {
        return false;
    }
    return true;
}

bool is_valid_ip(std::string_view ip)
{
    if (ip.empty()) {
        return true;
    }
    if (ip.find(':') != std::string_view::npos) {
        return is_valid_ipv6(ip);
    }
    return is_valid_ipv4(ip);
}

class ScreenConfig {
public:
    static ScreenConfig &instance()
    {
        static ScreenConfig inst;
        return inst;
    }

    void set_bus(event_bus_t *bus)
    {
        bus_ = bus;
        if (bus_) {
            save_handler_ = [bus](const hmi_config_t &cfg, bool mqtt_only) {
                user_input_write_config_t req{};
                req.config    = cfg;
                req.mqtt_only = mqtt_only;
                event_t evt{};
                evt.type      = EVENT_USER_INPUT_WRITE_CONFIG;
                evt.data      = &req;
                evt.data_size = sizeof(req);
                event_bus_publish(bus, &evt);
            };

            reload_handler_ = [bus](bool include_mqtt) {
                user_input_reload_config_t req{};
                req.include_mqtt = include_mqtt;
                event_t evt{};
                evt.type      = EVENT_USER_INPUT_RELOAD_CONFIG;
                evt.data      = &req;
                evt.data_size = sizeof(req);
                event_bus_publish(bus, &evt);
            };
        } else {
            save_handler_   = nullptr;
            reload_handler_ = nullptr;
        }
    }

    void set_save_callback(screen_config::SaveCallback cb) { save_handler_ = std::move(cb); }

    void set_reload_callback(screen_config::ReloadCallback cb) { reload_handler_ = std::move(cb); }

    void set_loading(bool loading, const char *message)
    {
        if (spinner_) {
            if (loading) {
                lv_obj_clear_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        set_status(message ? message : "",
                   loading ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_GREY));
    }

    void create(lv_obj_t *parent)
    {
        lv_obj_set_style_pad_all(parent, 12, 0);
        lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(parent,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER);

        lbl_title_ = lv_label_create(parent);
        lv_obj_set_style_text_font(lbl_title_, &lv_font_montserrat_18, 0);

        lv_obj_t *section_wifi = create_section(parent, ui_i18n("config.section.wifi"), &lbl_section_wifi_);
        fields_.ssid           = create_text_field(section_wifi, ui_i18n("config.label.ssid"), false, &lbl_ssid_);
        fields_.password = create_text_field(section_wifi, ui_i18n("config.label.password"), true, &lbl_password_);
        fields_.static_ip = create_text_field(section_wifi, ui_i18n("config.label.static_ip"), false, &lbl_static_ip_);
        lv_textarea_set_placeholder_text(fields_.static_ip, ui_i18n("config.placeholder.static_ip"));

        lv_obj_t *section_mqtt = create_section(parent, ui_i18n("config.section.mqtt"), &lbl_section_mqtt_);
        fields_.mqtt_broker    = create_text_field(section_mqtt, ui_i18n("config.label.broker"), false, &lbl_broker_);
        fields_.mqtt_pub       = create_text_field(section_mqtt, ui_i18n("config.label.pub"), false, &lbl_pub_);
        fields_.mqtt_sub       = create_text_field(section_mqtt, ui_i18n("config.label.sub"), false, &lbl_sub_);

        lv_obj_t *section_bus = create_section(parent, ui_i18n("config.section.bus"), &lbl_section_bus_);
        fields_.can_bitrate   = create_text_field(section_bus, ui_i18n("config.label.can"), false, &lbl_can_);
        fields_.uart_baud = create_text_field(section_bus, ui_i18n("config.label.uart_baud"), false, &lbl_uart_baud_);
        fields_.uart_parity = create_text_field(section_bus, ui_i18n("config.label.uart_parity"), false, &lbl_uart_parity_);

        lv_obj_t *row_language = lv_obj_create(section_bus);
        lv_obj_remove_style_all(row_language);
        lv_obj_set_width(row_language, LV_PCT(100));
        lv_obj_set_flex_flow(row_language, LV_FLEX_FLOW_COLUMN);
        lbl_language_ = lv_label_create(row_language);
        ui_i18n_label_set_text(lbl_language_, "config.label.language");
        dd_language_ = lv_dropdown_create(row_language);
        lv_dropdown_set_options_static(dd_language_, "Français\nEnglish");
        lv_obj_add_event_cb(dd_language_, on_language_event, LV_EVENT_VALUE_CHANGED, nullptr);
        update_language_dropdown();

        lv_obj_t *row_actions = lv_obj_create(parent);
        lv_obj_remove_style_all(row_actions);
        lv_obj_set_width(row_actions, LV_PCT(100));
        lv_obj_set_flex_flow(row_actions, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_actions,
                              LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *btn_reload = lv_btn_create(row_actions);
        lv_obj_add_event_cb(btn_reload, on_reload_event, LV_EVENT_CLICKED, nullptr);
        lbl_btn_reload_ = lv_label_create(btn_reload);
        ui_i18n_label_set_text(lbl_btn_reload_, "config.btn.reload");

        lv_obj_t *btn_reconnect = lv_btn_create(row_actions);
        lv_obj_add_event_cb(btn_reconnect, on_reconnect_event, LV_EVENT_CLICKED, nullptr);
        lbl_btn_reconnect_ = lv_label_create(btn_reconnect);
        ui_i18n_label_set_text(lbl_btn_reconnect_, "config.btn.reconnect");

        lv_obj_t *btn_save = lv_btn_create(row_actions);
        lv_obj_add_event_cb(btn_save, on_save_event, LV_EVENT_CLICKED, nullptr);
        lbl_btn_save_ = lv_label_create(btn_save);
        ui_i18n_label_set_text(lbl_btn_save_, "config.btn.save");

        lv_obj_t *row_status = lv_obj_create(parent);
        lv_obj_remove_style_all(row_status);
        lv_obj_set_width(row_status, LV_PCT(100));
        lv_obj_set_flex_flow(row_status, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_status,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        spinner_ = lv_spinner_create(row_status, 1000, 60);
        lv_obj_set_size(spinner_, 32, 32);
        lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);

        status_label_ = lv_label_create(row_status);
        set_status(ui_i18n("config.status.ready"), lv_palette_main(LV_PALETTE_GREY));

        apply_local_to_fields();
        apply_texts();
    }

    void apply(const hmi_config_t *config)
    {
        if (!config) {
            return;
        }
        current_ = *config;
        apply_local_to_fields();
        set_loading(false, ui_i18n("config.status.updated"));
    }

    void show_result(const cmd_result_t *result)
    {
        if (!result) {
            return;
        }
        const lv_color_t color = result->success ? lv_palette_main(LV_PALETTE_GREEN)
                                                 : lv_palette_main(LV_PALETTE_RED);
        set_status(result->message, color);
        if (spinner_) {
            lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void refresh_texts() { apply_texts(); }

private:
    struct ConfigFormFields {
        struct Data {
            std::string wifi_ssid;
            std::string wifi_password;
            std::string static_ip;
            std::string mqtt_broker;
            std::string mqtt_topic_pub;
            std::string mqtt_topic_sub;
            std::string uart_parity;
            std::optional<int> can_bitrate;
            std::optional<int> uart_baudrate;
        };

        lv_obj_t *ssid         = nullptr;
        lv_obj_t *password     = nullptr;
        lv_obj_t *static_ip    = nullptr;
        lv_obj_t *mqtt_broker  = nullptr;
        lv_obj_t *mqtt_pub     = nullptr;
        lv_obj_t *mqtt_sub     = nullptr;
        lv_obj_t *can_bitrate  = nullptr;
        lv_obj_t *uart_baud    = nullptr;
        lv_obj_t *uart_parity  = nullptr;

        Data read() const;
        void write(const hmi_config_t &config) const;

    private:
        static std::string read_text(lv_obj_t *ta);
        static void write_text(lv_obj_t *ta, std::string_view value);
        static std::optional<int> parse_integer(std::string_view text);
        static void write_int(lv_obj_t *ta, int value);
    };

    struct ValidationResult {
        bool ok;
        const char *message_key;

        static ValidationResult success() { return ValidationResult{true, nullptr}; }
        static ValidationResult error(const char *key) { return ValidationResult{false, key}; }
    };

    static lv_obj_t *create_section(lv_obj_t *parent, const char *title, lv_obj_t **out_label)
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

        if (out_label) {
            *out_label = lbl;
        }
        return cont;
    }

    static lv_obj_t *create_text_field(lv_obj_t *parent, const char *label, bool password, lv_obj_t **out_label)
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

        if (out_label) {
            *out_label = lbl;
        }
        return ta;
    }

    static void set_label_text(lv_obj_t *label, const char *key)
    {
        if (label) {
            ui_i18n_label_set_text(label, key);
        }
    }

    void set_status(const char *msg, lv_color_t color)
    {
        if (status_label_) {
            lv_label_set_text(status_label_, msg ? msg : "");
            lv_obj_set_style_text_color(status_label_, color, 0);
        }
    }

    void update_language_dropdown()
    {
        if (!dd_language_) {
            return;
        }
        ui_language_t lang = ui_i18n_get_language();
        lv_dropdown_set_selected(dd_language_, (lang == UI_LANG_FR) ? 0 : 1);
    }

    void apply_texts()
    {
        set_label_text(lbl_title_, "config.title");
        set_label_text(lbl_section_wifi_, "config.section.wifi");
        set_label_text(lbl_section_mqtt_, "config.section.mqtt");
        set_label_text(lbl_section_bus_, "config.section.bus");

        set_label_text(lbl_ssid_, "config.label.ssid");
        set_label_text(lbl_password_, "config.label.password");
        set_label_text(lbl_static_ip_, "config.label.static_ip");
        if (fields_.static_ip) {
            lv_textarea_set_placeholder_text(fields_.static_ip, ui_i18n("config.placeholder.static_ip"));
        }
        set_label_text(lbl_broker_, "config.label.broker");
        set_label_text(lbl_pub_, "config.label.pub");
        set_label_text(lbl_sub_, "config.label.sub");
        set_label_text(lbl_can_, "config.label.can");
        set_label_text(lbl_uart_baud_, "config.label.uart_baud");
        set_label_text(lbl_uart_parity_, "config.label.uart_parity");
        set_label_text(lbl_language_, "config.label.language");

        set_label_text(lbl_btn_reload_, "config.btn.reload");
        set_label_text(lbl_btn_reconnect_, "config.btn.reconnect");
        set_label_text(lbl_btn_save_, "config.btn.save");

        update_language_dropdown();
    }

    void apply_local_to_fields() { fields_.write(current_); }

    ValidationResult validate(const ConfigFormFields::Data &data) const
    {
        if (data.wifi_ssid.empty()) {
            return ValidationResult::error("config.error.ssid");
        }
        if (data.mqtt_broker.empty()) {
            return ValidationResult::error("config.error.broker");
        }
        if (!is_valid_ip(data.static_ip)) {
            return ValidationResult::error("config.error.ip");
        }
        if (!data.can_bitrate || data.can_bitrate.value() <= 0) {
            return ValidationResult::error("config.error.can");
        }
        if (!data.uart_baudrate || data.uart_baudrate.value() <= 0) {
            return ValidationResult::error("config.error.baud");
        }
        return ValidationResult::success();
    }

    template <size_t N>
    static void copy_text(char (&dest)[N], std::string_view src)
    {
        if (N == 0) {
            return;
        }
        const size_t length = std::min(src.size(), static_cast<size_t>(N - 1));
        if (length > 0) {
            std::memcpy(dest, src.data(), length);
        }
        dest[length] = '\0';
    }

    hmi_config_t make_config(const ConfigFormFields::Data &data) const
    {
        hmi_config_t cfg{};
        copy_text(cfg.wifi_ssid, data.wifi_ssid);
        copy_text(cfg.wifi_password, data.wifi_password);
        copy_text(cfg.static_ip, data.static_ip);
        copy_text(cfg.mqtt_broker, data.mqtt_broker);
        copy_text(cfg.mqtt_topic_pub, data.mqtt_topic_pub);
        copy_text(cfg.mqtt_topic_sub, data.mqtt_topic_sub);

        std::string parity = data.uart_parity;
        if (parity.empty()) {
            parity = "N";
        } else if (parity.size() > 1) {
            parity.resize(1);
        }
        copy_text(cfg.uart_parity, parity);

        cfg.can_bitrate   = data.can_bitrate.value_or(0);
        cfg.uart_baudrate = data.uart_baudrate.value_or(0);
        return cfg;
    }

    void handle_reload()
    {
        if (!reload_handler_) {
            return;
        }
        reload_handler_(true);
        set_loading(true, ui_i18n("config.status.loading"));
    }

    void handle_reconnect()
    {
        if (!bus_) {
            return;
        }
        user_input_change_mode_t req{};
        req.mode = HMI_MODE_CONNECTED_S3;
        event_t evt{};
        evt.type      = EVENT_USER_INPUT_CHANGE_MODE;
        evt.data      = &req;
        evt.data_size = sizeof(req);
        event_bus_publish(bus_, &evt);
        set_status(ui_i18n("config.status.reconnect"), lv_palette_main(LV_PALETTE_BLUE));
    }

    void handle_save()
    {
        ConfigFormFields::Data form = fields_.read();
        ValidationResult validation  = validate(form);
        if (!validation.ok) {
            set_status(ui_i18n(validation.message_key), lv_palette_main(LV_PALETTE_RED));
            return;
        }

        hmi_config_t cfg = make_config(form);
        current_         = cfg;
        apply_local_to_fields();
        if (!save_handler_) {
            return;
        }
        save_handler_(cfg, false);
        set_loading(true, ui_i18n("config.status.saving"));
    }

    void handle_language_changed()
    {
        if (!dd_language_) {
            return;
        }
        const uint16_t sel = lv_dropdown_get_selected(dd_language_);
        ui_language_t lang = (sel == 0) ? UI_LANG_FR : UI_LANG_EN;
        ui_i18n_set_language(lang);
    }

    static void on_reload_event(lv_event_t *e)
    {
        if (e->code == LV_EVENT_CLICKED) {
            instance().handle_reload();
        }
    }

    static void on_reconnect_event(lv_event_t *e)
    {
        if (e->code == LV_EVENT_CLICKED) {
            instance().handle_reconnect();
        }
    }

    static void on_save_event(lv_event_t *e)
    {
        if (e->code == LV_EVENT_CLICKED) {
            instance().handle_save();
        }
    }

    static void on_language_event(lv_event_t *e)
    {
        if (e->code == LV_EVENT_VALUE_CHANGED) {
            instance().handle_language_changed();
        }
    }

    event_bus_t *bus_ = nullptr;
    hmi_config_t current_{};
    ConfigFormFields fields_{};

    lv_obj_t *lbl_title_          = nullptr;
    lv_obj_t *lbl_section_wifi_   = nullptr;
    lv_obj_t *lbl_section_mqtt_   = nullptr;
    lv_obj_t *lbl_section_bus_    = nullptr;
    lv_obj_t *lbl_ssid_           = nullptr;
    lv_obj_t *lbl_password_       = nullptr;
    lv_obj_t *lbl_static_ip_      = nullptr;
    lv_obj_t *lbl_broker_         = nullptr;
    lv_obj_t *lbl_pub_            = nullptr;
    lv_obj_t *lbl_sub_            = nullptr;
    lv_obj_t *lbl_can_            = nullptr;
    lv_obj_t *lbl_uart_baud_      = nullptr;
    lv_obj_t *lbl_uart_parity_    = nullptr;
    lv_obj_t *lbl_language_       = nullptr;
    lv_obj_t *lbl_btn_reload_     = nullptr;
    lv_obj_t *lbl_btn_save_       = nullptr;
    lv_obj_t *lbl_btn_reconnect_  = nullptr;

    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *spinner_      = nullptr;
    lv_obj_t *dd_language_  = nullptr;

    screen_config::SaveCallback save_handler_{};
    screen_config::ReloadCallback reload_handler_{};
};

ScreenConfig::ConfigFormFields::Data ScreenConfig::ConfigFormFields::read() const
{
    Data data{};
    data.wifi_ssid     = read_text(ssid);
    data.wifi_password = read_text(password);
    data.static_ip     = read_text(static_ip);
    data.mqtt_broker   = read_text(mqtt_broker);
    data.mqtt_topic_pub = read_text(mqtt_pub);
    data.mqtt_topic_sub = read_text(mqtt_sub);
    data.can_bitrate    = parse_integer(read_text(can_bitrate));
    data.uart_baudrate  = parse_integer(read_text(uart_baud));
    data.uart_parity    = read_text(uart_parity);
    return data;
}

void ScreenConfig::ConfigFormFields::write(const hmi_config_t &config) const
{
    write_text(ssid, config.wifi_ssid);
    write_text(password, config.wifi_password);
    write_text(static_ip, config.static_ip);
    write_text(mqtt_broker, config.mqtt_broker);
    write_text(mqtt_pub, config.mqtt_topic_pub);
    write_text(mqtt_sub, config.mqtt_topic_sub);
    write_int(can_bitrate, config.can_bitrate);
    write_int(uart_baud, config.uart_baudrate);
    size_t parity_len = strnlen(config.uart_parity, sizeof(config.uart_parity));
    std::string parity_value;
    if (parity_len == 0) {
        parity_value = "N";
    } else {
        parity_value.assign(config.uart_parity, config.uart_parity + parity_len);
    }
    if (parity_value.size() > 1) {
        parity_value.resize(1);
    }
    write_text(uart_parity, parity_value);
}

std::string ScreenConfig::ConfigFormFields::read_text(lv_obj_t *ta)
{
    if (!ta) {
        return {};
    }
    const char *text = lv_textarea_get_text(ta);
    return text ? std::string(text) : std::string{};
}

void ScreenConfig::ConfigFormFields::write_text(lv_obj_t *ta, std::string_view value)
{
    if (!ta) {
        return;
    }
    if (value.empty()) {
        lv_textarea_set_text(ta, "");
        return;
    }
    std::string buffer(value);
    lv_textarea_set_text(ta, buffer.c_str());
}

std::optional<int> ScreenConfig::ConfigFormFields::parse_integer(std::string_view text)
{
    if (text.empty()) {
        return std::nullopt;
    }
    int value = 0;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

void ScreenConfig::ConfigFormFields::write_int(lv_obj_t *ta, int value)
{
    if (!ta) {
        return;
    }
    std::array<char, 32> buffer{};
    const int written = std::snprintf(buffer.data(), buffer.size(), "%d", value);
    if (written < 0) {
        return;
    }
    lv_textarea_set_text(ta, buffer.data());
}

}  // namespace

extern "C" {

void screen_config_set_bus(event_bus_t *bus)
{
    ScreenConfig::instance().set_bus(bus);
}

void screen_config_set_loading(bool loading, const char *message)
{
    ScreenConfig::instance().set_loading(loading, message);
}

void screen_config_create(lv_obj_t *parent)
{
    ScreenConfig::instance().create(parent);
}

void screen_config_apply(const hmi_config_t *config)
{
    ScreenConfig::instance().apply(config);
}

void screen_config_show_result(const cmd_result_t *result)
{
    ScreenConfig::instance().show_result(result);
}

void screen_config_refresh_texts(void)
{
    ScreenConfig::instance().refresh_texts();
}

}  // extern "C"

namespace screen_config {

void set_save_callback(SaveCallback cb)
{
    ScreenConfig::instance().set_save_callback(std::move(cb));
}

void set_reload_callback(ReloadCallback cb)
{
    ScreenConfig::instance().set_reload_callback(std::move(cb));
}

}  // namespace screen_config
