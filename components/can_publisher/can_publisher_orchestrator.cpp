/**
 * @file can_publisher_orchestrator.cpp
 * @brief Implementation of modern C++ CAN publisher orchestrator
 */

#include "can_publisher_orchestrator.hpp"

extern "C" {
#include "can_victron.h"
#include "conversion_table.h"
#include "cvl_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_types.h"
}

#include <algorithm>
#include <cstring>

namespace can {
namespace publisher {

namespace {
const char* TAG = "can_pub_cpp";

uint64_t timestamp_ms()
{
    return esp_timer_get_time() / 1000ULL;
}

// Simple FNV-1a hash for BMS data
uint32_t hash_bms_data(const uart_bms_live_data_t& data)
{
    const uint32_t FNV_PRIME = 0x01000193;
    const uint32_t FNV_OFFSET = 0x811c9dc5;

    uint32_t hash = FNV_OFFSET;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);

    // Hash key fields only (not all fields to allow some tolerance)
    const size_t offset_soc = offsetof(uart_bms_live_data_t, state_of_charge_pct);
    const size_t offset_voltage = offsetof(uart_bms_live_data_t, pack_voltage_v);
    const size_t offset_current = offsetof(uart_bms_live_data_t, pack_current_a);

    for (size_t i = offset_soc; i < offset_soc + sizeof(float); ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    for (size_t i = offset_voltage; i < offset_voltage + sizeof(float); ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    for (size_t i = offset_current; i < offset_current + sizeof(float); ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }

    return hash;
}

} // anonymous namespace

// =============================================================================
// Circuit Breaker Implementation
// =============================================================================

bool CircuitBreaker::allow_request(uint64_t now_ms)
{
    const State current_state = state_.load(std::memory_order_acquire);

    switch (current_state) {
    case State::CLOSED:
        return true;

    case State::OPEN: {
        const uint64_t last_failure = last_failure_time_ms_.load(std::memory_order_acquire);
        if ((now_ms - last_failure) >= config::kCircuitBreakerTimeoutMs) {
            // Try half-open
            state_.store(State::HALF_OPEN, std::memory_order_release);
            ESP_LOGI(TAG, "Circuit breaker entering HALF_OPEN state");
            return true;
        }
        return false;
    }

    case State::HALF_OPEN:
        return true;

    default:
        return false;
    }
}

void CircuitBreaker::record_success()
{
    const State current_state = state_.load(std::memory_order_acquire);

    if (current_state == State::HALF_OPEN) {
        const uint32_t successes = success_count_.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (successes >= config::kCircuitBreakerSuccessThreshold) {
            state_.store(State::CLOSED, std::memory_order_release);
            failure_count_.store(0, std::memory_order_release);
            success_count_.store(0, std::memory_order_release);
            ESP_LOGI(TAG, "Circuit breaker CLOSED after %u successes", successes);
        }
    } else if (current_state == State::CLOSED) {
        // Reset failure count on success
        failure_count_.store(0, std::memory_order_release);
    }
}

void CircuitBreaker::record_failure()
{
    const State current_state = state_.load(std::memory_order_acquire);

    if (current_state == State::HALF_OPEN) {
        // Failed during test, back to open
        state_.store(State::OPEN, std::memory_order_release);
        last_failure_time_ms_.store(timestamp_ms(), std::memory_order_release);
        success_count_.store(0, std::memory_order_release);
        ESP_LOGW(TAG, "Circuit breaker back to OPEN after failure in HALF_OPEN");
        return;
    }

    const uint32_t failures = failure_count_.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (failures >= config::kCircuitBreakerThreshold) {
        state_.store(State::OPEN, std::memory_order_release);
        last_failure_time_ms_.store(timestamp_ms(), std::memory_order_release);
        ESP_LOGW(TAG, "Circuit breaker OPEN after %u failures", failures);
    }
}

void CircuitBreaker::reset()
{
    state_.store(State::CLOSED, std::memory_order_release);
    failure_count_.store(0, std::memory_order_release);
    success_count_.store(0, std::memory_order_release);
    last_failure_time_ms_.store(0, std::memory_order_release);
}

// =============================================================================
// Rate Limiter Implementation
// =============================================================================

void RateLimiter::refill(uint64_t now_ms)
{
    const uint64_t last_refill = last_refill_ms_.load(std::memory_order_acquire);
    if (last_refill == 0) {
        last_refill_ms_.store(now_ms, std::memory_order_release);
        tokens_.store(config::kTokenBucketCapacity, std::memory_order_release);
        return;
    }

    const uint64_t elapsed_ms = now_ms - last_refill;
    if (elapsed_ms < config::kTokenRefillRateMs) {
        return;
    }

    const uint32_t tokens_to_add = elapsed_ms / config::kTokenRefillRateMs;
    if (tokens_to_add == 0) {
        return;
    }

    const uint32_t current_tokens = tokens_.load(std::memory_order_acquire);
    const uint32_t new_tokens = std::min(current_tokens + tokens_to_add,
                                         config::kTokenBucketCapacity);

    tokens_.store(new_tokens, std::memory_order_release);
    last_refill_ms_.store(now_ms, std::memory_order_release);
}

bool RateLimiter::try_consume(uint64_t now_ms, uint32_t tokens)
{
    refill(now_ms);

    uint32_t current = tokens_.load(std::memory_order_acquire);
    while (current >= tokens) {
        if (tokens_.compare_exchange_weak(current, current - tokens,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

void RateLimiter::reset()
{
    tokens_.store(config::kTokenBucketCapacity, std::memory_order_release);
    last_refill_ms_.store(timestamp_ms(), std::memory_order_release);
}

// =============================================================================
// Frame Cache Implementation
// =============================================================================

FrameCache::FrameCache()
{
    for (auto& frame : frames_) {
        frame.valid = false;
        frame.can_id = 0;
        frame.dlc = 0;
        frame.timestamp_ms = 0;
        frame.hash = 0;
        frame.data.fill(0);
    }
}

size_t FrameCache::find_slot(uint32_t can_id) const
{
    // Simple hash-based slot selection
    return can_id % config::kMaxCachedFrames;
}

std::optional<CachedFrame> FrameCache::get(uint32_t can_id, uint32_t data_hash) const
{
    const size_t slot = find_slot(can_id);
    const CachedFrame& frame = frames_[slot];

    if (frame.valid && frame.can_id == can_id && frame.hash == data_hash) {
        hit_count_.fetch_add(1, std::memory_order_relaxed);
        return frame;
    }

    miss_count_.fetch_add(1, std::memory_order_relaxed);
    return std::nullopt;
}

void FrameCache::put(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                     uint32_t data_hash, uint64_t timestamp_ms)
{
    const size_t slot = find_slot(can_id);
    CachedFrame& frame = frames_[slot];

    frame.can_id = can_id;
    frame.dlc = dlc;
    frame.hash = data_hash;
    frame.timestamp_ms = timestamp_ms;
    std::memcpy(frame.data.data(), data, std::min<size_t>(dlc, 8));
    frame.valid = true;
}

void FrameCache::invalidate()
{
    for (auto& frame : frames_) {
        frame.valid = false;
    }
}

// =============================================================================
// Metrics Collector Implementation
// =============================================================================

void MetricsCollector::record_publish_start(uint64_t timestamp_ms)
{
    total_publishes_.fetch_add(1, std::memory_order_relaxed);
    last_publish_start_ms_.store(timestamp_ms, std::memory_order_release);
}

void MetricsCollector::record_publish_success(uint64_t timestamp_ms)
{
    successful_publishes_.fetch_add(1, std::memory_order_relaxed);
    last_publish_ms_.store(timestamp_ms, std::memory_order_release);

    // Calculate latency
    const uint64_t start_ms = last_publish_start_ms_.load(std::memory_order_acquire);
    if (start_ms > 0) {
        const double latency = static_cast<double>(timestamp_ms - start_ms);

        std::lock_guard<std::mutex> lock(latency_mutex_);
        total_latency_ms_ += latency;
        latency_sample_count_++;
        if (latency > max_latency_ms_) {
            max_latency_ms_ = latency;
        }
    }
}

void MetricsCollector::record_publish_failure(uint64_t timestamp_ms)
{
    (void)timestamp_ms;
    failed_publishes_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_throttled()
{
    throttled_publishes_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_cache_hit()
{
    cache_hits_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_cache_miss()
{
    cache_misses_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_circuit_breaker_trip()
{
    circuit_breaker_trips_.fetch_add(1, std::memory_order_relaxed);
}

PublishMetrics MetricsCollector::get_metrics() const
{
    PublishMetrics metrics;
    metrics.total_publishes = total_publishes_.load(std::memory_order_relaxed);
    metrics.successful_publishes = successful_publishes_.load(std::memory_order_relaxed);
    metrics.failed_publishes = failed_publishes_.load(std::memory_order_relaxed);
    metrics.throttled_publishes = throttled_publishes_.load(std::memory_order_relaxed);
    metrics.cache_hits = cache_hits_.load(std::memory_order_relaxed);
    metrics.cache_misses = cache_misses_.load(std::memory_order_relaxed);
    metrics.last_publish_ms = last_publish_ms_.load(std::memory_order_relaxed);
    metrics.circuit_breaker_trips = circuit_breaker_trips_.load(std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(latency_mutex_);
    if (latency_sample_count_ > 0) {
        metrics.avg_latency_ms = total_latency_ms_ / latency_sample_count_;
    }
    metrics.max_latency_ms = max_latency_ms_;

    return metrics;
}

void MetricsCollector::reset()
{
    total_publishes_.store(0, std::memory_order_relaxed);
    successful_publishes_.store(0, std::memory_order_relaxed);
    failed_publishes_.store(0, std::memory_order_relaxed);
    throttled_publishes_.store(0, std::memory_order_relaxed);
    cache_hits_.store(0, std::memory_order_relaxed);
    cache_misses_.store(0, std::memory_order_relaxed);
    last_publish_start_ms_.store(0, std::memory_order_relaxed);
    last_publish_ms_.store(0, std::memory_order_relaxed);
    circuit_breaker_trips_.store(0, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(latency_mutex_);
    total_latency_ms_ = 0.0;
    max_latency_ms_ = 0.0;
    latency_sample_count_ = 0;
}

// =============================================================================
// Orchestrator Implementation
// =============================================================================

Orchestrator& Orchestrator::instance()
{
    static Orchestrator instance;
    return instance;
}

esp_err_t Orchestrator::init()
{
    if (initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing CAN Publisher Orchestrator (C++)");

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Get event bus
    event_bus_ = event_bus_get_instance();
    if (event_bus_ == nullptr) {
        ESP_LOGE(TAG, "EventBus not available");
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
        return ESP_FAIL;
    }

    // Initialize CVL controller
    can_publisher_cvl_init();

    // Restore energy state from NVS
    esp_err_t err = can_publisher_conversion_restore_energy_state();
    if (err == ESP_OK) {
        double charged_wh, discharged_wh;
        can_publisher_conversion_get_energy_state(&charged_wh, &discharged_wh);
        ESP_LOGI(TAG, "Energy restored: charge=%.1fWh, discharge=%.1fWh",
                 charged_wh, discharged_wh);
    } else {
        ESP_LOGW(TAG, "No energy state in NVS (first boot): %s", esp_err_to_name(err));
        can_publisher_conversion_reset_state();
    }

    // Initialize components
    circuit_breaker_.reset();
    rate_limiter_.reset();
    frame_cache_.invalidate();
    metrics_.reset();

    // Subscribe to TinyBMS events
    event_bus_subscribe(event_bus_, EVENT_TINYBMS_REGISTER_UPDATED,
                       on_tinybms_register_updated, this);

    initialized_.store(true, std::memory_order_release);

    ESP_LOGI(TAG, "CAN Publisher Orchestrator initialized with advanced features:");
    ESP_LOGI(TAG, "  - Circuit breaker (threshold=%u, timeout=%ums)",
             config::kCircuitBreakerThreshold, config::kCircuitBreakerTimeoutMs);
    ESP_LOGI(TAG, "  - Token bucket rate limiter (capacity=%u, refill=%ums)",
             config::kTokenBucketCapacity, config::kTokenRefillRateMs);
    ESP_LOGI(TAG, "  - Frame cache (capacity=%u)", config::kMaxCachedFrames);

    return ESP_OK;
}

void Orchestrator::deinit()
{
    if (!initialized_.exchange(false, std::memory_order_acq_rel)) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }

    ESP_LOGI(TAG, "Deinitializing CAN Publisher Orchestrator");

    // Unsubscribe from events
    if (event_bus_ != nullptr) {
        event_bus_unsubscribe(event_bus_, EVENT_TINYBMS_REGISTER_UPDATED,
                             on_tinybms_register_updated, this);
    }

    // Persist energy state
    esp_err_t err = can_publisher_conversion_persist_energy_state();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Energy state persisted");
    } else {
        ESP_LOGW(TAG, "Failed to persist energy state: %s", esp_err_to_name(err));
    }

    // Clean up
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }

    event_bus_ = nullptr;

    // Print final metrics
    const PublishMetrics final_metrics = metrics_.get_metrics();
    ESP_LOGI(TAG, "Final metrics:");
    ESP_LOGI(TAG, "  Total publishes: %llu", final_metrics.total_publishes);
    ESP_LOGI(TAG, "  Successful: %llu", final_metrics.successful_publishes);
    ESP_LOGI(TAG, "  Failed: %llu", final_metrics.failed_publishes);
    ESP_LOGI(TAG, "  Throttled: %llu", final_metrics.throttled_publishes);
    ESP_LOGI(TAG, "  Cache hits: %llu (%.1f%%)",
             final_metrics.cache_hits,
             final_metrics.cache_hits + final_metrics.cache_misses > 0
                ? 100.0 * final_metrics.cache_hits / (final_metrics.cache_hits + final_metrics.cache_misses)
                : 0.0);
    ESP_LOGI(TAG, "  Avg latency: %.2fms, Max latency: %.2fms",
             final_metrics.avg_latency_ms, final_metrics.max_latency_ms);

    ESP_LOGI(TAG, "CAN Publisher Orchestrator deinitialized");
}

void Orchestrator::on_tinybms_register_updated(event_bus_t* bus,
                                               const event_t* event,
                                               void* ctx)
{
    (void)bus;
    (void)event;
    auto* orchestrator = static_cast<Orchestrator*>(ctx);
    if (orchestrator != nullptr) {
        orchestrator->handle_tinybms_update();
    }
}

void Orchestrator::handle_tinybms_update()
{
    if (!initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Callback received while not initialized");
        return;
    }

    const uint64_t now_ms = timestamp_ms();

    // Rate limiting with token bucket
    if (!rate_limiter_.try_consume(now_ms)) {
        metrics_.record_throttled();
        ESP_LOGV(TAG, "Publish throttled (no tokens available)");
        return;
    }

    // Circuit breaker check
    if (!circuit_breaker_.allow_request(now_ms)) {
        metrics_.record_throttled();
        ESP_LOGW(TAG, "Circuit breaker OPEN, blocking publish");
        return;
    }

    // Convert BMS data
    uart_bms_live_data_t bms_data;
    if (tinybms_adapter_convert(&bms_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to convert TinyBMS data");
        circuit_breaker_.record_failure();
        metrics_.record_publish_failure(now_ms);
        return;
    }

    ESP_LOGD(TAG, "BMS data: SOC=%.1f%%, V=%.2fV, I=%.2fA",
             bms_data.state_of_charge_pct,
             bms_data.pack_voltage_v,
             bms_data.pack_current_a);

    // Prepare CVL and energy tracking
    can_publisher_cvl_prepare(&bms_data);
    can_publisher_conversion_ingest_sample(&bms_data);

    // Publish all channels
    metrics_.record_publish_start(now_ms);
    const esp_err_t err = publish_all_channels(bms_data);

    if (err == ESP_OK) {
        circuit_breaker_.record_success();
        metrics_.record_publish_success(now_ms);
        ESP_LOGI(TAG, "CAN publish cycle completed successfully");
    } else {
        circuit_breaker_.record_failure();
        metrics_.record_publish_failure(now_ms);
        if (circuit_breaker_.state() == CircuitBreaker::State::OPEN) {
            metrics_.record_circuit_breaker_trip();
        }
        ESP_LOGW(TAG, "CAN publish cycle failed: %s", esp_err_to_name(err));
    }

    // Publish CVL limits event
    can_publisher_cvl_result_t cvl_result;
    if (can_publisher_cvl_get_latest(&cvl_result)) {
        cvl_limits_event_t limits_event = {
            .cvl_voltage_v = cvl_result.result.cvl_voltage_v,
            .ccl_current_a = cvl_result.result.ccl_limit_a,
            .dcl_current_a = cvl_result.result.dcl_limit_a,
            .cvl_state = cvl_result.result.state,
            .imbalance_hold_active = cvl_result.result.imbalance_hold_active,
            .cell_protection_active = cvl_result.result.cell_protection_active,
            .timestamp_ms = cvl_result.timestamp_ms
        };

        event_bus_event_t limits_evt = {
            .id = EVENT_CVL_LIMITS_UPDATED,
            .data = &limits_event,
            .data_size = sizeof(limits_event)
        };

        event_bus_publish(event_bus_, &limits_evt, pdMS_TO_TICKS(10));
    }
}

uint32_t Orchestrator::compute_data_hash(const uart_bms_live_data_t& bms_data) const
{
    return hash_bms_data(bms_data);
}

esp_err_t Orchestrator::publish_all_channels(const uart_bms_live_data_t& bms_data)
{
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    const uint32_t data_hash = compute_data_hash(bms_data);
    const uint64_t now_ms = timestamp_ms();
    size_t success_count = 0;
    size_t fail_count = 0;

    for (size_t i = 0; i < g_can_publisher_channel_count; i++) {
        const can_publisher_channel_t* channel = &g_can_publisher_channels[i];

        // Check cache first
        auto cached = frame_cache_.get(channel->can_id, data_hash);
        if (cached.has_value()) {
            metrics_.record_cache_hit();
            const esp_err_t err = can_victron_publish_frame(
                cached->can_id,
                cached->data.data(),
                cached->dlc,
                channel->description
            );

            if (err == ESP_OK) {
                success_count++;
                ESP_LOGV(TAG, "CAN 0x%03lX published (cached): %s",
                         static_cast<unsigned long>(cached->can_id), channel->description);
            } else {
                fail_count++;
                ESP_LOGW(TAG, "Failed to publish cached 0x%03lX: %s",
                         static_cast<unsigned long>(cached->can_id), esp_err_to_name(err));
            }
            continue;
        }

        metrics_.record_cache_miss();

        // Encode new frame
        can_publisher_frame_t frame = {0};
        if (channel->fill_fn && channel->fill_fn(&bms_data, &frame)) {
            const esp_err_t err = can_victron_publish_frame(
                frame.id,
                frame.data,
                frame.dlc,
                channel->description
            );

            if (err == ESP_OK) {
                success_count++;
                // Update cache
                frame_cache_.put(frame.id, frame.data, frame.dlc, data_hash, now_ms);
                ESP_LOGV(TAG, "CAN 0x%03lX published (new): %s",
                         static_cast<unsigned long>(frame.id), channel->description);
            } else {
                fail_count++;
                ESP_LOGW(TAG, "Failed to publish 0x%03lX: %s",
                         static_cast<unsigned long>(frame.id), esp_err_to_name(err));
            }
        }
    }

    ESP_LOGI(TAG, "Published %zu/%zu CAN frames (%zu failed)",
             success_count, g_can_publisher_channel_count, fail_count);

    return (fail_count == 0) ? ESP_OK : ESP_FAIL;
}

void Orchestrator::get_stats(uint32_t* publish_count, uint64_t* last_publish_ms)
{
    const PublishMetrics metrics = metrics_.get_metrics();
    if (publish_count != nullptr) {
        *publish_count = static_cast<uint32_t>(metrics.successful_publishes);
    }
    if (last_publish_ms != nullptr) {
        *last_publish_ms = metrics.last_publish_ms;
    }
}

PublishMetrics Orchestrator::get_detailed_metrics() const
{
    return metrics_.get_metrics();
}

} // namespace publisher
} // namespace can

// =============================================================================
// C API Wrappers (backward compatibility)
// =============================================================================

extern "C" {

void can_publisher_init(void)
{
    can::publisher::Orchestrator::instance().init();
}

void can_publisher_deinit(void)
{
    can::publisher::Orchestrator::instance().deinit();
}

void can_publisher_get_stats(uint32_t* publish_count, uint64_t* last_publish_ms)
{
    can::publisher::Orchestrator::instance().get_stats(publish_count, last_publish_ms);
}

} // extern "C"
