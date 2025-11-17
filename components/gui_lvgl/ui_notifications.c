// components/gui_lvgl/ui_notifications.c

#include "ui_notifications.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event_types.h"

typedef struct {
    cmd_result_t result;
} cmd_result_ctx_t;

typedef struct {
    network_request_t request;
    bool success;
    int  status;
} network_ctx_t;

static event_bus_t *s_bus                 = NULL;
static lv_obj_t    *s_toast               = NULL;
static lv_obj_t    *s_toast_label         = NULL;
static lv_timer_t  *s_toast_timer         = NULL;
static lv_obj_t    *s_loading_card        = NULL;
static lv_obj_t    *s_loading_label       = NULL;
static lv_obj_t    *s_loading_spinner     = NULL;
static uint16_t     s_loading_requests    = 0;
static char         s_last_request_label[80] = "";

static void hide_toast(lv_timer_t *timer)
{
    (void) timer;
    if (s_toast) {
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ensure_toast_created(void)
{
    if (s_toast) {
        return;
    }

    lv_obj_t *layer = lv_layer_top();
    s_toast         = lv_obj_create(layer);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_toast, 12, 0);
    lv_obj_set_style_pad_all(s_toast, 12, 0);
    lv_obj_set_style_border_width(s_toast, 0, 0);
    lv_obj_set_style_shadow_width(s_toast, 8, 0);
    lv_obj_set_style_shadow_opa(s_toast, LV_OPA_40, 0);
    lv_obj_set_style_width(s_toast, LV_SIZE_CONTENT, 0);
    lv_obj_set_style_height(s_toast, LV_SIZE_CONTENT, 0);
    lv_obj_set_flex_flow(s_toast, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_min_width(s_toast, 160, 0);
    lv_obj_center(s_toast);

    s_toast_label = lv_label_create(s_toast);
    lv_label_set_long_mode(s_toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_toast_label, 200);
}

static void show_toast(const char *text, lv_color_t bg_color)
{
    ensure_toast_created();

    lv_obj_set_style_bg_color(s_toast, bg_color, 0);
    lv_label_set_text(s_toast_label, text ? text : "");
    lv_obj_clear_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(s_toast);

    if (!s_toast_timer) {
        s_toast_timer = lv_timer_create(hide_toast, 2500, NULL);
    }
    lv_timer_reset(s_toast_timer);
}

static void ensure_loading_created(lv_obj_t *layer)
{
    if (s_loading_card) {
        return;
    }

    lv_obj_t *parent = layer ? layer : lv_layer_top();
    s_loading_card   = lv_obj_create(parent);
    lv_obj_set_style_pad_all(s_loading_card, 10, 0);
    lv_obj_set_style_radius(s_loading_card, 8, 0);
    lv_obj_set_style_bg_opa(s_loading_card, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(s_loading_card,
                              lv_palette_lighten(LV_PALETTE_GREY, 4),
                              0);
    lv_obj_set_style_border_width(s_loading_card, 0, 0);
    lv_obj_set_style_shadow_width(s_loading_card, 6, 0);
    lv_obj_set_style_shadow_opa(s_loading_card, LV_OPA_35, 0);
    lv_obj_set_flex_flow(s_loading_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_loading_card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(s_loading_card, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(s_loading_card, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    s_loading_spinner = lv_spinner_create(s_loading_card, 800, 90);
    lv_obj_set_size(s_loading_spinner, 28, 28);

    s_loading_label = lv_label_create(s_loading_card);
    lv_label_set_text(s_loading_label, "");

    lv_obj_add_flag(s_loading_card, LV_OBJ_FLAG_HIDDEN);
}

static void update_loading(void)
{
    if (!s_loading_card) {
        ensure_loading_created(lv_layer_top());
    }

    if (s_loading_requests == 0) {
        lv_obj_add_flag(s_loading_card, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(s_loading_card, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(s_loading_label, "%s (%u)",
                          s_last_request_label[0] ? s_last_request_label : "Requête...",
                          (unsigned int) s_loading_requests);
}

static void lvgl_show_cmd_result(void *user_data)
{
    cmd_result_ctx_t *ctx = (cmd_result_ctx_t *) user_data;
    if (!ctx) {
        return;
    }

    lv_color_t color = ctx->result.success
                           ? lv_palette_main(LV_PALETTE_GREEN)
                           : lv_palette_main(LV_PALETTE_RED);
    show_toast(ctx->result.message, color);
    free(ctx);
}

static void on_cmd_result(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    cmd_result_ctx_t *ctx = (cmd_result_ctx_t *) malloc(sizeof(cmd_result_ctx_t));
    if (!ctx) {
        return;
    }
    ctx->result = *(const cmd_result_t *) event->data;
    lv_async_call(lvgl_show_cmd_result, ctx);
}

static void lvgl_on_request_started(void *user_data)
{
    network_ctx_t *ctx = (network_ctx_t *) user_data;
    if (!ctx) {
        return;
    }

    s_loading_requests++;
    snprintf(s_last_request_label, sizeof(s_last_request_label), "%s %s",
             ctx->request.method, ctx->request.path);
    update_loading();
    free(ctx);
}

static void lvgl_on_request_finished(void *user_data)
{
    network_ctx_t *ctx = (network_ctx_t *) user_data;
    if (!ctx) {
        return;
    }

    if (s_loading_requests > 0) {
        s_loading_requests--;
    }
    snprintf(s_last_request_label, sizeof(s_last_request_label), "%s %s",
             ctx->request.method, ctx->request.path);
    update_loading();

    if (!ctx->success) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s %s en échec (%d)",
                 ctx->request.method,
                 ctx->request.path,
                 ctx->status);
        show_toast(msg, lv_palette_main(LV_PALETTE_RED));
    }

    free(ctx);
}

static void on_request_started(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    network_ctx_t *ctx = (network_ctx_t *) malloc(sizeof(network_ctx_t));
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->request = *(const network_request_t *) event->data;
    lv_async_call(lvgl_on_request_started, ctx);
}

static void on_request_finished(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    network_ctx_t *ctx = (network_ctx_t *) malloc(sizeof(network_ctx_t));
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    const network_request_status_t *src = (const network_request_status_t *) event->data;
    ctx->request = src->request;
    ctx->success = src->success;
    ctx->status  = src->status;
    lv_async_call(lvgl_on_request_finished, ctx);
}

void ui_notifications_attach(lv_obj_t *layer)
{
    ensure_loading_created(layer);
    update_loading();
}

void ui_notifications_init(event_bus_t *bus)
{
    s_bus = bus;
    ui_notifications_attach(lv_layer_top());

    if (s_bus) {
        event_bus_subscribe(s_bus, EVENT_REMOTE_CMD_RESULT, on_cmd_result, NULL);
        event_bus_subscribe(s_bus, EVENT_NETWORK_REQUEST_STARTED, on_request_started, NULL);
        event_bus_subscribe(s_bus, EVENT_NETWORK_REQUEST_FINISHED, on_request_finished, NULL);
    }
}
