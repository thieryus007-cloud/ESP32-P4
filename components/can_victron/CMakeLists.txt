idf_component_register(
    SRCS
        "can_victron.c"
        "can_victron_driver.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_timer
        freertos
        event_bus
        event_types
)
