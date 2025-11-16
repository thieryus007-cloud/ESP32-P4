idf_component_register(
    SRCS "tinybms_adapter.c"
    INCLUDE_DIRS "."
    REQUIRES
        tinybms_model
        esp_timer
)
