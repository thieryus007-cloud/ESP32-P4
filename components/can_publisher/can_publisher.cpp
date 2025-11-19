/**
 * @file can_publisher.c
 * @brief CAN Publisher - Orchestrateur Phase 4
 *
 * Orchestre la publication des messages CAN Victron:
 * - S'abonne aux événements TinyBMS (EVENT_TINYBMS_REGISTER_UPDATED)
 * - Convertit via tinybms_adapter
 * - Encode via conversion_table
 * - Publie via can_victron
 * - Gère la state machine CVL
 */

#include "can_publisher.h"
#include "tinybms_adapter.h"
#include "conversion_table.h"
#include "cvl_controller.h"
#include "can_victron.h"
#include "event_bus.h"
#include "event_types.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "can_publisher";

// État du module
static bool s_initialized = false;
static EventBus *s_event_bus = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static uint32_t s_publish_count = 0;
static uint64_t s_last_publish_ms = 0;

// Configuration
#define CAN_PUBLISHER_PUBLISH_INTERVAL_MS 1000  // Publier toutes les 1s

/**
 * @brief Callback appelé lors d'une mise à jour de registre TinyBMS
 */
static void on_tinybms_register_updated(const event_bus_event_t *event, void *context)
{
    (void)context;

    if (!s_initialized) {
        ESP_LOGW(TAG, "Callback reçu alors que non initialisé");
        return;
    }

    // Vérifier throttle (ne pas publier trop souvent)
    uint64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_last_publish_ms < CAN_PUBLISHER_PUBLISH_INTERVAL_MS) {
        ESP_LOGV(TAG, "Throttle: ignorer mise à jour (dernière: %llu ms)",
                 (unsigned long long)(now_ms - s_last_publish_ms));
        return;
    }

    ESP_LOGD(TAG, "Déclenchement publication CAN suite à EVENT_TINYBMS_REGISTER_UPDATED");

    // Convertir les données TinyBMS via l'adaptateur
    uart_bms_live_data_t bms_data;
    if (tinybms_adapter_convert(&bms_data) != ESP_OK) {
        ESP_LOGE(TAG, "Échec conversion tinybms_adapter");
        return;
    }

    ESP_LOGD(TAG, "Conversion réussie: SOC=%.1f%%, V=%.2fV, I=%.2fA",
             bms_data.state_of_charge_pct,
             bms_data.pack_voltage_v,
             bms_data.pack_current_a);

    // Préparer les données pour le contrôleur CVL
    can_publisher_cvl_prepare(&bms_data);

    // Intégrer l'échantillon pour les compteurs d'énergie
    can_publisher_conversion_ingest_sample(&bms_data);

    // Parcourir tous les canaux CAN et publier
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (size_t i = 0; i < g_can_publisher_channel_count; i++) {
        const can_publisher_channel_t *channel = &g_can_publisher_channels[i];

        // Créer une frame vide
        can_publisher_frame_t frame = {0};

        // Appeler l'encodeur
        if (channel->fill_fn && channel->fill_fn(&bms_data, &frame)) {
            // Publier via can_victron
            esp_err_t err = can_victron_publish_frame(
                frame.id,
                frame.data,
                frame.dlc,
                channel->description
            );

            if (err == ESP_OK) {
                ESP_LOGV(TAG, "CAN 0x%03lX publié: %s",
                         (unsigned long)frame.id, channel->description);
            } else {
                ESP_LOGW(TAG, "Échec publication 0x%03lX: %s",
                         (unsigned long)frame.id, esp_err_to_name(err));
            }
        }
    }

    s_publish_count++;
    s_last_publish_ms = now_ms;

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Publication CAN #%lu terminée (%zu canaux)",
             s_publish_count, g_can_publisher_channel_count);

    // Publier un événement de mise à jour CVL si applicable
    can_publisher_cvl_result_t cvl_result;
    if (can_publisher_cvl_get_latest(&cvl_result)) {
        cvl_limits_event_t limits_event = {
            .cvl_voltage_v = cvl_result.result.cvl_voltage_v,
            .ccl_current_a = cvl_result.result.ccl_limit_a,
            .dcl_current_a = cvl_result.result.dcl_limit_a,
            .cvl_state = cvl_result.result.state,
            .imbalance_hold_active = cvl_result.result.imbalance_hold_active,
            .cell_protection_active = cvl_result.result.cell_protection_active,
            .timestamp_ms = cvl_result.timestamp_ms
        };

        event_bus_event_t limits_evt = {
            .id = EVENT_CVL_LIMITS_UPDATED,
            .data = &limits_event,
            .data_size = sizeof(limits_event)
        };

        event_bus_publish(s_event_bus, &limits_evt, pdMS_TO_TICKS(10));
    }
}

void can_publisher_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Déjà initialisé");
        return;
    }

    ESP_LOGI(TAG, "Initialisation CAN Publisher (Phase 4)");

    // Créer le mutex
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Échec création mutex");
        return;
    }

    // Récupérer l'EventBus
    s_event_bus = event_bus_get_instance();
    if (!s_event_bus) {
        ESP_LOGE(TAG, "EventBus non disponible");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return;
    }

    // Initialiser le contrôleur CVL
    can_publisher_cvl_init();

    // Restaurer les compteurs d'énergie depuis NVS
    esp_err_t err = can_publisher_conversion_restore_energy_state();
    if (err == ESP_OK) {
        double charged_wh, discharged_wh;
        can_publisher_conversion_get_energy_state(&charged_wh, &discharged_wh);
        ESP_LOGI(TAG, "Énergie restaurée: charge=%.1fWh, décharge=%.1fWh",
                 charged_wh, discharged_wh);
    } else {
        ESP_LOGW(TAG, "Pas de compteurs énergie NVS (première utilisation): %s",
                 esp_err_to_name(err));
        can_publisher_conversion_reset_state();
    }

    // S'abonner aux événements TinyBMS
    event_bus_subscribe(s_event_bus, EVENT_TINYBMS_REGISTER_UPDATED,
                       on_tinybms_register_updated, NULL);

    ESP_LOGI(TAG, "Abonné à EVENT_TINYBMS_REGISTER_UPDATED");
    ESP_LOGI(TAG, "CAN Publisher initialisé (%zu canaux disponibles)",
             g_can_publisher_channel_count);

    s_initialized = true;
}

void can_publisher_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Pas initialisé");
        return;
    }

    ESP_LOGI(TAG, "Dé-initialisation CAN Publisher");

    // Se désabonner des événements
    if (s_event_bus) {
        event_bus_unsubscribe(s_event_bus, EVENT_TINYBMS_REGISTER_UPDATED,
                             on_tinybms_register_updated, NULL);
    }

    // Sauvegarder les compteurs d'énergie
    esp_err_t err = can_publisher_conversion_persist_energy_state();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Compteurs énergie sauvegardés");
    } else {
        ESP_LOGW(TAG, "Échec sauvegarde énergie: %s", esp_err_to_name(err));
    }

    // Nettoyer le mutex
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_event_bus = NULL;
    s_initialized = false;

    ESP_LOGI(TAG, "CAN Publisher dé-initialisé");
}

void can_publisher_get_stats(uint32_t *publish_count, uint64_t *last_publish_ms)
{
    if (publish_count) {
        *publish_count = s_publish_count;
    }
    if (last_publish_ms) {
        *last_publish_ms = s_last_publish_ms;
    }
}
