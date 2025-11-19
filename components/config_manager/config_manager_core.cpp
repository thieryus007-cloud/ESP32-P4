/**
 * @file config_manager_core.cpp
 * @brief Implementation of modern C++ configuration manager
 */

#include "config_manager_core.hpp"

extern "C" {
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
}

#include <algorithm>
#include <cctype>
#include <cstring>
#include <thread>

namespace config {

namespace {
const char* TAG = "cfg_mgr_cpp";

// Helper to check if string contains only printable ASCII
bool is_printable_ascii(std::string_view str)
{
    return std::all_of(str.begin(), str.end(), [](char c) {
        return std::isprint(static_cast<unsigned char>(c)) || c == '\0';
    });
}

// Helper to check if URL is well-formed (basic check)
bool has_valid_url_scheme(std::string_view url)
{
    if (url.empty()) {
        return true; // Empty URLs are allowed (optional config)
    }
    return url.starts_with("http://") || url.starts_with("https://") ||
           url.starts_with("mqtt://") || url.starts_with("mqtts://") ||
           url.starts_with("ws://") || url.starts_with("wss://");
}

} // anonymous namespace

// =============================================================================
// Validator Implementation
// =============================================================================

Validator::ValidationResult Validator::validate(const hmi_persistent_config_t& cfg)
{
    ValidationResult result{true, ""};

    // Validate alert thresholds
    if (!is_valid_float_range(cfg.alert_threshold_high,
                              constants::kAlertThresholdMin,
                              constants::kAlertThresholdMax)) {
        result.valid = false;
        result.error_message = "alert_threshold_high out of range";
        return result;
    }

    if (!is_valid_float_range(cfg.alert_threshold_low,
                              constants::kAlertThresholdMin,
                              constants::kAlertThresholdMax)) {
        result.valid = false;
        result.error_message = "alert_threshold_low out of range";
        return result;
    }

    if (cfg.alert_threshold_low >= cfg.alert_threshold_high) {
        result.valid = false;
        result.error_message = "alert_threshold_low must be < alert_threshold_high";
        return result;
    }

    // Validate log retention
    if (!is_valid_uint32_range(cfg.log_retention_days,
                               constants::kLogRetentionMinDays,
                               constants::kLogRetentionMaxDays)) {
        result.valid = false;
        result.error_message = "log_retention_days out of range";
        return result;
    }

    // Validate status publish period
    if (!is_valid_uint32_range(cfg.status_publish_period_ms,
                               constants::kStatusPublishMinMs,
                               constants::kStatusPublishMaxMs)) {
        result.valid = false;
        result.error_message = "status_publish_period_ms out of range";
        return result;
    }

    // Validate strings
    if (!is_safe_string(cfg.mqtt_broker, constants::kMqttBrokerSize)) {
        result.valid = false;
        result.error_message = "mqtt_broker contains invalid characters";
        return result;
    }

    if (!is_safe_string(cfg.mqtt_topic, constants::kMqttTopicSize)) {
        result.valid = false;
        result.error_message = "mqtt_topic contains invalid characters";
        return result;
    }

    if (!is_safe_string(cfg.http_endpoint, constants::kHttpEndpointSize)) {
        result.valid = false;
        result.error_message = "http_endpoint contains invalid characters";
        return result;
    }

    // Validate URLs
    if (!is_valid_url(cfg.mqtt_broker)) {
        result.valid = false;
        result.error_message = "mqtt_broker is not a valid URL";
        return result;
    }

    if (!is_valid_url(cfg.http_endpoint)) {
        result.valid = false;
        result.error_message = "http_endpoint is not a valid URL";
        return result;
    }

    return result;
}

bool Validator::is_valid_float_range(float value, float min, float max)
{
    return value >= min && value <= max && !std::isnan(value) && !std::isinf(value);
}

bool Validator::is_valid_uint32_range(uint32_t value, uint32_t min, uint32_t max)
{
    return value >= min && value <= max;
}

bool Validator::is_valid_url(std::string_view url)
{
    if (!is_printable_ascii(url)) {
        return false;
    }
    return has_valid_url_scheme(url);
}

bool Validator::is_safe_string(const char* str, size_t max_len)
{
    if (str == nullptr) {
        return false;
    }

    const size_t len = strnlen(str, max_len);
    if (len >= max_len) {
        return false; // Not null-terminated
    }

    return is_printable_ascii(std::string_view(str, len));
}

// =============================================================================
// Observer Manager Implementation
// =============================================================================

void ObserverManager::add_observer(std::shared_ptr<ConfigObserver> observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(std::move(observer));
}

void ObserverManager::add_callback(ConfigObserverCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(callback));
}

void ObserverManager::remove_observer(std::shared_ptr<ConfigObserver> observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end());
}

void ObserverManager::notify_all(const hmi_persistent_config_t& config)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& observer : observers_) {
        if (observer) {
            observer->on_config_changed(config);
        }
    }

    for (const auto& callback : callbacks_) {
        if (callback) {
            callback(config);
        }
    }
}

// =============================================================================
// NVS Persister Implementation
// =============================================================================

esp_err_t NvsPersister::save(const hmi_persistent_config_t& cfg)
{
    return retry_operation([this, &cfg]() { return save_impl(cfg); });
}

esp_err_t NvsPersister::load(hmi_persistent_config_t& cfg)
{
    return retry_operation([this, &cfg]() { return load_impl(cfg); });
}

esp_err_t NvsPersister::save_impl(const hmi_persistent_config_t& cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(constants::kNvsNamespace.data(), NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for save: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, constants::kNvsKey.data(), &cfg, sizeof(cfg));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err == ESP_OK) {
        save_count_.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGI(TAG, "Configuration saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t NvsPersister::load_impl(hmi_persistent_config_t& cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(constants::kNvsNamespace.data(), NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing config in NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t size = sizeof(cfg);
    err = nvs_get_blob(handle, constants::kNvsKey.data(), &cfg, &size);
    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(cfg)) {
        ESP_LOGW(TAG, "Invalid stored config (err=%s, size=%zu)",
                 esp_err_to_name(err), size);
        return ESP_FAIL;
    }

    load_count_.fetch_add(1, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Configuration loaded from NVS");
    return ESP_OK;
}

esp_err_t NvsPersister::retry_operation(std::function<esp_err_t()> operation)
{
    esp_err_t err = ESP_FAIL;

    for (uint32_t attempt = 0; attempt < constants::kNvsMaxRetries; ++attempt) {
        err = operation();

        if (err == ESP_OK) {
            return ESP_OK;
        }

        // Only retry on transient errors
        if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE ||
            err == ESP_ERR_NVS_PAGE_FULL) {
            // Don't retry these, they require intervention
            break;
        }

        if (attempt < constants::kNvsMaxRetries - 1) {
            retry_count_.fetch_add(1, std::memory_order_relaxed);
            const uint32_t delay_ms = constants::kNvsRetryDelayMs * (1 << attempt);
            ESP_LOGW(TAG, "NVS operation failed (attempt %u/%u), retrying in %ums",
                     attempt + 1, constants::kNvsMaxRetries, delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    return err;
}

// =============================================================================
// Config Manager Implementation
// =============================================================================

ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

esp_err_t ConfigManager::init()
{
    if (initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Configuration Manager (C++)");

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Apply defaults
    apply_defaults(config_);

    // Try to load from NVS
    hmi_persistent_config_t loaded_config;
    esp_err_t err = load_impl(loaded_config);

    if (err == ESP_OK) {
        // Validate loaded config
        auto validation = Validator::validate(loaded_config);
        if (validation.valid) {
            config_ = loaded_config;
            ESP_LOGI(TAG, "Loaded and validated configuration from NVS");
        } else {
            ESP_LOGW(TAG, "Loaded config failed validation: %s",
                     validation.error_message.c_str());
            validation_failures_.fetch_add(1, std::memory_order_relaxed);
            ESP_LOGI(TAG, "Using default configuration");
            err = save_impl(config_); // Save defaults
        }
    } else {
        ESP_LOGI(TAG, "No valid config in NVS, using defaults");
        err = save_impl(config_); // Save defaults
    }

    initialized_.store(true, std::memory_order_release);

    ESP_LOGI(TAG, "Configuration Manager initialized");
    ESP_LOGI(TAG, "  MQTT broker: %s", config_.mqtt_broker);
    ESP_LOGI(TAG, "  MQTT topic: %s", config_.mqtt_topic);
    ESP_LOGI(TAG, "  HTTP endpoint: %s", config_.http_endpoint);
    ESP_LOGI(TAG, "  Alert thresholds: %.1f / %.1f",
             config_.alert_threshold_low, config_.alert_threshold_high);

    return err;
}

void ConfigManager::apply_defaults(hmi_persistent_config_t& cfg)
{
    std::memset(&cfg, 0, sizeof(cfg));

    cfg.alert_threshold_high = CONFIG_HMI_ALERT_THRESHOLD_HIGH;
    cfg.alert_threshold_low = CONFIG_HMI_ALERT_THRESHOLD_LOW;
    cfg.log_retention_days = CONFIG_HMI_LOG_RETENTION_DAYS;
    cfg.status_publish_period_ms = CONFIG_HMI_STATUS_PUBLISH_PERIOD_MS;

    strlcpy(cfg.mqtt_broker, CONFIG_HMI_MQTT_BROKER_URI, sizeof(cfg.mqtt_broker));
    strlcpy(cfg.mqtt_topic, CONFIG_HMI_MQTT_TOPIC, sizeof(cfg.mqtt_topic));
    strlcpy(cfg.http_endpoint, CONFIG_HMI_HTTP_ENDPOINT, sizeof(cfg.http_endpoint));
}

hmi_persistent_config_t ConfigManager::get() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        ESP_LOGW(TAG, "Failed to acquire mutex for get()");
        hmi_persistent_config_t empty{};
        return empty;
    }
    return config_;
}

esp_err_t ConfigManager::set(const hmi_persistent_config_t& cfg, bool persist)
{
    // Validate first (outside mutex)
    auto validation = Validator::validate(cfg);
    if (!validation.valid) {
        ESP_LOGE(TAG, "Configuration validation failed: %s",
                 validation.error_message.c_str());
        validation_failures_.fetch_add(1, std::memory_order_relaxed);
        return ESP_ERR_INVALID_ARG;
    }

    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        ESP_LOGW(TAG, "Failed to acquire mutex for set()");
        return ESP_ERR_TIMEOUT;
    }

    // Check if actually changed (dirty tracking)
    const bool changed = std::memcmp(&config_, &cfg, sizeof(cfg)) != 0;
    if (!changed) {
        ESP_LOGD(TAG, "Configuration unchanged, skipping update");
        return ESP_OK;
    }

    // Save old config for rollback
    const hmi_persistent_config_t old_config = config_;

    // Update in-memory config
    config_ = cfg;
    dirty_.store(true, std::memory_order_release);

    // Persist if requested
    esp_err_t err = ESP_OK;
    if (persist) {
        err = save_impl(cfg);
        if (err != ESP_OK) {
            // Rollback on failure
            ESP_LOGW(TAG, "Failed to persist config, rolling back");
            config_ = old_config;
            dirty_.store(false, std::memory_order_release);
            return err;
        }
        dirty_.store(false, std::memory_order_release);
    }

    // Notify observers (outside mutex to avoid deadlock)
    lock.~ScopedMutex(); // Explicitly release mutex
    notify_observers(cfg);

    return err;
}

esp_err_t ConfigManager::save_impl(const hmi_persistent_config_t& cfg)
{
    return persister_.save(cfg);
}

esp_err_t ConfigManager::load_impl(hmi_persistent_config_t& cfg)
{
    return persister_.load(cfg);
}

void ConfigManager::notify_observers(const hmi_persistent_config_t& cfg)
{
    observer_manager_.notify_all(cfg);
}

// Typed accessors
std::optional<float> ConfigManager::get_alert_threshold_high() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return config_.alert_threshold_high;
}

std::optional<float> ConfigManager::get_alert_threshold_low() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return config_.alert_threshold_low;
}

std::optional<std::string> ConfigManager::get_mqtt_broker() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return std::string(config_.mqtt_broker);
}

std::optional<std::string> ConfigManager::get_mqtt_topic() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return std::string(config_.mqtt_topic);
}

std::optional<std::string> ConfigManager::get_http_endpoint() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return std::string(config_.http_endpoint);
}

std::optional<uint32_t> ConfigManager::get_log_retention_days() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return config_.log_retention_days;
}

std::optional<uint32_t> ConfigManager::get_status_publish_period_ms() const
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) return std::nullopt;
    return config_.status_publish_period_ms;
}

esp_err_t ConfigManager::set_alert_threshold_high(float value)
{
    hmi_persistent_config_t cfg = get();
    cfg.alert_threshold_high = value;
    return set(cfg);
}

esp_err_t ConfigManager::set_alert_threshold_low(float value)
{
    hmi_persistent_config_t cfg = get();
    cfg.alert_threshold_low = value;
    return set(cfg);
}

esp_err_t ConfigManager::set_mqtt_broker(std::string_view value)
{
    hmi_persistent_config_t cfg = get();
    strlcpy(cfg.mqtt_broker, value.data(), sizeof(cfg.mqtt_broker));
    return set(cfg);
}

esp_err_t ConfigManager::set_mqtt_topic(std::string_view value)
{
    hmi_persistent_config_t cfg = get();
    strlcpy(cfg.mqtt_topic, value.data(), sizeof(cfg.mqtt_topic));
    return set(cfg);
}

esp_err_t ConfigManager::set_http_endpoint(std::string_view value)
{
    hmi_persistent_config_t cfg = get();
    strlcpy(cfg.http_endpoint, value.data(), sizeof(cfg.http_endpoint));
    return set(cfg);
}

esp_err_t ConfigManager::set_log_retention_days(uint32_t value)
{
    hmi_persistent_config_t cfg = get();
    cfg.log_retention_days = value;
    return set(cfg);
}

esp_err_t ConfigManager::set_status_publish_period_ms(uint32_t value)
{
    hmi_persistent_config_t cfg = get();
    cfg.status_publish_period_ms = value;
    return set(cfg);
}

void ConfigManager::add_observer(std::shared_ptr<ConfigObserver> observer)
{
    observer_manager_.add_observer(std::move(observer));
}

void ConfigManager::add_callback(ConfigObserverCallback callback)
{
    observer_manager_.add_callback(std::move(callback));
}

void ConfigManager::remove_observer(std::shared_ptr<ConfigObserver> observer)
{
    observer_manager_.remove_observer(std::move(observer));
}

ConfigManager::Stats ConfigManager::get_stats() const
{
    return {
        .saves = persister_.save_count(),
        .loads = persister_.load_count(),
        .retries = persister_.retry_count(),
        .validation_failures = validation_failures_.load(std::memory_order_relaxed),
        .dirty = dirty_.load(std::memory_order_acquire)
    };
}

} // namespace config

// =============================================================================
// C API Wrappers (backward compatibility)
// =============================================================================

extern "C" {

esp_err_t config_manager_init(void)
{
    return config::ConfigManager::instance().init();
}

esp_err_t config_manager_save(const hmi_persistent_config_t* cfg)
{
    if (cfg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return config::ConfigManager::instance().set(*cfg, true);
}

const hmi_persistent_config_t* config_manager_get(void)
{
    static thread_local hmi_persistent_config_t cached_config;
    cached_config = config::ConfigManager::instance().get();
    return &cached_config;
}

} // extern "C"
