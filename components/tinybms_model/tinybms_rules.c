// components/tinybms_model/tinybms_rules.c

#include "tinybms_rules.h"

#include "event_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *key;
    float       threshold;
    float       hysteresis;
    uint32_t    min_duration_ms;
    int         severity;
    const char *message;
} tinybms_rule_def_t;

typedef struct {
    tinybms_rule_def_t def;
    bool               active;
    uint64_t           over_since_ms;
    alert_entry_t      alert;
} tinybms_rule_runtime_t;

static const char *TAG = "tinybms_rules";

// Valeurs par défaut (peuvent être adaptées via registres/JSON plus tard)
static tinybms_rule_def_t s_rule_defs[] = {
    {.key = "cell_voltage_mv", .threshold = 3650.f, .hysteresis = 50.f, .min_duration_ms = 500, .severity = 3, .message = "Tension cellule haute"},
    {.key = "pack_delta_mv",   .threshold = 120.f,  .hysteresis = 20.f, .min_duration_ms = 500, .severity = 2, .message = "Delta pack élevé"},
    {.key = "bms_temperature_c", .threshold = 60.f, .hysteresis = 5.f,  .min_duration_ms = 500, .severity = 4, .message = "Température BMS élevée"},
};

static struct {
    event_bus_t *bus;
    tinybms_rule_runtime_t rules[sizeof(s_rule_defs) / sizeof(s_rule_defs[0])];
    uint32_t next_id;
    uint32_t active_count;
    uint32_t ack_count;
    uint64_t last_frame_ms;
    uint32_t watchdog_timeout_ms;
    bool     comm_alert_active;
} s_ctx = {
    .bus = NULL,
    .next_id = 1,
    .watchdog_timeout_ms = 5000,
};

static uint64_t now_ms(void)
{
    return (uint64_t) xTaskGetTickCount() * (uint64_t) portTICK_PERIOD_MS;
}

static void publish_counters(void)
{
    if (!s_ctx.bus) {
        return;
    }

    tinybms_alert_counters_t counters = {
        .active_count = s_ctx.active_count,
        .acknowledged_count = s_ctx.ack_count,
        .comm_watchdog = s_ctx.comm_alert_active,
        .last_frame_ms = s_ctx.last_frame_ms,
    };

    event_t evt = {
        .type = EVENT_TINYBMS_ALERT_COUNTERS,
        .data = &counters,
    };
    event_bus_publish(s_ctx.bus, &evt);
}

static void publish_alert(const alert_entry_t *alert, bool active)
{
    if (!s_ctx.bus) {
        return;
    }

    tinybms_alert_event_t payload = {
        .alert = *alert,
        .active = active,
    };

    event_t evt = {
        .type = active ? EVENT_TINYBMS_ALERT_TRIGGERED : EVENT_TINYBMS_ALERT_RECOVERED,
        .data = &payload,
    };
    event_bus_publish(s_ctx.bus, &evt);
}

static void update_ack_counter(const alert_entry_t *alert)
{
    // Mise à jour du compteur d'acquittement
    s_ctx.ack_count = 0;
    for (size_t i = 0; i < sizeof(s_ctx.rules) / sizeof(s_ctx.rules[0]); i++) {
        if (s_ctx.rules[i].active && s_ctx.rules[i].alert.acknowledged) {
            s_ctx.ack_count++;
        }
    }
    // Inclure l'alerte watchdog éventuellement
    if (s_ctx.comm_alert_active && alert && alert->acknowledged) {
        s_ctx.ack_count++;
    }
}

static void trigger_rule(tinybms_rule_runtime_t *rule)
{
    if (rule->active) {
        return;
    }

    rule->active = true;
    s_ctx.active_count++;

    memset(&rule->alert, 0, sizeof(rule->alert));
    rule->alert.id = (int) s_ctx.next_id++;
    rule->alert.code = 0;
    rule->alert.severity = rule->def.severity;
    rule->alert.timestamp_ms = now_ms();
    rule->alert.acknowledged = false;
    snprintf(rule->alert.message, sizeof(rule->alert.message), "%s (%.2f)", rule->def.message, rule->def.threshold);
    strncpy(rule->alert.source, "TinyBMS", sizeof(rule->alert.source) - 1);
    strncpy(rule->alert.status, "active", sizeof(rule->alert.status) - 1);

    publish_alert(&rule->alert, true);
    update_ack_counter(&rule->alert);
    publish_counters();
    ESP_LOGW(TAG, "Alerte TinyBMS activée: %s", rule->def.message);
}

static void recover_rule(tinybms_rule_runtime_t *rule)
{
    if (!rule->active) {
        return;
    }

    rule->active = false;
    if (s_ctx.active_count > 0) {
        s_ctx.active_count--;
    }
    strncpy(rule->alert.status, "resolved", sizeof(rule->alert.status) - 1);
    publish_alert(&rule->alert, false);
    update_ack_counter(&rule->alert);
    publish_counters();
    ESP_LOGI(TAG, "Alerte TinyBMS résolue: %s", rule->def.message);
}

static void evaluate_rule(tinybms_rule_runtime_t *rule, float value)
{
    if (!rule || !rule->def.key) {
        return;
    }

    uint64_t now = now_ms();
    float on_threshold = rule->def.threshold;
    float off_threshold = rule->def.threshold - rule->def.hysteresis;

    if (value >= on_threshold) {
        if (rule->over_since_ms == 0) {
            rule->over_since_ms = now;
        }
        if (!rule->active && (now - rule->over_since_ms) >= rule->def.min_duration_ms) {
            trigger_rule(rule);
        }
    } else if (value <= off_threshold) {
        rule->over_since_ms = 0;
        if (rule->active) {
            recover_rule(rule);
        }
    }
}

static void handle_register_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_register_update_t *update = (const tinybms_register_update_t *) event->data;
    s_ctx.last_frame_ms = now_ms();

    for (size_t i = 0; i < sizeof(s_ctx.rules) / sizeof(s_ctx.rules[0]); i++) {
        if (strncmp(update->key, s_ctx.rules[i].def.key, sizeof(update->key)) == 0) {
            evaluate_rule(&s_ctx.rules[i], update->user_value);
        }
    }
    publish_counters();
}

static void handle_stats_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const tinybms_stats_event_t *stats_evt = (const tinybms_stats_event_t *) event->data;
    s_ctx.last_frame_ms = stats_evt->timestamp_ms;

    uint32_t comm_errors = stats_evt->stats.timeouts + stats_evt->stats.crc_errors + stats_evt->stats.nacks;
    // Règle dédiée aux erreurs de com (seuil simple, pas de table dynamique)
    if (comm_errors > 0) {
        ESP_LOGW(TAG, "Erreurs de communication TinyBMS: %u", comm_errors);
    }
    publish_counters();
}

static void handle_ack_request(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const user_input_ack_alert_t *req = (const user_input_ack_alert_t *) event->data;
    for (size_t i = 0; i < sizeof(s_ctx.rules) / sizeof(s_ctx.rules[0]); i++) {
        if (s_ctx.rules[i].alert.id == req->alert_id && s_ctx.rules[i].active) {
            s_ctx.rules[i].alert.acknowledged = true;
            update_ack_counter(&s_ctx.rules[i].alert);
            publish_counters();
            ESP_LOGI(TAG, "Alerte TinyBMS %d acquittée", req->alert_id);
            break;
        }
    }
}

static void watchdog_task(void *param)
{
    (void) param;
    const TickType_t delay = pdMS_TO_TICKS(1000);
    while (1) {
        vTaskDelay(delay);

        uint64_t now = now_ms();
        bool timeout = (s_ctx.last_frame_ms > 0) && ((now - s_ctx.last_frame_ms) > s_ctx.watchdog_timeout_ms);

        if (timeout && !s_ctx.comm_alert_active) {
            s_ctx.comm_alert_active = true;
            s_ctx.active_count++;

            alert_entry_t alert = {0};
            alert.id = (int) s_ctx.next_id++;
            alert.severity = 4;
            alert.timestamp_ms = now;
            snprintf(alert.message, sizeof(alert.message), "Watchdog TinyBMS: aucune frame > %lu ms", (unsigned long) s_ctx.watchdog_timeout_ms);
            strncpy(alert.source, "TinyBMS", sizeof(alert.source) - 1);
            strncpy(alert.status, "active", sizeof(alert.status) - 1);

            publish_alert(&alert, true);
            update_ack_counter(&alert);
            publish_counters();
            ESP_LOGE(TAG, "%s", alert.message);
        } else if (!timeout && s_ctx.comm_alert_active) {
            s_ctx.comm_alert_active = false;
            if (s_ctx.active_count > 0) {
                s_ctx.active_count--;
            }

            alert_entry_t alert = {0};
            alert.id = 0;
            alert.severity = 1;
            alert.timestamp_ms = now;
            strncpy(alert.message, "Watchdog TinyBMS résolu", sizeof(alert.message) - 1);
            strncpy(alert.source, "TinyBMS", sizeof(alert.source) - 1);
            strncpy(alert.status, "resolved", sizeof(alert.status) - 1);

            publish_alert(&alert, false);
            publish_counters();
            ESP_LOGI(TAG, "Watchdog TinyBMS rétabli");
        }
    }
}

void tinybms_rules_init(event_bus_t *bus)
{
    if (!bus) {
        ESP_LOGE(TAG, "EventBus manquant pour tinybms_rules");
        return;
    }

    s_ctx.bus = bus;
    s_ctx.last_frame_ms = now_ms();

    for (size_t i = 0; i < sizeof(s_rule_defs) / sizeof(s_rule_defs[0]); i++) {
        s_ctx.rules[i].def = s_rule_defs[i];
        s_ctx.rules[i].active = false;
        s_ctx.rules[i].over_since_ms = 0;
    }

    event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, handle_register_update, NULL);
    event_bus_subscribe(bus, EVENT_TINYBMS_STATS_UPDATED, handle_stats_update, NULL);
    event_bus_subscribe(bus, EVENT_USER_INPUT_ACK_ALERT, handle_ack_request, NULL);

    xTaskCreate(watchdog_task, "tinybms_watchdog", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    publish_counters();
    ESP_LOGI(TAG, "Moteur de règles TinyBMS initialisé (%d règles)", (int) (sizeof(s_rule_defs) / sizeof(s_rule_defs[0])));
}
