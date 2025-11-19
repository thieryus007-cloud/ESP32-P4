idf_component_register(
    SRCS
        "tinybms_adapter.c"
        "conversion_table.c"
        "cvl_logic.c"
        "cvl_controller.c"
        "can_publisher.c"
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
