idf_component_register(
    SRCS "history_model.cpp"
         "HistoryModel.cpp"
         "history_model_c_api.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus event_types net_client cjson
)
