idf_component_register(
    SRCS "tinybms_client.c"
         "tinybms_protocol.c"
    INCLUDE_DIRS "."
    REQUIRES driver event_bus event_types
)
