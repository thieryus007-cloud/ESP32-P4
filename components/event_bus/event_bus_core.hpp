/**
 * @file event_bus_core.hpp
 * @brief Modern C++ event bus with thread-safety and enhanced features
 *
 * Critical improvements over C implementation:
 * - Thread-safe subscriber management (CRITICAL - was completely unsafe)
 * - RAII mutex management
 * - Dynamic subscriber list (vs fixed array)
 * - Unsubscribe capability (was impossible)
 * - Memory pool for payloads (reduces fragmentation)
 * - Backpressure handling
 * - Comprehensive statistics
 * - Type-safe event handling
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

extern "C" {
#include "event_bus.h"
#include "event_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

namespace event_bus {

// =============================================================================
// Configuration
// =============================================================================

namespace config {
constexpr uint32_t kDefaultQueueLength = 32;
constexpr uint32_t kMaxSubscribersPerEvent = 16;
constexpr size_t kPayloadPoolSize = 64;  // Number of pre-allocated payload buffers
constexpr size_t kMaxPayloadSize = 512;  // Max size for pooled payloads
} // namespace config

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
// Memory Pool for Event Payloads
// =============================================================================

/**
 * @brief Fixed-size memory pool to reduce fragmentation
 */
class PayloadPool {
public:
    PayloadPool();
    ~PayloadPool();

    void* allocate(size_t size);
    void deallocate(void* ptr);

    uint32_t allocations() const { return allocations_.load(std::memory_order_relaxed); }
    uint32_t deallocations() const { return deallocations_.load(std::memory_order_relaxed); }
    uint32_t pool_hits() const { return pool_hits_.load(std::memory_order_relaxed); }
    uint32_t pool_misses() const { return pool_misses_.load(std::memory_order_relaxed); }

private:
    struct PoolSlot {
        uint8_t buffer[config::kMaxPayloadSize];
        bool in_use;
    };

    PoolSlot* slots_;
    std::mutex mutex_;
    std::atomic<uint32_t> allocations_{0};
    std::atomic<uint32_t> deallocations_{0};
    std::atomic<uint32_t> pool_hits_{0};
    std::atomic<uint32_t> pool_misses_{0};
};

// =============================================================================
// Subscriber Management
// =============================================================================

/**
 * @brief Subscriber entry with unique ID
 */
struct Subscriber {
    uint64_t id;
    event_type_t type;
    event_callback_t callback;
    void* user_ctx;

    Subscriber(uint64_t id_, event_type_t type_, event_callback_t cb, void* ctx)
        : id(id_), type(type_), callback(cb), user_ctx(ctx) {}
};

using SubscriberList = std::vector<Subscriber>;
using SubscriberMap = std::unordered_map<event_type_t, SubscriberList>;

/**
 * @brief Thread-safe subscriber registry
 */
class SubscriberRegistry {
public:
    SubscriberRegistry() = default;

    uint64_t subscribe(event_type_t type, event_callback_t callback, void* user_ctx);
    bool unsubscribe(uint64_t subscriber_id);
    void dispatch(event_bus_t* bus_handle, const event_t& event);

    uint32_t count() const;
    uint32_t count_for_type(event_type_t type) const;

private:
    mutable std::mutex mutex_;
    SubscriberMap subscribers_;
    std::atomic<uint64_t> next_id_{1};
};

// =============================================================================
// Queue Item with Smart Payload
// =============================================================================

struct QueueItem {
    event_t event;
    void* payload_ptr;
    size_t payload_size;
    bool payload_from_pool;

    QueueItem() : payload_ptr(nullptr), payload_size(0), payload_from_pool(false) {}
};

// =============================================================================
// Event Bus Statistics
// =============================================================================

struct BusStatistics {
    std::atomic<uint64_t> published_total{0};
    std::atomic<uint64_t> dispatched_total{0};
    std::atomic<uint64_t> dropped_total{0};
    std::atomic<uint64_t> queue_full_count{0};
    std::atomic<uint64_t> allocation_failures{0};

    void reset() {
        published_total.store(0);
        dispatched_total.store(0);
        dropped_total.store(0);
        queue_full_count.store(0);
        allocation_failures.store(0);
    }
};

// =============================================================================
// Event Bus Core
// =============================================================================

/**
 * @brief Main event bus implementation
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    void init(uint32_t queue_length = config::kDefaultQueueLength);
    void deinit();

    uint64_t subscribe(event_type_t type, event_callback_t callback, void* user_ctx);
    bool unsubscribe(uint64_t subscriber_id);

    bool publish(const event_t& event);

    // Statistics
    struct Metrics {
        uint32_t subscribers;
        uint64_t published_total;
        uint64_t dispatched_total;
        uint64_t dropped_total;
        uint32_t queue_capacity;
        uint32_t queue_depth;
        uint32_t pool_hits;
        uint32_t pool_misses;
    };

    Metrics get_metrics() const;

    // Queue management
    void dispatch_task_loop(event_bus_t* bus_handle);

private:
    void dispatch_event(event_bus_t* bus_handle, const QueueItem& item);
    void* allocate_payload(size_t size, bool& from_pool);
    void free_payload(void* ptr, bool from_pool);

    // State
    std::atomic<bool> initialized_{false};
    QueueHandle_t queue_{nullptr};
    uint32_t queue_length_{config::kDefaultQueueLength};

    // Components
    SubscriberRegistry registry_;
    PayloadPool payload_pool_;
    BusStatistics stats_;

    // Thread-safety
    mutable SemaphoreHandle_t queue_mutex_{nullptr};
};

} // namespace event_bus

// =============================================================================
// C API Wrappers (backward compatibility)
// =============================================================================

extern "C" {

// Internal: Get C++ instance from C handle
event_bus::EventBus* event_bus_get_cpp_instance(event_bus_t* bus);
void event_bus_set_cpp_instance(event_bus_t* bus, event_bus::EventBus* instance);

} // extern "C"
