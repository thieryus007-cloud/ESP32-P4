// components/gui_lvgl/gui_init.cpp

#include "gui_init.hpp"

#include <array>

#include "esp_log.h"

#include "screen_home.h"
#include "screen_dashboard.h"
#include "screen_battery.h"
#include "screen_cells.h"
#include "screen_power.h"
#include "screen_config.h"
#include "screen_alerts.h"
#include "screen_tinybms_status.h"
#include "screen_tinybms_config.h"
#include "screen_can_status.h"
#include "screen_can_config.h"
#include "screen_bms_control.h"
#include "screen_history.h"
#include "ui_notifications.h"
#include "ui_theme.h"
#include "ui_i18n.h"

#include "event_types.h"
#include "tinybms_client.h"
#include "tinybms_model.h"
#include "can_victron.h"

namespace gui {

namespace {

constexpr const char *TAG = "GUI_INIT";

}  // namespace

GuiRoot::GuiRoot(event_bus_t *bus) : bus_(bus) {}

void GuiRoot::init()
{
    ui_i18n_init();

    ESP_LOGI(TAG,
             "Initializing GUI (LVGL with dashboard + existing tabs: Home, Pack, Cells, Power, Config, TinyBMS, CAN, BMS Control)");

    ui_theme_init(lv_display_get_default());
    ui_notifications_init(bus_);
    ui_theme_create_quick_menu(lv_layer_top());

    create_tabs();
    refresh_language();

    ui_i18n_register_listener(&GuiRoot::language_listener, this);

    register_event_bus_handlers();
}

void GuiRoot::start()
{
    ESP_LOGI(TAG, "GUI started");
}

void GuiRoot::create_tabs()
{
    lv_obj_t *root = lv_scr_act();
    tabview_       = lv_tabview_create(root, LV_DIR_TOP, 35);

    tab_dashboard_  = lv_tabview_add_tab(tabview_, ui_i18n("tab.dashboard"));
    tab_home_       = lv_tabview_add_tab(tabview_, ui_i18n("tab.home"));
    tab_pack_       = lv_tabview_add_tab(tabview_, ui_i18n("tab.pack"));
    tab_cells_      = lv_tabview_add_tab(tabview_, ui_i18n("tab.cells"));
    tab_power_      = lv_tabview_add_tab(tabview_, ui_i18n("tab.power"));
    tab_alerts_     = lv_tabview_add_tab(tabview_, ui_i18n("tab.alerts"));
    tab_config_     = lv_tabview_add_tab(tabview_, ui_i18n("tab.config"));
    tab_tbms_stat_  = lv_tabview_add_tab(tabview_, ui_i18n("tab.tbms_status"));
    tab_tbms_conf_  = lv_tabview_add_tab(tabview_, ui_i18n("tab.tbms_config"));
    tab_can_status_ = lv_tabview_add_tab(tabview_, ui_i18n("tab.can_status"));
    tab_can_config_ = lv_tabview_add_tab(tabview_, ui_i18n("tab.can_config"));
    tab_bms_ctrl_   = lv_tabview_add_tab(tabview_, ui_i18n("tab.bms_control"));
    tab_history_    = lv_tabview_add_tab(tabview_, ui_i18n("tab.history"));

    screen_dashboard_create(tab_dashboard_);
    screen_home_ = create_screen_home(tab_home_);
    screen_battery_create(tab_pack_);
    screen_cells_create(tab_cells_);
    screen_power_create(tab_power_);
    screen_alerts_set_bus(bus_);
    screen_alerts_create(tab_alerts_);
    screen_config_set_bus(bus_);
    screen_config_create(tab_config_);
    screen_tinybms_status_create(tab_tbms_stat_);
    screen_tinybms_config_create(tab_tbms_conf_);
    screen_can_status_create(tab_can_status_);
    screen_can_config_create(tab_can_config_);
    screen_bms_control_create(tab_bms_ctrl_);
    screen_history_set_bus(bus_);
    screen_history_create(tab_history_);
}

void GuiRoot::refresh_language()
{
    if (!tabview_) {
        return;
    }

    lv_tabview_set_tab_name(tabview_, tab_dashboard_, ui_i18n("tab.dashboard"));
    lv_tabview_set_tab_name(tabview_, tab_home_, ui_i18n("tab.home"));
    lv_tabview_set_tab_name(tabview_, tab_pack_, ui_i18n("tab.pack"));
    lv_tabview_set_tab_name(tabview_, tab_cells_, ui_i18n("tab.cells"));
    lv_tabview_set_tab_name(tabview_, tab_power_, ui_i18n("tab.power"));
    lv_tabview_set_tab_name(tabview_, tab_alerts_, ui_i18n("tab.alerts"));
    lv_tabview_set_tab_name(tabview_, tab_config_, ui_i18n("tab.config"));
    lv_tabview_set_tab_name(tabview_, tab_tbms_stat_, ui_i18n("tab.tbms_status"));
    lv_tabview_set_tab_name(tabview_, tab_tbms_conf_, ui_i18n("tab.tbms_config"));
    lv_tabview_set_tab_name(tabview_, tab_can_status_, ui_i18n("tab.can_status"));
    lv_tabview_set_tab_name(tabview_, tab_can_config_, ui_i18n("tab.can_config"));
    lv_tabview_set_tab_name(tabview_, tab_bms_ctrl_, ui_i18n("tab.bms_control"));
    lv_tabview_set_tab_name(tabview_, tab_history_, ui_i18n("tab.history"));

    if (screen_home_) {
        screen_home_->refresh_texts();
    }
    screen_dashboard_refresh_texts();
    screen_power_refresh_texts();
    screen_config_refresh_texts();
}

void GuiRoot::language_listener(void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (self) {
        self->refresh_language();
    }
}

void GuiRoot::register_event_bus_handlers()
{
    if (!bus_) {
        ESP_LOGW(TAG, "Event bus is NULL, skipping subscriptions");
        return;
    }

    constexpr std::array<EventSubscription, 16> subscriptions = {{
        {EVENT_BATTERY_STATUS_UPDATED, telemetry_event_handler},
        {EVENT_SYSTEM_STATUS_UPDATED, system_event_handler},
        {EVENT_PACK_STATS_UPDATED, pack_stats_event_handler},
        {EVENT_TINYBMS_CONNECTED, tinybms_connected_handler},
        {EVENT_TINYBMS_DISCONNECTED, tinybms_disconnected_handler},
        {EVENT_TINYBMS_CONFIG_CHANGED, tinybms_config_changed_handler},
        {EVENT_TINYBMS_REGISTER_UPDATED, tinybms_register_updated_handler},
        {EVENT_TINYBMS_UART_LOG, tinybms_uart_log_handler},
        {EVENT_CVL_LIMITS_UPDATED, cvl_limits_event_handler},
        {EVENT_CONFIG_UPDATED, config_event_handler},
        {EVENT_REMOTE_CMD_RESULT, cmd_result_event_handler},
        {EVENT_ALERTS_ACTIVE_UPDATED, alerts_active_event_handler},
        {EVENT_ALERTS_HISTORY_UPDATED, alerts_history_event_handler},
        {EVENT_ALERT_FILTERS_UPDATED, alert_filters_event_handler},
        {EVENT_HISTORY_UPDATED, history_event_handler},
        {EVENT_HISTORY_EXPORTED, history_export_event_handler},
    }};

    for (const auto &sub : subscriptions) {
        event_bus_subscribe(bus_, sub.type, sub.callback, this);
    }
}

void GuiRoot::telemetry_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_battery_status(*static_cast<const battery_status_t *>(event->data));
}

void GuiRoot::system_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_system_status(*static_cast<const system_status_t *>(event->data));
}

void GuiRoot::pack_stats_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_pack_stats(*static_cast<const pack_stats_t *>(event->data));
}

void GuiRoot::config_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_config(*static_cast<const hmi_config_t *>(event->data));
}

void GuiRoot::cmd_result_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_cmd_result(*static_cast<const cmd_result_t *>(event->data));
}

void GuiRoot::alerts_active_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_alert_list(*static_cast<const alert_list_t *>(event->data), false);
}

void GuiRoot::alerts_history_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_alert_list(*static_cast<const alert_list_t *>(event->data), true);
}

void GuiRoot::alert_filters_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_alert_filters(*static_cast<const alert_filters_t *>(event->data));
}

void GuiRoot::history_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_history(*static_cast<const history_snapshot_t *>(event->data));
}

void GuiRoot::history_export_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_history_export(*static_cast<const history_export_result_t *>(event->data));
}

void GuiRoot::cvl_limits_event_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_cvl_limits(*static_cast<const cvl_limits_event_t *>(event->data));
}

void GuiRoot::tinybms_connected_handler(event_bus_t *, const event_t *, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self) {
        return;
    }

    screen_tinybms_status_update_connection(true);
}

void GuiRoot::tinybms_disconnected_handler(event_bus_t *, const event_t *, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self) {
        return;
    }

    screen_tinybms_status_update_connection(false);
}

void GuiRoot::tinybms_config_changed_handler(event_bus_t *, const event_t *, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self) {
        return;
    }

    self->handle_tinybms_config_changed();
}

void GuiRoot::tinybms_register_updated_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_tinybms_register_update(*static_cast<const tinybms_register_update_t *>(event->data));
}

void GuiRoot::tinybms_uart_log_handler(event_bus_t *, const event_t *event, void *user_ctx)
{
    auto *self = static_cast<GuiRoot *>(user_ctx);
    if (!self || !event || !event->data) {
        return;
    }

    self->handle_tinybms_uart_log(*static_cast<const tinybms_uart_log_entry_t *>(event->data));
}

void GuiRoot::handle_battery_status(const battery_status_t &status)
{
    BatteryContext ctx{status};
    dispatch_to_lvgl(
        [this](BatteryContext &context) {
            if (screen_home_) {
                screen_home_->update_battery(context.status);
            }
            screen_dashboard_update_battery(&context.status);
            screen_battery_update_pack_basic(&context.status);
            screen_power_update(&context.status);
            screen_cells_update_pack(&context.status);
        },
        ctx);
}

void GuiRoot::handle_system_status(const system_status_t &status)
{
    SystemContext ctx{status};
    dispatch_to_lvgl(
        [this](SystemContext &context) {
            if (screen_home_) {
                screen_home_->update_system(context.status);
            }
            screen_dashboard_update_system(&context.status);
            screen_power_update_system(&context.status);
        },
        ctx);
}

void GuiRoot::handle_pack_stats(const pack_stats_t &stats)
{
    PackContext ctx{stats};
    dispatch_to_lvgl(
        [this](PackContext &context) {
            screen_battery_update_pack_stats(&context.stats);
            screen_cells_update_cells(&context.stats);
            screen_dashboard_update_cells(&context.stats);
            if (screen_home_) {
                screen_home_->update_balancing(&context.stats);
            }
        },
        ctx);
}

void GuiRoot::handle_config(const hmi_config_t &config)
{
    ConfigContext ctx{config};
    dispatch_to_lvgl([](ConfigContext &context) { screen_config_apply(&context.config); }, ctx);
}

void GuiRoot::handle_cmd_result(const cmd_result_t &result)
{
    CommandResultContext ctx{result};
    dispatch_to_lvgl([](CommandResultContext &context) { screen_config_show_result(&context.result); }, ctx);
}

void GuiRoot::handle_alert_list(const alert_list_t &alerts, bool is_history)
{
    AlertListContext ctx{alerts, is_history};
    dispatch_to_lvgl(
        [](AlertListContext &context) {
            if (context.is_history) {
                screen_alerts_update_history(&context.alerts);
            } else {
                screen_alerts_update_active(&context.alerts);
            }
        },
        ctx);
}

void GuiRoot::handle_alert_filters(const alert_filters_t &filters)
{
    AlertFiltersContext ctx{filters};
    dispatch_to_lvgl([](AlertFiltersContext &context) { screen_alerts_apply_filters(&context.filters); }, ctx);
}

void GuiRoot::handle_history(const history_snapshot_t &snapshot)
{
    HistoryContext ctx{snapshot};
    dispatch_to_lvgl([](HistoryContext &context) { screen_history_update(&context.snapshot); }, ctx);
}

void GuiRoot::handle_history_export(const history_export_result_t &result)
{
    HistoryExportContext ctx{result};
    dispatch_to_lvgl([](HistoryExportContext &context) { screen_history_show_export(&context.result); }, ctx);
}

void GuiRoot::handle_cvl_limits(const cvl_limits_event_t &limits)
{
    cvl_limits_event_t copy = limits;
    screen_bms_control_update_cvl(&copy);
}

void GuiRoot::handle_tinybms_config_changed()
{
    tinybms_config_t config;
    if (tinybms_model_get_config(&config) == ESP_OK) {
        screen_tinybms_config_update(&config);

        tinybms_stats_t stats;
        if (tinybms_get_stats(&stats) == ESP_OK) {
            screen_tinybms_status_update_stats(&stats);
        }
    }
}

void GuiRoot::handle_tinybms_register_update(const tinybms_register_update_t &update)
{
    tinybms_register_update_t copy = update;
    screen_tinybms_config_apply_register(&copy);
}

void GuiRoot::handle_tinybms_uart_log(const tinybms_uart_log_entry_t &entry)
{
    tinybms_uart_log_entry_t copy = entry;
    screen_tinybms_status_append_log(&copy);
}

}  // namespace gui

