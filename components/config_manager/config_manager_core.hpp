/**
 * @file config_manager_core.hpp
 * @brief Modern C++ configuration manager with thread-safety and validation
 *
 * Key improvements over C implementation:
 * - Thread-safe access with RAII mutex
 * - Configuration validation (ranges, strings)
 * - Observer pattern for change notifications
 * - Versioning with automatic migration
 * - Atomic save with rollback on failure
 * - Dirty tracking to avoid unnecessary NVS writes
 * - Retry logic for transient NVS errors
 * - Typed accessors with validation
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "config_manager.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
}

namespace config {

// =============================================================================
// Configuration Constants
// =============================================================================

namespace constants {
// String field sizes (matching C struct)
constexpr size_t kMqttBrokerSize = 96;
constexpr size_t kMqttTopicSize = 96;
constexpr size_t kHttpEndpointSize = 96;

// Validation ranges
constexpr float kAlertThresholdMin = -50.0f;
constexpr float kAlertThresholdMax = 100.0f;
constexpr uint32_t kLogRetentionMinDays = 1;
constexpr uint32_t kLogRetentionMaxDays = 365;
constexpr uint32_t kStatusPublishMinMs = 100;
constexpr uint32_t kStatusPublishMaxMs = 60000;

// NVS configuration
constexpr std::string_view kNvsNamespace = "hmi_cfg";
constexpr std::string_view kNvsKey = "persist_v1";
constexpr uint32_t kConfigVersion = 1;

// Retry configuration
constexpr uint32_t kNvsMaxRetries = 3;
constexpr uint32_t kNvsRetryDelayMs = 100;
} // namespace constants

// =============================================================================
// RAII Mutex Guard
// =============================================================================

class ScopedMutex {
public:
    explicit ScopedMutex(SemaphoreHandle_t mutex, TickType_t timeout = pdMS_TO_TICKS(100))
        : mutex_(mutex), locked_(false)
    {
        if (mutex_ != nullptr) {
            locked_ = xSemaphoreTake(mutex_, timeout) == pdTRUE;
        }
    }

    ~ScopedMutex()
    {
        if (locked_ && mutex_ != nullptr) {
            xSemaphoreGive(mutex_);
        }
    }

    ScopedMutex(const ScopedMutex&) = delete;
    ScopedMutex& operator=(const ScopedMutex&) = delete;

    [[nodiscard]] bool is_locked() const { return locked_; }

private:
    SemaphoreHandle_t mutex_;
    bool locked_;
};

// =============================================================================
// Configuration Validator
// =============================================================================

/**
 * @brief Validates configuration fields
 */
class Validator {
public:
    struct ValidationResult {
        bool valid;
        std::string error_message;

        explicit operator bool() const { return valid; }
    };

    static ValidationResult validate(const hmi_persistent_config_t& cfg);

private:
    static bool is_valid_float_range(float value, float min, float max);
    static bool is_valid_uint32_range(uint32_t value, uint32_t min, uint32_t max);
    static bool is_valid_url(std::string_view url);
    static bool is_safe_string(const char* str, size_t max_len);
};

// =============================================================================
// Configuration Observer Pattern
// =============================================================================

/**
 * @brief Observer interface for configuration changes
 */
class ConfigObserver {
public:
    virtual ~ConfigObserver() = default;
    virtual void on_config_changed(const hmi_persistent_config_t& new_config) = 0;
};

using ConfigObserverCallback = std::function<void(const hmi_persistent_config_t&)>;

/**
 * @brief Manages configuration change observers
 */
class ObserverManager {
public:
    ObserverManager() = default;

    void add_observer(std::shared_ptr<ConfigObserver> observer);
    void add_callback(ConfigObserverCallback callback);
    void remove_observer(std::shared_ptr<ConfigObserver> observer);
    void notify_all(const hmi_persistent_config_t& config);

private:
    std::vector<std::shared_ptr<ConfigObserver>> observers_;
    std::vector<ConfigObserverCallback> callbacks_;
    std::mutex mutex_;
};

// =============================================================================
// NVS Persister with Retry
// =============================================================================

/**
 * @brief Handles NVS operations with retry logic
 */
class NvsPersister {
public:
    NvsPersister() = default;

    esp_err_t save(const hmi_persistent_config_t& cfg);
    esp_err_t load(hmi_persistent_config_t& cfg);

    uint32_t save_count() const { return save_count_.load(std::memory_order_relaxed); }
    uint32_t load_count() const { return load_count_.load(std::memory_order_relaxed); }
    uint32_t retry_count() const { return retry_count_.load(std::memory_order_relaxed); }

private:
    esp_err_t save_impl(const hmi_persistent_config_t& cfg);
    esp_err_t load_impl(hmi_persistent_config_t& cfg);
    esp_err_t retry_operation(std::function<esp_err_t()> operation);

    std::atomic<uint32_t> save_count_{0};
    std::atomic<uint32_t> load_count_{0};
    std::atomic<uint32_t> retry_count_{0};
};

// =============================================================================
// Configuration Manager (Singleton)
// =============================================================================

/**
 * @brief Thread-safe configuration manager with advanced features
 */
class ConfigManager {
public:
    static ConfigManager& instance();

    // Lifecycle
    esp_err_t init();

    // Configuration access (thread-safe)
    hmi_persistent_config_t get() const;
    esp_err_t set(const hmi_persistent_config_t& cfg, bool persist = true);

    // Typed accessors with validation
    std::optional<float> get_alert_threshold_high() const;
    std::optional<float> get_alert_threshold_low() const;
    std::optional<std::string> get_mqtt_broker() const;
    std::optional<std::string> get_mqtt_topic() const;
    std::optional<std::string> get_http_endpoint() const;
    std::optional<uint32_t> get_log_retention_days() const;
    std::optional<uint32_t> get_status_publish_period_ms() const;

    esp_err_t set_alert_threshold_high(float value);
    esp_err_t set_alert_threshold_low(float value);
    esp_err_t set_mqtt_broker(std::string_view value);
    esp_err_t set_mqtt_topic(std::string_view value);
    esp_err_t set_http_endpoint(std::string_view value);
    esp_err_t set_log_retention_days(uint32_t value);
    esp_err_t set_status_publish_period_ms(uint32_t value);

    // Observer management
    void add_observer(std::shared_ptr<ConfigObserver> observer);
    void add_callback(ConfigObserverCallback callback);
    void remove_observer(std::shared_ptr<ConfigObserver> observer);

    // Statistics
    struct Stats {
        uint32_t saves;
        uint32_t loads;
        uint32_t retries;
        uint32_t validation_failures;
        bool dirty;
    };
    Stats get_stats() const;

    // Dirty tracking
    bool is_dirty() const { return dirty_.load(std::memory_order_acquire); }
    void mark_clean() { dirty_.store(false, std::memory_order_release); }

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Internal helpers
    void apply_defaults(hmi_persistent_config_t& cfg);
    esp_err_t save_impl(const hmi_persistent_config_t& cfg);
    esp_err_t load_impl(hmi_persistent_config_t& cfg);
    void notify_observers(const hmi_persistent_config_t& cfg);

    // State
    mutable SemaphoreHandle_t mutex_{nullptr};
    hmi_persistent_config_t config_{};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> dirty_{false};
    std::atomic<uint32_t> validation_failures_{0};

    // Components
    NvsPersister persister_;
    ObserverManager observer_manager_;
};

} // namespace config
