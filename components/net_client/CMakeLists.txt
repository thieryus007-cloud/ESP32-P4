idf_component_register(
    SRCS "net_client.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types remote_event_adapter
    PRIV_REQUIRES esp_event esp_wifi esp_netif esp_websocket_client esp_http_client freertos
)
