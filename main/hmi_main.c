// main/hmi_main.c
#include "hmi_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "event_bus.h"
#include "event_types.h"
#include "logger.h"
#include "net_client.h"
#include "remote_event_adapter.h"
#include "telemetry_model.h"
#include "system_events_model.h"
#include "config_model.h"
#include "tinybms_client.h"
#include "tinybms_model.h"
#include "gui_init.h"
#include "history_model.h"
#include "operation_mode.h"

static const char *TAG = "HMI_MAIN";

// EventBus global pour ce firmware HMI
static event_bus_t s_event_bus;
static hmi_operation_mode_t s_operation_mode = HMI_MODE_CONNECTED_S3;
static bool s_remote_initialized = false;
static bool s_remote_started     = false;

static void hmi_create_core_tasks(void);
static void publish_operation_mode_state(bool telemetry_expected);
static void handle_user_change_mode(event_bus_t *bus, const event_t *event, void *user_ctx);
static void ensure_remote_modules_started(bool telemetry_expected);

void hmi_main_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI core");

    // 1) Init EventBus
    event_bus_init(&s_event_bus);

    // Charger le mode de fonctionnement (persisté en NVS)
    if (operation_mode_init() == ESP_OK) {
        s_operation_mode = operation_mode_get();
    }

    // 2) Init logger (optionnel : logger peut publier certains events)
    logger_init(&s_event_bus);

    // 3) Init modules "modèle" et "comm"
    bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);
    if (telemetry_expected) {
        net_client_init(&s_event_bus);             // WiFi + WS/HTTP client vers S3
        remote_event_adapter_init(&s_event_bus);   // JSON <-> EventBus
        s_remote_initialized = true;
    } else {
        ESP_LOGI(TAG, "Autonome TinyBMS : net_client/remote_adapter non initialisés");
    }

    telemetry_model_init(&s_event_bus);        // modèle batterie / pack
    system_events_model_init(&s_event_bus);    // modèle statut système (wifi, storage, etc.)
    config_model_init(&s_event_bus);           // modèle config (plus tard)
    history_model_init(&s_event_bus);          // historique local + backend

    // 3b) Init TinyBMS (UART direct)
    tinybms_client_init(&s_event_bus);         // Client UART TinyBMS
    tinybms_model_init(&s_event_bus);          // Modèle registres TinyBMS

    // 4) Init GUI (LVGL + écrans)
    gui_init(&s_event_bus);

    // Abonnement au changement de mode (toggle GUI/menu futur)
    event_bus_subscribe(&s_event_bus, EVENT_USER_INPUT_CHANGE_MODE, handle_user_change_mode, NULL);
}

void hmi_main_start(void)
{
    ESP_LOGI(TAG, "Starting HMI modules");

    // 1) Lancer les tâches centrales
    hmi_create_core_tasks();

    // 2) Démarrer les modules qui ont un "start"
    bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);

    if (telemetry_expected && s_remote_initialized) {
        ensure_remote_modules_started(telemetry_expected);
    } else {
        publish_operation_mode_state(telemetry_expected);
    }

    telemetry_model_start();        // si besoin d'une task (sinon peut être vide)
    system_events_model_start();
    config_model_start();
    history_model_start();

    // 2b) Démarrer TinyBMS
    tinybms_client_start();         // Connexion UART TinyBMS

    gui_start();                    // si la GUI a besoin d'une task spécifique (en plus de LVGL)
}

static void hmi_create_core_tasks(void)
{
    // Si tu veux une task de dispatch EventBus dédiée, tu peux la créer ici
    // ex: xTaskCreate(event_bus_dispatch_task, "event_dispatch", 4096, &s_event_bus, 5, NULL);
}

static void publish_operation_mode_state(bool telemetry_expected)
{
    operation_mode_event_t mode_evt = {
        .mode = s_operation_mode,
        .telemetry_expected = telemetry_expected,
    };

    event_t evt_mode = {
        .type = EVENT_OPERATION_MODE_CHANGED,
        .data = &mode_evt,
    };
    event_bus_publish(&s_event_bus, &evt_mode);

    system_status_t status = {
        .wifi_connected = false,
        .server_reachable = false,
        .storage_ok = true,
        .has_error = false,
        .operation_mode = s_operation_mode,
        .telemetry_expected = telemetry_expected,
    };

    event_t evt_sys = {
        .type = EVENT_SYSTEM_STATUS_UPDATED,
        .data = &status,
    };
    event_bus_publish(&s_event_bus, &evt_sys);
}

static void ensure_remote_modules_started(bool telemetry_expected)
{
    if (!telemetry_expected) {
        publish_operation_mode_state(telemetry_expected);
        return;
    }

    if (!s_remote_initialized) {
        net_client_init(&s_event_bus);
        remote_event_adapter_init(&s_event_bus);
        s_remote_initialized = true;
    }

    net_client_set_operation_mode(s_operation_mode, telemetry_expected);
    remote_event_adapter_set_operation_mode(s_operation_mode, telemetry_expected);

    if (!s_remote_started) {
        net_client_start();
        remote_event_adapter_start();
        s_remote_started = true;
    }

    publish_operation_mode_state(telemetry_expected);
}

static void handle_user_change_mode(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    const user_input_change_mode_t *req = (const user_input_change_mode_t *) event->data;
    if (!req) {
        return;
    }

    if (operation_mode_set(req->mode) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid operation mode requested: %d", req->mode);
        return;
    }

    s_operation_mode = req->mode;
    bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);

    if (telemetry_expected) {
        ensure_remote_modules_started(telemetry_expected);
    } else {
        if (s_remote_initialized) {
            net_client_set_operation_mode(s_operation_mode, telemetry_expected);
            remote_event_adapter_set_operation_mode(s_operation_mode, telemetry_expected);
        }
        publish_operation_mode_state(telemetry_expected);
    }
}
