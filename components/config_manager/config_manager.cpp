#include "config_manager.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>

#define CONFIG_NAMESPACE "hmi_cfg"
#define CONFIG_KEY       "persist_v1"

static const char *TAG = "cfg_mgr";
static hmi_persistent_config_t s_config;

static void apply_defaults(hmi_persistent_config_t *cfg)
{
    if (!cfg) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->alert_threshold_high    = CONFIG_HMI_ALERT_THRESHOLD_HIGH;
    cfg->alert_threshold_low     = CONFIG_HMI_ALERT_THRESHOLD_LOW;
    cfg->log_retention_days      = CONFIG_HMI_LOG_RETENTION_DAYS;
    cfg->status_publish_period_ms = CONFIG_HMI_STATUS_PUBLISH_PERIOD_MS;

    strlcpy(cfg->mqtt_broker, CONFIG_HMI_MQTT_BROKER_URI, sizeof(cfg->mqtt_broker));
    strlcpy(cfg->mqtt_topic, CONFIG_HMI_MQTT_TOPIC, sizeof(cfg->mqtt_topic));
    strlcpy(cfg->http_endpoint, CONFIG_HMI_HTTP_ENDPOINT, sizeof(cfg->http_endpoint));
}

static esp_err_t persist_config(const hmi_persistent_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, CONFIG_KEY, cfg, sizeof(*cfg));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist config: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t load_config(hmi_persistent_config_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t size = sizeof(*out);
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing config in NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(handle, CONFIG_KEY, out, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(*out)) {
        ESP_LOGW(TAG, "Invalid stored config (%s, size=%zu)", esp_err_to_name(err), size);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t config_manager_init(void)
{
    apply_defaults(&s_config);

    if (load_config(&s_config) != ESP_OK) {
        ESP_LOGI(TAG, "Applying default configuration");
        return persist_config(&s_config);
    }

    ESP_LOGI(TAG, "Configuration loaded: mqtt=%s topic=%s http=%s log_retention=%u days",
             s_config.mqtt_broker,
             s_config.mqtt_topic,
             s_config.http_endpoint,
             (unsigned) s_config.log_retention_days);
    return ESP_OK;
}

esp_err_t config_manager_save(const hmi_persistent_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    s_config = *cfg;
    return persist_config(&s_config);
}

const hmi_persistent_config_t *config_manager_get(void)
{
    return &s_config;
}
