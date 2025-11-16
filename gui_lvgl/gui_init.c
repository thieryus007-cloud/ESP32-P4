// components/gui_lvgl/gui_init.c

#include "gui_init.h"

#include "screen_home.h"
#include "screen_battery.h"
#include "screen_cells.h"
#include "screen_power.h"
#include "screen_config.h"

#include "event_bus.h"
#include "event_types.h"

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

// --- Callbacks exécutés dans le contexte LVGL (via lv_async_call) ---

static void lvgl_apply_battery_update(void *user_data)
{
    gui_batt_ctx_t *ctx = (gui_batt_ctx_t *) user_data;
    if (ctx) {
        // Home + résumé pack + power flow + cells (pour afficher la tension de référence)
        screen_home_update_battery(&ctx->status);
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
        screen_power_update_system(&ctx->status);
        free(ctx);
    }
}

static void lvgl_apply_pack_update(void *user_data)
{
    gui_pack_ctx_t *ctx = (gui_pack_ctx_t *) user_data;
    if (ctx) {
        screen_battery_update_pack_stats(&ctx->stats);
        screen_cells_update_cells(&ctx->stats);
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

// --- API publique ---

void gui_init(event_bus_t *bus)
{
    s_bus = bus;

    ESP_LOGI(TAG, "Initializing GUI (LVGL with Home + Pack + Cells + Power + Config tabs)");

    // ⚠️ Hypothèse : LVGL + driver écran + esp_lvgl_port sont déjà initialisés

    lv_obj_t *root = lv_scr_act();

    // Tabview avec 5 onglets
    lv_obj_t *tabview = lv_tabview_create(root, LV_DIR_TOP, 40);

    lv_obj_t *tab_home   = lv_tabview_add_tab(tabview, "Home");
    lv_obj_t *tab_pack   = lv_tabview_add_tab(tabview, "Pack");
    lv_obj_t *tab_cells  = lv_tabview_add_tab(tabview, "Cells");
    lv_obj_t *tab_power  = lv_tabview_add_tab(tabview, "Power");
    lv_obj_t *tab_config = lv_tabview_add_tab(tabview, "Config");

    screen_home_create(tab_home);
    screen_battery_create(tab_pack);
    screen_cells_create(tab_cells);
    screen_power_create(tab_power);
    screen_config_create(tab_config);

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
    }
}

void gui_start(void)
{
    // Dans beaucoup de configs avec esp_lvgl_port, la task LVGL tourne déjà.
    ESP_LOGI(TAG, "GUI started");
}
