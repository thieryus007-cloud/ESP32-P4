idf_component_register(
    SRCS "telemetry_model.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types tinybms_model freertos
)
