idf_component_register(
    SRCS "diagnostic_logger.c"
         "diagnostic_logger_core.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types nvs_flash nvs esp_timer
)
