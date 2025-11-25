// components/gui_lvgl/ui_theme.c

#include "ui_theme.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include <string.h>
#include <time.h>

static const char *TAG = "ui_theme";
static const char *NVS_NAMESPACE = "theme";
static const char *NVS_KEY_MODE = "mode";

static lv_display_t   *s_disp        = NULL;
static lv_obj_t       *s_menu_card   = NULL;
static lv_obj_t       *s_menu_label  = NULL;
static lv_obj_t       *s_menu_choice = NULL;
static ui_theme_mode_t s_mode        = UI_THEME_MODE_AUTO;
static theme_palette_t s_current_palette = {0};
static bool s_auto_enabled = false;
static uint8_t s_dark_hour = 19;  // 19h par défaut
static uint8_t s_light_hour = 7;  // 7h par défaut

// Définition du thème sombre
static const theme_palette_t THEME_DARK_PALETTE = {
    .bg_primary     = LV_COLOR_MAKE(0x1A, 0x20, 0x2C),
    .bg_secondary   = LV_COLOR_MAKE(0x2D, 0x37, 0x48),
    .bg_tertiary    = LV_COLOR_MAKE(0x4A, 0x55, 0x68),
    .text_primary   = LV_COLOR_MAKE(0xF7, 0xFA, 0xFC),
    .text_secondary = LV_COLOR_MAKE(0xA0, 0xAE, 0xC0),
    .text_disabled  = LV_COLOR_MAKE(0x71, 0x80, 0x96),
    .border_default = LV_COLOR_MAKE(0x4A, 0x55, 0x68),
    .border_focus   = LV_COLOR_MAKE(0x4299, 0xE1, 0xFF),
    .accent_primary = LV_COLOR_MAKE(0x42, 0x99, 0xE1),
    .accent_success = LV_COLOR_MAKE(0x38, 0xA1, 0x69),
    .accent_warning = LV_COLOR_MAKE(0xED, 0x89, 0x36),
    .accent_error   = LV_COLOR_MAKE(0xE5, 0x3E, 0x3E),
    .charging       = LV_COLOR_MAKE(0x38, 0xA1, 0x69),
    .discharging    = LV_COLOR_MAKE(0xED, 0x89, 0x36),
    .balancing      = LV_COLOR_MAKE(0xF6, 0xE0, 0x5E),
    .idle           = LV_COLOR_MAKE(0x71, 0x80, 0x96),
};

// Définition du thème clair
static const theme_palette_t THEME_LIGHT_PALETTE = {
    .bg_primary     = LV_COLOR_MAKE(0xF7, 0xFA, 0xFC),
    .bg_secondary   = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    .bg_tertiary    = LV_COLOR_MAKE(0xE2, 0xE8, 0xF0),
    .text_primary   = LV_COLOR_MAKE(0x1A, 0x20, 0x2C),
    .text_secondary = LV_COLOR_MAKE(0x4A, 0x55, 0x68),
    .text_disabled  = LV_COLOR_MAKE(0xA0, 0xAE, 0xC0),
    .border_default = LV_COLOR_MAKE(0xE2, 0xE8, 0xF0),
    .border_focus   = LV_COLOR_MAKE(0x42, 0x99, 0xE1),
    .accent_primary = LV_COLOR_MAKE(0x30, 0x70, 0xB3),
    .accent_success = LV_COLOR_MAKE(0x2F, 0x85, 0x5A),
    .accent_warning = LV_COLOR_MAKE(0xC0, 0x5F, 0x21),
    .accent_error   = LV_COLOR_MAKE(0xC5, 0x2A, 0x2A),
    .charging       = LV_COLOR_MAKE(0x2F, 0x85, 0x5A),
    .discharging    = LV_COLOR_MAKE(0xC0, 0x5F, 0x21),
    .balancing      = LV_COLOR_MAKE(0xD6, 0x9E, 0x2E),
    .idle           = LV_COLOR_MAKE(0x71, 0x80, 0x96),
};

static bool default_dark_preference(void)
{
#if defined(CONFIG_LV_THEME_DEFAULT_DARK) || defined(CONFIG_LVGL_THEME_DEFAULT_DARK)
    return true;
#else
    return false;
#endif
}

static bool should_use_dark_mode_by_time(void)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    uint8_t hour = timeinfo.tm_hour;

    // Si dark_hour < light_hour (ex: 19h -> 7h)
    if (s_dark_hour < s_light_hour) {
        return (hour >= s_dark_hour || hour < s_light_hour);
    }
    // Si light_hour < dark_hour (ex: 7h -> 19h)
    else {
        return (hour >= s_dark_hour && hour < 24) || (hour >= 0 && hour < s_light_hour);
    }
}

static esp_err_t save_theme_to_nvs(ui_theme_mode_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_MODE, (uint8_t)mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving theme: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Theme saved to NVS: %d", mode);
        }
    }

    nvs_close(handle);
    return err;
}

static ui_theme_mode_t load_theme_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not available, using default theme");
        return UI_THEME_MODE_AUTO;
    }

    uint8_t mode = UI_THEME_MODE_AUTO;
    err = nvs_get_u8(handle, NVS_KEY_MODE, &mode);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Theme loaded from NVS: %d", mode);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved theme, using default");
    }

    nvs_close(handle);
    return (ui_theme_mode_t)mode;
}

static void apply_theme(ui_theme_mode_t mode)
{
    if (!s_disp) {
        return;
    }

    bool use_dark = false;
    if (mode == UI_THEME_MODE_AUTO) {
        if (s_auto_enabled) {
            use_dark = should_use_dark_mode_by_time();
        } else {
            use_dark = default_dark_preference();
        }
    } else {
        use_dark = (mode == UI_THEME_MODE_DARK);
    }

    // Mettre à jour la palette courante
    if (use_dark) {
        memcpy(&s_current_palette, &THEME_DARK_PALETTE, sizeof(theme_palette_t));
    } else {
        memcpy(&s_current_palette, &THEME_LIGHT_PALETTE, sizeof(theme_palette_t));
    }

    lv_theme_t *theme = lv_theme_default_init(s_disp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_GREY),
                                              use_dark,
                                              LV_FONT_DEFAULT);
    if (theme) {
        lv_display_set_theme(s_disp, theme);
        ESP_LOGI(TAG, "Theme applied: %s", use_dark ? "dark" : "light");
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
    s_mode = load_theme_from_nvs();
    apply_theme(s_mode);
    ESP_LOGI(TAG, "Theme initialized");
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
    save_theme_to_nvs(mode);
    apply_theme(s_mode);
    update_menu_label();
}

ui_theme_mode_t ui_theme_get_mode(void)
{
    return s_mode;
}

void ui_theme_set_auto(bool enable, uint8_t dark_hour, uint8_t light_hour)
{
    s_auto_enabled = enable;
    if (dark_hour < 24) s_dark_hour = dark_hour;
    if (light_hour < 24) s_light_hour = light_hour;

    ESP_LOGI(TAG, "Auto theme %s (dark: %dh, light: %dh)",
             enable ? "enabled" : "disabled", s_dark_hour, s_light_hour);

    // Réappliquer le thème si mode AUTO
    if (s_mode == UI_THEME_MODE_AUTO) {
        apply_theme(s_mode);
    }
}

const theme_palette_t* ui_theme_get_palette(void)
{
    return &s_current_palette;
}

void ui_theme_toggle(void)
{
    ui_theme_mode_t new_mode;
    switch (s_mode) {
        case UI_THEME_MODE_AUTO:
            new_mode = UI_THEME_MODE_LIGHT;
            break;
        case UI_THEME_MODE_LIGHT:
            new_mode = UI_THEME_MODE_DARK;
            break;
        case UI_THEME_MODE_DARK:
        default:
            new_mode = UI_THEME_MODE_AUTO;
            break;
    }
    ui_theme_set_mode(new_mode);
}
