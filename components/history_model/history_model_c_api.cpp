#include "HistoryModel.hpp"

// Instance globale unique (Singleton implicite)
static std::unique_ptr<HistoryModel> s_modelInstance;

extern "C" {

void history_model_init(event_bus_t *bus) {
    if (!s_modelInstance) {
        s_modelInstance = std::make_unique<HistoryModel>(bus);
    }
}

void history_model_start(void) {
    if (s_modelInstance) {
        s_modelInstance->start();
    }
}

void history_model_on_remote_history(int status_code, const char *body) {
    if (s_modelInstance) {
        // Conversion sécurisée char* -> string
        std::string bodyStr = body ? body : "";
        s_modelInstance->onRemoteHistoryResponse(status_code, bodyStr);
    }
}

} // extern "C"
