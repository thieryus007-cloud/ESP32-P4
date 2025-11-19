idf_component_register(
    SRCS "network_publisher.cpp" "status_endpoint.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client freertos diagnostic_logger config_manager
)
