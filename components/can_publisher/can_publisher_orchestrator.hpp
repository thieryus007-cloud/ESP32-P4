/**
 * @file can_publisher_orchestrator.hpp
 * @brief Modern C++ orchestrator for CAN message publishing
 *
 * This header provides performance and robustness improvements over the C implementation:
 * - Circuit breaker pattern for error handling
 * - Advanced rate limiting with token bucket algorithm
 * - Frame caching to avoid redundant encoding
 * - Detailed metrics (latency, throughput, error rates)
 * - RAII resource management
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

extern "C" {
#include "can_publisher.h"
#include "event_bus.h"
#include "tinybms_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
}

namespace can {
namespace publisher {

// =============================================================================
// Configuration
// =============================================================================

namespace config {
constexpr uint32_t kPublishIntervalMs = 1000;
constexpr uint32_t kMaxCachedFrames = 32;
constexpr size_t kMetricsWindowSize = 100;

// Circuit breaker configuration
constexpr uint32_t kCircuitBreakerThreshold = 5;  // failures before opening
constexpr uint32_t kCircuitBreakerTimeoutMs = 30000;  // 30s before retry
constexpr uint32_t kCircuitBreakerSuccessThreshold = 3;  // successes to close

// Rate limiter configuration (token bucket)
constexpr uint32_t kTokenBucketCapacity = 10;
constexpr uint32_t kTokenRefillRateMs = 100;  // 1 token per 100ms
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
// Circuit Breaker Pattern
// =============================================================================

/**
 * @brief Circuit breaker for fault tolerance
 *
 * States:
 * - CLOSED: Normal operation, failures counted
 * - OPEN: Too many failures, block all requests
 * - HALF_OPEN: Testing if service recovered
 */
class CircuitBreaker {
public:
    enum class State {
        CLOSED,
        OPEN,
        HALF_OPEN
    };

    CircuitBreaker() = default;

    bool allow_request(uint64_t now_ms);
    void record_success();
    void record_failure();
    void reset();

    State state() const { return state_.load(std::memory_order_acquire); }
    uint32_t failure_count() const { return failure_count_.load(std::memory_order_acquire); }

private:
    std::atomic<State> state_{State::CLOSED};
    std::atomic<uint32_t> failure_count_{0};
    std::atomic<uint32_t> success_count_{0};
    std::atomic<uint64_t> last_failure_time_ms_{0};
};

// =============================================================================
// Token Bucket Rate Limiter
// =============================================================================

/**
 * @brief Token bucket algorithm for rate limiting
 *
 * More sophisticated than simple throttling:
 * - Allows bursts up to capacity
 * - Smooth rate limiting over time
 * - No artificial delays
 */
class RateLimiter {
public:
    RateLimiter() = default;

    bool try_consume(uint64_t now_ms, uint32_t tokens = 1);
    void reset();

    uint32_t available_tokens() const { return tokens_.load(std::memory_order_acquire); }

private:
    void refill(uint64_t now_ms);

    std::atomic<uint32_t> tokens_{config::kTokenBucketCapacity};
    std::atomic<uint64_t> last_refill_ms_{0};
};

// =============================================================================
// Frame Cache
// =============================================================================

/**
 * @brief Cache for encoded CAN frames
 *
 * Avoids redundant encoding when BMS data hasn't changed significantly.
 */
struct CachedFrame {
    uint32_t can_id;
    std::array<uint8_t, 8> data;
    uint8_t dlc;
    uint64_t timestamp_ms;
    uint32_t hash;  // Hash of source BMS data
    bool valid;
};

class FrameCache {
public:
    FrameCache();

    std::optional<CachedFrame> get(uint32_t can_id, uint32_t data_hash) const;
    void put(uint32_t can_id, const uint8_t* data, uint8_t dlc,
             uint32_t data_hash, uint64_t timestamp_ms);
    void invalidate();

    uint32_t hit_count() const { return hit_count_.load(std::memory_order_relaxed); }
    uint32_t miss_count() const { return miss_count_.load(std::memory_order_relaxed); }

private:
    size_t find_slot(uint32_t can_id) const;

    std::array<CachedFrame, config::kMaxCachedFrames> frames_;
    mutable std::atomic<uint32_t> hit_count_{0};
    mutable std::atomic<uint32_t> miss_count_{0};
};

// =============================================================================
// Metrics Collector
// =============================================================================

/**
 * @brief Detailed metrics for monitoring and optimization
 */
struct PublishMetrics {
    uint64_t total_publishes{0};
    uint64_t successful_publishes{0};
    uint64_t failed_publishes{0};
    uint64_t throttled_publishes{0};
    uint64_t cache_hits{0};
    uint64_t cache_misses{0};
    double avg_latency_ms{0.0};
    double max_latency_ms{0.0};
    uint64_t last_publish_ms{0};
    uint32_t circuit_breaker_trips{0};
};

class MetricsCollector {
public:
    MetricsCollector() = default;

    void record_publish_start(uint64_t timestamp_ms);
    void record_publish_success(uint64_t timestamp_ms);
    void record_publish_failure(uint64_t timestamp_ms);
    void record_throttled();
    void record_cache_hit();
    void record_cache_miss();
    void record_circuit_breaker_trip();

    PublishMetrics get_metrics() const;
    void reset();

private:
    std::atomic<uint64_t> total_publishes_{0};
    std::atomic<uint64_t> successful_publishes_{0};
    std::atomic<uint64_t> failed_publishes_{0};
    std::atomic<uint64_t> throttled_publishes_{0};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::atomic<uint64_t> last_publish_start_ms_{0};
    std::atomic<uint64_t> last_publish_ms_{0};
    std::atomic<uint32_t> circuit_breaker_trips_{0};

    // Latency tracking (simplified - could use a ring buffer for percentiles)
    mutable std::mutex latency_mutex_;
    double total_latency_ms_{0.0};
    double max_latency_ms_{0.0};
    uint64_t latency_sample_count_{0};
};

// =============================================================================
// CAN Publisher Orchestrator
// =============================================================================

/**
 * @brief Main orchestrator class with enhanced features
 */
class Orchestrator {
public:
    static Orchestrator& instance();

    esp_err_t init();
    void deinit();
    void get_stats(uint32_t* publish_count, uint64_t* last_publish_ms);
    PublishMetrics get_detailed_metrics() const;

private:
    Orchestrator() = default;
    ~Orchestrator() = default;

    Orchestrator(const Orchestrator&) = delete;
    Orchestrator& operator=(const Orchestrator&) = delete;

    // Event handler
    static void on_tinybms_register_updated(event_bus_t* bus, const event_t* event, void* ctx);
    void handle_tinybms_update();

    // Publishing logic
    esp_err_t publish_all_channels(const uart_bms_live_data_t& bms_data);
    uint32_t compute_data_hash(const uart_bms_live_data_t& bms_data) const;

    // State
    std::atomic<bool> initialized_{false};
    EventBus* event_bus_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};

    // Enhanced components
    CircuitBreaker circuit_breaker_;
    RateLimiter rate_limiter_;
    FrameCache frame_cache_;
    MetricsCollector metrics_;
};

} // namespace publisher
} // namespace can
