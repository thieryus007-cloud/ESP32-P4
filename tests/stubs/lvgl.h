#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lv_obj_t {
    uint8_t dummy;
} lv_obj_t;

typedef uint32_t lv_color_t;

typedef enum {
    LV_PALETTE_RED,
    LV_PALETTE_GREEN,
    LV_PALETTE_BLUE,
    LV_PALETTE_YELLOW,
    LV_PALETTE_ORANGE,
    LV_PALETTE_GREY
} lv_palette_t;

static inline lv_color_t lv_palette_main(lv_palette_t palette)
{
    (void) palette;
    return 0;
}

static inline void lv_obj_set_style_text_color(lv_obj_t *, lv_color_t, int)
{
}

static inline void lv_label_set_text(lv_obj_t *, const char *)
{
}

#ifdef __cplusplus
}
#endif
