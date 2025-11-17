#ifndef UI_I18N_H
#define UI_I18N_H

#include <stddef.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_LANG_FR = 0,
    UI_LANG_EN = 1,
} ui_language_t;

typedef void (*ui_i18n_listener_t)(void *user_ctx);

void ui_i18n_init(void);
ui_language_t ui_i18n_get_language(void);
void ui_i18n_set_language(ui_language_t lang);

const char *ui_i18n(const char *key);

void ui_i18n_label_set_text(lv_obj_t *label, const char *key);

void ui_i18n_register_listener(ui_i18n_listener_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif // UI_I18N_H
