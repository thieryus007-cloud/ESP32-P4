idf_component_register(
    SRCS "HistoryModel.cpp"
         "history_model_c_api.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client cjson
)
