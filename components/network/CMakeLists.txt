idf_component_register(
    SRCS "network_publisher.cpp" "status_endpoint.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client freertos diagnostic_logger config_manager
)
