#include "history_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "history_manager";
static history_manager_t g_history = {0};

static esp_err_t ring_buffer_init(
    history_ring_buffer_t *rb,
    uint32_t capacity,
    uint32_t sample_interval_ms) {

    if (rb == NULL) return ESP_ERR_INVALID_ARG;

    // Allocation en PSRAM si disponible
    rb->buffer = heap_caps_malloc(
        capacity * sizeof(history_point_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (rb->buffer == NULL) {
        // Fallback sur SRAM interne
        ESP_LOGW(TAG, "PSRAM not available, using internal RAM");
        rb->buffer = malloc(capacity * sizeof(history_point_t));
        if (rb->buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for ring buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    rb->sample_interval_ms = sample_interval_ms;
    rb->last_sample_time = 0;

    ESP_LOGI(TAG, "Ring buffer initialized: capacity=%u, interval=%ums", capacity, sample_interval_ms);
    return ESP_OK;
}

static void ring_buffer_push(
    history_ring_buffer_t *rb,
    const history_point_t *point) {

    if (rb == NULL || point == NULL || rb->buffer == NULL) return;

    memcpy(&rb->buffer[rb->head], point, sizeof(history_point_t));
    rb->head = (rb->head + 1) % rb->capacity;
    if (rb->count < rb->capacity) {
        rb->count++;
    }
}

static void ring_buffer_free(history_ring_buffer_t *rb) {
    if (rb && rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
    }
}

esp_err_t history_manager_init(void) {
    ESP_LOGI(TAG, "Initializing history manager...");

    g_history.mutex = xSemaphoreCreateMutex();
    if (g_history.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret;
    ret = ring_buffer_init(&g_history.buf_1min, HISTORY_POINTS_1MIN, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init 1min buffer");
        return ret;
    }

    ret = ring_buffer_init(&g_history.buf_1h, HISTORY_POINTS_1H, 10000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init 1h buffer");
        return ret;
    }

    ret = ring_buffer_init(&g_history.buf_24h, HISTORY_POINTS_24H, 60000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init 24h buffer");
        return ret;
    }

    ret = ring_buffer_init(&g_history.buf_7d, HISTORY_POINTS_7D, 300000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init 7d buffer");
        return ret;
    }

    // Charger données persistées
    history_load_from_flash();

    ESP_LOGI(TAG, "History manager initialized successfully");
    return ESP_OK;
}

void history_manager_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing history manager...");

    if (g_history.mutex) {
        // Sauvegarder avant de libérer
        history_save_to_flash();

        vSemaphoreDelete(g_history.mutex);
        g_history.mutex = NULL;
    }

    ring_buffer_free(&g_history.buf_1min);
    ring_buffer_free(&g_history.buf_1h);
    ring_buffer_free(&g_history.buf_24h);
    ring_buffer_free(&g_history.buf_7d);

    ESP_LOGI(TAG, "History manager deinitialized");
}

void history_add_point(const history_point_t *point) {
    if (point == NULL) return;

    if (xSemaphoreTake(g_history.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        return;
    }

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Ajouter au buffer 1min (toujours)
    if (now_ms - g_history.buf_1min.last_sample_time >= g_history.buf_1min.sample_interval_ms) {
        ring_buffer_push(&g_history.buf_1min, point);
        g_history.buf_1min.last_sample_time = now_ms;
    }

    // Ajouter au buffer 1h (tous les 10s)
    if (now_ms - g_history.buf_1h.last_sample_time >= g_history.buf_1h.sample_interval_ms) {
        ring_buffer_push(&g_history.buf_1h, point);
        g_history.buf_1h.last_sample_time = now_ms;
    }

    // Ajouter au buffer 24h (toutes les minutes)
    if (now_ms - g_history.buf_24h.last_sample_time >= g_history.buf_24h.sample_interval_ms) {
        ring_buffer_push(&g_history.buf_24h, point);
        g_history.buf_24h.last_sample_time = now_ms;
    }

    // Ajouter au buffer 7d (toutes les 5 minutes)
    if (now_ms - g_history.buf_7d.last_sample_time >= g_history.buf_7d.sample_interval_ms) {
        ring_buffer_push(&g_history.buf_7d, point);
        g_history.buf_7d.last_sample_time = now_ms;
    }

    xSemaphoreGive(g_history.mutex);
}

static history_ring_buffer_t* get_buffer_for_period(history_period_t period) {
    switch (period) {
        case HISTORY_1MIN:
            return &g_history.buf_1min;
        case HISTORY_1H:
            return &g_history.buf_1h;
        case HISTORY_24H:
            return &g_history.buf_24h;
        case HISTORY_7D:
            return &g_history.buf_7d;
        default:
            return NULL;
    }
}

uint32_t history_get_points(
    history_period_t period,
    history_point_t *points,
    uint32_t max_points) {

    if (points == NULL || max_points == 0) return 0;

    history_ring_buffer_t *rb = get_buffer_for_period(period);
    if (rb == NULL || rb->buffer == NULL) return 0;

    if (xSemaphoreTake(g_history.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        return 0;
    }

    uint32_t count = rb->count < max_points ? rb->count : max_points;
    uint32_t start_idx;

    if (rb->count < rb->capacity) {
        // Buffer pas encore plein, commencer au début
        start_idx = 0;
    } else {
        // Buffer plein, commencer à head (le plus ancien)
        start_idx = rb->head;
    }

    // Copier les points
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % rb->capacity;
        memcpy(&points[i], &rb->buffer[idx], sizeof(history_point_t));
    }

    xSemaphoreGive(g_history.mutex);

    ESP_LOGD(TAG, "Retrieved %u points for period %d", count, period);
    return count;
}

esp_err_t history_export_csv(
    history_period_t period,
    const char *filename) {

    if (filename == NULL) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Exporting history to CSV: %s", filename);

    // Allouer buffer temporaire
    history_point_t *points = malloc(2016 * sizeof(history_point_t)); // Max 7d
    if (points == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temp buffer");
        return ESP_ERR_NO_MEM;
    }

    uint32_t count = history_get_points(period, points, 2016);
    if (count == 0) {
        ESP_LOGW(TAG, "No data to export");
        free(points);
        return ESP_OK;
    }

    // Ouvrir le fichier pour écriture
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        free(points);
        return ESP_FAIL;
    }

    // Header CSV
    fprintf(f, "timestamp,voltage_v,current_a,soc,temp_c,cell_min_mv,cell_max_mv\n");

    // Données
    for (uint32_t i = 0; i < count; i++) {
        fprintf(f, "%u,%.2f,%.2f,%u,%d,%u,%u\n",
            points[i].timestamp,
            points[i].voltage_cv / 100.0f,
            points[i].current_ca / 100.0f,
            points[i].soc,
            points[i].temperature,
            points[i].cell_min_mv,
            points[i].cell_max_mv);
    }

    fclose(f);
    free(points);

    ESP_LOGI(TAG, "CSV export complete: %u points", count);
    return ESP_OK;
}

esp_err_t history_save_to_flash(void) {
    ESP_LOGI(TAG, "Saving history to flash...");

    // Pour simplifier, on sauvegarde seulement le buffer 24h
    // qui contient les données les plus importantes

    if (xSemaphoreTake(g_history.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }

    FILE *f = fopen("/spiffs/history_24h.dat", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        xSemaphoreGive(g_history.mutex);
        return ESP_FAIL;
    }

    // Écrire métadonnées
    fwrite(&g_history.buf_24h.count, sizeof(uint32_t), 1, f);
    fwrite(&g_history.buf_24h.head, sizeof(uint32_t), 1, f);

    // Écrire les données
    if (g_history.buf_24h.buffer && g_history.buf_24h.count > 0) {
        fwrite(g_history.buf_24h.buffer,
               sizeof(history_point_t),
               g_history.buf_24h.capacity,
               f);
    }

    fclose(f);
    xSemaphoreGive(g_history.mutex);

    ESP_LOGI(TAG, "History saved to flash");
    return ESP_OK;
}

esp_err_t history_load_from_flash(void) {
    ESP_LOGI(TAG, "Loading history from flash...");

    FILE *f = fopen("/spiffs/history_24h.dat", "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "No saved history found (first boot?)");
        return ESP_OK; // Pas une erreur
    }

    if (xSemaphoreTake(g_history.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take mutex");
        fclose(f);
        return ESP_FAIL;
    }

    // Lire métadonnées
    fread(&g_history.buf_24h.count, sizeof(uint32_t), 1, f);
    fread(&g_history.buf_24h.head, sizeof(uint32_t), 1, f);

    // Lire les données
    if (g_history.buf_24h.buffer) {
        fread(g_history.buf_24h.buffer,
              sizeof(history_point_t),
              g_history.buf_24h.capacity,
              f);
    }

    fclose(f);
    xSemaphoreGive(g_history.mutex);

    ESP_LOGI(TAG, "History loaded: %u points", g_history.buf_24h.count);
    return ESP_OK;
}
