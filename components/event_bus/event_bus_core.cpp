/**
 * @file event_bus_core.cpp
 * @brief Implementation of modern C++ event bus with thread-safety
 */

#include "event_bus_core.hpp"
#include <algorithm>
#include <cstring>

extern "C" {
#include "esp_log.h"
#include "esp_heap_caps.h"
}

static const char* TAG = "event_bus_core";

namespace event_bus {

// =============================================================================
// PayloadPool Implementation
// =============================================================================

PayloadPool::PayloadPool() {
    slots_ = new PoolSlot[config::kPayloadPoolSize];
    std::memset(slots_, 0, sizeof(PoolSlot) * config::kPayloadPoolSize);
    for (size_t i = 0; i < config::kPayloadPoolSize; ++i) {
        slots_[i].in_use = false;
    }
}

PayloadPool::~PayloadPool() {
    delete[] slots_;
}

void* PayloadPool::allocate(size_t size) {
    allocations_.fetch_add(1, std::memory_order_relaxed);

    if (size > config::kMaxPayloadSize) {
        pool_misses_.fetch_add(1, std::memory_order_relaxed);
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
        if (ptr == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes from heap", size);
        }
        return ptr;
    }

    // Try to find a free slot from pool
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < config::kPayloadPoolSize; ++i) {
        if (!slots_[i].in_use) {
            slots_[i].in_use = true;
            pool_hits_.fetch_add(1, std::memory_order_relaxed);
            return slots_[i].buffer;
        }
    }

    // Pool exhausted, allocate from heap
    pool_misses_.fetch_add(1, std::memory_order_relaxed);
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (ptr == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes from heap (pool exhausted)", size);
    }
    return ptr;
}

void PayloadPool::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    deallocations_.fetch_add(1, std::memory_order_relaxed);

    // Check if pointer is from pool
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < config::kPayloadPoolSize; ++i) {
        if (ptr == slots_[i].buffer) {
            if (!slots_[i].in_use) {
                ESP_LOGW(TAG, "Double free detected in pool slot %zu", i);
                return;
            }
            slots_[i].in_use = false;
            return;
        }
    }

    // Not from pool, free from heap
    heap_caps_free(ptr);
}

// =============================================================================
// SubscriberRegistry Implementation
// =============================================================================

uint64_t SubscriberRegistry::subscribe(event_type_t type, event_callback_t callback, void* user_ctx) {
    if (callback == nullptr) {
        ESP_LOGE(TAG, "Cannot subscribe with null callback");
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);
    subscribers_[type].emplace_back(id, type, callback, user_ctx);

    ESP_LOGD(TAG, "Subscriber %llu registered for event type 0x%08X", id, type);
    return id;
}

bool SubscriberRegistry::unsubscribe(uint64_t subscriber_id) {
    if (subscriber_id == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& pair : subscribers_) {
        auto& list = pair.second;
        auto it = std::find_if(list.begin(), list.end(),
            [subscriber_id](const Subscriber& sub) { return sub.id == subscriber_id; });

        if (it != list.end()) {
            ESP_LOGD(TAG, "Unsubscribing %llu from event type 0x%08X", subscriber_id, pair.first);
            list.erase(it);
            return true;
        }
    }

    ESP_LOGW(TAG, "Subscriber %llu not found", subscriber_id);
    return false;
}

void SubscriberRegistry::dispatch(event_bus_t* bus_handle, const event_t& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscribers_.find(event.type);
    if (it == subscribers_.end() || it->second.empty()) {
        ESP_LOGV(TAG, "No subscribers for event type 0x%08X", event.type);
        return;
    }

    // Create a copy to allow callbacks to modify subscriptions
    SubscriberList subscribers_copy = it->second;

    // Release lock before invoking callbacks to prevent deadlock
    mutex_.unlock();

    for (const auto& sub : subscribers_copy) {
        if (sub.callback != nullptr) {
            sub.callback(bus_handle, &event, sub.user_ctx);
        } else {
            ESP_LOGW(TAG, "Null callback for subscriber %llu", sub.id);
        }
    }

    // Re-acquire lock (will be released by lock_guard destructor)
    mutex_.lock();
}

uint32_t SubscriberRegistry::count() const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t total = 0;
    for (const auto& pair : subscribers_) {
        total += pair.second.size();
    }
    return total;
}

uint32_t SubscriberRegistry::count_for_type(event_type_t type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = subscribers_.find(type);
    if (it == subscribers_.end()) {
        return 0;
    }
    return it->second.size();
}

// =============================================================================
// EventBus Implementation
// =============================================================================

EventBus::EventBus() {
    queue_mutex_ = xSemaphoreCreateMutex();
    if (queue_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create queue mutex");
    }
}

EventBus::~EventBus() {
    deinit();
    if (queue_mutex_ != nullptr) {
        vSemaphoreDelete(queue_mutex_);
        queue_mutex_ = nullptr;
    }
}

void EventBus::init(uint32_t queue_length) {
    if (initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "EventBus already initialized");
        return;
    }

    queue_length_ = queue_length;
    queue_ = xQueueCreate(queue_length, sizeof(QueueItem));

    if (queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    initialized_.store(true, std::memory_order_release);
    ESP_LOGI(TAG, "EventBus initialized with queue length %u", queue_length);
}

void EventBus::deinit() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }

    initialized_.store(false, std::memory_order_release);

    // Drain queue and free payloads
    if (queue_ != nullptr) {
        QueueItem item;
        while (xQueueReceive(queue_, &item, 0) == pdTRUE) {
            if (item.payload_ptr != nullptr) {
                free_payload(item.payload_ptr, item.payload_from_pool);
            }
        }
        vQueueDelete(queue_);
        queue_ = nullptr;
    }

    ESP_LOGI(TAG, "EventBus deinitialized");
}

uint64_t EventBus::subscribe(event_type_t type, event_callback_t callback, void* user_ctx) {
    return registry_.subscribe(type, callback, user_ctx);
}

bool EventBus::unsubscribe(uint64_t subscriber_id) {
    return registry_.unsubscribe(subscriber_id);
}

bool EventBus::publish(const event_t& event) {
    if (!initialized_.load(std::memory_order_acquire)) {
        ESP_LOGE(TAG, "Cannot publish: EventBus not initialized");
        return false;
    }

    QueueItem item;
    item.event = event;
    item.payload_ptr = nullptr;
    item.payload_size = 0;
    item.payload_from_pool = false;

    // Allocate and copy payload if present
    if (event.data != nullptr && event.data_len > 0) {
        bool from_pool = false;
        void* payload_copy = allocate_payload(event.data_len, from_pool);

        if (payload_copy == nullptr) {
            stats_.allocation_failures.fetch_add(1, std::memory_order_relaxed);
            ESP_LOGE(TAG, "Failed to allocate payload of size %zu", event.data_len);
            return false;
        }

        std::memcpy(payload_copy, event.data, event.data_len);
        item.payload_ptr = payload_copy;
        item.payload_size = event.data_len;
        item.payload_from_pool = from_pool;
        item.event.data = static_cast<uint8_t*>(payload_copy);
    }

    // Try to enqueue
    ScopedMutex lock(queue_mutex_);
    if (!lock.is_locked()) {
        ESP_LOGE(TAG, "Failed to acquire queue mutex");
        if (item.payload_ptr != nullptr) {
            free_payload(item.payload_ptr, item.payload_from_pool);
        }
        return false;
    }

    if (xQueueSend(queue_, &item, 0) != pdTRUE) {
        stats_.queue_full_count.fetch_add(1, std::memory_order_relaxed);
        stats_.dropped_total.fetch_add(1, std::memory_order_relaxed);

        if (item.payload_ptr != nullptr) {
            free_payload(item.payload_ptr, item.payload_from_pool);
        }

        ESP_LOGW(TAG, "Event queue full, dropped event type 0x%08X", event.type);
        return false;
    }

    stats_.published_total.fetch_add(1, std::memory_order_relaxed);
    return true;
}

EventBus::Metrics EventBus::get_metrics() const {
    Metrics m;
    m.subscribers = registry_.count();
    m.published_total = stats_.published_total.load(std::memory_order_relaxed);
    m.dispatched_total = stats_.dispatched_total.load(std::memory_order_relaxed);
    m.dropped_total = stats_.dropped_total.load(std::memory_order_relaxed);
    m.queue_capacity = queue_length_;
    m.queue_depth = (queue_ != nullptr) ? uxQueueMessagesWaiting(queue_) : 0;
    m.pool_hits = payload_pool_.pool_hits();
    m.pool_misses = payload_pool_.pool_misses();
    return m;
}

void EventBus::dispatch_task_loop(event_bus_t* bus_handle) {
    QueueItem item;

    while (initialized_.load(std::memory_order_acquire)) {
        if (xQueueReceive(queue_, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
            dispatch_event(bus_handle, item);
        }
    }
}

void EventBus::dispatch_event(event_bus_t* bus_handle, const QueueItem& item) {
    registry_.dispatch(bus_handle, item.event);

    stats_.dispatched_total.fetch_add(1, std::memory_order_relaxed);

    // Free payload after dispatch
    if (item.payload_ptr != nullptr) {
        free_payload(item.payload_ptr, item.payload_from_pool);
    }
}

void* EventBus::allocate_payload(size_t size, bool& from_pool) {
    void* ptr = payload_pool_.allocate(size);
    from_pool = (size <= config::kMaxPayloadSize && ptr != nullptr);
    return ptr;
}

void EventBus::free_payload(void* ptr, bool from_pool) {
    if (from_pool) {
        payload_pool_.deallocate(ptr);
    } else {
        heap_caps_free(ptr);
    }
}

} // namespace event_bus

// =============================================================================
// C API Wrappers Implementation
// =============================================================================

extern "C" {

// Global instance pointer (stored in event_bus_t->impl_data)
event_bus::EventBus* event_bus_get_cpp_instance(event_bus_t* bus) {
    if (bus == nullptr || bus->impl_data == nullptr) {
        return nullptr;
    }
    return static_cast<event_bus::EventBus*>(bus->impl_data);
}

void event_bus_set_cpp_instance(event_bus_t* bus, event_bus::EventBus* instance) {
    if (bus != nullptr) {
        bus->impl_data = instance;
    }
}

} // extern "C"
