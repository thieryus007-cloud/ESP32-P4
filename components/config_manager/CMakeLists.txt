idf_component_register(
    SRCS
        "config_manager.cpp"
        "config_manager_core.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
)
