// main/stats_aggregator.c
#include "stats_aggregator.h"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <float.h>
#include <stdio.h>
#include <string.h>

#include "event_types.h"
#include "net_client.h"

#define STATS_TASK_PERIOD_MS 60000
#define HOUR_MS (60ULL * 60ULL * 1000ULL)
#define DAY_MS  (24ULL * HOUR_MS)

static const char *TAG = "STATS_AGG";

typedef struct {
    float    min;
    float    max;
    double   sum;
    uint32_t count;
} stat_accumulator_t;

typedef struct {
    stat_accumulator_t voltage;
    stat_accumulator_t current;
    stat_accumulator_t temperature;
    stat_accumulator_t soc;
    stat_accumulator_t cell_min;
    stat_accumulator_t cell_max;
    stat_accumulator_t cell_delta;
    uint32_t           cycle_count;
    uint32_t           balancing_events;
    uint32_t           comm_errors;
    uint64_t           start_ms;
    uint64_t           end_ms;
} stats_bucket_t;

typedef struct {
    event_bus_t        *bus;
    TaskHandle_t        task;
    stats_bucket_t      hourly[24];
    stats_bucket_t      daily[7];
    stats_bucket_t      current_hour;
    stats_bucket_t      current_day;
    stats_pdf_renderer_t pdf_renderer;
    char                pdf_output_path[64];
    bool                initialized;
} stats_state_t;

static stats_state_t s_state = { 0 };

static uint64_t now_ms(void)
{
    return (uint64_t) (esp_timer_get_time() / 1000ULL);
}

static void reset_bucket(stats_bucket_t *bucket, uint64_t start_ms)
{
    if (!bucket) {
        return;
    }
    memset(bucket, 0, sizeof(*bucket));
    bucket->start_ms = start_ms;
    bucket->end_ms = start_ms;
    bucket->voltage.min = bucket->current.min = bucket->temperature.min = bucket->soc.min = bucket->cell_min.min = bucket->cell_max.min = bucket->cell_delta.min = FLT_MAX;
    bucket->voltage.max = bucket->current.max = bucket->temperature.max = bucket->soc.max = bucket->cell_min.max = bucket->cell_max.max = bucket->cell_delta.max = -FLT_MAX;
}

static void reset_state_buffers(uint64_t start_ms)
{
    for (size_t i = 0; i < 24; ++i) {
        reset_bucket(&s_state.hourly[i], start_ms);
    }
    for (size_t i = 0; i < 7; ++i) {
        reset_bucket(&s_state.daily[i], start_ms);
    }
    reset_bucket(&s_state.current_hour, start_ms);
    reset_bucket(&s_state.current_day, start_ms);
}

static void accumulate(stat_accumulator_t *acc, float value)
{
    if (!acc) {
        return;
    }
    if (acc->count == 0) {
        acc->min = value;
        acc->max = value;
    } else {
        if (value < acc->min) acc->min = value;
        if (value > acc->max) acc->max = value;
    }
    acc->sum += value;
    acc->count++;
}

static void merge_bucket(stats_bucket_t *dest, const stats_bucket_t *src)
{
    if (!dest || !src || src->voltage.count == 0) {
        return;
    }

    const stat_accumulator_t *src_acc[] = {
        &src->voltage, &src->current, &src->temperature, &src->soc,
        &src->cell_min, &src->cell_max, &src->cell_delta
    };
    stat_accumulator_t *dst_acc[] = {
        &dest->voltage, &dest->current, &dest->temperature, &dest->soc,
        &dest->cell_min, &dest->cell_max, &dest->cell_delta
    };

    for (size_t i = 0; i < sizeof(src_acc) / sizeof(src_acc[0]); ++i) {
        if (src_acc[i]->count == 0) {
            continue;
        }
        if (dst_acc[i]->count == 0) {
            *dst_acc[i] = *src_acc[i];
        } else {
            if (src_acc[i]->min < dst_acc[i]->min) dst_acc[i]->min = src_acc[i]->min;
            if (src_acc[i]->max > dst_acc[i]->max) dst_acc[i]->max = src_acc[i]->max;
            dst_acc[i]->sum += src_acc[i]->sum;
            dst_acc[i]->count += src_acc[i]->count;
        }
    }

    if (dest->start_ms == 0 || src->start_ms < dest->start_ms) {
        dest->start_ms = src->start_ms;
    }
    if (src->end_ms > dest->end_ms) {
        dest->end_ms = src->end_ms;
    }

    dest->cycle_count += src->cycle_count;
    dest->balancing_events += src->balancing_events;
    dest->comm_errors += src->comm_errors;
}

static stats_summary_t bucket_to_summary(const stats_bucket_t *bucket)
{
    stats_summary_t s = {0};
    if (!bucket) {
        return s;
    }

    const stat_accumulator_t *acc = &bucket->voltage;
    if (acc->count > 0) {
        s.min_value = acc->min;
        s.max_value = acc->max;
        s.avg_value = (float) (acc->sum / (double) acc->count);
    }
    s.sample_count = bucket->voltage.count;
    s.cycle_count = bucket->cycle_count;
    s.balancing_events = bucket->balancing_events;
    s.comm_errors = bucket->comm_errors;
    s.period_start_ms = bucket->start_ms;
    s.period_end_ms = bucket->end_ms;
    return s;
}

static void roll_hour_bucket(uint64_t timestamp_ms)
{
    memmove(&s_state.hourly[1], &s_state.hourly[0], sizeof(stats_bucket_t) * 23);
    s_state.hourly[0] = s_state.current_hour;
    reset_bucket(&s_state.current_hour, timestamp_ms);
}

static void roll_day_bucket(uint64_t timestamp_ms)
{
    memmove(&s_state.daily[1], &s_state.daily[0], sizeof(stats_bucket_t) * 6);
    s_state.daily[0] = s_state.current_day;
    reset_bucket(&s_state.current_day, timestamp_ms);
}

static void update_buckets_from_battery(const battery_status_t *batt)
{
    if (!batt) {
        return;
    }

    float voltage = batt->voltage;
    float current = batt->current;
    float temperature = batt->temperature;
    float soc = batt->soc;
    uint64_t ts = now_ms();

    stat_accumulator_t *accs_hour[] = {
        &s_state.current_hour.voltage,
        &s_state.current_hour.current,
        &s_state.current_hour.temperature,
        &s_state.current_hour.soc,
    };
    stat_accumulator_t *accs_day[] = {
        &s_state.current_day.voltage,
        &s_state.current_day.current,
        &s_state.current_day.temperature,
        &s_state.current_day.soc,
    };
    float values[] = {voltage, current, temperature, soc};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        accumulate(accs_hour[i], values[i]);
        accumulate(accs_day[i], values[i]);
    }

    s_state.current_hour.cycle_count++;
    s_state.current_day.cycle_count++;
    s_state.current_hour.end_ms = ts;
    s_state.current_day.end_ms = ts;
}

static void update_buckets_from_pack(const pack_stats_t *pack)
{
    if (!pack) {
        return;
    }

    uint64_t ts = now_ms();
    stat_accumulator_t *hour_accs[] = {
        &s_state.current_hour.cell_min,
        &s_state.current_hour.cell_max,
        &s_state.current_hour.cell_delta,
    };
    stat_accumulator_t *day_accs[] = {
        &s_state.current_day.cell_min,
        &s_state.current_day.cell_max,
        &s_state.current_day.cell_delta,
    };
    float values[] = {pack->cell_min, pack->cell_max, pack->cell_delta};

    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        accumulate(hour_accs[i], values[i]);
        accumulate(day_accs[i], values[i]);
    }

    uint32_t balancing_count = 0;
    for (uint8_t i = 0; i < pack->cell_count && i < PACK_MAX_CELLS; ++i) {
        if (pack->balancing[i]) {
            balancing_count++;
        }
    }
    s_state.current_hour.balancing_events += balancing_count;
    s_state.current_day.balancing_events += balancing_count;
    s_state.current_hour.end_ms = ts;
    s_state.current_day.end_ms = ts;
}

static void update_buckets_from_system(const system_status_t *status)
{
    if (!status) {
        return;
    }

    if (status->network_state == NETWORK_STATE_ERROR || !status->wifi_connected || !status->storage_ok) {
        s_state.current_hour.comm_errors++;
        s_state.current_day.comm_errors++;
    }
}

static stats_bucket_t aggregate_window(const stats_bucket_t *buckets, size_t count)
{
    stats_bucket_t agg;
    reset_bucket(&agg, 0);
    for (size_t i = 0; i < count; ++i) {
        merge_bucket(&agg, &buckets[i]);
    }
    return agg;
}

static void export_summary_to_csv(FILE *f, const char *label, const stats_bucket_t *bucket, const char *fw_version)
{
    if (!f || !bucket) {
        return;
    }
    if (bucket->voltage.count == 0) {
        return;
    }

    double avg_voltage = bucket->voltage.count ? bucket->voltage.sum / bucket->voltage.count : 0.0;
    double avg_temp = bucket->temperature.count ? bucket->temperature.sum / bucket->temperature.count : 0.0;
    double avg_soc = bucket->soc.count ? bucket->soc.sum / bucket->soc.count : 0.0;

    fprintf(f, "%s,%llu,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%s\n",
            label,
            (unsigned long long) bucket->start_ms,
            (unsigned long long) bucket->end_ms,
            (double) bucket->voltage.min,
            (double) bucket->voltage.max,
            avg_voltage,
            (double) bucket->temperature.min,
            (double) bucket->temperature.max,
            avg_temp,
            bucket->cycle_count,
            bucket->balancing_events,
            bucket->comm_errors,
            fw_version ? fw_version : "");

    fprintf(f, "%s_cells,%llu,%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%s\n",
            label,
            (unsigned long long) bucket->start_ms,
            (unsigned long long) bucket->end_ms,
            (double) bucket->cell_min.min,
            (double) bucket->cell_max.max,
            bucket->cell_min.count ? bucket->cell_min.sum / bucket->cell_min.count : 0.0,
            (double) bucket->cell_delta.min,
            (double) bucket->cell_delta.max,
            bucket->cell_delta.count ? bucket->cell_delta.sum / bucket->cell_delta.count : 0.0,
            bucket->cycle_count,
            bucket->balancing_events,
            bucket->comm_errors,
            fw_version ? fw_version : "");
}

static void export_summary_to_json(cJSON *root, const char *label, const stats_bucket_t *bucket, const char *fw_version)
{
    if (!root || !bucket) {
        return;
    }
    if (bucket->voltage.count == 0) {
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "start_ms", (double) bucket->start_ms);
    cJSON_AddNumberToObject(obj, "end_ms", (double) bucket->end_ms);
    cJSON_AddNumberToObject(obj, "voltage_min", bucket->voltage.min);
    cJSON_AddNumberToObject(obj, "voltage_max", bucket->voltage.max);
    cJSON_AddNumberToObject(obj, "voltage_avg", bucket->voltage.count ? bucket->voltage.sum / bucket->voltage.count : 0.0);
    cJSON_AddNumberToObject(obj, "temperature_min", bucket->temperature.min);
    cJSON_AddNumberToObject(obj, "temperature_max", bucket->temperature.max);
    cJSON_AddNumberToObject(obj, "temperature_avg", bucket->temperature.count ? bucket->temperature.sum / bucket->temperature.count : 0.0);
    cJSON_AddNumberToObject(obj, "soc_min", bucket->soc.min);
    cJSON_AddNumberToObject(obj, "soc_max", bucket->soc.max);
    cJSON_AddNumberToObject(obj, "soc_avg", bucket->soc.count ? bucket->soc.sum / bucket->soc.count : 0.0);
    cJSON_AddNumberToObject(obj, "cell_min", bucket->cell_min.min);
    cJSON_AddNumberToObject(obj, "cell_max", bucket->cell_max.max);
    cJSON_AddNumberToObject(obj, "cell_delta_min", bucket->cell_delta.min);
    cJSON_AddNumberToObject(obj, "cell_delta_max", bucket->cell_delta.max);
    cJSON_AddNumberToObject(obj, "cell_delta_avg", bucket->cell_delta.count ? bucket->cell_delta.sum / bucket->cell_delta.count : 0.0);
    cJSON_AddNumberToObject(obj, "cycle_count", bucket->cycle_count);
    cJSON_AddNumberToObject(obj, "balancing_events", bucket->balancing_events);
    cJSON_AddNumberToObject(obj, "comm_errors", bucket->comm_errors);
    cJSON_AddStringToObject(obj, "firmware", fw_version ? fw_version : "");

    cJSON_AddItemToObject(root, label, obj);
}

static char firmware_version[32] = {0};

static const char *get_firmware_version(void)
{
    if (firmware_version[0] == '\0') {
        const esp_app_desc_t *app = esp_app_get_description();
        if (app) {
            strncpy(firmware_version, app->version, sizeof(firmware_version) - 1);
        }
    }
    return firmware_version;
}

static void stats_task(void *arg)
{
    (void) arg;
    uint64_t last_hour_roll = now_ms();
    uint64_t last_day_roll = last_hour_roll;

    while (1) {
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

static void handle_battery(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;
    if (!event || !event->data) {
        return;
    }
    update_buckets_from_battery((const battery_status_t *) event->data);
}

static void handle_pack(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;
    if (!event || !event->data) {
        return;
    }
    update_buckets_from_pack((const pack_stats_t *) event->data);
}

static void handle_system(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;
    if (!event || !event->data) {
        return;
    }
    update_buckets_from_system((const system_status_t *) event->data);
}

esp_err_t stats_aggregator_init(event_bus_t *bus)
{
    if (s_state.initialized) {
        return ESP_OK;
    }
    if (!bus) {
        return ESP_ERR_INVALID_ARG;
    }

    s_state.bus = bus;
    reset_state_buffers(now_ms());

    event_bus_subscribe(bus, EVENT_BATTERY_STATUS_UPDATED, handle_battery, NULL);
    event_bus_subscribe(bus, EVENT_PACK_STATS_UPDATED, handle_pack, NULL);
    event_bus_subscribe(bus, EVENT_SYSTEM_STATUS_UPDATED, handle_system, NULL);

    s_state.initialized = true;
    ESP_LOGI(TAG, "Stats aggregator initialized");
    return ESP_OK;
}

esp_err_t stats_aggregator_start(void)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state.task) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(stats_task, "stats_aggregator", 4096, NULL, 5, &s_state.task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Unable to create stats task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Stats aggregator task started");
    return ESP_OK;
}

esp_err_t stats_aggregator_export_to_flash(void)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *fw = get_firmware_version();
    stats_bucket_t day = aggregate_window(s_state.hourly, 24);
    merge_bucket(&day, &s_state.current_hour);
    stats_bucket_t week = aggregate_window(s_state.daily, 7);
    merge_bucket(&week, &s_state.current_day);

    const char *csv_path = "/spiflash/stats_summary.csv";
    const char *json_path = "/spiflash/stats_summary.json";

    FILE *csv = fopen(csv_path, "w");
    if (csv) {
        fprintf(csv, "period,start_ms,end_ms,v_min,v_max,v_avg,t_min,t_max,t_avg,cycles,balancing,comm_errors,fw\n");
        export_summary_to_csv(csv, "24h", &day, fw);
        export_summary_to_csv(csv, "7d", &week, fw);
        fclose(csv);
    } else {
        ESP_LOGE(TAG, "Failed to open %s", csv_path);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", fw ? fw : "");
    export_summary_to_json(root, "day", &day, fw);
    export_summary_to_json(root, "week", &week, fw);

    FILE *json = fopen(json_path, "w");
    if (json) {
        char *rendered = cJSON_PrintUnformatted(root);
        if (rendered) {
            fprintf(json, "%s", rendered);
            cJSON_free(rendered);
        }
        fclose(json);
    } else {
        ESP_LOGE(TAG, "Failed to open %s", json_path);
    }
    cJSON_Delete(root);

    if (s_state.pdf_renderer && s_state.pdf_output_path[0] != '\0') {
        stats_summary_t day_summary = bucket_to_summary(&day);
        stats_summary_t week_summary = bucket_to_summary(&week);
        esp_err_t err = s_state.pdf_renderer(&day_summary, &week_summary, s_state.pdf_output_path);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PDF renderer returned error %s", esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

bool stats_aggregator_send_http(const char *path)
{
    const char *fw = get_firmware_version();
    stats_bucket_t day = aggregate_window(s_state.hourly, 24);
    merge_bucket(&day, &s_state.current_hour);
    stats_bucket_t week = aggregate_window(s_state.daily, 7);
    merge_bucket(&week, &s_state.current_day);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", fw ? fw : "");
    export_summary_to_json(root, "day", &day, fw);
    export_summary_to_json(root, "week", &week, fw);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return false;
    }

    bool ok = net_client_send_http_request(path, "POST", payload, strlen(payload));
    cJSON_free(payload);
    return ok;
}

void stats_aggregator_set_pdf_renderer(stats_pdf_renderer_t renderer, const char *output_path)
{
    s_state.pdf_renderer = renderer;
    if (output_path) {
        strncpy(s_state.pdf_output_path, output_path, sizeof(s_state.pdf_output_path) - 1);
        s_state.pdf_output_path[sizeof(s_state.pdf_output_path) - 1] = '\0';
    }
}
