// main/stats_aggregator.cpp
#include "stats_aggregator.h"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <memory>

#include "event_types.h"
#include "net_client.h"

namespace {

constexpr uint32_t STATS_TASK_PERIOD_MS = 60000;
constexpr uint64_t HOUR_MS = 60ULL * 60ULL * 1000ULL;
constexpr uint64_t DAY_MS = 24ULL * HOUR_MS;

const char *TAG = "STATS_AGG";

struct StatAccumulator {
    float    min = FLT_MAX;
    float    max = -FLT_MAX;
    double   sum = 0.0;
    uint32_t count = 0;

    void reset()
    {
        min = FLT_MAX;
        max = -FLT_MAX;
        sum = 0.0;
        count = 0;
    }

    void accumulate(float value)
    {
        if (count == 0) {
            min = value;
            max = value;
        } else {
            if (value < min) {
                min = value;
            }
            if (value > max) {
                max = value;
            }
        }
        sum += value;
        ++count;
    }

    float average() const
    {
        if (count == 0) {
            return 0.0f;
        }
        return static_cast<float>(sum / static_cast<double>(count));
    }
};

struct StatsBucket {
    StatAccumulator voltage;
    StatAccumulator current;
    StatAccumulator temperature;
    StatAccumulator soc;
    StatAccumulator cell_min;
    StatAccumulator cell_max;
    StatAccumulator cell_delta;
    uint32_t       cycle_count = 0;
    uint32_t       balancing_events = 0;
    uint32_t       comm_errors = 0;
    uint64_t       start_ms = 0;
    uint64_t       end_ms = 0;

    void reset(uint64_t start)
    {
        voltage.reset();
        current.reset();
        temperature.reset();
        soc.reset();
        cell_min.reset();
        cell_max.reset();
        cell_delta.reset();
        cycle_count = 0;
        balancing_events = 0;
        comm_errors = 0;
        start_ms = start;
        end_ms = start;
    }

    bool empty() const
    {
        return voltage.count == 0;
    }

    void merge(const StatsBucket &src)
    {
        if (src.empty()) {
            return;
        }

        const StatAccumulator *src_accs[] = {
            &src.voltage, &src.current, &src.temperature, &src.soc,
            &src.cell_min, &src.cell_max, &src.cell_delta,
        };
        StatAccumulator *dst_accs[] = {
            &voltage, &current, &temperature, &soc,
            &cell_min, &cell_max, &cell_delta,
        };

        for (size_t i = 0; i < sizeof(src_accs) / sizeof(src_accs[0]); ++i) {
            if (src_accs[i]->count == 0) {
                continue;
            }
            if (dst_accs[i]->count == 0) {
                *dst_accs[i] = *src_accs[i];
            } else {
                if (src_accs[i]->min < dst_accs[i]->min) {
                    dst_accs[i]->min = src_accs[i]->min;
                }
                if (src_accs[i]->max > dst_accs[i]->max) {
                    dst_accs[i]->max = src_accs[i]->max;
                }
                dst_accs[i]->sum += src_accs[i]->sum;
                dst_accs[i]->count += src_accs[i]->count;
            }
        }

        if (start_ms == 0 || src.start_ms < start_ms) {
            start_ms = src.start_ms;
        }
        if (src.end_ms > end_ms) {
            end_ms = src.end_ms;
        }

        cycle_count += src.cycle_count;
        balancing_events += src.balancing_events;
        comm_errors += src.comm_errors;
    }

    stats_summary_t to_summary() const
    {
        stats_summary_t summary = {0};
        if (empty()) {
            return summary;
        }
        summary.min_value = voltage.min;
        summary.max_value = voltage.max;
        summary.avg_value = voltage.average();
        summary.sample_count = voltage.count;
        summary.cycle_count = cycle_count;
        summary.balancing_events = balancing_events;
        summary.comm_errors = comm_errors;
        summary.period_start_ms = start_ms;
        summary.period_end_ms = end_ms;
        return summary;
    }
};

uint64_t now_ms()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

struct CJsonDeleter {
    void operator()(cJSON *ptr) const noexcept
    {
        if (ptr) {
            cJSON_Delete(ptr);
        }
    }
};

using UniqueCJson = std::unique_ptr<cJSON, CJsonDeleter>;

struct CStringDeleter {
    void operator()(char *ptr) const noexcept
    {
        if (ptr) {
            cJSON_free(ptr);
        }
    }
};

using UniqueCString = std::unique_ptr<char, CStringDeleter>;

class StatsAggregator {
public:
    static StatsAggregator &instance()
    {
        static StatsAggregator inst;
        return inst;
    }

    esp_err_t init(event_bus_t *bus)
    {
        if (initialized_) {
            return ESP_OK;
        }
        if (!bus) {
            return ESP_ERR_INVALID_ARG;
        }

        bus_ = bus;
        reset_state_buffers(now_ms());

        event_bus_subscribe(bus_, EVENT_BATTERY_STATUS_UPDATED, &StatsAggregator::handle_battery, this);
        event_bus_subscribe(bus_, EVENT_PACK_STATS_UPDATED, &StatsAggregator::handle_pack, this);
        event_bus_subscribe(bus_, EVENT_SYSTEM_STATUS_UPDATED, &StatsAggregator::handle_system, this);

        initialized_ = true;
        ESP_LOGI(TAG, "Stats aggregator initialized");
        return ESP_OK;
    }

    esp_err_t start()
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }
        if (task_) {
            return ESP_OK;
        }

        BaseType_t ok = xTaskCreate(&StatsAggregator::task_trampoline, "stats_aggregator", 4096, this, 5, &task_);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Unable to create stats task");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Stats aggregator task started");
        return ESP_OK;
    }

    esp_err_t export_to_flash()
    {
        if (!initialized_) {
            return ESP_ERR_INVALID_STATE;
        }

        const char *fw = get_firmware_version();
        StatsBucket day = aggregate_window(hourly_);
        day.merge(current_hour_);
        StatsBucket week = aggregate_window(daily_);
        week.merge(current_day_);

        constexpr const char *csv_path = "/spiflash/stats_summary.csv";
        constexpr const char *json_path = "/spiflash/stats_summary.json";

        FILE *csv = fopen(csv_path, "w");
        if (csv) {
            fprintf(csv, "period,start_ms,end_ms,v_min,v_max,v_avg,t_min,t_max,t_avg,cycles,balancing,comm_errors,fw\n");
            export_summary_to_csv(csv, "24h", day, fw);
            export_summary_to_csv(csv, "7d", week, fw);
            fclose(csv);
        } else {
            ESP_LOGE(TAG, "Failed to open %s", csv_path);
        }

        UniqueCJson root(cJSON_CreateObject());
        if (!root) {
            ESP_LOGE(TAG, "Failed to allocate JSON root object");
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(root.get(), "firmware", fw ? fw : "");
        export_summary_to_json(root.get(), "day", day, fw);
        export_summary_to_json(root.get(), "week", week, fw);

        FILE *json = fopen(json_path, "w");
        if (json) {
            UniqueCString rendered(cJSON_PrintUnformatted(root.get()));
            if (rendered) {
                fprintf(json, "%s", rendered.get());
            }
            fclose(json);
        } else {
            ESP_LOGE(TAG, "Failed to open %s", json_path);
        }

        if (pdf_renderer_ && pdf_output_path_[0] != '\0') {
            stats_summary_t day_summary = day.to_summary();
            stats_summary_t week_summary = week.to_summary();
            esp_err_t err = pdf_renderer_(&day_summary, &week_summary, pdf_output_path_.data());
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "PDF renderer returned error %s", esp_err_to_name(err));
            }
        }

        return ESP_OK;
    }

    bool send_http(const char *path)
    {
        if (!initialized_) {
            ESP_LOGE(TAG, "Stats aggregator not initialized, cannot send HTTP");
            return false;
        }
        if (!path || path[0] == '\0') {
            ESP_LOGE(TAG, "Invalid HTTP path for stats export");
            return false;
        }

        const char *fw = get_firmware_version();
        StatsBucket day = aggregate_window(hourly_);
        day.merge(current_hour_);
        StatsBucket week = aggregate_window(daily_);
        week.merge(current_day_);

        UniqueCJson root(cJSON_CreateObject());
        if (!root) {
            ESP_LOGE(TAG, "Failed to allocate JSON root object");
            return false;
        }
        cJSON_AddStringToObject(root.get(), "firmware", fw ? fw : "");
        export_summary_to_json(root.get(), "day", day, fw);
        export_summary_to_json(root.get(), "week", week, fw);

        UniqueCString payload(cJSON_PrintUnformatted(root.get()));
        if (!payload) {
            return false;
        }

        return net_client_send_http_request(path, "POST", payload.get(), strlen(payload.get()));
    }

    void set_pdf_renderer(stats_pdf_renderer_t renderer, const char *output_path)
    {
        pdf_renderer_ = renderer;
        pdf_output_path_.fill('\0');
        if (output_path) {
            strncpy(pdf_output_path_.data(), output_path, pdf_output_path_.size() - 1);
            pdf_output_path_.back() = '\0';
        }
    }

private:
    using HourlyBuckets = std::array<StatsBucket, 24>;
    using DailyBuckets = std::array<StatsBucket, 7>;

    static void task_trampoline(void *arg)
    {
        auto *self = static_cast<StatsAggregator *>(arg);
        self->task_loop();
    }

    static void handle_battery(event_bus_t *bus, const event_t *event, void *user_ctx)
    {
        (void) bus;
        if (!event || !event->data) {
            return;
        }
        auto *self = static_cast<StatsAggregator *>(user_ctx);
        self->update_from_battery(static_cast<const battery_status_t *>(event->data));
    }

    static void handle_pack(event_bus_t *bus, const event_t *event, void *user_ctx)
    {
        (void) bus;
        if (!event || !event->data) {
            return;
        }
        auto *self = static_cast<StatsAggregator *>(user_ctx);
        self->update_from_pack(static_cast<const pack_stats_t *>(event->data));
    }

    static void handle_system(event_bus_t *bus, const event_t *event, void *user_ctx)
    {
        (void) bus;
        if (!event || !event->data) {
            return;
        }
        auto *self = static_cast<StatsAggregator *>(user_ctx);
        self->update_from_system(static_cast<const system_status_t *>(event->data));
    }

    void task_loop()
    {
        uint64_t last_hour_roll = now_ms();
        uint64_t last_day_roll = last_hour_roll;

        while (true) {
            vTaskDelay(pdMS_TO_TICKS(STATS_TASK_PERIOD_MS));
            uint64_t ts = now_ms();

            if (ts - last_hour_roll >= HOUR_MS) {
                roll_hour_bucket(ts);
                last_hour_roll = ts;
            }
            if (ts - last_day_roll >= DAY_MS) {
                roll_day_bucket(ts);
                last_day_roll = ts;
            }
        }
    }

    void reset_state_buffers(uint64_t start_ms)
    {
        for (auto &bucket : hourly_) {
            bucket.reset(start_ms);
        }
        for (auto &bucket : daily_) {
            bucket.reset(start_ms);
        }
        current_hour_.reset(start_ms);
        current_day_.reset(start_ms);
    }

    void roll_hour_bucket(uint64_t timestamp_ms)
    {
        for (size_t i = hourly_.size() - 1; i > 0; --i) {
            hourly_[i] = hourly_[i - 1];
        }
        hourly_[0] = current_hour_;
        current_hour_.reset(timestamp_ms);
    }

    void roll_day_bucket(uint64_t timestamp_ms)
    {
        for (size_t i = daily_.size() - 1; i > 0; --i) {
            daily_[i] = daily_[i - 1];
        }
        daily_[0] = current_day_;
        current_day_.reset(timestamp_ms);
    }

    void accumulate_parallel(StatAccumulator *const *hour_accs,
                             StatAccumulator *const *day_accs,
                             const float *values,
                             size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            if (hour_accs[i]) {
                hour_accs[i]->accumulate(values[i]);
            }
            if (day_accs[i]) {
                day_accs[i]->accumulate(values[i]);
            }
        }
    }

    void update_from_battery(const battery_status_t *batt)
    {
        if (!batt) {
            return;
        }

        const float values[] = {batt->voltage, batt->current, batt->temperature, batt->soc};
        StatAccumulator *hour_accs[] = {
            &current_hour_.voltage,
            &current_hour_.current,
            &current_hour_.temperature,
            &current_hour_.soc,
        };
        StatAccumulator *day_accs[] = {
            &current_day_.voltage,
            &current_day_.current,
            &current_day_.temperature,
            &current_day_.soc,
        };
        accumulate_parallel(hour_accs, day_accs, values, sizeof(values) / sizeof(values[0]));

        current_hour_.cycle_count++;
        current_day_.cycle_count++;
        uint64_t ts = now_ms();
        current_hour_.end_ms = ts;
        current_day_.end_ms = ts;
    }

    void update_from_pack(const pack_stats_t *pack)
    {
        if (!pack) {
            return;
        }

        const float values[] = {pack->cell_min, pack->cell_max, pack->cell_delta};
        StatAccumulator *hour_accs[] = {
            &current_hour_.cell_min,
            &current_hour_.cell_max,
            &current_hour_.cell_delta,
        };
        StatAccumulator *day_accs[] = {
            &current_day_.cell_min,
            &current_day_.cell_max,
            &current_day_.cell_delta,
        };
        accumulate_parallel(hour_accs, day_accs, values, sizeof(values) / sizeof(values[0]));

        uint32_t balancing_count = 0;
        for (uint8_t i = 0; i < pack->cell_count && i < PACK_MAX_CELLS; ++i) {
            if (pack->balancing[i]) {
                ++balancing_count;
            }
        }
        current_hour_.balancing_events += balancing_count;
        current_day_.balancing_events += balancing_count;
        uint64_t ts = now_ms();
        current_hour_.end_ms = ts;
        current_day_.end_ms = ts;
    }

    void update_from_system(const system_status_t *status)
    {
        if (!status) {
            return;
        }
        if (status->network_state == NETWORK_STATE_ERROR || !status->wifi_connected || !status->storage_ok) {
            current_hour_.comm_errors++;
            current_day_.comm_errors++;
        }
    }

    template <size_t N>
    StatsBucket aggregate_window(const std::array<StatsBucket, N> &buckets) const
    {
        StatsBucket agg;
        agg.reset(0);
        for (const auto &bucket : buckets) {
            agg.merge(bucket);
        }
        return agg;
    }

    void export_summary_to_csv(FILE *f, const char *label, const StatsBucket &bucket, const char *fw_version)
    {
        if (!f || bucket.empty()) {
            return;
        }

        double avg_voltage = bucket.voltage.count ? bucket.voltage.sum / bucket.voltage.count : 0.0;
        double avg_temp = bucket.temperature.count ? bucket.temperature.sum / bucket.temperature.count : 0.0;

        fprintf(f, "%s,%llu,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%s\n",
                label,
                static_cast<unsigned long long>(bucket.start_ms),
                static_cast<unsigned long long>(bucket.end_ms),
                static_cast<double>(bucket.voltage.min),
                static_cast<double>(bucket.voltage.max),
                avg_voltage,
                static_cast<double>(bucket.temperature.min),
                static_cast<double>(bucket.temperature.max),
                avg_temp,
                bucket.cycle_count,
                bucket.balancing_events,
                bucket.comm_errors,
                fw_version ? fw_version : "");

        fprintf(f, "%s_cells,%llu,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%s\n",
                label,
                static_cast<unsigned long long>(bucket.start_ms),
                static_cast<unsigned long long>(bucket.end_ms),
                static_cast<double>(bucket.cell_min.min),
                static_cast<double>(bucket.cell_max.max),
                bucket.cell_min.count ? bucket.cell_min.sum / bucket.cell_min.count : 0.0,
                static_cast<double>(bucket.cell_delta.min),
                static_cast<double>(bucket.cell_delta.max),
                bucket.cell_delta.count ? bucket.cell_delta.sum / bucket.cell_delta.count : 0.0,
                bucket.cycle_count,
                bucket.balancing_events,
                bucket.comm_errors,
                fw_version ? fw_version : "");
    }

    void export_summary_to_json(cJSON *root, const char *label, const StatsBucket &bucket, const char *fw_version)
    {
        if (!root || bucket.empty()) {
            return;
        }

        cJSON *obj = cJSON_CreateObject();
        if (!obj) {
            return;
        }

        cJSON_AddNumberToObject(obj, "start_ms", static_cast<double>(bucket.start_ms));
        cJSON_AddNumberToObject(obj, "end_ms", static_cast<double>(bucket.end_ms));
        cJSON_AddNumberToObject(obj, "voltage_min", bucket.voltage.min);
        cJSON_AddNumberToObject(obj, "voltage_max", bucket.voltage.max);
        cJSON_AddNumberToObject(obj, "voltage_avg", bucket.voltage.count ? bucket.voltage.sum / bucket.voltage.count : 0.0);
        cJSON_AddNumberToObject(obj, "temperature_min", bucket.temperature.min);
        cJSON_AddNumberToObject(obj, "temperature_max", bucket.temperature.max);
        cJSON_AddNumberToObject(obj, "temperature_avg", bucket.temperature.count ? bucket.temperature.sum / bucket.temperature.count : 0.0);
        cJSON_AddNumberToObject(obj, "soc_min", bucket.soc.min);
        cJSON_AddNumberToObject(obj, "soc_max", bucket.soc.max);
        cJSON_AddNumberToObject(obj, "soc_avg", bucket.soc.count ? bucket.soc.sum / bucket.soc.count : 0.0);
        cJSON_AddNumberToObject(obj, "cell_min", bucket.cell_min.min);
        cJSON_AddNumberToObject(obj, "cell_max", bucket.cell_max.max);
        cJSON_AddNumberToObject(obj, "cell_delta_min", bucket.cell_delta.min);
        cJSON_AddNumberToObject(obj, "cell_delta_max", bucket.cell_delta.max);
        cJSON_AddNumberToObject(obj, "cell_delta_avg", bucket.cell_delta.count ? bucket.cell_delta.sum / bucket.cell_delta.count : 0.0);
        cJSON_AddNumberToObject(obj, "cycle_count", bucket.cycle_count);
        cJSON_AddNumberToObject(obj, "balancing_events", bucket.balancing_events);
        cJSON_AddNumberToObject(obj, "comm_errors", bucket.comm_errors);
        cJSON_AddStringToObject(obj, "firmware", fw_version ? fw_version : "");

        cJSON_AddItemToObject(root, label, obj);
    }

    const char *get_firmware_version()
    {
        if (firmware_version_[0] == '\0') {
            const esp_app_desc_t *app = esp_app_get_description();
            if (app) {
                strncpy(firmware_version_.data(), app->version, firmware_version_.size() - 1);
                firmware_version_.back() = '\0';
            }
        }
        return firmware_version_.data();
    }

    event_bus_t       *bus_ = nullptr;
    TaskHandle_t       task_ = nullptr;
    HourlyBuckets      hourly_ = {};
    DailyBuckets       daily_ = {};
    StatsBucket        current_hour_;
    StatsBucket        current_day_;
    stats_pdf_renderer_t pdf_renderer_ = nullptr;
    std::array<char, 64> pdf_output_path_ = {0};
    bool               initialized_ = false;
    std::array<char, 32> firmware_version_ = {0};
};

} // namespace

extern "C" {

esp_err_t stats_aggregator_init(event_bus_t *bus)
{
    return StatsAggregator::instance().init(bus);
}

esp_err_t stats_aggregator_start(void)
{
    return StatsAggregator::instance().start();
}

esp_err_t stats_aggregator_export_to_flash(void)
{
    return StatsAggregator::instance().export_to_flash();
}

bool stats_aggregator_send_http(const char *path)
{
    return StatsAggregator::instance().send_http(path);
}

void stats_aggregator_set_pdf_renderer(stats_pdf_renderer_t renderer, const char *output_path)
{
    StatsAggregator::instance().set_pdf_renderer(renderer, output_path);
}

} // extern "C"
