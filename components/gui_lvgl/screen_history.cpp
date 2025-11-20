// components/gui_lvgl/screen_history.c

#include "screen_history.h"

#include <stdio.h>
#include <string.h>

static event_bus_t *s_bus = NULL;

static lv_obj_t *s_chart_voltage = NULL;
static lv_obj_t *s_chart_current = NULL;
static lv_obj_t *s_chart_temp    = NULL;
static lv_obj_t *s_chart_soc     = NULL;
static lv_obj_t *s_label_range   = NULL;
static lv_obj_t *s_slider_zoom   = NULL;
static lv_obj_t *s_slider_offset = NULL;
static lv_obj_t *s_status_export = NULL;

static history_snapshot_t s_last_snapshot = { 0 };

static const char *range_to_text(history_range_t range)
{
    switch (range) {
        case HISTORY_RANGE_LAST_DAY:  return "24h";
        case HISTORY_RANGE_LAST_WEEK: return "7 jours";
        case HISTORY_RANGE_LAST_HOUR:
        default:                      return "1h";
    }
}

static void publish_request(history_range_t range)
{
    if (!s_bus) {
        return;
    }
    user_input_history_request_t req = { .range = range };
    event_t evt = {
        .type = EVENT_USER_INPUT_REQUEST_HISTORY,
        .data = &req,
        .data_size = sizeof(req),
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_export(history_range_t range)
{
    if (!s_bus) {
        return;
    }
    user_input_history_export_t req = { .range = range };
    event_t evt = {
        .type = EVENT_USER_INPUT_EXPORT_HISTORY,
        .data = &req,
        .data_size = sizeof(req),
    };
    event_bus_publish(s_bus, &evt);
}

static void on_range_click(lv_event_t *e)
{
    if (!e) {
        return;
    }
    lv_obj_t *btn = lv_event_get_target(e);
    history_range_t range = (history_range_t) (uintptr_t) lv_event_get_user_data(e);
    lv_obj_add_state(btn, LV_STATE_CHECKED);
    publish_request(range);
}

static void on_export_click(lv_event_t *e)
{
    (void) e;
    publish_export(s_last_snapshot.range);
}

static void on_zoom_change(lv_event_t *e)
{
    (void) e;
    screen_history_update(&s_last_snapshot);
}

static void on_offset_change(lv_event_t *e)
{
    (void) e;
    screen_history_update(&s_last_snapshot);
}

void screen_history_set_bus(event_bus_t *bus)
{
    s_bus = bus;
}

static lv_obj_t *create_chart(lv_obj_t *parent, const char *title, lv_color_t color)
{
    lv_obj_t *wrapper = lv_obj_create(parent);
    lv_obj_set_width(wrapper, LV_PCT(100));
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wrapper, 6, 0);

    lv_obj_t *label = lv_label_create(wrapper);
    lv_label_set_text(label, title);

    lv_obj_t *chart = lv_chart_create(wrapper);
    lv_obj_set_size(chart, LV_PCT(100), 120);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_chart_set_point_count(chart, 10);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -5000, 5000);

    lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
    return chart;
}

static void fill_chart(lv_obj_t *chart, const float *values, uint16_t count)
{
    if (!chart) {
        return;
    }

    lv_chart_set_point_count(chart, count > 2 ? count : 2);
    lv_chart_series_t *series = lv_chart_get_series_next(chart, NULL);
    if (!series) {
        return;
    }
    lv_chart_clear_series(chart, series);

    for (uint16_t i = 0; i < count; ++i) {
        lv_chart_set_value_by_id(chart, series, i, values[i]);
    }
}

static void apply_snapshot_window(const history_snapshot_t *snapshot,
                                  float *voltage,
                                  float *current,
                                  float *temperature,
                                  float *soc,
                                  uint16_t *out_count)
{
    if (!snapshot || snapshot->count == 0) {
        *out_count = 0;
        return;
    }

    uint16_t zoom = s_slider_zoom ? (uint16_t) lv_slider_get_value(s_slider_zoom) : 100;
    uint16_t offset = s_slider_offset ? (uint16_t) lv_slider_get_value(s_slider_offset) : 0;

    uint16_t count = snapshot->count;
    uint16_t visible = (count * zoom) / 100;
    if (visible < 2) {
        visible = 2;
    }
    if (visible > count) {
        visible = count;
    }

    uint16_t max_offset = (count > visible) ? (count - visible) : 0;
    if (offset > 100) {
        offset = 100;
    }
    uint16_t start = (max_offset * offset) / 100;

    for (uint16_t i = 0; i < visible; ++i) {
        const history_sample_t *s = &snapshot->samples[start + i];
        voltage[i]     = s->voltage;
        current[i]     = s->current;
        temperature[i] = s->temperature;
        soc[i]         = s->soc;
    }

    *out_count = visible;
}

void screen_history_update(const history_snapshot_t *snapshot)
{
    if (!snapshot) {
        return;
    }
    s_last_snapshot = *snapshot;

    if (s_label_range) {
        const char *src = snapshot->from_backend ? "cloud" : "local";
        char buf[64];
        snprintf(buf, sizeof(buf), "Fenêtre: %s (%s)", range_to_text(snapshot->range), src);
        lv_label_set_text(s_label_range, buf);
    }

    if (snapshot->count == 0) {
        return;
    }

    float voltage[HISTORY_SNAPSHOT_MAX];
    float current[HISTORY_SNAPSHOT_MAX];
    float temperature[HISTORY_SNAPSHOT_MAX];
    float soc[HISTORY_SNAPSHOT_MAX];
    uint16_t count = 0;

    apply_snapshot_window(snapshot, voltage, current, temperature, soc, &count);
    if (count == 0) {
        return;
    }

    fill_chart(s_chart_voltage, voltage, count);
    fill_chart(s_chart_current, current, count);
    fill_chart(s_chart_temp, temperature, count);
    fill_chart(s_chart_soc, soc, count);
}

void screen_history_show_export(const history_export_result_t *result)
{
    if (!s_status_export || !result) {
        return;
    }

    char buf[96];
    if (result->success) {
        snprintf(buf, sizeof(buf), "Export OK (%zu points) -> %s", result->exported_count, result->path);
    } else {
        snprintf(buf, sizeof(buf), "Export échoué");
    }
    lv_label_set_text(s_status_export, buf);
}

void screen_history_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_style_pad_row(parent, 6, 0);

    lv_obj_t *toolbar = lv_obj_create(parent);
    lv_obj_remove_style_all(toolbar);
    lv_obj_set_width(toolbar, LV_PCT(100));
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toolbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *range_group = lv_obj_create(toolbar);
    lv_obj_remove_style_all(range_group);
    lv_obj_set_flex_flow(range_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(range_group, 4, 0);

    lv_obj_t *btn_1h = lv_btn_create(range_group);
    lv_obj_add_event_cb(btn_1h, on_range_click, LV_EVENT_CLICKED, (void *) HISTORY_RANGE_LAST_HOUR);
    lv_label_set_text(lv_label_create(btn_1h), "1h");

    lv_obj_t *btn_24h = lv_btn_create(range_group);
    lv_obj_add_event_cb(btn_24h, on_range_click, LV_EVENT_CLICKED, (void *) HISTORY_RANGE_LAST_DAY);
    lv_label_set_text(lv_label_create(btn_24h), "24h");

    lv_obj_t *btn_7d = lv_btn_create(range_group);
    lv_obj_add_event_cb(btn_7d, on_range_click, LV_EVENT_CLICKED, (void *) HISTORY_RANGE_LAST_WEEK);
    lv_label_set_text(lv_label_create(btn_7d), "7j");

    lv_obj_t *btn_export = lv_btn_create(toolbar);
    lv_obj_add_event_cb(btn_export, on_export_click, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(btn_export), "Export CSV");

    s_label_range = lv_label_create(parent);
    lv_label_set_text(s_label_range, "Fenêtre: --");

    lv_obj_t *zoom_row = lv_obj_create(parent);
    lv_obj_remove_style_all(zoom_row);
    lv_obj_set_width(zoom_row, LV_PCT(100));
    lv_obj_set_flex_flow(zoom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(zoom_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_label_set_text(lv_label_create(zoom_row), "Zoom");
    s_slider_zoom = lv_slider_create(zoom_row);
    lv_slider_set_range(s_slider_zoom, 10, 100);
    lv_slider_set_value(s_slider_zoom, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider_zoom, on_zoom_change, LV_EVENT_VALUE_CHANGED, NULL);

    lv_label_set_text(lv_label_create(zoom_row), "Scroll");
    s_slider_offset = lv_slider_create(zoom_row);
    lv_slider_set_range(s_slider_offset, 0, 100);
    lv_slider_set_value(s_slider_offset, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider_offset, on_offset_change, LV_EVENT_VALUE_CHANGED, NULL);

    s_chart_voltage = create_chart(parent, "Tension (V)", lv_palette_main(LV_PALETTE_BLUE));
    s_chart_current = create_chart(parent, "Courant (A)", lv_palette_main(LV_PALETTE_GREEN));
    s_chart_temp    = create_chart(parent, "Temp (°C)", lv_palette_main(LV_PALETTE_ORANGE));
    s_chart_soc     = create_chart(parent, "SOC (%)", lv_palette_main(LV_PALETTE_TEAL));

    s_status_export = lv_label_create(parent);
    lv_label_set_text(s_status_export, "Export CSV en attente...");

    // Charger un historique initial
    publish_request(HISTORY_RANGE_LAST_HOUR);
}
