// components/gui_lvgl/screen_home.cpp

// components/gui_lvgl/screen_home.cpp

#include "screen_home.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "lvgl.h"

#include "ui_i18n.h"

namespace gui {
namespace {

class TranslatableLabel {
public:
    TranslatableLabel() = default;
    TranslatableLabel(lv_obj_t *label, std::string key) { reset(label, std::move(key)); }

    void reset(lv_obj_t *label, std::string key)
    {
        label_ = label;
        key_   = std::move(key);
        apply();
    }

    void apply() const
    {
        if (label_ && !key_.empty()) {
            ui_i18n_label_set_text(label_, key_.c_str());
        }
    }

    lv_obj_t *get() const { return label_; }

private:
    lv_obj_t    *label_{nullptr};
    std::string  key_;
};

lv_color_t color_ok() { return lv_palette_main(LV_PALETTE_GREEN); }
lv_color_t color_warn() { return lv_palette_main(LV_PALETTE_YELLOW); }
lv_color_t color_error() { return lv_palette_main(LV_PALETTE_RED); }
lv_color_t color_neutral() { return lv_palette_main(LV_PALETTE_GREY); }

void set_status_label(lv_obj_t *label, const char *text, lv_color_t color)
{
    if (!label) return;
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
}

constexpr const char *kAutonomousText = "Autonome";

template <typename... Args>
std::string format_value(const char *fmt, Args... args)
{
    std::array<char, 64> buffer{};
    int                  written = std::snprintf(buffer.data(), buffer.size(), fmt, args...);
    if (written < 0) {
        return {};
    }
    if (static_cast<size_t>(written) >= buffer.size()) {
        written = buffer.size() - 1;
    }
    return std::string(buffer.data(), static_cast<size_t>(written));
}

}  // namespace

class ScreenHome::Impl {
public:
    explicit Impl(lv_obj_t *parent)
    {
        if (!parent) {
            return;
        }

        lv_obj_set_style_pad_all(parent, 8, 0);

        lv_obj_t *cont = lv_obj_create(parent);
        lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cont,
                              LV_FLEX_ALIGN_SPACE_AROUND,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *row_soc = lv_obj_create(cont);
        lv_obj_remove_style_all(row_soc);
        lv_obj_set_width(row_soc, LV_PCT(100));
        lv_obj_set_flex_flow(row_soc, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_soc,
                              LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        soc_title_.reset(lv_label_create(row_soc), "home.soc");

        soc_value_ = lv_label_create(row_soc);
        lv_obj_set_style_text_font(soc_value_, &lv_font_montserrat_32, 0);
        lv_label_set_text(soc_value_, "-- %");

        lv_obj_t *row_values = lv_obj_create(cont);
        lv_obj_remove_style_all(row_values);
        lv_obj_set_width(row_values, LV_PCT(100));
        lv_obj_set_flex_flow(row_values, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_values,
                              LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *col_left = lv_obj_create(row_values);
        lv_obj_remove_style_all(col_left);
        lv_obj_set_flex_flow(col_left, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col_left,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER);

        lv_obj_t *col_right = lv_obj_create(row_values);
        lv_obj_remove_style_all(col_right);
        lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col_right,
                              LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER);

        voltage_title_.reset(lv_label_create(col_left), "home.voltage");
        voltage_value_ = lv_label_create(col_right);
        lv_label_set_text(voltage_value_, "--.- V");

        current_title_.reset(lv_label_create(col_left), "home.current");
        current_value_ = lv_label_create(col_right);
        lv_label_set_text(current_value_, "--.- A");

        power_title_.reset(lv_label_create(col_left), "home.power");
        power_value_ = lv_label_create(col_right);
        lv_label_set_text(power_value_, "---- W");

        temp_title_.reset(lv_label_create(col_left), "home.temperature");
        temp_value_ = lv_label_create(col_right);
        lv_label_set_text(temp_value_, "--.- °C");

        lv_obj_t *row_status = lv_obj_create(cont);
        lv_obj_remove_style_all(row_status);
        lv_obj_set_width(row_status, LV_PCT(100));
        lv_obj_set_flex_flow(row_status, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_status,
                              LV_FLEX_ALIGN_SPACE_AROUND,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);

        status_bms_label_  = lv_label_create(row_status);
        status_can_label_  = lv_label_create(row_status);
        status_mqtt_label_ = lv_label_create(row_status);
        status_wifi_label_ = lv_label_create(row_status);
        status_bal_label_  = lv_label_create(row_status);
        status_alm_label_  = lv_label_create(row_status);

        apply_static_texts();
    }

    void update_battery(const battery_status_t &status)
    {
        last_battery_ = status;

        if (soc_value_) {
            auto text = format_value("%.1f %%", status.soc);
            lv_label_set_text(soc_value_, text.c_str());
        }
        if (voltage_value_) {
            auto text = format_value("%.2f V", status.voltage);
            lv_label_set_text(voltage_value_, text.c_str());
        }
        if (current_value_) {
            auto text = format_value("%.2f A", status.current);
            lv_label_set_text(current_value_, text.c_str());
        }
        if (power_value_) {
            auto text = format_value("%.0f W", status.power);
            lv_label_set_text(power_value_, text.c_str());
        }
        if (temp_value_) {
            auto text = format_value("%.1f °C", status.temperature);
            lv_label_set_text(temp_value_, text.c_str());
        }

        if (status_bms_label_) {
            set_status_label(status_bms_label_, ui_i18n("home.status.bms"),
                             status.bms_ok ? color_ok() : color_error());
        }
        if (status_can_label_) {
            set_status_label(status_can_label_, ui_i18n("home.status.can"),
                             status.can_ok ? color_ok() : color_error());
        }

        update_mqtt_badge(status);
    }

    void update_system(const system_status_t &status)
    {
        last_system_ = status;

        if (status_wifi_label_) {
            if (!status.telemetry_expected) {
                set_status_label(status_wifi_label_, kAutonomousText, lv_palette_main(LV_PALETTE_BLUE));
            } else {
                lv_color_t color = color_neutral();
                const char *text = ui_i18n("home.status.wifi");

                if (status.network_state == NETWORK_STATE_NOT_CONFIGURED) {
                    color = color_warn();
                    text  = "WiFi N/A";
                } else if (status.network_state == NETWORK_STATE_CONNECTING) {
                    color = color_warn();
                    text  = "Connexion...";
                } else if (status.network_state != NETWORK_STATE_ACTIVE) {
                    color = color_error();
                } else if (!status.server_reachable || !status.storage_ok) {
                    color = color_warn();
                } else if (status.has_error) {
                    color = color_error();
                } else {
                    color = color_ok();
                }

                set_status_label(status_wifi_label_, text, color);
            }
        }

        if (status_alm_label_) {
            if (status.has_error) {
                set_status_label(status_alm_label_, ui_i18n("home.status.alm"), color_error());
            } else {
                set_status_label(status_alm_label_, ui_i18n("home.status.alm"), color_neutral());
            }
        }

        if (!status.telemetry_expected) {
            if (status_mqtt_label_) {
                set_status_label(status_mqtt_label_, kAutonomousText, lv_palette_main(LV_PALETTE_BLUE));
            }
        } else if (last_battery_) {
            update_mqtt_badge(*last_battery_);
        } else if (status_mqtt_label_) {
            set_status_label(status_mqtt_label_, ui_i18n("home.status.mqtt"), color_neutral());
        }
    }

    void update_balancing(const pack_stats_t *stats)
    {
        if (stats) {
            last_pack_stats_ = *stats;
        } else {
            last_pack_stats_.reset();
        }

        if (!status_bal_label_) {
            return;
        }

        if (!stats || stats->cell_count == 0) {
            set_status_label(status_bal_label_, ui_i18n("home.status.bal"), color_neutral());
            return;
        }

        uint8_t count = std::min(stats->cell_count, static_cast<uint8_t>(PACK_MAX_CELLS));
        bool    any_balancing = false;
        for (uint8_t i = 0; i < count; ++i) {
            if (stats->balancing[i]) {
                any_balancing = true;
                break;
            }
        }

        if (any_balancing) {
            set_status_label(status_bal_label_, ui_i18n("home.status.bal"),
                             lv_palette_main(LV_PALETTE_ORANGE));
        } else {
            set_status_label(status_bal_label_, ui_i18n("home.status.bal"), color_neutral());
        }
    }

    void refresh_texts()
    {
        apply_static_texts();

        if (last_battery_) {
            update_battery(*last_battery_);
        }
        if (last_system_) {
            update_system(*last_system_);
        }
        if (last_pack_stats_) {
            update_balancing(&*last_pack_stats_);
        } else {
            update_balancing(nullptr);
        }
    }

private:
    void apply_static_texts()
    {
        soc_title_.apply();
        voltage_title_.apply();
        current_title_.apply();
        power_title_.apply();
        temp_title_.apply();

        set_status_label(status_bms_label_, ui_i18n("home.status.bms"), color_neutral());
        set_status_label(status_can_label_, ui_i18n("home.status.can"), color_neutral());
        set_status_label(status_mqtt_label_, ui_i18n("home.status.mqtt"), color_neutral());
        set_status_label(status_wifi_label_, ui_i18n("home.status.wifi"), color_neutral());
        set_status_label(status_bal_label_, ui_i18n("home.status.bal"), color_neutral());
        set_status_label(status_alm_label_, ui_i18n("home.status.alm"), color_neutral());
    }

    void update_mqtt_badge(const battery_status_t &status)
    {
        if (!status_mqtt_label_) {
            return;
        }

        if (last_system_ && !last_system_->telemetry_expected) {
            set_status_label(status_mqtt_label_, kAutonomousText, lv_palette_main(LV_PALETTE_BLUE));
            return;
        }

        set_status_label(status_mqtt_label_, ui_i18n("home.status.mqtt"),
                         status.mqtt_ok ? color_ok() : color_error());
    }

    TranslatableLabel soc_title_;
    TranslatableLabel voltage_title_;
    TranslatableLabel current_title_;
    TranslatableLabel power_title_;
    TranslatableLabel temp_title_;

    lv_obj_t *soc_value_{nullptr};
    lv_obj_t *voltage_value_{nullptr};
    lv_obj_t *current_value_{nullptr};
    lv_obj_t *power_value_{nullptr};
    lv_obj_t *temp_value_{nullptr};

    lv_obj_t *status_bms_label_{nullptr};
    lv_obj_t *status_can_label_{nullptr};
    lv_obj_t *status_mqtt_label_{nullptr};
    lv_obj_t *status_wifi_label_{nullptr};
    lv_obj_t *status_bal_label_{nullptr};
    lv_obj_t *status_alm_label_{nullptr};

    std::optional<battery_status_t> last_battery_;
    std::optional<system_status_t>  last_system_;
    std::optional<pack_stats_t>     last_pack_stats_;
};

ScreenHome::ScreenHome(lv_obj_t *parent) : impl_(std::make_unique<Impl>(parent)) {}

ScreenHome::~ScreenHome() = default;

void ScreenHome::update_battery(const battery_status_t &status)
{
    if (impl_) {
        impl_->update_battery(status);
    }
}

void ScreenHome::update_system(const system_status_t &status)
{
    if (impl_) {
        impl_->update_system(status);
    }
}

void ScreenHome::update_balancing(const pack_stats_t *stats)
{
    if (impl_) {
        impl_->update_balancing(stats);
    }
}

void ScreenHome::refresh_texts()
{
    if (impl_) {
        impl_->refresh_texts();
    }
}

std::unique_ptr<ScreenHome> create_screen_home(lv_obj_t *parent)
{
    return std::unique_ptr<ScreenHome>(new ScreenHome(parent));
}

}  // namespace gui
