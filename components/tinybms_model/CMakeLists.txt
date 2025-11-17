idf_component_register(
    SRCS "tinybms_model.c"
         "tinybms_registers.c"
         "tinybms_rules.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types tinybms_client
)
