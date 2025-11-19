idf_component_register(
    SRCS "event_bus.cpp"
         "event_bus_core.cpp"
    INCLUDE_DIRS "."
    REQUIRES freertos event_types
)
