idf_component_register(
    SRCS "gui_init.c"
         "screen_home.c"
         "screen_dashboard.c"
         "screen_battery.c"
         "screen_cells.c"
         "screen_power.c"
         "screen_config.c"
         "screen_tinybms_status.c"
         "screen_tinybms_config.c"
         "screen_can_status.c"
         "screen_can_config.c"
         "screen_bms_control.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lvgl_port event_bus event_types tinybms_client tinybms_model can_victron can_publisher
)
