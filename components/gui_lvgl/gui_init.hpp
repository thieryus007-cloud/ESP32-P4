// components/gui_lvgl/gui_init.hpp
#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "event_bus.h"
#include "lvgl.h"

#ifdef __cplusplus

namespace gui {

class ScreenHome;
class ScreenDashboard;
class ScreenBattery;
class ScreenCells;
class ScreenPower;
class ScreenAlerts;
class ScreenConfig;
class ScreenTinybmsStatus;
class ScreenTinybmsConfig;
class ScreenCanStatus;
class ScreenCanConfig;
class ScreenBmsControl;
class ScreenHistory;

struct BatteryContext {
    battery_status_t status{};
};

struct SystemContext {
    system_status_t status{};
};

struct PackContext {
    pack_stats_t stats{};
};

struct ConfigContext {
    hmi_config_t config{};
};

struct CommandResultContext {
    cmd_result_t result{};
};

struct AlertListContext {
    alert_list_t alerts{};
    bool         is_history{false};
};

struct AlertFiltersContext {
    alert_filters_t filters{};
};

struct HistoryContext {
    history_snapshot_t snapshot{};
};

struct HistoryExportContext {
    history_export_result_t result{};
};

namespace detail {

template <typename T, typename Fn>
struct LvglDispatchPayload {
    Fn fn;
    std::unique_ptr<T> data;
};

template <typename T, typename Fn>
void lvgl_dispatch_trampoline(void *raw)
{
    std::unique_ptr<LvglDispatchPayload<T, Fn>> payload(static_cast<LvglDispatchPayload<T, Fn> *>(raw));
    payload->fn(*payload->data);
}

}  // namespace detail

/**
 * @brief Helper pour planifier une mise à jour dans le contexte LVGL sans malloc/free manuels.
 */
template <typename T, typename Fn>
void dispatch_to_lvgl(Fn &&fn, T &&data)
{
    using DataT = std::decay_t<T>;
    using FnT   = std::decay_t<Fn>;

    auto payload         = std::make_unique<detail::LvglDispatchPayload<DataT, FnT>>();
    payload->fn          = std::forward<Fn>(fn);
    payload->data        = std::make_unique<DataT>(std::forward<T>(data));
    auto *payload_raw    = payload.release();
    lv_async_call(detail::lvgl_dispatch_trampoline<DataT, FnT>, payload_raw);
}

class GuiRoot {
public:
    explicit GuiRoot(event_bus_t *bus);

    void init();
    void start();

    void create_tabs();
    void refresh_language();

private:
    struct EventSubscription {
        event_type_t    type;
        event_callback_t callback;
    };

    static void language_listener(void *user_ctx);

    static void telemetry_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void system_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void pack_stats_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void config_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void cmd_result_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void alerts_active_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void alerts_history_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void alert_filters_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void history_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void history_export_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void cvl_limits_event_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void tinybms_connected_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void tinybms_disconnected_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void tinybms_config_changed_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void tinybms_register_updated_handler(event_bus_t *bus, const event_t *event, void *user_ctx);
    static void tinybms_uart_log_handler(event_bus_t *bus, const event_t *event, void *user_ctx);

    void register_event_bus_handlers();

    void handle_battery_status(const battery_status_t &status);
    void handle_system_status(const system_status_t &status);
    void handle_pack_stats(const pack_stats_t &stats);
    void handle_config(const hmi_config_t &config);
    void handle_cmd_result(const cmd_result_t &result);
    void handle_alert_list(const alert_list_t &alerts, bool is_history);
    void handle_alert_filters(const alert_filters_t &filters);
    void handle_history(const history_snapshot_t &snapshot);
    void handle_history_export(const history_export_result_t &result);
    void handle_cvl_limits(const cvl_limits_event_t &limits);
    void handle_tinybms_config_changed();
    void handle_tinybms_register_update(const tinybms_register_update_t &update);
    void handle_tinybms_uart_log(const tinybms_uart_log_entry_t &entry);

    event_bus_t *bus_;
    lv_obj_t    *tabview_        = nullptr;
    lv_obj_t    *tab_dashboard_  = nullptr;
    lv_obj_t    *tab_home_       = nullptr;
    lv_obj_t    *tab_pack_       = nullptr;
    lv_obj_t    *tab_cells_      = nullptr;
    lv_obj_t    *tab_power_      = nullptr;
    lv_obj_t    *tab_alerts_     = nullptr;
    lv_obj_t    *tab_config_     = nullptr;
    lv_obj_t    *tab_tbms_stat_  = nullptr;
    lv_obj_t    *tab_tbms_conf_  = nullptr;
    lv_obj_t    *tab_can_status_ = nullptr;
    lv_obj_t    *tab_can_config_ = nullptr;
    lv_obj_t    *tab_bms_ctrl_   = nullptr;
    lv_obj_t    *tab_history_    = nullptr;

    std::unique_ptr<ScreenHome>          screen_home_;
    std::unique_ptr<ScreenDashboard>     screen_dashboard_;
    std::unique_ptr<ScreenBattery>       screen_battery_;
    std::unique_ptr<ScreenCells>         screen_cells_;
    std::unique_ptr<ScreenPower>         screen_power_;
    std::unique_ptr<ScreenAlerts>        screen_alerts_;
    std::unique_ptr<ScreenConfig>        screen_config_;
    std::unique_ptr<ScreenTinybmsStatus> screen_tinybms_status_;
    std::unique_ptr<ScreenTinybmsConfig> screen_tinybms_config_;
    std::unique_ptr<ScreenCanStatus>     screen_can_status_;
    std::unique_ptr<ScreenCanConfig>     screen_can_config_;
    std::unique_ptr<ScreenBmsControl>    screen_bms_control_;
    std::unique_ptr<ScreenHistory>       screen_history_;
};

}  // namespace gui

#endif  // __cplusplus

