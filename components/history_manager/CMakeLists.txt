idf_component_register(
    SRCS "history_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer spiffs
)
