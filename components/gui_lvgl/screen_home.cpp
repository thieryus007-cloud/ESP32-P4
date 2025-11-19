// components/gui_lvgl/screen_home.cpp

// components/gui_lvgl/screen_home.cpp

#include "screen_home.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "lvgl.h"

#include "gui_format.hpp"
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

constexpr const char *kAutonomousText = "Autonome";

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

        status_bms_label_.reset(lv_label_create(row_status));
        status_can_label_.reset(lv_label_create(row_status));
        status_mqtt_label_.reset(lv_label_create(row_status));
        status_wifi_label_.reset(lv_label_create(row_status));
        status_bal_label_.reset(lv_label_create(row_status));
        status_alm_label_.reset(lv_label_create(row_status));

        apply_static_texts();
    }

    void update_battery(const battery_status_t &status)
    {
        last_battery_ = status;

        if (soc_value_) {
            set_label_textf(soc_value_, "{:.1f} %", status.soc);
        }
        if (voltage_value_) {
            set_label_textf(voltage_value_, "{:.2f} V", status.voltage);
        }
        if (current_value_) {
            set_label_textf(current_value_, "{:.2f} A", status.current);
        }
        if (power_value_) {
            set_label_textf(power_value_, "{:.0f} W", status.power);
        }
        if (temp_value_) {
            set_label_textf(temp_value_, "{:.1f} °C", status.temperature);
        }

        status_bms_label_.set(ui_i18n("home.status.bms"),
                              status.bms_ok ? StatusState::Ok : StatusState::Error);
        status_can_label_.set(ui_i18n("home.status.can"),
                              status.can_ok ? StatusState::Ok : StatusState::Error);

        update_mqtt_badge(status);
    }

    void update_system(const system_status_t &status)
    {
        last_system_ = status;

        if (!status.telemetry_expected) {
            status_wifi_label_.set_with_palette(kAutonomousText, LV_PALETTE_BLUE);
        } else {
            const char *text = ui_i18n("home.status.wifi");
            StatusState state = StatusState::Neutral;

            if (status.network_state == NETWORK_STATE_NOT_CONFIGURED) {
                state = StatusState::Warn;
                text  = "WiFi N/A";
            } else if (status.network_state == NETWORK_STATE_CONNECTING) {
                state = StatusState::Warn;
                text  = "Connexion...";
            } else if (status.network_state != NETWORK_STATE_ACTIVE) {
                state = StatusState::Error;
            } else if (!status.server_reachable || !status.storage_ok) {
                state = StatusState::Warn;
            } else if (status.has_error) {
                state = StatusState::Error;
            } else {
                state = StatusState::Ok;
            }

            status_wifi_label_.set(text, state);
        }

        status_alm_label_.set(ui_i18n("home.status.alm"),
                              status.has_error ? StatusState::Error : StatusState::Neutral);

        if (!status.telemetry_expected) {
            status_mqtt_label_.set_with_palette(kAutonomousText, LV_PALETTE_BLUE);
        } else if (last_battery_) {
            update_mqtt_badge(*last_battery_);
        } else {
            status_mqtt_label_.set(ui_i18n("home.status.mqtt"), StatusState::Neutral);
        }
    }

    void update_balancing(const pack_stats_t *stats)
    {
        if (stats) {
            last_pack_stats_ = *stats;
        } else {
            last_pack_stats_.reset();
        }

        if (!stats || stats->cell_count == 0) {
            status_bal_label_.set(ui_i18n("home.status.bal"), StatusState::Neutral);
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
            status_bal_label_.set_with_palette(ui_i18n("home.status.bal"), LV_PALETTE_ORANGE);
        } else {
            status_bal_label_.set(ui_i18n("home.status.bal"), StatusState::Neutral);
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

        status_bms_label_.set(ui_i18n("home.status.bms"), StatusState::Neutral);
        status_can_label_.set(ui_i18n("home.status.can"), StatusState::Neutral);
        status_mqtt_label_.set(ui_i18n("home.status.mqtt"), StatusState::Neutral);
        status_wifi_label_.set(ui_i18n("home.status.wifi"), StatusState::Neutral);
        status_bal_label_.set(ui_i18n("home.status.bal"), StatusState::Neutral);
        status_alm_label_.set(ui_i18n("home.status.alm"), StatusState::Neutral);
    }

    void update_mqtt_badge(const battery_status_t &status)
    {
        if (last_system_ && !last_system_->telemetry_expected) {
            status_mqtt_label_.set_with_palette(kAutonomousText, LV_PALETTE_BLUE);
            return;
        }

        status_mqtt_label_.set(ui_i18n("home.status.mqtt"),
                               status.mqtt_ok ? StatusState::Ok : StatusState::Error);
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

    StatusLabel status_bms_label_;
    StatusLabel status_can_label_;
    StatusLabel status_mqtt_label_;
    StatusLabel status_wifi_label_;
    StatusLabel status_bal_label_;
    StatusLabel status_alm_label_;

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
