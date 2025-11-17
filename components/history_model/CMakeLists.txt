idf_component_register(
    SRCS "history_model.c"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client cjson
)
