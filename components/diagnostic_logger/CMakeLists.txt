idf_component_register(
    SRCS "diagnostic_logger.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types nvs_flash nvs esp_timer
)
