/**
 * @file diagnostic_logger_core.cpp
 * @brief Implementation of modern C++ diagnostic logger
 */

#include "diagnostic_logger_core.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>

extern "C" {
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
}

static const char* TAG = "diag_logger_core";

namespace diagnostic {

// =============================================================================
// Helper Functions
// =============================================================================

static uint64_t now_ms() {
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

// =============================================================================
// RLE Codec Implementation
// =============================================================================

size_t RleCodec::compress(const uint8_t* input, size_t len, uint8_t* output, size_t out_max) {
    if (input == nullptr || output == nullptr || len == 0) {
        return 0;
    }

    size_t i = 0;
    size_t out_len = 0;

    while (i < len) {
        uint8_t byte = input[i];
        size_t run = 1;

        while ((i + run) < len && input[i + run] == byte && run < 255) {
            run++;
        }

        if ((out_len + 2) > out_max) {
            return 0; // Not enough space
        }

        output[out_len++] = byte;
        output[out_len++] = static_cast<uint8_t>(run);
        i += run;
    }

    return out_len;
}

size_t RleCodec::decompress(const uint8_t* input, size_t len, uint8_t* output, size_t out_max) {
    if (input == nullptr || output == nullptr || len == 0 || len % 2 != 0) {
        return 0;
    }

    size_t out_len = 0;

    for (size_t i = 0; i < len; i += 2) {
        uint8_t byte = input[i];
        uint8_t run = input[i + 1];

        if (out_len + run > out_max) {
            return 0; // Not enough space
        }

        std::memset(&output[out_len], byte, run);
        out_len += run;
    }

    return out_len;
}

// =============================================================================
// RingBuffer Implementation
// =============================================================================

RingBuffer::RingBuffer() {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create ring buffer mutex");
        healthy_.store(false, std::memory_order_release);
    }
}

bool RingBuffer::append(diag_log_source_t source, std::string_view message) {
    if (message.empty() || message.size() >= config::kMaxPayloadSize) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGW(TAG, "Message dropped (empty or too long: %zu bytes)", message.size());
        return false;
    }

    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGE(TAG, "Failed to acquire ring buffer mutex");
        return false;
    }

    // Try RLE compression
    uint8_t compressed[config::kMaxPayloadSize];
    size_t compressed_len = RleCodec::compress(
        reinterpret_cast<const uint8_t*>(message.data()),
        message.size(),
        compressed,
        sizeof(compressed)
    );

    LogEntry entry;
    entry.timestamp_ms = now_ms();
    entry.sequence = next_sequence_++;
    entry.source = static_cast<uint8_t>(source);
    entry.original_len = static_cast<uint16_t>(message.size());

    if (compressed_len > 0 && compressed_len < message.size()) {
        // Compression successful
        entry.stored_len = static_cast<uint16_t>(compressed_len);
        entry.compression = 1; // RLE
        std::memcpy(entry.payload, compressed, compressed_len);
    } else {
        // Store uncompressed
        entry.stored_len = static_cast<uint16_t>(message.size());
        entry.compression = 0; // None
        std::memcpy(entry.payload, message.data(), message.size());
    }

    // Add to ring buffer
    const uint32_t idx = head_ % config::kMaxEntries;
    entries_[idx] = entry;
    head_ = (head_ + 1) % config::kMaxEntries;

    if (count_ < config::kMaxEntries) {
        count_++;
    }

    return true;
}

std::optional<LogEntry> RingBuffer::get(uint32_t index) const {
    ScopedMutex lock(mutex_);
    if (!lock.is_locked() || index >= count_) {
        return std::nullopt;
    }

    const uint32_t actual_idx = (head_ + config::kMaxEntries - count_ + index) % config::kMaxEntries;
    return entries_[actual_idx];
}

uint32_t RingBuffer::count() const {
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        return 0;
    }
    return count_;
}

RingBuffer::Snapshot RingBuffer::take_snapshot() const {
    ScopedMutex lock(mutex_);

    Snapshot snapshot;
    snapshot.head = head_;
    snapshot.count = count_;
    snapshot.next_sequence = next_sequence_;
    snapshot.dropped = dropped_.load(std::memory_order_relaxed);
    snapshot.healthy = healthy_.load(std::memory_order_acquire);
    snapshot.entries = entries_;

    return snapshot;
}

void RingBuffer::restore_snapshot(const Snapshot& snapshot) {
    ScopedMutex lock(mutex_);
    if (!lock.is_locked()) {
        ESP_LOGE(TAG, "Failed to restore snapshot: mutex timeout");
        return;
    }

    head_ = snapshot.head;
    count_ = snapshot.count;
    next_sequence_ = snapshot.next_sequence;
    dropped_.store(snapshot.dropped, std::memory_order_relaxed);
    healthy_.store(snapshot.healthy, std::memory_order_release);
    entries_ = snapshot.entries;

    ESP_LOGI(TAG, "Restored snapshot: %u entries, %u dropped", count_, snapshot.dropped);
}

// =============================================================================
// NvsPersister Implementation
// =============================================================================

esp_err_t NvsPersister::save(const RingBuffer::Snapshot& snapshot) {
    esp_err_t err = save_impl(snapshot);

    if (err == ESP_OK) {
        save_count_.fetch_add(1, std::memory_order_relaxed);
        return ESP_OK;
    }

    // Retry on failure
    for (uint32_t i = 0; i < config::kMaxRetries; ++i) {
        retry_count_.fetch_add(1, std::memory_order_relaxed);
        vTaskDelay(pdMS_TO_TICKS(config::kRetryDelayMs * (1 << i))); // Exponential backoff

        err = save_impl(snapshot);
        if (err == ESP_OK) {
            save_count_.fetch_add(1, std::memory_order_relaxed);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Failed to save after %u retries: %s", config::kMaxRetries, esp_err_to_name(err));
    return err;
}

esp_err_t NvsPersister::load(RingBuffer::Snapshot& snapshot) {
    return load_impl(snapshot);
}

esp_err_t NvsPersister::save_impl(const RingBuffer::Snapshot& snapshot) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(config::kNvsNamespace.data(), NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, config::kNvsKey.data(), &snapshot, sizeof(snapshot));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist snapshot: %s", esp_err_to_name(err));
    }

    return err;
}

esp_err_t NvsPersister::load_impl(RingBuffer::Snapshot& snapshot) {
    nvs_handle_t handle;
    size_t size = sizeof(snapshot);

    esp_err_t err = nvs_open(config::kNvsNamespace.data(), NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing diagnostic log storage: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(handle, config::kNvsKey.data(), &snapshot, &size);
    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(snapshot)) {
        ESP_LOGW(TAG, "Invalid diagnostic log storage (err=%s, size=%zu)",
                 esp_err_to_name(err), size);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// =============================================================================
// FlushManager Implementation
// =============================================================================

void FlushManager::record_write() {
    pending_writes_.fetch_add(1, std::memory_order_relaxed);
}

bool FlushManager::should_flush(uint64_t now_ms) const {
    const uint32_t pending = pending_writes_.load(std::memory_order_relaxed);
    if (pending >= config::kBatchFlushThreshold) {
        return true;
    }

    const uint64_t last_flush = last_flush_ms_.load(std::memory_order_relaxed);
    const uint64_t elapsed = now_ms - last_flush;

    return (pending > 0 && elapsed >= config::kTimeFlushThresholdMs);
}

void FlushManager::mark_flushed(uint64_t now_ms) {
    pending_writes_.store(0, std::memory_order_relaxed);
    last_flush_ms_.store(now_ms, std::memory_order_relaxed);
}

// =============================================================================
// Logger Implementation
// =============================================================================

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (initialized_.load(std::memory_order_acquire)) {
        flush(); // Final flush on destruction
    }
}

esp_err_t Logger::init(event_bus_t* bus) {
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Logger already initialized");
        return ESP_OK;
    }

    bus_ = bus;

    // Load existing logs from NVS
    load_from_nvs();

    // Subscribe to events
    event_bus_subscribe(bus, EVENT_TINYBMS_UART_LOG, handle_uart_log, this);
    event_bus_subscribe(bus, EVENT_TINYBMS_REGISTER_UPDATED, handle_register_update, this);
    event_bus_subscribe(bus, EVENT_TINYBMS_STATS_UPDATED, handle_stats_update, this);

    initialized_.store(true, std::memory_order_release);

    ESP_LOGI(TAG, "Diagnostic logger initialized (entries=%u, dropped=%u)",
             ring_.count(), ring_.dropped());
    return ESP_OK;
}

void Logger::append(diag_log_source_t source, std::string_view message) {
    if (!initialized_.load(std::memory_order_acquire)) {
        ESP_LOGW(TAG, "Logger not initialized");
        return;
    }

    if (ring_.append(source, message)) {
        stats_.entries_written.fetch_add(1, std::memory_order_relaxed);
        stats_.compression_success.fetch_add(1, std::memory_order_relaxed);
        flush_manager_.record_write();
        maybe_flush();
    } else {
        stats_.entries_dropped.fetch_add(1, std::memory_order_relaxed);
    }
}

void Logger::flush() {
    const uint64_t now = now_ms();

    auto snapshot = ring_.take_snapshot();
    esp_err_t err = persister_.save(snapshot);

    if (err == ESP_OK) {
        stats_.nvs_saves.fetch_add(1, std::memory_order_relaxed);
        flush_manager_.mark_flushed(now);
        ESP_LOGD(TAG, "Flushed %u entries to NVS", snapshot.count);
    } else {
        stats_.nvs_failures.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGE(TAG, "Failed to flush to NVS: %s", esp_err_to_name(err));
    }
}

void Logger::maybe_flush() {
    const uint64_t now = now_ms();

    if (flush_manager_.should_flush(now)) {
        flush();
    }
}

void Logger::load_from_nvs() {
    RingBuffer::Snapshot snapshot;
    esp_err_t err = persister_.load(snapshot);

    if (err == ESP_OK) {
        ring_.restore_snapshot(snapshot);
        ESP_LOGI(TAG, "Loaded diagnostic logs from NVS");
    } else {
        ESP_LOGI(TAG, "No previous diagnostic logs found, starting fresh");
    }
}

void Logger::save_to_nvs() {
    flush();
}

Logger::Metrics Logger::get_metrics() const {
    Metrics m;
    m.ring_used = ring_.count();
    m.ring_capacity = ring_.capacity();
    m.ring_dropped = ring_.dropped();
    m.ring_healthy = ring_.is_healthy();
    m.pending_writes = flush_manager_.pending_writes();
    m.entries_written = stats_.entries_written.load(std::memory_order_relaxed);
    m.entries_dropped = stats_.entries_dropped.load(std::memory_order_relaxed);
    m.nvs_saves = stats_.nvs_saves.load(std::memory_order_relaxed);
    m.nvs_failures = stats_.nvs_failures.load(std::memory_order_relaxed);
    m.compression_success = stats_.compression_success.load(std::memory_order_relaxed);
    m.compression_failures = stats_.compression_failures.load(std::memory_order_relaxed);

    // Get event bus status (using C API)
    m.event_bus_status = diagnostic_logger_get_status();

    return m;
}

// =============================================================================
// Event Handlers
// =============================================================================

void Logger::handle_uart_log(event_bus_t* bus, const event_t* event, void* user_ctx) {
    (void)bus;

    if (event == nullptr || event->data == nullptr || user_ctx == nullptr) {
        return;
    }

    auto* logger = static_cast<Logger*>(user_ctx);
    const auto* entry = static_cast<const tinybms_uart_log_entry_t*>(event->data);

    char message[config::kMaxPayloadSize];
    int len = snprintf(message, sizeof(message),
                      "UART action=%s addr=0x%04X res=%d msg=%s",
                      entry->action, entry->address, entry->result, entry->message);

    if (len > 0 && len < static_cast<int>(sizeof(message))) {
        logger->append(DIAG_LOG_SOURCE_UART, std::string_view(message, len));
    }
}

void Logger::handle_register_update(event_bus_t* bus, const event_t* event, void* user_ctx) {
    (void)bus;

    if (event == nullptr || event->data == nullptr || user_ctx == nullptr) {
        return;
    }

    auto* logger = static_cast<Logger*>(user_ctx);
    const auto* update = static_cast<const tinybms_register_update_t*>(event->data);

    char message[config::kMaxPayloadSize];
    int len = snprintf(message, sizeof(message),
                      "Reg %s=%.3f (0x%04X)",
                      update->key, update->user_value, update->raw_value);

    if (len > 0 && len < static_cast<int>(sizeof(message))) {
        logger->append(DIAG_LOG_SOURCE_RS485, std::string_view(message, len));
    }
}

void Logger::handle_stats_update(event_bus_t* bus, const event_t* event, void* user_ctx) {
    (void)bus;

    if (event == nullptr || event->data == nullptr || user_ctx == nullptr) {
        return;
    }

    auto* logger = static_cast<Logger*>(user_ctx);
    const auto* stats_evt = static_cast<const tinybms_stats_event_t*>(event->data);

    char message[config::kMaxPayloadSize];
    int len = snprintf(message, sizeof(message),
                      "stats ok_r=%lu ok_w=%lu crc=%lu timeouts=%lu nacks=%lu retries=%lu",
                      (unsigned long)stats_evt->stats.reads_ok,
                      (unsigned long)stats_evt->stats.writes_ok,
                      (unsigned long)stats_evt->stats.crc_errors,
                      (unsigned long)stats_evt->stats.timeouts,
                      (unsigned long)stats_evt->stats.nacks,
                      (unsigned long)stats_evt->stats.retries);

    if (len > 0 && len < static_cast<int>(sizeof(message))) {
        logger->append(DIAG_LOG_SOURCE_UART, std::string_view(message, len));
    }
}

} // namespace diagnostic

// =============================================================================
// C API Wrappers Implementation
// =============================================================================

extern "C" {

diagnostic::Logger* diagnostic_logger_get_cpp_instance() {
    return &diagnostic::Logger::instance();
}

} // extern "C"
