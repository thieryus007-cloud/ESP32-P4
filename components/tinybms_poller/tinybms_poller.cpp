/**
 * @file tinybms_poller.cpp
 * @brief TinyBMS Periodic Poller Implementation
 */

extern "C" {
#include "tinybms_poller.h"
#include "tinybms_model.h"
#include "tinybms_registers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
}

#include <array>
#include <atomic>

namespace {

static const char *TAG = "tinybms_poller";

// Live data registers to poll (based on web interface strategy)
// These are the essential real-time telemetry registers
constexpr std::array<uint16_t, 29> LIVE_DATA_REGISTERS = {{
    // Cell voltages (0-15)
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    // Pack measurements
    36,  // pack_voltage_v
    38,  // pack_current_a
    40,  // min_cell_voltage_mv
    41,  // max_cell_voltage_mv
    // Temperatures
    42,  // ext_temp_sensor_1
    43,  // ext_temp_sensor_2
    48,  // internal_temperature
    // State
    45,  // state_of_health
    46,  // state_of_charge (UINT32, 2 registers)
    50,  // online_status (BMS state)
    52,  // real_balancing (balancing bits)
}};

// Configuration registers to poll (less frequently)
constexpr std::array<uint16_t, 34> CONFIG_REGISTERS = {{
    300, 301, 303, 304, 305, 306, 307, 308, 310, 311,
    312, 315, 316, 317, 318, 319, 320, 321, 322, 323,
    328, 329, 330, 331, 332, 333, 334, 335, 337, 338,
    339, 340, 342, 343
}};

class TinyBMSPoller {
public:
    static TinyBMSPoller &instance()
    {
        static TinyBMSPoller inst;
        return inst;
    }

    esp_err_t init(event_bus_t *bus, const tinybms_poller_config_t *config)
    {
        if (initialized_) {
            ESP_LOGW(TAG, "Already initialized");
            return ESP_OK;
        }

        if (!bus) {
            ESP_LOGE(TAG, "EventBus is NULL");
            return ESP_ERR_INVALID_ARG;
        }

        bus_ = bus;

        if (config) {
            config_ = *config;
        } else {
            config_ = tinybms_poller_get_default_config();
        }

        mutex_ = xSemaphoreCreateMutex();
        if (!mutex_) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }

        total_polls_ = 0;
        successful_reads_ = 0;
        failed_reads_ = 0;

        initialized_ = true;
        ESP_LOGI(TAG, "TinyBMS poller initialized (live_period=%lums, config_period=%lums)",
                 config_.live_data_period_ms, config_.config_data_period_ms);
        return ESP_OK;
    }

    esp_err_t start()
    {
        if (!initialized_) {
            ESP_LOGE(TAG, "Not initialized");
            return ESP_ERR_INVALID_STATE;
        }

        if (task_running_) {
            ESP_LOGW(TAG, "Already running");
            return ESP_OK;
        }

        stop_requested_ = false;

        BaseType_t rc = xTaskCreate(task_trampoline, "tinybms_poller", 4096, this, 5, &task_handle_);
        if (rc != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task");
            return ESP_FAIL;
        }

        task_running_ = true;
        ESP_LOGI(TAG, "TinyBMS poller started");
        return ESP_OK;
    }

    esp_err_t stop()
    {
        if (!task_running_) {
            return ESP_OK;
        }

        stop_requested_ = true;

        // Wait for task to finish
        for (int i = 0; i < 50 && task_running_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (task_handle_) {
            vTaskDelete(task_handle_);
            task_handle_ = nullptr;
        }

        task_running_ = false;
        ESP_LOGI(TAG, "TinyBMS poller stopped");
        return ESP_OK;
    }

    esp_err_t trigger_now()
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        trigger_immediate_ = true;
        return ESP_OK;
    }

    esp_err_t set_config(const tinybms_poller_config_t *config)
    {
        if (!config) {
            return ESP_ERR_INVALID_ARG;
        }

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            config_ = *config;
            xSemaphoreGive(mutex_);
            ESP_LOGI(TAG, "Config updated (live_period=%lums, config_period=%lums)",
                     config_.live_data_period_ms, config_.config_data_period_ms);
            return ESP_OK;
        }

        return ESP_ERR_TIMEOUT;
    }

    esp_err_t get_config(tinybms_poller_config_t *config)
    {
        if (!config) {
            return ESP_ERR_INVALID_ARG;
        }

        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            *config = config_;
            xSemaphoreGive(mutex_);
            return ESP_OK;
        }

        return ESP_ERR_TIMEOUT;
    }

    esp_err_t get_stats(uint32_t *total_polls, uint32_t *successful_reads, uint32_t *failed_reads)
    {
        if (total_polls) {
            *total_polls = total_polls_.load();
        }
        if (successful_reads) {
            *successful_reads = successful_reads_.load();
        }
        if (failed_reads) {
            *failed_reads = failed_reads_.load();
        }
        return ESP_OK;
    }

    void reset_stats()
    {
        total_polls_ = 0;
        successful_reads_ = 0;
        failed_reads_ = 0;
        ESP_LOGI(TAG, "Statistics reset");
    }

private:
    static void task_trampoline(void *arg)
    {
        auto *self = static_cast<TinyBMSPoller *>(arg);
        self->task_loop();
    }

    void task_loop()
    {
        uint32_t last_live_poll_ms = 0;
        uint32_t last_config_poll_ms = 0;

        // Initial delay to let system stabilize
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Do initial configuration read
        if (config_.enable_config_data) {
            ESP_LOGI(TAG, "Initial configuration read");
            poll_config_registers();
        }

        while (!stop_requested_) {
            uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            bool did_poll = false;

            // Check for immediate trigger
            if (trigger_immediate_) {
                trigger_immediate_ = false;
                ESP_LOGI(TAG, "Immediate poll triggered");
                if (config_.enable_live_data) {
                    poll_live_registers();
                }
                if (config_.enable_config_data) {
                    poll_config_registers();
                }
                did_poll = true;
                total_polls_++;
                last_live_poll_ms = now_ms;
                last_config_poll_ms = now_ms;
            }

            // Live data polling
            if (config_.enable_live_data &&
                (now_ms - last_live_poll_ms >= config_.live_data_period_ms)) {
                poll_live_registers();
                did_poll = true;
                last_live_poll_ms = now_ms;
            }

            // Config data polling (less frequent)
            if (config_.enable_config_data &&
                (now_ms - last_config_poll_ms >= config_.config_data_period_ms)) {
                poll_config_registers();
                last_config_poll_ms = now_ms;
            }

            if (did_poll) {
                total_polls_++;
            }

            // Sleep for a reasonable interval
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        task_running_ = false;
        vTaskDelete(nullptr);
    }

    void poll_live_registers()
    {
        uint32_t success_count = 0;
        uint32_t fail_count = 0;

        for (uint16_t reg_addr : LIVE_DATA_REGISTERS) {
            if (stop_requested_) {
                break;
            }

            float user_value = 0.0f;
            esp_err_t ret = tinybms_model_read_register(reg_addr, &user_value);

            if (ret == ESP_OK) {
                success_count++;
                successful_reads_++;
            } else {
                fail_count++;
                failed_reads_++;
                ESP_LOGD(TAG, "Failed to read live register 0x%04X: %s",
                         reg_addr, esp_err_to_name(ret));
            }

            // Delay between reads to avoid overwhelming the BMS
            // (same strategy as web interface: 50ms between reads)
            if (config_.inter_register_delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(config_.inter_register_delay_ms));
            }
        }

        ESP_LOGD(TAG, "Live poll complete: %lu success, %lu failed",
                 static_cast<unsigned long>(success_count),
                 static_cast<unsigned long>(fail_count));
    }

    void poll_config_registers()
    {
        uint32_t success_count = 0;
        uint32_t fail_count = 0;

        for (uint16_t reg_addr : CONFIG_REGISTERS) {
            if (stop_requested_) {
                break;
            }

            float user_value = 0.0f;
            esp_err_t ret = tinybms_model_read_register(reg_addr, &user_value);

            if (ret == ESP_OK) {
                success_count++;
                successful_reads_++;
            } else {
                fail_count++;
                failed_reads_++;
                ESP_LOGW(TAG, "Failed to read config register 0x%04X: %s",
                         reg_addr, esp_err_to_name(ret));
            }

            // Delay between reads
            if (config_.inter_register_delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(config_.inter_register_delay_ms));
            }
        }

        ESP_LOGI(TAG, "Config poll complete: %lu success, %lu failed",
                 static_cast<unsigned long>(success_count),
                 static_cast<unsigned long>(fail_count));
    }

    event_bus_t *bus_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    tinybms_poller_config_t config_ = {};

    std::atomic<bool> initialized_{false};
    std::atomic<bool> task_running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> trigger_immediate_{false};

    std::atomic<uint32_t> total_polls_{0};
    std::atomic<uint32_t> successful_reads_{0};
    std::atomic<uint32_t> failed_reads_{0};
};

} // namespace

extern "C" {

tinybms_poller_config_t tinybms_poller_get_default_config(void)
{
    tinybms_poller_config_t config = {
        .live_data_period_ms = 2000,      // Poll live data every 2 seconds
        .config_data_period_ms = 30000,   // Poll config data every 30 seconds
        .inter_register_delay_ms = 50,    // 50ms delay between reads (same as web interface)
        .enable_live_data = true,         // Enable live data polling by default
        .enable_config_data = false,      // Disable config polling by default (only on demand)
    };
    return config;
}

esp_err_t tinybms_poller_init(event_bus_t *bus, const tinybms_poller_config_t *config)
{
    return TinyBMSPoller::instance().init(bus, config);
}

esp_err_t tinybms_poller_start(void)
{
    return TinyBMSPoller::instance().start();
}

esp_err_t tinybms_poller_stop(void)
{
    return TinyBMSPoller::instance().stop();
}

esp_err_t tinybms_poller_trigger_now(void)
{
    return TinyBMSPoller::instance().trigger_now();
}

esp_err_t tinybms_poller_set_config(const tinybms_poller_config_t *config)
{
    return TinyBMSPoller::instance().set_config(config);
}

esp_err_t tinybms_poller_get_config(tinybms_poller_config_t *config)
{
    return TinyBMSPoller::instance().get_config(config);
}

esp_err_t tinybms_poller_get_stats(uint32_t *total_polls, uint32_t *successful_reads,
                                    uint32_t *failed_reads)
{
    return TinyBMSPoller::instance().get_stats(total_polls, successful_reads, failed_reads);
}

void tinybms_poller_reset_stats(void)
{
    TinyBMSPoller::instance().reset_stats();
}

} // extern "C"
