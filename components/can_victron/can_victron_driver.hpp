/**
 * @file can_victron_driver.hpp
 * @brief Modern C++ wrapper for CAN Victron driver
 *
 * This header provides a C++ interface with RAII, strong typing, and better
 * encapsulation compared to the C implementation. The C API remains available
 * for backward compatibility.
 *
 * Key improvements:
 * - RAII mutex management (std::lock_guard)
 * - Encapsulated state in singleton class
 * - Type-safe enums and constants
 * - Modern C++ idioms (optional, string_view, etc.)
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

extern "C" {
#include "can_victron.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef ESP_PLATFORM
#include "driver/twai.h"
#include "freertos/semphr.h"
#endif
}

namespace can {
namespace victron {

// =============================================================================
// Configuration Constants
// =============================================================================

namespace config {
constexpr gpio_num_t kTxGpio = GPIO_NUM_22;
constexpr gpio_num_t kRxGpio = GPIO_NUM_21;
constexpr uint32_t kKeepaliveIntervalMs = 1000;
constexpr uint32_t kKeepaliveTimeoutMs = 5000;
constexpr uint32_t kKeepaliveRetryMs = 2000;
constexpr uint32_t kTaskDelayMs = 50;
constexpr uint32_t kRxTimeoutMs = 10;
constexpr uint32_t kTxTimeoutMs = 50;
constexpr uint32_t kLockTimeoutMs = 50;
constexpr size_t kTxQueueLen = 16;
constexpr size_t kRxQueueLen = 16;
constexpr size_t kTaskStackSize = 4096;
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY + 6;
constexpr uint32_t kBitrateHz = 500000;

// CAN IDs
constexpr uint32_t kKeepaliveId = 0x305U;
constexpr uint32_t kHandshakeId = 0x307U;
constexpr uint8_t kKeepaliveDLC = 8U;

// Identity strings
constexpr std::string_view kHandshakeAscii = "VIC";
constexpr std::string_view kManufacturer = "Enepaq";
constexpr std::string_view kBatteryName = "ESP32-P4-BMS";
constexpr std::string_view kBatteryFamily = "LiFePO4";
constexpr std::string_view kSerialNumber = "ESP32P4-00000001";
} // namespace config

// =============================================================================
// RAII Mutex Guard
// =============================================================================

/**
 * @brief RAII wrapper for FreeRTOS semaphore (mutex)
 *
 * Ensures mutex is always released, even in case of early returns or exceptions.
 */
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

    // Non-copyable, non-movable
    ScopedMutex(const ScopedMutex&) = delete;
    ScopedMutex& operator=(const ScopedMutex&) = delete;
    ScopedMutex(ScopedMutex&&) = delete;
    ScopedMutex& operator=(ScopedMutex&&) = delete;

    [[nodiscard]] bool is_locked() const { return locked_; }

private:
    SemaphoreHandle_t mutex_;
    bool locked_;
};

// =============================================================================
// Statistics Tracker
// =============================================================================

/**
 * @brief Thread-safe statistics tracking
 */
class Statistics {
public:
    Statistics() = default;

    void record_tx_frame(size_t dlc, uint64_t timestamp);
    void record_rx_frame(size_t dlc, uint64_t timestamp);
    void reset();

    uint64_t tx_frame_count() const { return tx_frame_count_.load(std::memory_order_relaxed); }
    uint64_t rx_frame_count() const { return rx_frame_count_.load(std::memory_order_relaxed); }
    uint64_t tx_byte_count() const { return tx_byte_count_.load(std::memory_order_relaxed); }
    uint64_t rx_byte_count() const { return rx_byte_count_.load(std::memory_order_relaxed); }

private:
    std::atomic<uint64_t> tx_frame_count_{0};
    std::atomic<uint64_t> rx_frame_count_{0};
    std::atomic<uint64_t> tx_byte_count_{0};
    std::atomic<uint64_t> rx_byte_count_{0};
};

// =============================================================================
// Keepalive Manager
// =============================================================================

/**
 * @brief Manages CAN keepalive state and timing
 */
class KeepaliveManager {
public:
    KeepaliveManager() = default;

    void on_rx_keepalive(uint64_t timestamp_ms);
    void on_tx_keepalive(uint64_t timestamp_ms);
    bool should_send_keepalive(uint64_t now_ms) const;
    bool is_timeout(uint64_t now_ms) const;
    void reset(uint64_t now_ms);

    bool is_ok() const { return ok_.load(std::memory_order_acquire); }
    uint64_t last_tx_ms() const { return last_tx_ms_.load(std::memory_order_acquire); }
    uint64_t last_rx_ms() const { return last_rx_ms_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> ok_{false};
    std::atomic<uint64_t> last_tx_ms_{0};
    std::atomic<uint64_t> last_rx_ms_{0};
};

// =============================================================================
// CAN Victron Driver (Singleton)
// =============================================================================

/**
 * @brief Main CAN Victron driver class
 *
 * Singleton pattern ensures single instance. Provides thread-safe access
 * to CAN hardware and manages lifecycle, keepalive, and events.
 */
class Driver {
public:
    static Driver& instance();

    // Lifecycle
    esp_err_t init();
    void deinit();

    // Frame transmission
    esp_err_t publish_frame(uint32_t can_id,
                           const uint8_t* data,
                           size_t length,
                           std::string_view description);

    // Status and configuration
    esp_err_t get_status(can_victron_status_t* status);
    void set_event_bus(event_bus_t* bus);

    // Driver state
    bool is_driver_started() const { return driver_started_.load(std::memory_order_acquire); }

private:
    Driver() = default;
    ~Driver() = default;

    // Non-copyable, non-movable
    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;
    Driver(Driver&&) = delete;
    Driver& operator=(Driver&&) = delete;

    // Internal methods
    esp_err_t start_driver();
    void stop_driver();
    void send_keepalive(uint64_t now_ms);
    void process_keepalive_rx(bool remote_request, uint64_t now_ms);
    void service_keepalive(uint64_t now_ms);
    void handle_rx_message(const twai_message_t* message);
    static void task_entry(void* context);
    void task_loop();

    // State
    std::atomic<bool> driver_started_{false};
    std::atomic<bool> task_should_exit_{false};
    event_bus_t* event_bus_{nullptr};
    TaskHandle_t task_handle_{nullptr};

    // Thread-safe components
    KeepaliveManager keepalive_;
    Statistics stats_;

#ifdef ESP_PLATFORM
    // Mutexes (using FreeRTOS handles for now, could wrap in std::unique_ptr with custom deleter)
    SemaphoreHandle_t twai_mutex_{nullptr};
    SemaphoreHandle_t driver_state_mutex_{nullptr};
    SemaphoreHandle_t stats_mutex_{nullptr};
#endif
};

} // namespace victron
} // namespace can
