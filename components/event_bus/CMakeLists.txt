idf_component_register(
    SRCS "event_bus.cpp"
    INCLUDE_DIRS "."
    REQUIRES freertos event_types
)
