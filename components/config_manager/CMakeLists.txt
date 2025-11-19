idf_component_register(
    SRCS
        "config_manager.c"
        "config_manager_core.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
)
