// main/hmi_main.cpp
#include "hmi_main.h"

#include <memory>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "config_manager.h"
#include "config_model.h"
#include "diagnostic_logger.h"
#include "event_bus.h"
#include "event_types.h"
#include "gui_init.hpp"
#include "history_model.h"
#include "logger.h"
#include "mqtt_gateway.h"
#include "net_client.h"
#include "network_publisher.h"
#include "operation_mode.h"
#include "remote_event_adapter.h"
#include "stats_aggregator.h"
#include "status_endpoint.h"
#include "system_events_model.h"
#include "telemetry_model.h"
#include "tinybms_client.h"
#include "tinybms_model.h"

static const char *TAG = "HMI_MAIN";

#ifndef EVENT_BUS_TASK_STACK_SIZE
#define EVENT_BUS_TASK_STACK_SIZE 5120
#endif

#ifdef CONFIG_LVGL_TASK_PRIORITY
#define EVENT_BUS_TASK_PRIORITY (CONFIG_LVGL_TASK_PRIORITY + 1)
#else
#define EVENT_BUS_TASK_PRIORITY 6
#endif

// EventBus global pour ce firmware HMI
static event_bus_t s_event_bus;
static hmi_operation_mode_t s_operation_mode = HMI_MODE_CONNECTED_S3;
static bool s_remote_initialized = false;
static bool s_remote_started = false;
static system_status_t s_last_system_status;
static bool s_has_last_status = false;
static std::unique_ptr<gui::GuiRoot> s_gui_root;

static void hmi_create_core_tasks(void);
static void publish_operation_mode_state(bool telemetry_expected);
static void handle_user_change_mode(event_bus_t *bus, const event_t *event,
                                    void *user_ctx);
static void handle_network_failover(event_bus_t *bus, const event_t *event,
                                    void *user_ctx);
static void handle_system_status(event_bus_t *bus, const event_t *event,
                                 void *user_ctx);
static void ensure_remote_modules_started(bool telemetry_expected);
static void ensure_remote_modules_stopped(bool telemetry_expected);

#ifdef __cplusplus
extern "C" {
#endif

void hmi_main_init(void) {
  ESP_LOGI(TAG, "Initializing HMI core");

  // 1) Init EventBus
  event_bus_init(&s_event_bus);

  event_bus_subscribe(&s_event_bus, EVENT_SYSTEM_STATUS_UPDATED,
                      handle_system_status, NULL);

  // Config persistante (seuils, destinations, etc.)
  config_manager_init();

  // Journal circulaire de diagnostic (UART/RS485)
  diagnostic_logger_init(&s_event_bus);

  // Charger le mode de fonctionnement (persisté en NVS)
  if (operation_mode_init() == ESP_OK) {
    s_operation_mode = operation_mode_get();
  }

  // 2) Init logger (optionnel : logger peut publier certains events)
  logger_init(&s_event_bus);

  // 3) Init modules "modèle" et "comm"
  bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);
  if (telemetry_expected) {
    net_client_init(&s_event_bus);           // WiFi + WS/HTTP client vers S3
    remote_event_adapter_init(&s_event_bus); // JSON <-> EventBus
    s_remote_initialized = true;
  } else {
    ESP_LOGI(TAG,
             "Autonome TinyBMS : net_client/remote_adapter non initialisés");
  }

  telemetry_model_init(&s_event_bus); // modèle batterie / pack
  system_events_model_init(
      &s_event_bus); // modèle statut système (wifi, storage, etc.)
  config_model_init(&s_event_bus);      // modèle config (plus tard)
  history_model_init(&s_event_bus);     // historique local + backend
  stats_aggregator_init(&s_event_bus);  // agrégation locale des stats 24h/7j
  network_publisher_init(&s_event_bus); // publication périodique MQTT/HTTP
  status_endpoint_init(&s_event_bus);   // exposition statut backend
  mqtt_gateway_init(&s_event_bus); // passerelle MQTT TinyBMS (local + MQTT)

  // 3b) Init TinyBMS (UART direct)
  tinybms_client_init(&s_event_bus); // Client UART TinyBMS
  tinybms_model_init(&s_event_bus);  // Modèle registres TinyBMS

  // 4) Init GUI (LVGL + écrans)
  s_gui_root = std::make_unique<gui::GuiRoot>(&s_event_bus);
  s_gui_root->init();

  // Abonnement au changement de mode (toggle GUI/menu futur)
  event_bus_subscribe(&s_event_bus, EVENT_USER_INPUT_CHANGE_MODE,
                      handle_user_change_mode, NULL);
  event_bus_subscribe(&s_event_bus, EVENT_NETWORK_FAILOVER_ACTIVATED,
                      handle_network_failover, NULL);
}

void hmi_main_start(void) {
  ESP_LOGI(TAG, "Starting HMI modules");

  // 1) Lancer les tâches centrales
  hmi_create_core_tasks();

  // 2) Démarrer les modules qui ont un "start"
  bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);

  if (telemetry_expected && s_remote_initialized) {
    ensure_remote_modules_started(telemetry_expected);
    publish_operation_mode_state(telemetry_expected);
  } else {
    publish_operation_mode_state(telemetry_expected);
  }

  telemetry_model_start(); // si besoin d'une task (sinon peut être vide)
  system_events_model_start();
  config_model_start();
  history_model_start();
  stats_aggregator_start();
  network_publisher_start();
  status_endpoint_start();

  // 2b) Démarrer TinyBMS
  tinybms_client_start(); // Connexion UART TinyBMS
  mqtt_gateway_start();

  if (s_gui_root) {
    s_gui_root
        ->start(); // si la GUI a besoin d'une task spécifique (en plus de LVGL)
  }
}

#ifdef __cplusplus
} // extern "C"
#endif

static void hmi_create_core_tasks(void) {
  BaseType_t rc;

#ifdef CONFIG_FREERTOS_UNICORE
  rc = xTaskCreate(event_bus_dispatch_task, "event_dispatch",
                   EVENT_BUS_TASK_STACK_SIZE, &s_event_bus,
                   EVENT_BUS_TASK_PRIORITY, NULL);
#else
  rc = xTaskCreatePinnedToCore(event_bus_dispatch_task, "event_dispatch",
                               EVENT_BUS_TASK_STACK_SIZE, &s_event_bus,
                               EVENT_BUS_TASK_PRIORITY, NULL, tskNO_AFFINITY);
#endif

  if (rc != pdPASS) {
    ESP_LOGE(TAG,
             "CRITICAL: Failed to start event dispatch task (rc=%ld). System "
             "halted.",
             (long)rc);
    abort(); // Arrêt immédiat si la tâche centrale ne peut pas démarrer
  }
}

static void publish_operation_mode_state(bool telemetry_expected) {
  operation_mode_event_t mode_evt = {};
  mode_evt.mode = s_operation_mode;
  mode_evt.telemetry_expected = telemetry_expected;

  event_t evt_mode = {};
  evt_mode.type = EVENT_OPERATION_MODE_CHANGED;
  evt_mode.data = &mode_evt;
  evt_mode.data_size = sizeof(mode_evt);
  event_bus_publish(&s_event_bus, &evt_mode);

  system_status_t status = {};
  status.wifi_connected = false;
  status.server_reachable = false;
  status.storage_ok = true;
  status.has_error = false;
  status.network_state = telemetry_expected ? NETWORK_STATE_CONNECTING
                                            : NETWORK_STATE_NOT_CONFIGURED;
  status.operation_mode = s_operation_mode;
  status.telemetry_expected = telemetry_expected;

  if (telemetry_expected && s_has_last_status &&
      s_last_system_status.telemetry_expected) {
    status.wifi_connected = s_last_system_status.wifi_connected;
    status.server_reachable = s_last_system_status.server_reachable;
    status.storage_ok = s_last_system_status.storage_ok;
    status.has_error = s_last_system_status.has_error;
    status.network_state = s_last_system_status.network_state;
  } else if (telemetry_expected && s_remote_started) {
    status.network_state = NETWORK_STATE_CONNECTING;
  }

  event_t evt_sys = {};
  evt_sys.type = EVENT_SYSTEM_STATUS_UPDATED;
  evt_sys.data = &status;
  evt_sys.data_size = sizeof(status);
  event_bus_publish(&s_event_bus, &evt_sys);
}

static void handle_system_status(event_bus_t *bus, const event_t *event,
                                 void *user_ctx) {
  (void)bus;
  (void)user_ctx;

  if (!event || !event->data) {
    return;
  }

  s_last_system_status = *(const system_status_t *)event->data;
  s_has_last_status = true;
}

static void ensure_remote_modules_started(bool telemetry_expected) {
  if (!telemetry_expected) {
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
}

static void ensure_remote_modules_stopped(bool telemetry_expected) {
  if (!s_remote_initialized) {
    publish_operation_mode_state(telemetry_expected);
    return;
  }

  if (s_remote_started) {
    net_client_stop();
    remote_event_adapter_stop();
    s_remote_started = false;
  }

  net_client_set_operation_mode(s_operation_mode, telemetry_expected);
  remote_event_adapter_set_operation_mode(s_operation_mode, telemetry_expected);

  publish_operation_mode_state(telemetry_expected);
}

static void transition_to_mode(hmi_operation_mode_t new_mode) {
  if (operation_mode_set(new_mode) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to persist mode %d", new_mode);
    return;
  }

  s_operation_mode = new_mode;
  bool telemetry_expected = (s_operation_mode == HMI_MODE_CONNECTED_S3);

  if (telemetry_expected) {
    ensure_remote_modules_started(telemetry_expected);
    publish_operation_mode_state(telemetry_expected);
  } else {
    ensure_remote_modules_stopped(telemetry_expected);
  }
}

static void handle_user_change_mode(event_bus_t *bus, const event_t *event,
                                    void *user_ctx) {
  (void)bus;
  (void)user_ctx;

  if (!event || !event->data) {
    ESP_LOGW(TAG, "Received NULL change-mode event");
    return;
  }

  const user_input_change_mode_t *req =
      (const user_input_change_mode_t *)event->data;
  ESP_LOGI(TAG, "User requested mode change to %d", req->mode);
  transition_to_mode(req->mode);
}

static void handle_network_failover(event_bus_t *bus, const event_t *event,
                                    void *user_ctx) {
  (void)bus;
  (void)user_ctx;

  if (!event || !event->data) {
    ESP_LOGW(TAG, "Received NULL failover event");
    return;
  }

  const network_failover_event_t *failover =
      (const network_failover_event_t *)event->data;

  if (s_operation_mode == failover->new_mode) {
    ESP_LOGW(TAG, "Failover event received but mode already %d",
             s_operation_mode);
    return;
  }

  ESP_LOGW(TAG, "WiFi failed %d times (threshold=%d), switching to mode %d",
           failover->fail_count, failover->fail_threshold, failover->new_mode);

  transition_to_mode(failover->new_mode);
}

static void hmi_create_core_tasks(void) {
  BaseType_t rc;

#ifdef CONFIG_FREERTOS_UNICORE
  rc = xTaskCreate(event_bus_dispatch_task, "event_dispatch",
                   EVENT_BUS_TASK_STACK_SIZE, &s_event_bus,
                   EVENT_BUS_TASK_PRIORITY, NULL);
#else
  rc = xTaskCreatePinnedToCore(event_bus_dispatch_task, "event_dispatch",
                               EVENT_BUS_TASK_STACK_SIZE, &s_event_bus,
                               EVENT_BUS_TASK_PRIORITY, NULL, tskNO_AFFINITY);
#endif

  if (rc != pdPASS) {
    ESP_LOGE(TAG,
             "CRITICAL: Failed to start event dispatch task (rc=%ld). System "
             "halted.",
             (long)rc);
    abort(); // Arrêt immédiat si la tâche centrale ne peut pas démarrer
  }
}
