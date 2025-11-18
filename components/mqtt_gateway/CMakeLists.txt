idf_component_register(SRCS "mqtt_gateway.c"
                       INCLUDE_DIRS "."
                       REQUIRES event_bus event_types config_manager tinybms_model json)
