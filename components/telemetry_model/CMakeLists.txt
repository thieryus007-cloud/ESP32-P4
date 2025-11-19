idf_component_register(
    SRCS "telemetry_model.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types tinybms_model freertos
)
