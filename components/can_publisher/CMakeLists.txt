idf_component_register(
    SRCS
        "tinybms_adapter.cpp"
        "conversion_table.cpp"
        "cvl_logic.cpp"
        "cvl_controller.cpp"
        "can_publisher.cpp"
        "can_publisher_orchestrator.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        tinybms_model
        esp_timer
        event_bus
        event_types
        can_victron
        freertos
        nvs_flash
)
