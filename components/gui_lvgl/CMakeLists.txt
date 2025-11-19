idf_component_register(
    SRCS "gui_init.cpp"
         "ui_i18n.cpp"
         "screen_home.cpp"
         "pack_stats_utils.cpp"
         "screen_dashboard.cpp"
         "screen_battery.cpp"
         "screen_cells.cpp"
         "screen_power.cpp"
         "screen_config.cpp"
         "screen_alerts.cpp"
         "screen_tinybms_status.cpp"
         "screen_tinybms_config.cpp"
         "screen_can_status.cpp"
         "screen_can_config.cpp"
         "screen_bms_control.cpp"
         "screen_history.cpp"
         "ui_notifications.cpp"
         "ui_theme.cpp"
    INCLUDE_DIRS "." "include"
    REQUIRES lvgl esp_lvgl_port event_bus event_types tinybms_client tinybms_model can_victron can_publisher
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
