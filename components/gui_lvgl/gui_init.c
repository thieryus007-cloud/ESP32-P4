// components/gui_lvgl/gui_init.c

#include "gui_init.h"

#include "screen_home.h"
#include "screen_dashboard.h"
#include "screen_battery.h"
#include "screen_cells.h"
#include "screen_power.h"
#include "screen_config.h"
#include "screen_tinybms_status.h"
#include "screen_tinybms_config.h"
#include "screen_can_status.h"
#include "screen_can_config.h"
#include "screen_bms_control.h"

#include "event_bus.h"
#include "event_types.h"
#include "tinybms_client.h"
#include "tinybms_model.h"
#include "can_victron.h"

#include "lvgl.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "GUI_INIT";

// On garde un pointeur vers l'EventBus si besoin ultérieur
static event_bus_t *s_bus = NULL;

/*
 * Pour rester thread-safe avec LVGL :
 * - les callbacks EventBus s'exécutent dans le contexte des tasks réseau / adapter
 * - on NE DOIT PAS appeler directement LVGL depuis ces tasks
 * - on utilise lv_async_call() pour exécuter la mise à jour dans le contexte LVGL
 */

typedef struct {
    battery_status_t status;
} gui_batt_ctx_t;

typedef struct {
    system_status_t status;
} gui_sys_ctx_t;

typedef struct {
    pack_stats_t stats;
} gui_pack_ctx_t;

typedef struct {
    tinybms_config_t config;
} gui_tinybms_ctx_t;

// --- Callbacks exécutés dans le contexte LVGL (via lv_async_call) ---

static void lvgl_apply_battery_update(void *user_data)
{
    gui_batt_ctx_t *ctx = (gui_batt_ctx_t *) user_data;
    if (ctx) {
        // Home + résumé pack + power flow + cells (pack global)
        screen_home_update_battery(&ctx->status);
        screen_dashboard_update_battery(&ctx->status);
        screen_battery_update_pack_basic(&ctx->status);
        screen_power_update(&ctx->status);
        screen_cells_update_pack(&ctx->status);
        free(ctx);
    }
}

static void lvgl_apply_system_update(void *user_data)
{
    gui_sys_ctx_t *ctx = (gui_sys_ctx_t *) user_data;
    if (ctx) {
        screen_home_update_system(&ctx->status);
        screen_dashboard_update_system(&ctx->status);
        screen_power_update_system(&ctx->status);
        free(ctx);
    }
}

static void lvgl_apply_pack_update(void *user_data)
{
    gui_pack_ctx_t *ctx = (gui_pack_ctx_t *) user_data;
    if (ctx) {
        // Pack : stats + table
        screen_battery_update_pack_stats(&ctx->stats);
        // Cells : barres + indicateurs de balancing
        screen_cells_update_cells(&ctx->stats);
        // 🔹 Home : badge global de balancing
        screen_home_update_balancing(&ctx->stats);
        free(ctx);
    }
}

// --- Callbacks EventBus (contexte tasks "non LVGL") ---

static void telemetry_event_handler(event_bus_t *bus,
                                    const event_t *event,
                                    void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const battery_status_t *src = (const battery_status_t *) event->data;

    gui_batt_ctx_t *ctx = (gui_batt_ctx_t *) malloc(sizeof(gui_batt_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to alloc gui_batt_ctx_t");
        return;
    }

    ctx->status = *src;  // copie par valeur

    // Planifie la mise à jour dans le contexte LVGL
    lv_async_call(lvgl_apply_battery_update, ctx);
}

static void system_event_handler(event_bus_t *bus,
                                 const event_t *event,
                                 void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const system_status_t *src = (const system_status_t *) event->data;

    gui_sys_ctx_t *ctx = (gui_sys_ctx_t *) malloc(sizeof(gui_sys_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to alloc gui_sys_ctx_t");
        return;
    }

    ctx->status = *src;

    lv_async_call(lvgl_apply_system_update, ctx);
}

static void pack_stats_event_handler(event_bus_t *bus,
                                     const event_t *event,
                                     void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const pack_stats_t *src = (const pack_stats_t *) event->data;

    gui_pack_ctx_t *ctx = (gui_pack_ctx_t *) malloc(sizeof(gui_pack_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to alloc gui_pack_ctx_t");
        return;
    }

    ctx->stats = *src;

    lv_async_call(lvgl_apply_pack_update, ctx);
}

// CVL limits updated handler
static void cvl_limits_event_handler(event_bus_t *bus,
                                      const event_t *event,
                                      void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const cvl_limits_event_t *limits = (const cvl_limits_event_t *) event->data;

    // Update screen_bms_control directly (already in LVGL task context)
    screen_bms_control_update_cvl(limits);
}

// TinyBMS connection event handler
static void tinybms_connected_handler(event_bus_t *bus,
                                       const event_t *event,
                                       void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;

    ESP_LOGI(TAG, "TinyBMS connected event");
    screen_tinybms_status_update_connection(true);
}

// TinyBMS disconnection event handler
static void tinybms_disconnected_handler(event_bus_t *bus,
                                          const event_t *event,
                                          void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;

    ESP_LOGI(TAG, "TinyBMS disconnected event");
    screen_tinybms_status_update_connection(false);
}

// TinyBMS configuration changed handler
static void tinybms_config_changed_handler(event_bus_t *bus,
                                            const event_t *event,
                                            void *user_ctx)
{
    (void) bus;
    (void) event;
    (void) user_ctx;

    ESP_LOGI(TAG, "TinyBMS config changed event");

    // Get config snapshot and update GUI
    tinybms_config_t config;
    if (tinybms_model_get_config(&config) == ESP_OK) {
        screen_tinybms_config_update(&config);

        // Update stats
        tinybms_stats_t stats;
        if (tinybms_get_stats(&stats) == ESP_OK) {
            screen_tinybms_status_update_stats(stats.reads_ok, stats.reads_failed,
                                                stats.writes_ok, stats.writes_failed,
                                                stats.crc_errors, stats.timeouts);
        }
    }
}

// --- API publique ---

void gui_init(event_bus_t *bus)
{
    s_bus = bus;

    ESP_LOGI(TAG, "Initializing GUI (LVGL with dashboard + existing tabs: Home, Pack, Cells, Power, Config, TinyBMS, CAN, BMS Control)");

    // ⚠️ Hypothèse : LVGL + driver écran + esp_lvgl_port sont déjà initialisés

    lv_obj_t *root = lv_scr_act();

    // Tabview avec Dashboard + 10 onglets historiques
    lv_obj_t *tabview = lv_tabview_create(root, LV_DIR_TOP, 35);

    lv_obj_t *tab_dashboard = lv_tabview_add_tab(tabview, "Dashboard");
    lv_obj_t *tab_home   = lv_tabview_add_tab(tabview, "Home");
    lv_obj_t *tab_pack   = lv_tabview_add_tab(tabview, "Pack");
    lv_obj_t *tab_cells  = lv_tabview_add_tab(tabview, "Cells");
    lv_obj_t *tab_power  = lv_tabview_add_tab(tabview, "Power");
    lv_obj_t *tab_config = lv_tabview_add_tab(tabview, "Config");
    lv_obj_t *tab_tbms_status = lv_tabview_add_tab(tabview, "TBMS Status");
    lv_obj_t *tab_tbms_config = lv_tabview_add_tab(tabview, "TBMS Config");
    lv_obj_t *tab_can_status = lv_tabview_add_tab(tabview, "CAN Status");
    lv_obj_t *tab_can_config = lv_tabview_add_tab(tabview, "CAN Config");
    lv_obj_t *tab_bms_control = lv_tabview_add_tab(tabview, "BMS Control");

    screen_dashboard_create(tab_dashboard);
    screen_home_create(tab_home);
    screen_battery_create(tab_pack);
    screen_cells_create(tab_cells);
    screen_power_create(tab_power);
    screen_config_create(tab_config);
    screen_tinybms_status_create(tab_tbms_status);
    screen_tinybms_config_create(tab_tbms_config);
    screen_can_status_create(tab_can_status);
    screen_can_config_create(tab_can_config);
    screen_bms_control_create(tab_bms_control);

    // Abonnements EventBus
    if (s_bus) {
        event_bus_subscribe(s_bus,
                            EVENT_BATTERY_STATUS_UPDATED,
                            telemetry_event_handler,
                            NULL);

        event_bus_subscribe(s_bus,
                            EVENT_SYSTEM_STATUS_UPDATED,
                            system_event_handler,
                            NULL);

        event_bus_subscribe(s_bus,
                            EVENT_PACK_STATS_UPDATED,
                            pack_stats_event_handler,
                            NULL);

        // TinyBMS events
        event_bus_subscribe(s_bus,
                            EVENT_TINYBMS_CONNECTED,
                            tinybms_connected_handler,
                            NULL);

        event_bus_subscribe(s_bus,
                            EVENT_TINYBMS_DISCONNECTED,
                            tinybms_disconnected_handler,
                            NULL);

        event_bus_subscribe(s_bus,
                            EVENT_TINYBMS_CONFIG_CHANGED,
                            tinybms_config_changed_handler,
                            NULL);

        // CAN/CVL events
        event_bus_subscribe(s_bus,
                            EVENT_CVL_LIMITS_UPDATED,
                            cvl_limits_event_handler,
                            NULL);
    }
}

void gui_start(void)
{
    // Dans beaucoup de configs avec esp_lvgl_port, la task LVGL tourne déjà.
    ESP_LOGI(TAG, "GUI started");
}
