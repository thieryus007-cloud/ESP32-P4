idf_component_register(
    SRCS "tinybms_client.cpp"
         "tinybms_protocol.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver event_bus event_types
)
