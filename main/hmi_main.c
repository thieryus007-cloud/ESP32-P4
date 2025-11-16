// main/hmi_main.c
#include "hmi_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "event_bus.h"
#include "logger.h"
#include "net_client.h"
#include "remote_event_adapter.h"
#include "telemetry_model.h"
#include "system_events_model.h"
#include "config_model.h"
#include "gui_init.h"

static const char *TAG = "HMI_MAIN";

// EventBus global pour ce firmware HMI
static event_bus_t s_event_bus;

static void hmi_create_core_tasks(void);

void hmi_main_init(void)
{
    ESP_LOGI(TAG, "Initializing HMI core");

    // 1) Init EventBus
    event_bus_init(&s_event_bus);

    // 2) Init logger (optionnel : logger peut publier certains events)
    logger_init(&s_event_bus);

    // 3) Init modules "modèle" et "comm"
    net_client_init(&s_event_bus);             // WiFi + WS/HTTP client vers S3
    remote_event_adapter_init(&s_event_bus);   // JSON <-> EventBus

    telemetry_model_init(&s_event_bus);        // modèle batterie / pack
    system_events_model_init(&s_event_bus);    // modèle statut système (wifi, storage, etc.)
    config_model_init(&s_event_bus);           // modèle config (plus tard)

    // 4) Init GUI (LVGL + écrans)
    gui_init(&s_event_bus);
}

void hmi_main_start(void)
{
    ESP_LOGI(TAG, "Starting HMI modules");

    // 1) Lancer les tâches centrales
    hmi_create_core_tasks();

    // 2) Démarrer les modules qui ont un "start"
    net_client_start();             // connexion WiFi + WS
    remote_event_adapter_start();   // loop de parsing / émission
    telemetry_model_start();        // si besoin d'une task (sinon peut être vide)
    system_events_model_start();
    config_model_start();
    gui_start();                    // si la GUI a besoin d'une task spécifique (en plus de LVGL)
}

static void hmi_create_core_tasks(void)
{
    // Si tu veux une task de dispatch EventBus dédiée, tu peux la créer ici
    // ex: xTaskCreate(event_bus_dispatch_task, "event_dispatch", 4096, &s_event_bus, 5, NULL);
}
