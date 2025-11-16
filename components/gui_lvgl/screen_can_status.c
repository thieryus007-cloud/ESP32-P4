// components/gui_lvgl/screen_can_status.c

#include "screen_can_status.h"
#include "lvgl.h"
#include <stdio.h>

// Pointeurs vers les widgets
static lv_obj_t *s_label_driver_status = NULL;
static lv_obj_t *s_label_keepalive_status = NULL;
static lv_obj_t *s_label_bus_state = NULL;
static lv_obj_t *s_label_tx_count = NULL;
static lv_obj_t *s_label_rx_count = NULL;
static lv_obj_t *s_label_tx_errors = NULL;
static lv_obj_t *s_label_rx_errors = NULL;
static lv_obj_t *s_label_last_keepalive_tx = NULL;
static lv_obj_t *s_label_last_keepalive_rx = NULL;

// Helpers couleur
static lv_color_t color_ok(void)      { return lv_palette_main(LV_PALETTE_GREEN); }
static lv_color_t color_warn(void)    { return lv_palette_main(LV_PALETTE_YELLOW); }
static lv_color_t color_error(void)   { return lv_palette_main(LV_PALETTE_RED); }
static lv_color_t color_neutral(void) { return lv_palette_main(LV_PALETTE_GREY); }

static void set_status_label(lv_obj_t *label, const char *text, lv_color_t color)
{
    if (!label) return;
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
}

void screen_can_status_create(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 8, 0);

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);

    // Titre
    lv_obj_t *label_title = lv_label_create(cont);
    lv_label_set_text(label_title, "CAN Bus Status");
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, 0);

    // Section: État driver
    lv_obj_t *row1 = lv_obj_create(cont);
    lv_obj_remove_style_all(row1);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label1_title = lv_label_create(row1);
    lv_label_set_text(label1_title, "Driver:");

    s_label_driver_status = lv_label_create(row1);
    lv_label_set_text(s_label_driver_status, "UNKNOWN");
    lv_obj_set_style_text_color(s_label_driver_status, color_neutral(), 0);

    // Section: Keepalive
    lv_obj_t *row2 = lv_obj_create(cont);
    lv_obj_remove_style_all(row2);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label2_title = lv_label_create(row2);
    lv_label_set_text(label2_title, "Keepalive:");

    s_label_keepalive_status = lv_label_create(row2);
    lv_label_set_text(s_label_keepalive_status, "UNKNOWN");
    lv_obj_set_style_text_color(s_label_keepalive_status, color_neutral(), 0);

    // Section: Bus state
    lv_obj_t *row3 = lv_obj_create(cont);
    lv_obj_remove_style_all(row3);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label3_title = lv_label_create(row3);
    lv_label_set_text(label3_title, "Bus State:");

    s_label_bus_state = lv_label_create(row3);
    lv_label_set_text(s_label_bus_state, "STOPPED");

    // Séparateur
    lv_obj_t *sep1 = lv_obj_create(cont);
    lv_obj_set_height(sep1, 1);
    lv_obj_set_width(sep1, LV_PCT(100));
    lv_obj_set_style_bg_color(sep1, lv_palette_main(LV_PALETTE_GREY), 0);

    // Stats TX
    lv_obj_t *row4 = lv_obj_create(cont);
    lv_obj_remove_style_all(row4);
    lv_obj_set_width(row4, LV_PCT(100));
    lv_obj_set_flex_flow(row4, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row4,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label4_title = lv_label_create(row4);
    lv_label_set_text(label4_title, "TX Frames:");

    s_label_tx_count = lv_label_create(row4);
    lv_label_set_text(s_label_tx_count, "0");

    // Stats RX
    lv_obj_t *row5 = lv_obj_create(cont);
    lv_obj_remove_style_all(row5);
    lv_obj_set_width(row5, LV_PCT(100));
    lv_obj_set_flex_flow(row5, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row5,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label5_title = lv_label_create(row5);
    lv_label_set_text(label5_title, "RX Frames:");

    s_label_rx_count = lv_label_create(row5);
    lv_label_set_text(s_label_rx_count, "0");

    // Erreurs TX
    lv_obj_t *row6 = lv_obj_create(cont);
    lv_obj_remove_style_all(row6);
    lv_obj_set_width(row6, LV_PCT(100));
    lv_obj_set_flex_flow(row6, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row6,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label6_title = lv_label_create(row6);
    lv_label_set_text(label6_title, "TX Errors:");

    s_label_tx_errors = lv_label_create(row6);
    lv_label_set_text(s_label_tx_errors, "0");

    // Erreurs RX
    lv_obj_t *row7 = lv_obj_create(cont);
    lv_obj_remove_style_all(row7);
    lv_obj_set_width(row7, LV_PCT(100));
    lv_obj_set_flex_flow(row7, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row7,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label7_title = lv_label_create(row7);
    lv_label_set_text(label7_title, "RX Errors:");

    s_label_rx_errors = lv_label_create(row7);
    lv_label_set_text(s_label_rx_errors, "0");

    // Séparateur
    lv_obj_t *sep2 = lv_obj_create(cont);
    lv_obj_set_height(sep2, 1);
    lv_obj_set_width(sep2, LV_PCT(100));
    lv_obj_set_style_bg_color(sep2, lv_palette_main(LV_PALETTE_GREY), 0);

    // Keepalive timestamps
    lv_obj_t *row8 = lv_obj_create(cont);
    lv_obj_remove_style_all(row8);
    lv_obj_set_width(row8, LV_PCT(100));
    lv_obj_set_flex_flow(row8, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row8,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label8_title = lv_label_create(row8);
    lv_label_set_text(label8_title, "Last Keepalive TX:");

    s_label_last_keepalive_tx = lv_label_create(row8);
    lv_label_set_text(s_label_last_keepalive_tx, "-- ms");

    lv_obj_t *row9 = lv_obj_create(cont);
    lv_obj_remove_style_all(row9);
    lv_obj_set_width(row9, LV_PCT(100));
    lv_obj_set_flex_flow(row9, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row9,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label9_title = lv_label_create(row9);
    lv_label_set_text(label9_title, "Last Keepalive RX:");

    s_label_last_keepalive_rx = lv_label_create(row9);
    lv_label_set_text(s_label_last_keepalive_rx, "-- ms");
}

void screen_can_status_update(const can_victron_status_t *status)
{
    if (!status) return;

    char buf[64];

    // Driver status
    if (status->driver_started) {
        set_status_label(s_label_driver_status, "STARTED", color_ok());
    } else {
        set_status_label(s_label_driver_status, "STOPPED", color_error());
    }

    // Keepalive status
    if (status->keepalive_ok) {
        set_status_label(s_label_keepalive_status, "OK", color_ok());
    } else {
        set_status_label(s_label_keepalive_status, "TIMEOUT", color_warn());
    }

    // Bus state
    const char *bus_state_str = "UNKNOWN";
    lv_color_t bus_color = color_neutral();
    switch (status->bus_state) {
        case TWAI_STATE_STOPPED:
            bus_state_str = "STOPPED";
            bus_color = color_neutral();
            break;
        case TWAI_STATE_RUNNING:
            bus_state_str = "RUNNING";
            bus_color = color_ok();
            break;
        case TWAI_STATE_BUS_OFF:
            bus_state_str = "BUS_OFF";
            bus_color = color_error();
            break;
        case TWAI_STATE_RECOVERING:
            bus_state_str = "RECOVERING";
            bus_color = color_warn();
            break;
        default:
            break;
    }
    set_status_label(s_label_bus_state, bus_state_str, bus_color);

    // Stats
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)status->tx_frame_count);
    lv_label_set_text(s_label_tx_count, buf);

    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)status->rx_frame_count);
    lv_label_set_text(s_label_rx_count, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)status->tx_error_counter);
    lv_label_set_text(s_label_tx_errors, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)status->rx_error_counter);
    lv_label_set_text(s_label_rx_errors, buf);

    // Keepalive timestamps
    snprintf(buf, sizeof(buf), "%llu ms", (unsigned long long)status->last_keepalive_tx_ms);
    lv_label_set_text(s_label_last_keepalive_tx, buf);

    snprintf(buf, sizeof(buf), "%llu ms", (unsigned long long)status->last_keepalive_rx_ms);
    lv_label_set_text(s_label_last_keepalive_rx, buf);
}
