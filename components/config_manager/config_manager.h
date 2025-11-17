#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    alert_threshold_high;
    float    alert_threshold_low;
    char     mqtt_broker[96];
    char     mqtt_topic[96];
    char     http_endpoint[96];
    uint32_t log_retention_days;
    uint32_t status_publish_period_ms;
} hmi_persistent_config_t;

esp_err_t config_manager_init(void);

esp_err_t config_manager_save(const hmi_persistent_config_t *cfg);

const hmi_persistent_config_t *config_manager_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
