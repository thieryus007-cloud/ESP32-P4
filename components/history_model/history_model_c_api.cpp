#include "HistoryModel.hpp"

// Instance globale unique (Singleton implicite)
static std::unique_ptr<HistoryModel> s_modelInstance;

// Déclaration externe du pointeur NetClient (ou récupéré via un getter global)
// Pour cet exemple, on suppose qu'on peut récupérer l'instance NetClient
// ou qu'on la passe à NULL si non dispo pour l'instant.
// extern NetClient* g_netClient; 

extern "C" {

void history_model_init(event_bus_t *bus) {
    if (!s_modelInstance) {
        // Note: Idéalement, on passerait l'instance de NetClient ici.
        // Si vous n'avez pas accès à l'objet NetClient C++, passez nullptr
        // et le modèle fonctionnera en mode local uniquement.
        s_modelInstance = std::make_unique<HistoryModel>(bus, nullptr);
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
