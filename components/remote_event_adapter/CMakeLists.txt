idf_component_register(
    SRCS "remote_event_adapter.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types
)
