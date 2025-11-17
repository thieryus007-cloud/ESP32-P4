// components/gui_lvgl/ui_theme.c

#include "ui_theme.h"

#include <string.h>

static lv_display_t   *s_disp        = NULL;
static lv_obj_t       *s_menu_card   = NULL;
static lv_obj_t       *s_menu_label  = NULL;
static lv_obj_t       *s_menu_choice = NULL;
static ui_theme_mode_t s_mode        = UI_THEME_MODE_AUTO;

static bool default_dark_preference(void)
{
#if defined(CONFIG_LV_THEME_DEFAULT_DARK) || defined(CONFIG_LVGL_THEME_DEFAULT_DARK)
    return true;
#else
    return false;
#endif
}

static void apply_theme(ui_theme_mode_t mode)
{
    if (!s_disp) {
        return;
    }

    bool use_dark = false;
    if (mode == UI_THEME_MODE_AUTO) {
        use_dark = default_dark_preference();
    } else {
        use_dark = (mode == UI_THEME_MODE_DARK);
    }

    lv_theme_t *theme = lv_theme_default_init(s_disp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_GREY),
                                              use_dark,
                                              LV_FONT_DEFAULT);
    if (theme) {
        lv_display_set_theme(s_disp, theme);
    }
}

static void update_menu_label(void)
{
    if (!s_menu_label) {
        return;
    }

    const char *label = (s_mode == UI_THEME_MODE_AUTO)  ? "Auto"
                          : (s_mode == UI_THEME_MODE_LIGHT) ? "Clair"
                                                           : "Sombre";
    lv_label_set_text_fmt(s_menu_label, "Thème : %s", label);

    if (s_menu_choice) {
        lv_dropdown_set_selected(s_menu_choice, (uint16_t) s_mode);
    }
}

static void on_dropdown_changed(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (!target) {
        return;
    }

    uint16_t idx = lv_dropdown_get_selected(target);
    if (idx > UI_THEME_MODE_DARK) {
        idx = UI_THEME_MODE_AUTO;
    }
    ui_theme_set_mode((ui_theme_mode_t) idx);
}

void ui_theme_init(lv_display_t *disp)
{
    s_disp = disp ? disp : lv_display_get_default();
    s_mode = UI_THEME_MODE_AUTO;
    apply_theme(s_mode);
}

lv_obj_t *ui_theme_create_quick_menu(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    s_menu_card = lv_obj_create(parent);
    lv_obj_set_style_pad_all(s_menu_card, 10, 0);
    lv_obj_set_style_radius(s_menu_card, 8, 0);
    lv_obj_set_style_bg_opa(s_menu_card, LV_OPA_80, 0);
    lv_obj_set_style_bg_color(s_menu_card,
                              lv_palette_lighten(LV_PALETTE_GREY, 3),
                              0);
    lv_obj_set_style_border_width(s_menu_card, 0, 0);
    lv_obj_set_style_shadow_width(s_menu_card, 6, 0);
    lv_obj_set_style_shadow_opa(s_menu_card, LV_OPA_40, 0);
    lv_obj_set_style_shadow_spread(s_menu_card, 2, 0);
    lv_obj_set_flex_flow(s_menu_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_menu_card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(s_menu_card, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(s_menu_card, LV_ALIGN_TOP_RIGHT, -8, 8);

    s_menu_label = lv_label_create(s_menu_card);
    lv_label_set_text(s_menu_label, "Thème : Auto");

    s_menu_choice = lv_dropdown_create(s_menu_card);
    lv_dropdown_set_options_static(s_menu_choice, "Auto\nClair\nSombre");
    lv_obj_set_width(s_menu_choice, 120);
    lv_dropdown_set_selected(s_menu_choice, (uint16_t) s_mode);
    lv_obj_add_event_cb(s_menu_choice, on_dropdown_changed, LV_EVENT_VALUE_CHANGED, NULL);

    update_menu_label();
    return s_menu_card;
}

void ui_theme_set_mode(ui_theme_mode_t mode)
{
    if (mode > UI_THEME_MODE_DARK) {
        mode = UI_THEME_MODE_AUTO;
    }

    s_mode = mode;
    apply_theme(s_mode);
    update_menu_label();
}

ui_theme_mode_t ui_theme_get_mode(void)
{
    return s_mode;
}
