#include "HistoryModel.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <chrono>

static const char* TAG = "HistoryModel";

// --- Helpers (Anonyme namespace pour fonctions locales) ---
namespace {
    uint64_t nowMs() {
        return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    }

    uint64_t getRangeDurationMs(history_range_t range) {
        using namespace std::chrono;
        switch (range) {
            case HISTORY_RANGE_LAST_DAY:  return duration_cast<milliseconds>(hours(24)).count();
            case HISTORY_RANGE_LAST_WEEK: return duration_cast<milliseconds>(hours(24 * 7)).count();
            case HISTORY_RANGE_LAST_HOUR: 
            default:                      return duration_cast<milliseconds>(hours(1)).count();
        }
    }

    const char* getRangeQueryString(history_range_t range) {
        switch (range) {
            case HISTORY_RANGE_LAST_DAY:  return "24h";
            case HISTORY_RANGE_LAST_WEEK: return "7d";
            case HISTORY_RANGE_LAST_HOUR: default: return "1h";
        }
    }

    // RAII pour cJSON
    struct CJsonDeleter {
        void operator()(cJSON* p) const { if (p) cJSON_Delete(p); }
    };
    using UniqueCJson = std::unique_ptr<cJSON, CJsonDeleter>;
}

// --- Constructeur / Destructeur ---

HistoryModel::HistoryModel(event_bus_t* bus, NetClient* netClient)
    : m_bus(bus)
    , m_netClient(netClient)
    , m_ring(CAPACITY) // Allocation du vecteur ici
{
    if (m_bus) {
        // Enregistrement des events via le wrapper statique
        event_bus_subscribe(m_bus, EVENT_BATTERY_STATUS_UPDATED, eventHandlerWrapper, this);
        event_bus_subscribe(m_bus, EVENT_USER_INPUT_REQUEST_HISTORY, eventHandlerWrapper, this);
        event_bus_subscribe(m_bus, EVENT_USER_INPUT_EXPORT_HISTORY, eventHandlerWrapper, this);
    }
}

HistoryModel::~HistoryModel() {
    // C++ gère la destruction du vecteur et du mutex automatiquement
}

void HistoryModel::start() {
    ESP_LOGI(TAG, "HistoryModel started. Capacity: %zu", CAPACITY);
}

// --- Trampoline C pour EventBus ---

void HistoryModel::eventHandlerWrapper(event_bus_t* bus, const event_t* event, void* ctx) {
    auto* self = static_cast<HistoryModel*>(ctx);
    if (!event || !event->data) return;

    switch (event->type) {
        case EVENT_BATTERY_STATUS_UPDATED:
            self->onBatteryUpdate(static_cast<const battery_status_t*>(event->data));
            break;
        case EVENT_USER_INPUT_REQUEST_HISTORY:
            self->onHistoryRequest(static_cast<const user_input_history_request_t*>(event->data));
            break;
        case EVENT_USER_INPUT_EXPORT_HISTORY:
            self->onHistoryExport(static_cast<const user_input_history_export_t*>(event->data));
            break;
        default: break;
    }
}

// --- Core Logic ---

void HistoryModel::pushSample(const history_sample_t& sample) {
    std::lock_guard<std::mutex> lock(m_mutex); // Thread-safety
    
    m_ring.data[m_ring.head] = sample;
    m_ring.head = (m_ring.head + 1) % CAPACITY;
    if (m_ring.count < CAPACITY) {
        m_ring.count++;
    }
}

void HistoryModel::onBatteryUpdate(const battery_status_t* status) {
    history_sample_t s = {};
    s.timestamp_ms = nowMs();
    s.voltage = status->voltage;
    s.current = status->current;
    s.temperature = status->temperature;
    s.soc = status->soc;

    pushSample(s);
}

void HistoryModel::onHistoryRequest(const user_input_history_request_t* req) {
    m_lastRequestedRange = req->range;
    
    std::string path = "/api/history?range=";
    path += getRangeQueryString(req->range);

    // On essaie de récupérer l'historique distant d'abord
    bool sent = false;
    if (m_netClient) {
        // On suppose que NetClient a été modernisé pour accepter std::string
        sent = m_netClient->sendHttpRequest(path, "GET", "");
    }

    if (!sent) {
        ESP_LOGW(TAG, "Backend unavailable, using local buffer");
        publishLocalSnapshot(req->range);
    }
}

void HistoryModel::publishLocalSnapshot(history_range_t range) {
    std::vector<history_sample_t> snapshotBuffer;
    // On réserve pour éviter realloc, max défini dans event_types normalement
    snapshotBuffer.reserve(HISTORY_SNAPSHOT_MAX); 

    uint64_t cutoff = nowMs() - getRangeDurationMs(range);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Parcours inverse (du plus récent au plus ancien)
        for (size_t i = 0; i < m_ring.count; ++i) {
            if (snapshotBuffer.size() >= HISTORY_SNAPSHOT_MAX) break;

            // Calcul d'index circulaire sécurisé
            // (head - 1 - i + CAPACITY) % CAPACITY
            size_t idx = (m_ring.head + CAPACITY - 1 - i) % CAPACITY;
            const auto& s = m_ring.data[idx];

            if (s.timestamp_ms >= cutoff) {
                snapshotBuffer.push_back(s);
            } else {
                break; // Trop vieux
            }
        }
    }

    // Inverser pour avoir l'ordre chronologique si nécessaire par l'UI, 
    // le code original renvoyait du plus récent au plus vieux dans 'tmp' puis inversait ?
    // Le code original : remplissait tmp du récent au vieux, puis memcpy.
    // L'UI attend souvent [Oldest -> Newest] pour un graphe.
    // Ici snapshotBuffer est [Newest -> Oldest].
    std::reverse(snapshotBuffer.begin(), snapshotBuffer.end());

    publishSnapshot(range, snapshotBuffer, false);
}

void HistoryModel::publishSnapshot(history_range_t range, const std::vector<history_sample_t>& samples, bool fromBackend) {
    if (!m_bus) return;

    // Utilisation heap (vector) puis copie dans la structure event (qui est sur la stack ou static)
    // Attention : event_t transporte des pointeurs ou copie ? 
    // HISTORY_SNAPSHOT_MAX est limité dans la structure history_snapshot_t.
    
    history_snapshot_t snapshot = {};
    snapshot.range = range;
    snapshot.from_backend = fromBackend;
    snapshot.count = std::min((size_t)HISTORY_SNAPSHOT_MAX, samples.size());

    if (snapshot.count > 0) {
        std::memcpy(snapshot.samples, samples.data(), snapshot.count * sizeof(history_sample_t));
    }

    event_t evt = {};
    evt.type = EVENT_HISTORY_UPDATED;
    evt.data = &snapshot;
    evt.data_size = sizeof(snapshot);
    event_bus_publish(m_bus, &evt);
}

// --- Export CSV ---

void HistoryModel::onHistoryExport(const user_input_history_export_t* req) {
    history_export_result_t result = {};
    result.success = false;
    const char* path = "/spiflash/history_export.csv";
    std::strncpy(result.path, path, sizeof(result.path) - 1);

    uint64_t cutoff = nowMs() - getRangeDurationMs(req->range);
    size_t exportedCount = 0;

    // Utilisation de FILE* standard avec wrapper RAII unique_ptr pour fermeture garantie
    auto fileCloser = [](FILE* f) { if(f) fclose(f); };
    std::unique_ptr<FILE, decltype(fileCloser)> f(fopen(path, "w"), fileCloser);

    if (f) {
        std::fprintf(f.get(), "timestamp_ms,voltage,current,temperature,soc\n");

        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Parcours chronologique (du plus vieux au plus récent dans la fenêtre)
        // Pour simplifier, on parcourt tout le ring buffer.
        // Note : Pour être parfait, il faudrait identifier l'index de départ "le plus vieux >= cutoff".
        
        // Itération simple : on dump tout ce qui correspond au critère
        // Le ring buffer n'est pas contigu, on doit boucler
        for (size_t i = 0; i < m_ring.count; ++i) {
            // Index du plus vieux au plus récent : 
            // Start index = (head - count + i + Capacity) % Capacity
            size_t idx = (m_ring.head + CAPACITY - m_ring.count + i) % CAPACITY;
            const auto& s = m_ring.data[idx];

            if (s.timestamp_ms >= cutoff) {
                std::fprintf(f.get(), "%llu,%.3f,%.3f,%.3f,%.2f\n",
                        (unsigned long long)s.timestamp_ms, 
                        (double)s.voltage, (double)s.current, 
                        (double)s.temperature, (double)s.soc);
                exportedCount++;
            }
        }
        result.success = (exportedCount > 0);
        result.exported_count = exportedCount;
    } else {
        ESP_LOGE(TAG, "Failed to open export file: %s", path);
    }

    event_t evt = { EVENT_HISTORY_EXPORTED, &result, sizeof(result) };
    event_bus_publish(m_bus, &evt);
}

// --- Parsing & Remote ---

void HistoryModel::onRemoteHistoryResponse(int statusCode, const std::string& body) {
    if (statusCode != 200 || body.empty()) {
        ESP_LOGW(TAG, "Remote history failed (%d), fallback local", statusCode);
        publishLocalSnapshot(m_lastRequestedRange);
        return;
    }

    if (!parseHistoryJson(body)) {
        publishLocalSnapshot(m_lastRequestedRange);
    }
}

bool HistoryModel::parseHistoryJson(const std::string& jsonBody) {
    UniqueCJson root(cJSON_Parse(jsonBody.c_str())); // Auto delete
    if (!root) return false;

    cJSON* array = cJSON_IsArray(root.get()) ? root.get() : nullptr;
    
    if (!array) {
        array = cJSON_GetObjectItemCaseSensitive(root.get(), "history");
        if (!array) array = cJSON_GetObjectItemCaseSensitive(root.get(), "samples");
    }

    if (!cJSON_IsArray(array)) return false;

    std::vector<history_sample_t> samples;
    samples.reserve(HISTORY_SNAPSHOT_MAX);

    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, array) {
        if (samples.size() >= HISTORY_SNAPSHOT_MAX) break;
        if (!cJSON_IsObject(item)) continue;

        history_sample_t s = {};
        
        // Parsing optimisé
        cJSON* ts = cJSON_GetObjectItemCaseSensitive(item, "timestamp_ms");
        if (cJSON_IsNumber(ts)) s.timestamp_ms = (uint64_t)ts->valuedouble;
        else {
            // Fallback secondes
            cJSON* ts_sec = cJSON_GetObjectItemCaseSensitive(item, "timestamp");
            if (cJSON_IsNumber(ts_sec)) s.timestamp_ms = (uint64_t)ts_sec->valuedouble * 1000ULL;
        }

        // Lambda helper pour réduire la verbosité
        auto getVal = [](cJSON* obj, const char* key) {
            cJSON* n = cJSON_GetObjectItemCaseSensitive(obj, key);
            return cJSON_IsNumber(n) ? (float)n->valuedouble : 0.0f;
        };

        s.voltage = getVal(item, "voltage");
        s.current = getVal(item, "current");
        s.temperature = getVal(item, "temperature");
        s.soc = getVal(item, "soc");

        if (s.timestamp_ms == 0) s.timestamp_ms = nowMs();
        
        samples.push_back(s);
        
        // On insère aussi dans le buffer local pour cache
        pushSample(s);
    }

    if (samples.empty()) return false;

    publishSnapshot(m_lastRequestedRange, samples, true);
    return true;
}
