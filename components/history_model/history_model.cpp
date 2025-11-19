// components/history_model/history_model.c

#include "history_model.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

#include "net_client.h"

#define HISTORY_BUFFER_CAPACITY 2048

static const char *TAG = "HISTORY_MODEL";

static event_bus_t *s_bus = NULL;

typedef struct {
    history_sample_t samples[HISTORY_BUFFER_CAPACITY];
    uint16_t         head;   // index de la prochaine insertion
    uint16_t         count;  // nombre d'échantillons valides
} history_ring_t;

static history_ring_t s_ring = { 0 };
static history_range_t s_last_requested_range = HISTORY_RANGE_LAST_HOUR;

static float json_get_number(cJSON *obj, const char *key, float def)
{
    if (!obj || !key) {
        return def;
    }
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return (float) item->valuedouble;
    }
    return def;
}

static uint64_t now_ms(void)
{
    return (uint64_t) (esp_timer_get_time() / 1000ULL);
}

static uint64_t range_duration_ms(history_range_t range)
{
    switch (range) {
        case HISTORY_RANGE_LAST_DAY:
            return 24ULL * 60ULL * 60ULL * 1000ULL;
        case HISTORY_RANGE_LAST_WEEK:
            return 7ULL * 24ULL * 60ULL * 60ULL * 1000ULL;
        case HISTORY_RANGE_LAST_HOUR:
        default:
            return 60ULL * 60ULL * 1000ULL;
    }
}

static const char *range_to_query(history_range_t range)
{
    switch (range) {
        case HISTORY_RANGE_LAST_DAY:  return "24h";
        case HISTORY_RANGE_LAST_WEEK: return "7d";
        case HISTORY_RANGE_LAST_HOUR:
        default:                      return "1h";
    }
}

static void ring_push_sample(const history_sample_t *sample)
{
    if (!sample) {
        return;
    }

    s_ring.samples[s_ring.head] = *sample;
    s_ring.head = (s_ring.head + 1) % HISTORY_BUFFER_CAPACITY;
    if (s_ring.count < HISTORY_BUFFER_CAPACITY) {
        s_ring.count++;
    }
}

static void publish_snapshot(history_range_t range,
                             const history_sample_t *samples,
                             uint16_t sample_count,
                             bool from_backend)
{
    if (!s_bus) {
        return;
    }

    history_snapshot_t snapshot = {
        .range = range,
        .count = 0,
        .from_backend = from_backend,
    };

    if (samples && sample_count > 0) {
        if (sample_count > HISTORY_SNAPSHOT_MAX) {
            samples += (sample_count - HISTORY_SNAPSHOT_MAX);
            sample_count = HISTORY_SNAPSHOT_MAX;
        }
        memcpy(snapshot.samples, samples, sample_count * sizeof(history_sample_t));
        snapshot.count = sample_count;
    }

    event_t evt = {
        .type = EVENT_HISTORY_UPDATED,
        .data = &snapshot,
        .data_size = sizeof(snapshot),
    };
    event_bus_publish(s_bus, &evt);
}

static void publish_local_snapshot(history_range_t range)
{
    if (!s_ring.count) {
        publish_snapshot(range, NULL, 0, false);
        return;
    }

    uint64_t cutoff = now_ms() - range_duration_ms(range);

    history_sample_t tmp[HISTORY_SNAPSHOT_MAX];
    uint16_t collected = 0;

    // parcourir du plus récent au plus ancien
    for (int i = 0; i < s_ring.count && collected < HISTORY_SNAPSHOT_MAX; ++i) {
        int idx = (s_ring.head - 1 - i + HISTORY_BUFFER_CAPACITY) % HISTORY_BUFFER_CAPACITY;
        const history_sample_t *s = &s_ring.samples[idx];
        if (s->timestamp_ms >= cutoff) {
            tmp[HISTORY_SNAPSHOT_MAX - 1 - collected] = *s;
            collected++;
        } else {
            break;
        }
    }

    if (collected == 0) {
        publish_snapshot(range, NULL, 0, false);
        return;
    }

    // réaligner en partant du début du buffer temporaire
    uint16_t start = HISTORY_SNAPSHOT_MAX - collected;
    publish_snapshot(range, &tmp[start], collected, false);
}

static bool parse_history_array(cJSON *array)
{
    if (!cJSON_IsArray(array)) {
        return false;
    }

    history_sample_t samples[HISTORY_SNAPSHOT_MAX];
    uint16_t count = 0;

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsObject(item) || count >= HISTORY_SNAPSHOT_MAX) {
            continue;
        }

        history_sample_t s = {0};
        cJSON *ts = cJSON_GetObjectItemCaseSensitive(item, "timestamp_ms");
        if (cJSON_IsNumber(ts)) {
            s.timestamp_ms = (uint64_t) ts->valuedouble;
        }
        if (s.timestamp_ms == 0) {
            cJSON *ts_sec = cJSON_GetObjectItemCaseSensitive(item, "timestamp");
            if (cJSON_IsNumber(ts_sec)) {
                s.timestamp_ms = ((uint64_t) ts_sec->valuedouble) * 1000ULL;
            }
        }

        s.voltage     = json_get_number(item, "voltage", 0.0f);
        s.current     = json_get_number(item, "current", 0.0f);
        s.temperature = json_get_number(item, "temperature", 0.0f);
        s.soc         = json_get_number(item, "soc", 0.0f);

        if (s.timestamp_ms == 0) {
            s.timestamp_ms = now_ms();
        }

        samples[count++] = s;
    }

    if (count == 0) {
        return false;
    }

    // Enregistrer dans le ring buffer
    for (uint16_t i = 0; i < count; ++i) {
        ring_push_sample(&samples[i]);
    }

    publish_snapshot(s_last_requested_range, samples, count, true);
    return true;
}

static void handle_battery_update(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const battery_status_t *status = (const battery_status_t *) event->data;

    history_sample_t s = {
        .timestamp_ms = now_ms(),
        .voltage = status->voltage,
        .current = status->current,
        .temperature = status->temperature,
        .soc = status->soc,
    };
    ring_push_sample(&s);
}

static void handle_history_request(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    if (!event || !event->data) {
        return;
    }

    const user_input_history_request_t *req = (const user_input_history_request_t *) event->data;
    s_last_requested_range = req->range;

    char path[64];
    snprintf(path, sizeof(path), "/api/history?range=%s", range_to_query(req->range));

    if (!net_client_send_http_request(path, "GET", NULL, 0)) {
        ESP_LOGW(TAG, "Backend history unavailable, using local buffer");
        publish_local_snapshot(req->range);
    }
}

static void handle_history_export(event_bus_t *bus, const event_t *event, void *user_ctx)
{
    (void) bus;
    (void) user_ctx;

    history_export_result_t result = {
        .success = false,
        .path = "",
        .exported_count = 0,
    };

    if (!event || !event->data) {
        goto publish;
    }

    const user_input_history_export_t *req = (const user_input_history_export_t *) event->data;
    uint64_t cutoff = now_ms() - range_duration_ms(req->range);

    const char *path = "/spiflash/history_export.csv";
    strncpy(result.path, path, sizeof(result.path) - 1);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Unable to open %s for export", path);
        goto publish;
    }

    fprintf(f, "timestamp_ms,voltage,current,temperature,soc\n");

    for (int i = 0; i < s_ring.count; ++i) {
        int idx = (s_ring.head - s_ring.count + i + HISTORY_BUFFER_CAPACITY) % HISTORY_BUFFER_CAPACITY;
        const history_sample_t *s = &s_ring.samples[idx];
        if (s->timestamp_ms >= cutoff) {
            fprintf(f, "%llu,%.3f,%.3f,%.3f,%.2f\n",
                    (unsigned long long) s->timestamp_ms,
                    (double) s->voltage,
                    (double) s->current,
                    (double) s->temperature,
                    (double) s->soc);
            result.exported_count++;
        }
    }

    fclose(f);
    result.success = (result.exported_count > 0);

publish:
    event_t evt = {
        .type = EVENT_HISTORY_EXPORTED,
        .data = &result,
        .data_size = sizeof(result),
    };
    event_bus_publish(s_bus, &evt);
}

void history_model_on_remote_history(int status_code, const char *body)
{
    if (status_code != 200) {
        ESP_LOGW(TAG, "History endpoint returned status %d, fallback to local", status_code);
        publish_local_snapshot(s_last_requested_range);
        return;
    }

    if (!body) {
        ESP_LOGW(TAG, "History endpoint returned empty body, fallback to local");
        publish_local_snapshot(s_last_requested_range);
        return;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse history JSON");
        publish_local_snapshot(s_last_requested_range);
        return;
    }

    bool parsed = false;
    if (cJSON_IsArray(root)) {
        parsed = parse_history_array(root);
    } else {
        cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "history");
        if (!arr) {
            arr = cJSON_GetObjectItemCaseSensitive(root, "samples");
        }
        if (arr) {
            parsed = parse_history_array(arr);
        }
    }

    cJSON_Delete(root);

    if (!parsed) {
        ESP_LOGW(TAG, "History payload empty, using local buffer");
        publish_local_snapshot(s_last_requested_range);
    }
}

void history_model_init(event_bus_t *bus)
{
    s_bus = bus;
    s_ring.head = 0;
    s_ring.count = 0;

    if (!s_bus) {
        return;
    }

    event_bus_subscribe(s_bus, EVENT_BATTERY_STATUS_UPDATED, handle_battery_update, NULL);
    event_bus_subscribe(s_bus, EVENT_USER_INPUT_REQUEST_HISTORY, handle_history_request, NULL);
    event_bus_subscribe(s_bus, EVENT_USER_INPUT_EXPORT_HISTORY, handle_history_export, NULL);
}

void history_model_start(void)
{
    ESP_LOGI(TAG, "History model started (ring capacity=%d)", HISTORY_BUFFER_CAPACITY);
}
