idf_component_register(
    SRCS "tinybms_model.c"
         "tinybms_registers.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types tinybms_client
)
