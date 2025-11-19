idf_component_register(
    SRCS "remote_event_adapter.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types
)
