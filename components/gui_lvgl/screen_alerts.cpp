// components/gui_lvgl/screen_alerts.cpp

#include "screen_alerts.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static event_bus_t *s_bus = NULL;
static lv_obj_t    *s_active_list = NULL;
static lv_obj_t    *s_history_list = NULL;
static lv_obj_t    *s_filter_slider = NULL;
static lv_obj_t    *s_filter_sev_label = NULL;
static lv_obj_t    *s_filter_switch = NULL;
static lv_obj_t    *s_filter_source = NULL;
static alert_list_t s_last_active = { 0 };
static alert_list_t s_last_history = { 0 };
static alert_filters_t s_current_filters = { 0 };

static const char *severity_to_text(int sev)
{
    switch (sev) {
        case 4: return "Critical";
        case 3: return "Error";
        case 2: return "Warning";
        case 1: return "Info";
        default: return "None";
    }
}

static void publish_refresh_history(void)
{
    if (!s_bus) {
        return;
    }
    event_t evt = {
        .type = EVENT_USER_INPUT_REFRESH_ALERT_HISTORY,
        .data = NULL,
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_filters(void)
{
    if (!s_bus) {
        return;
    }
    alert_filters_t filters = s_current_filters;
    if (s_filter_source) {
        const char *source_text = lv_textarea_get_text(s_filter_source);
        if (source_text) {
            strncpy(filters.source_filter, source_text, sizeof(filters.source_filter) - 1);
        }
    }
    if (s_filter_switch) {
        filters.hide_acknowledged = lv_obj_has_state(s_filter_switch, LV_STATE_CHECKED);
    }

    event_t evt = {
        .type = EVENT_USER_INPUT_UPDATE_ALERT_FILTERS,
        .data = &filters,
        .data_size = sizeof(filters),
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_ack(int alert_id)
{
    if (!s_bus || alert_id <= 0) {
        return;
    }

    user_input_ack_alert_t req = {
        .alert_id = alert_id,
    };
    event_t evt = {
        .type = EVENT_USER_INPUT_ACK_ALERT,
        .data = &req,
        .data_size = sizeof(req),
    };
    event_bus_publish(s_bus, &evt);
}

static void on_ack_clicked(lv_event_t *e)
{
    int alert_id = (int) (intptr_t) lv_event_get_user_data(e);
    publish_ack(alert_id);
}

static void on_filter_slider(lv_event_t *e)
{
    (void) e;
    if (!s_filter_sev_label) return;

    int sev = (int) lv_slider_get_value(lv_event_get_target(e));
    s_current_filters.min_severity = sev;
    lv_label_set_text_fmt(s_filter_sev_label, "Min severity: %s (%d)",
                          severity_to_text(sev), sev);
    publish_filters();
}

static void on_filter_switch(lv_event_t *e)
{
    (void) e;
    publish_filters();
}

static void on_filter_source(lv_event_t *e)
{
    if (e && (e->code == LV_EVENT_DEFOCUSED || e->code == LV_EVENT_READY)) {
        publish_filters();
    }
}

static void on_refresh_history(lv_event_t *e)
{
    (void) e;
    publish_refresh_history();
}

static lv_obj_t *create_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_width(cont, LV_PCT(100));
    lv_obj_set_style_pad_all(cont, 8, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_10, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    return cont;
}

static void render_alert_list(lv_obj_t *container, const alert_list_t *list, bool allow_ack)
{
    if (!container) {
        return;
    }

    lv_obj_clean(container);

    if (!list || list->count == 0) {
        lv_obj_t *lbl = lv_label_create(container);
        lv_label_set_text(lbl, allow_ack ? "Aucune alerte active" : "Historique vide");
        return;
    }

    for (uint8_t i = 0; i < list->count; ++i) {
        const alert_entry_t *a = &list->entries[i];
        lv_obj_t *row = lv_obj_create(container);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_style_pad_all(row, 6, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_10, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *col = lv_obj_create(row);
        lv_obj_remove_style_all(col);
        lv_obj_set_width(col, LV_PCT(70));
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(col, 1);

        lv_obj_t *msg = lv_label_create(col);
        lv_label_set_text_fmt(msg, "[%s] %s", severity_to_text(a->severity), a->message);
        lv_obj_set_style_text_color(msg,
                                    a->severity >= 3 ? lv_palette_main(LV_PALETTE_RED)
                                                     : lv_palette_main(LV_PALETTE_ORANGE),
                                    0);

        lv_obj_t *meta = lv_label_create(col);
        lv_label_set_text_fmt(meta, "ID:%d Src:%s Status:%s Ack:%s", a->id, a->source,
                              a->status,
                              a->acknowledged ? "yes" : "no");
        lv_obj_set_style_text_color(meta, lv_palette_main(LV_PALETTE_GREY), 0);

        lv_obj_t *time_lbl = lv_label_create(col);
        lv_label_set_text_fmt(time_lbl, "Timestamp: %llu ms", (unsigned long long) a->timestamp_ms);
        lv_obj_set_style_text_color(time_lbl, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);

        if (allow_ack) {
            lv_obj_t *btn = lv_btn_create(row);
            lv_obj_set_width(btn, LV_SIZE_CONTENT);
            lv_obj_add_event_cb(btn, on_ack_clicked, LV_EVENT_CLICKED, (void *) (intptr_t) a->id);
            if (a->acknowledged) {
                lv_obj_add_state(btn, LV_STATE_DISABLED);
            }
            lv_obj_t *lbl_btn = lv_label_create(btn);
            lv_label_set_text(lbl_btn, "Acknowledge");
        }
    }
}

void screen_alerts_set_bus(event_bus_t *bus)
{
    s_bus = bus;
}

void screen_alerts_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 6, 0);

    lv_obj_t *filter_section = create_section(parent, "Filtres / Seuils");
    lv_obj_t *row_filters = lv_obj_create(filter_section);
    lv_obj_remove_style_all(row_filters);
    lv_obj_set_width(row_filters, LV_PCT(100));
    lv_obj_set_flex_flow(row_filters, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_filters, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_filter_slider = lv_slider_create(row_filters);
    lv_slider_set_range(s_filter_slider, 0, 4);
    lv_slider_set_value(s_filter_slider, s_current_filters.min_severity, LV_ANIM_OFF);
    lv_obj_set_width(s_filter_slider, LV_PCT(40));
    lv_obj_add_event_cb(s_filter_slider, on_filter_slider, LV_EVENT_VALUE_CHANGED, NULL);

    s_filter_sev_label = lv_label_create(row_filters);
    lv_label_set_text(s_filter_sev_label, "Min severity: None (0)");

    s_filter_switch = lv_switch_create(row_filters);
    lv_obj_add_event_cb(s_filter_switch, on_filter_switch, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *switch_lbl = lv_label_create(row_filters);
    lv_label_set_text(switch_lbl, "Masquer ack");

    s_filter_source = lv_textarea_create(filter_section);
    lv_textarea_set_one_line(s_filter_source, true);
    lv_textarea_set_placeholder_text(s_filter_source, "Filtrer par source");
    lv_obj_set_width(s_filter_source, LV_PCT(100));
    lv_obj_add_event_cb(s_filter_source, on_filter_source, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(s_filter_source, on_filter_source, LV_EVENT_READY, NULL);

    lv_obj_t *active_section = create_section(parent, "Alertes actives");
    s_active_list = lv_obj_create(active_section);
    lv_obj_remove_style_all(s_active_list);
    lv_obj_set_width(s_active_list, LV_PCT(100));
    lv_obj_set_flex_flow(s_active_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_active_list, 4, 0);

    lv_obj_t *history_header = lv_obj_create(parent);
    lv_obj_remove_style_all(history_header);
    lv_obj_set_width(history_header, LV_PCT(100));
    lv_obj_set_flex_flow(history_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(history_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *history_title = lv_label_create(history_header);
    lv_label_set_text(history_title, "Historique des alertes");

    lv_obj_t *btn_refresh = lv_btn_create(history_header);
    lv_obj_add_event_cb(btn_refresh, on_refresh_history, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_refresh = lv_label_create(btn_refresh);
    lv_label_set_text(lbl_refresh, "Recharger");

    s_history_list = lv_obj_create(parent);
    lv_obj_remove_style_all(s_history_list);
    lv_obj_set_width(s_history_list, LV_PCT(100));
    lv_obj_set_flex_flow(s_history_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_history_list, 4, 0);

    render_alert_list(s_active_list, &s_last_active, true);
    render_alert_list(s_history_list, &s_last_history, false);
    screen_alerts_apply_filters(&s_current_filters);
    publish_refresh_history();
}

void screen_alerts_update_active(const alert_list_t *list)
{
    if (!list) return;
    s_last_active = *list;
    render_alert_list(s_active_list, &s_last_active, true);
}

void screen_alerts_update_history(const alert_list_t *list)
{
    if (!list) return;
    s_last_history = *list;
    render_alert_list(s_history_list, &s_last_history, false);
}

void screen_alerts_apply_filters(const alert_filters_t *filters)
{
    if (!filters) return;
    s_current_filters = *filters;

    if (s_filter_slider) {
        lv_slider_set_value(s_filter_slider, filters->min_severity, LV_ANIM_OFF);
    }
    if (s_filter_sev_label) {
        lv_label_set_text_fmt(s_filter_sev_label, "Min severity: %s (%d)",
                              severity_to_text(filters->min_severity), filters->min_severity);
    }
    if (s_filter_switch) {
        if (filters->hide_acknowledged) {
            lv_obj_add_state(s_filter_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_filter_switch, LV_STATE_CHECKED);
        }
    }
    if (s_filter_source) {
        lv_textarea_set_text(s_filter_source, filters->source_filter);
    }
}

