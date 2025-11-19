idf_component_register(
    SRCS "tinybms_model.cpp"
         "tinybms_registers.cpp"
         "tinybms_rules.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types tinybms_client
)
