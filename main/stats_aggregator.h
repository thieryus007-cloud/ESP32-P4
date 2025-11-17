// main/stats_aggregator.h
#ifndef STATS_AGGREGATOR_H
#define STATS_AGGREGATOR_H

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    min_value;
    float    max_value;
    float    avg_value;
    uint32_t sample_count;
    uint32_t cycle_count;
    uint32_t balancing_events;
    uint32_t comm_errors;
    uint64_t period_start_ms;
    uint64_t period_end_ms;
} stats_summary_t;

/**
 * @brief Type de callback optionnel pour générer un PDF léger.
 *
 * La toolchain peut fournir cette implémentation. Si NULL, aucun PDF
 * n'est produit et l'export CSV/JSON est privilégié.
 */
typedef esp_err_t (*stats_pdf_renderer_t)(const stats_summary_t *day_summary,
                                          const stats_summary_t *week_summary,
                                          const char *output_path);

/**
 * @brief Initialise l'agrégateur (abonnements EventBus, buffers internes).
 */
esp_err_t stats_aggregator_init(event_bus_t *bus);

/**
 * @brief Démarre la tâche FreeRTOS de consolidation périodique.
 */
esp_err_t stats_aggregator_start(void);

/**
 * @brief Export CSV/JSON en flash avec horodatage et version firmware.
 */
esp_err_t stats_aggregator_export_to_flash(void);

/**
 * @brief Envoi HTTP/MQTT (via net_client) du JSON agrégé.
 */
bool stats_aggregator_send_http(const char *path);

/**
 * @brief Registre un hook optionnel pour rendre un PDF si disponible.
 */
void stats_aggregator_set_pdf_renderer(stats_pdf_renderer_t renderer,
                                       const char *output_path);

#ifdef __cplusplus
}
#endif

#endif // STATS_AGGREGATOR_H
