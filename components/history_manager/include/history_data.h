#ifndef HISTORY_DATA_H
#define HISTORY_DATA_H

#include <stdint.h>
#include <stdbool.h>

// Point de données historique (16 bytes)
typedef struct __attribute__((packed)) {
    uint32_t timestamp;     // Unix timestamp (secondes)
    int16_t voltage_cv;     // Tension en centivolts (51.23V = 5123)
    int16_t current_ca;     // Courant en centi-ampères
    uint8_t soc;            // SOC 0-100
    int8_t temperature;     // Température -128 à +127°C
    uint16_t cell_min_mv;   // Tension cell min
    uint16_t cell_max_mv;   // Tension cell max
} history_point_t;

// Configuration du ring buffer
#define HISTORY_POINTS_1MIN    60      // 1 point/seconde pendant 1 min
#define HISTORY_POINTS_1H      360     // 1 point/10sec pendant 1h
#define HISTORY_POINTS_24H     1440    // 1 point/min pendant 24h
#define HISTORY_POINTS_7D      2016    // 1 point/5min pendant 7j

// Ring buffer en mémoire (PSRAM recommandé)
typedef struct {
    history_point_t *buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
    uint32_t sample_interval_ms;
    uint32_t last_sample_time;
} history_ring_buffer_t;

#endif // HISTORY_DATA_H
