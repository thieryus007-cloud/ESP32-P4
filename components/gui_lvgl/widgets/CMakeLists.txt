idf_component_register(
    SRCS "src/widget_soc_gauge.c"
         "src/widget_status_indicator.c"
         "src/widget_cell_bar.c"
         "src/widget_value_display.c"
         "src/widget_keyboard.c"
         "src/widget_gauge_circular.c"
         "src/widget_gauge_semicircular.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
)
