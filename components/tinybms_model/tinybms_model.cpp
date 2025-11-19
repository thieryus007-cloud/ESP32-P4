/**
 * @file tinybms_model.cpp
 * @brief TinyBMS Model Implementation using modern C++ idioms
 */

extern "C" {
#include "tinybms_model.h"
#include "tinybms_client.h"
#include "event_types.h"
#include "tinybms_rules.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "tinybms_time_utils.h"

namespace {

using tinybms::TimeUtils;

static const char *TAG = "tinybms_model";

extern "C" void tinybms_model_user_write_request(event_bus_t *bus, const event_t *event, void *ctx);

template <size_t N>
void copy_to_cstr(std::string_view source, char (&dest)[N])
{
    const size_t copy_len = std::min(source.size(), N - 1);
    if (copy_len > 0) {
        std::memcpy(dest, source.data(), copy_len);
    }
    dest[copy_len] = '\0';
}

class TinyBMSModel {
public:
    static TinyBMSModel &instance()
    {
        static TinyBMSModel inst;
        return inst;
    }

    esp_err_t init(event_bus_t *bus)
    {
        if (initialized_) {
            ESP_LOGW(TAG, "Already initialized");
            return ESP_OK;
        }

        if (bus == nullptr) {
            ESP_LOGE(TAG, "EventBus is NULL");
            return ESP_ERR_INVALID_ARG;
        }

        bus_ = bus;
        init_cache();
        total_reads_ = 0;
        total_writes_ = 0;
        cache_hits_ = 0;

        event_bus_subscribe(bus_, EVENT_USER_INPUT_TINYBMS_WRITE_REG,
                            tinybms_model_user_write_request, this);

        tinybms_rules_init(bus_);

        initialized_ = true;
        ESP_LOGI(TAG, "TinyBMS model initialized with %d registers", TINYBMS_REGISTER_COUNT);
        return ESP_OK;
    }

    esp_err_t read_all()
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGI(TAG, "Reading all %d registers...", TINYBMS_REGISTER_COUNT);

        const register_descriptor_t *catalog = tinybms_get_register_catalog();
        uint32_t success_count = 0;
        uint32_t fail_count = 0;

        for (size_t i = 0; i < TINYBMS_REGISTER_COUNT; ++i) {
            uint16_t raw_value = 0;
            const esp_err_t ret = tinybms_read_register(catalog[i].address, &raw_value);
            if (ret == ESP_OK) {
                update_cache_and_publish(catalog[i].address, raw_value);
                ++success_count;
                ++total_reads_;
            } else {
                ESP_LOGW(TAG, "Failed to read register 0x%04X (%s): %s",
                         catalog[i].address, catalog[i].key, esp_err_to_name(ret));
                ++fail_count;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        ESP_LOGI(TAG, "Read complete: %lu successful, %lu failed",
                 static_cast<unsigned long>(success_count),
                 static_cast<unsigned long>(fail_count));

        if (success_count > 0) {
            publish_config_changed();
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    esp_err_t read_register(uint16_t address, float *user_value)
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        const register_descriptor_t *desc = tinybms_get_register_by_address(address);
        if (desc == nullptr) {
            ESP_LOGE(TAG, "Unknown register address 0x%04X", address);
            return ESP_ERR_NOT_FOUND;
        }

        uint16_t raw_value = 0;
        const esp_err_t ret = tinybms_read_register(address, &raw_value);
        if (ret == ESP_OK) {
            update_cache_and_publish(address, raw_value);
            ++total_reads_;
            if (user_value != nullptr) {
                *user_value = tinybms_raw_to_user(desc, raw_value);
            }
        }
        return ret;
    }

    esp_err_t write_register(uint16_t address, float user_value)
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        const register_descriptor_t *desc = tinybms_get_register_by_address(address);
        if (desc == nullptr) {
            ESP_LOGE(TAG, "Unknown register address 0x%04X", address);
            return ESP_ERR_NOT_FOUND;
        }

        if (desc->read_only) {
            ESP_LOGE(TAG, "Register %s is read-only", desc->key);
            return ESP_ERR_NOT_ALLOWED;
        }

        uint16_t raw_value = 0;
        esp_err_t ret = tinybms_user_to_raw(desc, user_value, &raw_value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Invalid value %.2f for register %s", user_value, desc->key);
            return ret;
        }

        if (!tinybms_validate_raw(desc, raw_value)) {
            ESP_LOGE(TAG, "Validation failed for register %s (raw=0x%04X)",
                     desc->key, raw_value);
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI(TAG, "Writing register %s: user=%.2f, raw=0x%04X",
                 desc->key, user_value, raw_value);

        uint16_t verified_value = 0;
        ret = tinybms_write_register(address, raw_value, &verified_value);
        if (ret == ESP_OK) {
            update_cache_and_publish(address, verified_value);
            ++total_writes_;
            publish_config_changed();
        }
        return ret;
    }

    esp_err_t get_cached(uint16_t address, float *user_value)
    {
        if (!initialized_ || user_value == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }

        const register_descriptor_t *desc = tinybms_get_register_by_address(address);
        if (desc == nullptr) {
            return ESP_ERR_NOT_FOUND;
        }

        const register_cache_entry_t *entry = find_cache_entry(address);
        if (entry == nullptr || !entry->valid) {
            return ESP_ERR_NOT_FOUND;
        }

        *user_value = tinybms_raw_to_user(desc, entry->raw_value);
        ++cache_hits_;
        return ESP_OK;
    }

    esp_err_t get_config(tinybms_config_t *config)
    {
        if (!initialized_ || config == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }

        std::memset(config, 0, sizeof(tinybms_config_t));

        auto assign_if_cached = [this, config](auto member, const char *key) {
            const register_descriptor_t *desc = tinybms_get_register_by_key(key);
            if (desc == nullptr) {
                return;
            }
            float value = 0.0f;
            if (get_cached(desc->address, &value) == ESP_OK) {
                using MemberType = std::remove_reference_t<decltype(config->*member)>;
                config->*member = static_cast<MemberType>(value);
            }
        };

        assign_if_cached(&tinybms_config_t::fully_charged_voltage_mv, "fully_charged_voltage_mv");
        assign_if_cached(&tinybms_config_t::fully_discharged_voltage_mv, "fully_discharged_voltage_mv");
        assign_if_cached(&tinybms_config_t::early_balancing_threshold_mv, "early_balancing_threshold_mv");
        assign_if_cached(&tinybms_config_t::charge_finished_current_ma, "charge_finished_current_ma");
        assign_if_cached(&tinybms_config_t::peak_discharge_current_a, "peak_discharge_current_a");
        assign_if_cached(&tinybms_config_t::battery_capacity_ah, "battery_capacity_ah");
        assign_if_cached(&tinybms_config_t::cell_count, "cell_count");
        assign_if_cached(&tinybms_config_t::allowed_disbalance_mv, "allowed_disbalance_mv");

        assign_if_cached(&tinybms_config_t::overvoltage_cutoff_mv, "overvoltage_cutoff_mv");
        assign_if_cached(&tinybms_config_t::undervoltage_cutoff_mv, "undervoltage_cutoff_mv");
        assign_if_cached(&tinybms_config_t::discharge_overcurrent_a, "discharge_overcurrent_a");
        assign_if_cached(&tinybms_config_t::charge_overcurrent_a, "charge_overcurrent_a");
        assign_if_cached(&tinybms_config_t::overheat_cutoff_c, "overheat_cutoff_c");
        assign_if_cached(&tinybms_config_t::low_temp_charge_cutoff_c, "low_temp_charge_cutoff_c");

        assign_if_cached(&tinybms_config_t::charge_restart_level_percent, "charge_restart_level_percent");
        assign_if_cached(&tinybms_config_t::battery_max_cycles, "battery_max_cycles");
        assign_if_cached(&tinybms_config_t::state_of_health_permille, "state_of_health_permille");
        assign_if_cached(&tinybms_config_t::state_of_charge_permille, "state_of_charge_permille");

        assign_if_cached(&tinybms_config_t::charger_type, "charger_type");
        assign_if_cached(&tinybms_config_t::load_switch_type, "load_switch_type");
        assign_if_cached(&tinybms_config_t::operation_mode, "operation_mode");

        return ESP_OK;
    }

    bool is_cached(uint16_t address) const
    {
        const register_cache_entry_t *entry = find_cache_entry(address);
        return entry != nullptr && entry->valid;
    }

    uint32_t cache_age(uint16_t address) const
    {
        const register_cache_entry_t *entry = find_cache_entry(address);
        if (entry == nullptr || !entry->valid) {
            return 0;
        }
        return TimeUtils::now_ms() - entry->last_update_ms;
    }

    void invalidate_cache()
    {
        for (auto &entry : cache_) {
            entry.valid = false;
        }
        ESP_LOGI(TAG, "Cache invalidated");
    }

    void stats(uint32_t *total_reads, uint32_t *total_writes, uint32_t *cache_hits) const
    {
        if (total_reads != nullptr) {
            *total_reads = total_reads_;
        }
        if (total_writes != nullptr) {
            *total_writes = total_writes_;
        }
        if (cache_hits != nullptr) {
            *cache_hits = cache_hits_;
        }
    }

    void handle_user_write_request(const event_t *event)
    {
        if (event == nullptr || event->data == nullptr) {
            return;
        }

        const auto *req = static_cast<const user_input_tinybms_write_t *>(event->data);
        ESP_LOGI(TAG, "User write request: address=0x%04X, value=0x%04X",
                 req->address, req->value);

        const register_descriptor_t *desc = tinybms_get_register_by_address(req->address);
        if (desc == nullptr) {
            ESP_LOGE(TAG, "Unknown register address 0x%04X", req->address);
            return;
        }

        const float user_value = tinybms_raw_to_user(desc, req->value);
        const esp_err_t ret = write_register(req->address, user_value);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Register write successful");
        } else {
            ESP_LOGE(TAG, "Register write failed: %s", esp_err_to_name(ret));
        }
    }

private:
    TinyBMSModel() = default;

    void init_cache()
    {
        const register_descriptor_t *catalog = tinybms_get_register_catalog();
        for (size_t i = 0; i < cache_.size(); ++i) {
            cache_[i].address = catalog[i].address;
            cache_[i].raw_value = 0;
            cache_[i].valid = false;
            cache_[i].last_update_ms = 0;
        }
    }

    register_cache_entry_t* find_cache_entry(uint16_t address)
    {
        auto it = std::lower_bound(
            cache_.begin(), cache_.end(), address,
            [](const register_cache_entry_t &entry, uint16_t addr) {
                return entry.address < addr;
            });
        if (it != cache_.end() && it->address == address) {
            return &(*it);
        }
        return nullptr;
    }

    const register_cache_entry_t* find_cache_entry(uint16_t address) const
    {
        return const_cast<TinyBMSModel *>(this)->find_cache_entry(address);
    }

    void update_cache_and_publish(uint16_t address, uint16_t raw_value)
    {
        const register_descriptor_t *desc = tinybms_get_register_by_address(address);
        if (desc == nullptr) {
            return;
        }

        if (auto *entry = find_cache_entry(address); entry != nullptr) {
            entry->raw_value = raw_value;
            entry->valid = true;
            entry->last_update_ms = TimeUtils::now_ms();
        }

        if (bus_ == nullptr) {
            return;
        }

        tinybms_register_update_t update = {};
        update.address = address;
        update.raw_value = raw_value;
        update.user_value = tinybms_raw_to_user(desc, raw_value);
        copy_to_cstr(desc->key, update.key);

        event_t evt = {
            .type = EVENT_TINYBMS_REGISTER_UPDATED,
            .data = &update,
            .data_size = sizeof(update),
        };
        event_bus_publish(bus_, &evt);

        ESP_LOGD(TAG, "Register updated: %s = %.2f (0x%04X)",
                 desc->key, update.user_value, raw_value);
    }

    void publish_config_changed() const
    {
        if (bus_ == nullptr) {
            return;
        }
        event_t evt = {
            .type = EVENT_TINYBMS_CONFIG_CHANGED,
            .data = nullptr,
            .data_size = 0,
        };
        event_bus_publish(bus_, &evt);
    }

    event_bus_t *bus_ = nullptr;
    std::array<register_cache_entry_t, TINYBMS_REGISTER_COUNT> cache_{};
    uint32_t total_reads_ = 0;
    uint32_t total_writes_ = 0;
    uint32_t cache_hits_ = 0;
    bool initialized_ = false;
};

extern "C" void tinybms_model_user_write_request(event_bus_t *bus, const event_t *event, void *ctx)
{
    (void)bus;
    auto *self = static_cast<TinyBMSModel *>(ctx);
    if (self != nullptr) {
        self->handle_user_write_request(event);
    }
}

}  // namespace

extern "C" {

esp_err_t tinybms_model_init(event_bus_t *bus)
{
    return TinyBMSModel::instance().init(bus);
}

esp_err_t tinybms_model_read_all(void)
{
    return TinyBMSModel::instance().read_all();
}

esp_err_t tinybms_model_read_register(uint16_t address, float *user_value)
{
    return TinyBMSModel::instance().read_register(address, user_value);
}

esp_err_t tinybms_model_write_register(uint16_t address, float user_value)
{
    return TinyBMSModel::instance().write_register(address, user_value);
}

esp_err_t tinybms_model_get_cached(uint16_t address, float *user_value)
{
    return TinyBMSModel::instance().get_cached(address, user_value);
}

esp_err_t tinybms_model_get_config(tinybms_config_t *config)
{
    return TinyBMSModel::instance().get_config(config);
}

bool tinybms_model_is_cached(uint16_t address)
{
    return TinyBMSModel::instance().is_cached(address);
}

uint32_t tinybms_model_get_cache_age(uint16_t address)
{
    return TinyBMSModel::instance().cache_age(address);
}

void tinybms_model_invalidate_cache(void)
{
    TinyBMSModel::instance().invalidate_cache();
}

void tinybms_model_get_stats(uint32_t *total_reads, uint32_t *total_writes,
                             uint32_t *cache_hits)
{
    TinyBMSModel::instance().stats(total_reads, total_writes, cache_hits);
}

}  // extern "C"
