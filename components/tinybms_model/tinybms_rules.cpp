#include "tinybms_rules.h"

extern "C" {
#include "event_types.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "tinybms_time_utils.h"

namespace {

using tinybms::TimeUtils;

static const char *TAG = "tinybms_rules";

extern "C" void tinybms_rules_on_register(event_bus_t *bus, const event_t *event, void *ctx);
extern "C" void tinybms_rules_on_stats(event_bus_t *bus, const event_t *event, void *ctx);
extern "C" void tinybms_rules_on_ack(event_bus_t *bus, const event_t *event, void *ctx);
extern "C" void tinybms_rules_watchdog_task(void *param);

struct RuleDefinition {
    std::string_view key;
    float threshold;
    float hysteresis;
    uint32_t min_duration_ms;
    int severity;
    std::string_view message;
};

struct RuleRuntime {
    const RuleDefinition *def = nullptr;
    bool active = false;
    uint64_t over_since_ms = 0;
    alert_entry_t alert = {};
};

constexpr std::array<RuleDefinition, 3> kRuleDefinitions = {{{
    {"cell_voltage_mv", 3650.f, 50.f, 500, 3, "Tension cellule haute"},
    {"pack_delta_mv", 120.f, 20.f, 500, 2, "Delta pack élevé"},
    {"bms_temperature_c", 60.f, 5.f, 500, 4, "Température BMS élevée"},
}};

template <size_t N>
void copy_to_cstr(std::string_view src, char (&dest)[N])
{
    const size_t len = std::min(src.size(), N - 1);
    if (len > 0) {
        std::memcpy(dest, src.data(), len);
    }
    dest[len] = '\0';
}

template <size_t N>
std::string_view make_string_view(const char (&buffer)[N])
{
    size_t len = 0;
    while (len < N && buffer[len] != '\0') {
        ++len;
    }
    return std::string_view(buffer, len);
}

class TinyBMSRules {
public:
    static TinyBMSRules &instance()
    {
        static TinyBMSRules inst;
        return inst;
    }

    void init(event_bus_t *bus)
    {
        if (initialized_) {
            ESP_LOGW(TAG, "tinybms_rules already initialized");
            return;
        }
        if (bus == nullptr) {
            ESP_LOGE(TAG, "EventBus manquant pour tinybms_rules");
            return;
        }

        bus_ = bus;
        last_frame_ms_ = TimeUtils::now_ms_64();

        for (size_t i = 0; i < rules_.size(); ++i) {
            rules_[i].def = &kRuleDefinitions[i];
            rules_[i].active = false;
            rules_[i].over_since_ms = 0;
            std::memset(&rules_[i].alert, 0, sizeof(alert_entry_t));
        }

        event_bus_subscribe(bus_, EVENT_TINYBMS_REGISTER_UPDATED,
                            tinybms_rules_on_register, this);
        event_bus_subscribe(bus_, EVENT_TINYBMS_STATS_UPDATED,
                            tinybms_rules_on_stats, this);
        event_bus_subscribe(bus_, EVENT_USER_INPUT_ACK_ALERT,
                            tinybms_rules_on_ack, this);

        BaseType_t created = xTaskCreate(tinybms_rules_watchdog_task, "tinybms_watchdog",
                                         4096, this, tskIDLE_PRIORITY + 1, &watchdog_task_);
        if (created != pdPASS) {
            ESP_LOGE(TAG, "Impossible de créer la tâche watchdog TinyBMS");
        }

        publish_counters();
        initialized_ = true;
        ESP_LOGI(TAG, "Moteur de règles TinyBMS initialisé (%d règles)",
                 static_cast<int>(kRuleDefinitions.size()));
    }

private:
    TinyBMSRules() = default;

    void handle_register_update(const event_t *event)
    {
        if (event == nullptr || event->data == nullptr) {
            return;
        }
        const auto *update = static_cast<const tinybms_register_update_t *>(event->data);
        last_frame_ms_ = TimeUtils::now_ms_64();

        const std::string_view key = make_string_view(update->key);
        for (auto &rule : rules_) {
            if (rule.def != nullptr && key == rule.def->key) {
                evaluate_rule(rule, update->user_value);
            }
        }
        publish_counters();
    }

    void handle_stats_update(const event_t *event)
    {
        if (event == nullptr || event->data == nullptr) {
            return;
        }
        const auto *stats_evt = static_cast<const tinybms_stats_event_t *>(event->data);
        last_frame_ms_ = stats_evt->timestamp_ms;

        const uint32_t comm_errors = stats_evt->stats.timeouts +
                                     stats_evt->stats.crc_errors +
                                     stats_evt->stats.nacks;
        if (comm_errors > 0) {
            ESP_LOGW(TAG, "Erreurs de communication TinyBMS: %u", comm_errors);
        }
        publish_counters();
    }

    void handle_ack_request(const event_t *event)
    {
        if (event == nullptr || event->data == nullptr) {
            return;
        }
        const auto *req = static_cast<const user_input_ack_alert_t *>(event->data);
        for (auto &rule : rules_) {
            if (rule.active && rule.alert.id == req->alert_id) {
                rule.alert.acknowledged = true;
                update_ack_counter(&rule.alert);
                publish_counters();
                ESP_LOGI(TAG, "Alerte TinyBMS %d acquittée", req->alert_id);
                break;
            }
        }
    }

    void watchdog_loop()
    {
        const TickType_t delay = pdMS_TO_TICKS(1000);
        while (true) {
            vTaskDelay(delay);

            const uint64_t now = TimeUtils::now_ms_64();
            const bool timeout = (last_frame_ms_ > 0) &&
                                 ((now - last_frame_ms_) > watchdog_timeout_ms_);

            if (timeout && !comm_alert_active_) {
                comm_alert_active_ = true;
                ++active_count_;

                alert_entry_t alert = {};
                alert.id = static_cast<int>(next_id_++);
                alert.severity = 4;
                alert.timestamp_ms = now;
                std::snprintf(alert.message, sizeof(alert.message),
                              "Watchdog TinyBMS: aucune frame > %lu ms",
                              static_cast<unsigned long>(watchdog_timeout_ms_));
                copy_to_cstr("TinyBMS", alert.source);
                copy_to_cstr("active", alert.status);

                publish_alert(alert, true);
                update_ack_counter(&alert);
                publish_counters();
                ESP_LOGE(TAG, "%s", alert.message);
            } else if (!timeout && comm_alert_active_) {
                comm_alert_active_ = false;
                if (active_count_ > 0) {
                    --active_count_;
                }

                alert_entry_t alert = {};
                alert.id = 0;
                alert.severity = 1;
                alert.timestamp_ms = now;
                copy_to_cstr("Watchdog TinyBMS résolu", alert.message);
                copy_to_cstr("TinyBMS", alert.source);
                copy_to_cstr("resolved", alert.status);

                publish_alert(alert, false);
                publish_counters();
                ESP_LOGI(TAG, "Watchdog TinyBMS rétabli");
            }
        }
    }

    void evaluate_rule(RuleRuntime &rule, float value)
    {
        if (rule.def == nullptr) {
            return;
        }
        const uint64_t now = TimeUtils::now_ms_64();
        const float on_threshold = rule.def->threshold;
        const float off_threshold = rule.def->threshold - rule.def->hysteresis;

        if (value >= on_threshold) {
            if (rule.over_since_ms == 0) {
                rule.over_since_ms = now;
            }
            if (!rule.active && (now - rule.over_since_ms) >= rule.def->min_duration_ms) {
                trigger_rule(rule);
            }
        } else if (value <= off_threshold) {
            rule.over_since_ms = 0;
            if (rule.active) {
                recover_rule(rule);
            }
        }
    }

    void trigger_rule(RuleRuntime &rule)
    {
        if (rule.active || rule.def == nullptr) {
            return;
        }

        rule.active = true;
        ++active_count_;

        rule.alert = {};
        rule.alert.id = static_cast<int>(next_id_++);
        rule.alert.severity = rule.def->severity;
        rule.alert.timestamp_ms = TimeUtils::now_ms_64();
        rule.alert.acknowledged = false;
        std::snprintf(rule.alert.message, sizeof(rule.alert.message), "%s (%.2f)",
                      rule.def->message.data(), rule.def->threshold);
        copy_to_cstr("TinyBMS", rule.alert.source);
        copy_to_cstr("active", rule.alert.status);

        publish_alert(rule.alert, true);
        update_ack_counter(&rule.alert);
        publish_counters();
        ESP_LOGW(TAG, "Alerte TinyBMS activée: %s", rule.def->message.data());
    }

    void recover_rule(RuleRuntime &rule)
    {
        if (!rule.active) {
            return;
        }

        rule.active = false;
        if (active_count_ > 0) {
            --active_count_;
        }
        copy_to_cstr("resolved", rule.alert.status);
        publish_alert(rule.alert, false);
        update_ack_counter(&rule.alert);
        publish_counters();
        ESP_LOGI(TAG, "Alerte TinyBMS résolue: %s", rule.def->message.data());
    }

    void publish_alert(const alert_entry_t &alert, bool active) const
    {
        if (bus_ == nullptr) {
            return;
        }
        tinybms_alert_event_t payload = {
            .alert = alert,
            .active = active,
        };
        event_t evt = {
            .type = active ? EVENT_TINYBMS_ALERT_TRIGGERED : EVENT_TINYBMS_ALERT_RECOVERED,
            .data = &payload,
            .data_size = sizeof(payload),
        };
        event_bus_publish(bus_, &evt);
    }

    void update_ack_counter(const alert_entry_t *alert)
    {
        (void)alert;
        ack_count_ = 0;
        for (const auto &rule : rules_) {
            if (rule.active && rule.alert.acknowledged) {
                ++ack_count_;
            }
        }
        if (comm_alert_active_ && alert != nullptr && alert->acknowledged) {
            ++ack_count_;
        }
    }

    void publish_counters() const
    {
        if (bus_ == nullptr) {
            return;
        }
        tinybms_alert_counters_t counters = {
            .active_count = active_count_,
            .acknowledged_count = ack_count_,
            .comm_watchdog = comm_alert_active_,
            .last_frame_ms = last_frame_ms_,
        };
        event_t evt = {
            .type = EVENT_TINYBMS_ALERT_COUNTERS,
            .data = &counters,
            .data_size = sizeof(counters),
        };
        event_bus_publish(bus_, &evt);
    }

    event_bus_t *bus_ = nullptr;
    std::array<RuleRuntime, kRuleDefinitions.size()> rules_{};
    TaskHandle_t watchdog_task_ = nullptr;
    uint32_t next_id_ = 1;
    uint32_t active_count_ = 0;
    uint32_t ack_count_ = 0;
    uint64_t last_frame_ms_ = 0;
    uint32_t watchdog_timeout_ms_ = 5000;
    bool comm_alert_active_ = false;
    bool initialized_ = false;

    friend void tinybms_rules_on_register(event_bus_t *bus, const event_t *event, void *ctx);
    friend void tinybms_rules_on_stats(event_bus_t *bus, const event_t *event, void *ctx);
    friend void tinybms_rules_on_ack(event_bus_t *bus, const event_t *event, void *ctx);
    friend void tinybms_rules_watchdog_task(void *param);
};

extern "C" void tinybms_rules_on_register(event_bus_t *bus, const event_t *event, void *ctx)
{
    (void)bus;
    auto *self = static_cast<TinyBMSRules *>(ctx);
    if (self != nullptr) {
        self->handle_register_update(event);
    }
}

extern "C" void tinybms_rules_on_stats(event_bus_t *bus, const event_t *event, void *ctx)
{
    (void)bus;
    auto *self = static_cast<TinyBMSRules *>(ctx);
    if (self != nullptr) {
        self->handle_stats_update(event);
    }
}

extern "C" void tinybms_rules_on_ack(event_bus_t *bus, const event_t *event, void *ctx)
{
    (void)bus;
    auto *self = static_cast<TinyBMSRules *>(ctx);
    if (self != nullptr) {
        self->handle_ack_request(event);
    }
}

extern "C" void tinybms_rules_watchdog_task(void *param)
{
    auto *self = static_cast<TinyBMSRules *>(param);
    if (self != nullptr) {
        self->watchdog_loop();
    }
    vTaskDelete(nullptr);
}

}  // namespace

extern "C" void tinybms_rules_init(event_bus_t *bus)
{
    TinyBMSRules::instance().init(bus);
}
