idf_component_register(
    SRCS "network_publisher.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client freertos
)
