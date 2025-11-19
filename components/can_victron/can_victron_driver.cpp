/**
 * @file can_victron_driver.cpp
 * @brief Implementation of modern C++ CAN Victron driver
 */

#include "can_victron_driver.hpp"

extern "C" {
#include "esp_log.h"
#include "esp_timer.h"
#include "event_types.h"
}

#include <algorithm>
#include <cstring>

namespace can {
namespace victron {

namespace {
const char* TAG = "can_victron_cpp";

uint64_t timestamp_ms()
{
#ifdef ESP_PLATFORM
    return esp_timer_get_time() / 1000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
           static_cast<uint64_t>(tv.tv_usec / 1000ULL);
#endif
}

} // anonymous namespace

// =============================================================================
// Statistics Implementation
// =============================================================================

void Statistics::record_tx_frame(size_t dlc, uint64_t timestamp)
{
    (void)timestamp; // Reserved for future use (latency tracking)
    const uint32_t payload_bytes = std::min<size_t>(dlc, 8);
    tx_frame_count_.fetch_add(1, std::memory_order_relaxed);
    tx_byte_count_.fetch_add(payload_bytes, std::memory_order_relaxed);
}

void Statistics::record_rx_frame(size_t dlc, uint64_t timestamp)
{
    (void)timestamp;
    const uint32_t payload_bytes = std::min<size_t>(dlc, 8);
    rx_frame_count_.fetch_add(1, std::memory_order_relaxed);
    rx_byte_count_.fetch_add(payload_bytes, std::memory_order_relaxed);
}

void Statistics::reset()
{
    tx_frame_count_.store(0, std::memory_order_relaxed);
    rx_frame_count_.store(0, std::memory_order_relaxed);
    tx_byte_count_.store(0, std::memory_order_relaxed);
    rx_byte_count_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Keepalive Manager Implementation
// =============================================================================

void KeepaliveManager::on_rx_keepalive(uint64_t timestamp_ms)
{
    last_rx_ms_.store(timestamp_ms, std::memory_order_release);
    const bool was_not_ok = !ok_.exchange(true, std::memory_order_acq_rel);
    if (was_not_ok) {
        ESP_LOGI(TAG, "Victron keepalive detected");
    }
}

void KeepaliveManager::on_tx_keepalive(uint64_t timestamp_ms)
{
    last_tx_ms_.store(timestamp_ms, std::memory_order_release);
    ok_.store(true, std::memory_order_release);
}

bool KeepaliveManager::should_send_keepalive(uint64_t now_ms) const
{
    const uint64_t last_tx = last_tx_ms_.load(std::memory_order_acquire);
    const bool is_ok = ok_.load(std::memory_order_acquire);

    uint32_t interval = config::kKeepaliveIntervalMs;
    if (!is_ok && config::kKeepaliveRetryMs < interval) {
        interval = config::kKeepaliveRetryMs;
    }

    return (now_ms - last_tx) >= interval;
}

bool KeepaliveManager::is_timeout(uint64_t now_ms) const
{
    const uint64_t last_rx = last_rx_ms_.load(std::memory_order_acquire);
    const bool is_ok = ok_.load(std::memory_order_acquire);
    return is_ok && ((now_ms - last_rx) > config::kKeepaliveTimeoutMs);
}

void KeepaliveManager::reset(uint64_t now_ms)
{
    ok_.store(false, std::memory_order_release);
    last_rx_ms_.store(now_ms, std::memory_order_release);

    // Initialize last_tx to trigger immediate keepalive send
    const uint64_t init_tx = (now_ms >= config::kKeepaliveIntervalMs)
                                ? (now_ms - config::kKeepaliveIntervalMs)
                                : 0;
    last_tx_ms_.store(init_tx, std::memory_order_release);
}

// =============================================================================
// Driver Implementation
// =============================================================================

Driver& Driver::instance()
{
    static Driver instance;
    return instance;
}

esp_err_t Driver::init()
{
    if (driver_started_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing CAN Victron driver (C++)");

#ifdef ESP_PLATFORM
    // Create mutexes
    if (twai_mutex_ == nullptr) {
        twai_mutex_ = xSemaphoreCreateMutex();
        if (twai_mutex_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create TWAI mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (driver_state_mutex_ == nullptr) {
        driver_state_mutex_ = xSemaphoreCreateMutex();
        if (driver_state_mutex_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create driver state mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (stats_mutex_ == nullptr) {
        stats_mutex_ = xSemaphoreCreateMutex();
        if (stats_mutex_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create stats mutex");
            return ESP_ERR_NO_MEM;
        }
    }
#endif

    stats_.reset();

    esp_err_t err = start_driver();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start driver: %s", esp_err_to_name(err));
        return err;
    }

    // Create task
    BaseType_t rc = xTaskCreate(task_entry,
                                "can_victron_cpp",
                                config::kTaskStackSize,
                                this,
                                config::kTaskPriority,
                                &task_handle_);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN task");
        stop_driver();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "CAN Victron driver initialized (TX=%d, RX=%d)",
             config::kTxGpio, config::kRxGpio);

    return ESP_OK;
}

void Driver::deinit()
{
    ESP_LOGI(TAG, "Deinitializing CAN Victron driver");

    // Signal task to exit
    task_should_exit_.store(true, std::memory_order_release);

    // Give task time to exit
    vTaskDelay(pdMS_TO_TICKS(200));

    // Stop driver
    stop_driver();

#ifdef ESP_PLATFORM
    // Delete mutexes
    if (twai_mutex_ != nullptr) {
        vSemaphoreDelete(twai_mutex_);
        twai_mutex_ = nullptr;
    }
    if (driver_state_mutex_ != nullptr) {
        vSemaphoreDelete(driver_state_mutex_);
        driver_state_mutex_ = nullptr;
    }
    if (stats_mutex_ != nullptr) {
        vSemaphoreDelete(stats_mutex_);
        stats_mutex_ = nullptr;
    }
#endif

    task_handle_ = nullptr;
    task_should_exit_.store(false, std::memory_order_release);

    ESP_LOGI(TAG, "CAN Victron driver deinitialized");
}

esp_err_t Driver::start_driver()
{
#ifdef ESP_PLATFORM
    if (driver_started_.load(std::memory_order_acquire)) {
        return ESP_OK;
    }

    // Configure TWAI
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(config::kTxGpio, config::kRxGpio, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = config::kTxQueueLen;
    g_config.rx_queue_len = config::kRxQueueLen;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        return err;
    }

    err = twai_start();
    if (err != ESP_OK) {
        twai_driver_uninstall();
        return err;
    }

    driver_started_.store(true, std::memory_order_release);

    // Initialize keepalive
    const uint64_t now = timestamp_ms();
    keepalive_.reset(now);

    ESP_LOGI(TAG, "TWAI driver started successfully");
#endif
    return ESP_OK;
}

void Driver::stop_driver()
{
#ifdef ESP_PLATFORM
    if (!driver_started_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    ScopedMutex lock(twai_mutex_);
    if (!lock.is_locked()) {
        ESP_LOGW(TAG, "Failed to acquire mutex for driver stop");
    }

    twai_stop();
    twai_driver_uninstall();

    ESP_LOGI(TAG, "TWAI driver stopped");
#endif
}

esp_err_t Driver::publish_frame(uint32_t can_id,
                               const uint8_t* data,
                               size_t length,
                               std::string_view description)
{
    if (can_id > 0x7FFU) {
        ESP_LOGE(TAG, "Invalid CAN ID 0x%08" PRIX32 " (standard IDs only)", can_id);
        return ESP_ERR_INVALID_ARG;
    }

    if (length > 8U) {
        length = 8U;
    }

    if (length > 0 && data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    if (!driver_started_.load(std::memory_order_acquire)) {
        return ESP_ERR_INVALID_STATE;
    }

    twai_message_t message = {
        .identifier = can_id,
        .flags = 0,
        .data_length_code = static_cast<uint8_t>(length),
    };

    if (length > 0) {
        std::memcpy(message.data, data, length);
    }

    // Use RAII mutex guard
    ScopedMutex lock(twai_mutex_, pdMS_TO_TICKS(config::kLockTimeoutMs));
    if (!lock.is_locked()) {
        ESP_LOGW(TAG, "Timed out acquiring TX mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(config::kTxTimeoutMs));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to transmit CAN frame 0x%08" PRIX32 ": %s",
                 can_id, esp_err_to_name(err));
        return err;
    }

    // Record statistics
    const uint64_t ts = timestamp_ms();
    stats_.record_tx_frame(length, ts);

    ESP_LOGV(TAG, "CAN 0x%03" PRIX32 " TX: %.*s",
             can_id, static_cast<int>(description.size()), description.data());
#endif

    return ESP_OK;
}

void Driver::send_keepalive(uint64_t now_ms)
{
    std::array<uint8_t, config::kKeepaliveDLC> payload = {0};
    const esp_err_t err = publish_frame(config::kKeepaliveId,
                                       payload.data(),
                                       payload.size(),
                                       "Victron keepalive");
    if (err == ESP_OK) {
        keepalive_.on_tx_keepalive(now_ms);
    } else {
        ESP_LOGW(TAG, "Failed to send keepalive: %s", esp_err_to_name(err));
    }
}

void Driver::process_keepalive_rx(bool remote_request, uint64_t now_ms)
{
    keepalive_.on_rx_keepalive(now_ms);

    if (remote_request) {
        ESP_LOGD(TAG, "Victron keepalive request received");
        send_keepalive(now_ms);
    }
}

void Driver::service_keepalive(uint64_t now_ms)
{
    if (!driver_started_.load(std::memory_order_acquire)) {
        return;
    }

    // Check timeout
    if (keepalive_.is_timeout(now_ms)) {
        const uint64_t last_rx = keepalive_.last_rx_ms();
        ESP_LOGW(TAG, "Victron keepalive timeout after %" PRIu64 " ms", now_ms - last_rx);

        // Publish timeout event
        if (event_bus_ != nullptr) {
            event_t event = {.type = EVENT_CAN_KEEPALIVE_TIMEOUT, .data = nullptr};
            event_bus_publish(event_bus_, &event);
        }

        send_keepalive(now_ms);
        return;
    }

    // Send periodic keepalive
    if (keepalive_.should_send_keepalive(now_ms)) {
        send_keepalive(now_ms);
    }
}

void Driver::handle_rx_message(const twai_message_t* message)
{
    if (message == nullptr) {
        return;
    }

    const bool is_remote = (message->flags & TWAI_MSG_FLAG_RTR) != 0U;
    const bool is_extended = (message->flags & TWAI_MSG_FLAG_EXTD) != 0U;
    const uint32_t identifier = message->identifier;
    const uint64_t timestamp = timestamp_ms();

    // Record statistics
    stats_.record_rx_frame(message->data_length_code, timestamp);

    // Handle keepalive
    if (!is_extended && identifier == config::kKeepaliveId) {
        process_keepalive_rx(is_remote, timestamp);
        ESP_LOGV(TAG, "RX Keepalive (remote=%d)", is_remote);
    }
    // Handle handshake
    else if (!is_extended && identifier == config::kHandshakeId) {
        if (message->data_length_code >= 7 &&
            message->data[4] == 'V' &&
            message->data[5] == 'I' &&
            message->data[6] == 'C') {
            ESP_LOGI(TAG, "Received valid 0x307 handshake with 'VIC' signature");

            if (event_bus_ != nullptr) {
                event_t event = {.type = EVENT_CAN_MESSAGE_RX, .data = nullptr};
                event_bus_publish(event_bus_, &event);
            }
        } else {
            ESP_LOGW(TAG, "Received 0x307 handshake but missing 'VIC' signature");
        }
    }
}

void Driver::task_entry(void* context)
{
    auto* driver = static_cast<Driver*>(context);
    if (driver != nullptr) {
        driver->task_loop();
    }
    vTaskDelete(nullptr);
}

void Driver::task_loop()
{
    ESP_LOGI(TAG, "CAN task started");

    while (!task_should_exit_.load(std::memory_order_acquire)) {
        const uint64_t now = timestamp_ms();

        if (driver_started_.load(std::memory_order_acquire)) {
#ifdef ESP_PLATFORM
            // Receive messages
            twai_message_t message = {0};
            while (!task_should_exit_.load(std::memory_order_acquire)) {
                const esp_err_t rx = twai_receive(&message, pdMS_TO_TICKS(config::kRxTimeoutMs));
                if (rx == ESP_OK) {
                    handle_rx_message(&message);
                } else if (rx == ESP_ERR_TIMEOUT) {
                    break;
                } else {
                    ESP_LOGW(TAG, "CAN receive error: %s", esp_err_to_name(rx));
                    break;
                }
            }

            // Service keepalive
            if (!task_should_exit_.load(std::memory_order_acquire)) {
                service_keepalive(now);
            }
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(config::kTaskDelayMs));
    }

    ESP_LOGI(TAG, "CAN task exiting");
}

esp_err_t Driver::get_status(can_victron_status_t* status)
{
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::memset(status, 0, sizeof(*status));
    status->timestamp_ms = timestamp_ms();
    status->driver_started = driver_started_.load(std::memory_order_acquire);
    status->keepalive_ok = keepalive_.is_ok();
    status->last_keepalive_tx_ms = keepalive_.last_tx_ms();
    status->last_keepalive_rx_ms = keepalive_.last_rx_ms();
    status->keepalive_interval_ms = config::kKeepaliveIntervalMs;
    status->keepalive_timeout_ms = config::kKeepaliveTimeoutMs;
    status->keepalive_retry_ms = config::kKeepaliveRetryMs;

    // Statistics
    status->tx_frame_count = stats_.tx_frame_count();
    status->rx_frame_count = stats_.rx_frame_count();
    status->tx_byte_count = stats_.tx_byte_count();
    status->rx_byte_count = stats_.rx_byte_count();

#ifdef ESP_PLATFORM
    twai_status_info_t info = {0};
    if (status->driver_started && twai_get_status_info(&info) == ESP_OK) {
        status->tx_error_counter = info.tx_error_counter;
        status->rx_error_counter = info.rx_error_counter;
        status->tx_failed_count = info.tx_failed_count;
        status->rx_missed_count = info.rx_missed_count;
        status->arbitration_lost_count = info.arb_lost_count;
        status->bus_error_count = info.bus_error_count;
        status->bus_state = info.state;
    }
#endif

    return ESP_OK;
}

void Driver::set_event_bus(event_bus_t* bus)
{
    event_bus_ = bus;
}

} // namespace victron
} // namespace can

// =============================================================================
// C API Wrappers
// =============================================================================

extern "C" {

void can_victron_init(void)
{
    can::victron::Driver::instance().init();
}

void can_victron_deinit(void)
{
    can::victron::Driver::instance().deinit();
}

void can_victron_set_event_bus(event_bus_t* bus)
{
    can::victron::Driver::instance().set_event_bus(bus);
}

esp_err_t can_victron_publish_frame(uint32_t can_id,
                                    const uint8_t* data,
                                    size_t length,
                                    const char* description)
{
    const std::string_view desc(description != nullptr ? description : "");
    return can::victron::Driver::instance().publish_frame(can_id, data, length, desc);
}

esp_err_t can_victron_get_status(can_victron_status_t* status)
{
    return can::victron::Driver::instance().get_status(status);
}

} // extern "C"
