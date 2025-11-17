// main/operation_mode.c
#include "operation_mode.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "hmi_mode"
#define NVS_KEY_MODE   "mode"

#ifndef CONFIG_HMI_DEFAULT_OPERATION_MODE
#define CONFIG_HMI_DEFAULT_OPERATION_MODE 0
#endif

static const char *TAG = "OP_MODE";
static hmi_operation_mode_t s_operation_mode = HMI_MODE_CONNECTED_S3;

static esp_err_t load_mode_from_nvs(hmi_operation_mode_t *out_mode)
{
    if (!out_mode) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    int8_t stored = 0;
    err = nvs_get_i8(handle, NVS_KEY_MODE, &stored);
    nvs_close(handle);

    if (err == ESP_OK) {
        if (stored < (int8_t) HMI_MODE_CONNECTED_S3 || stored > (int8_t) HMI_MODE_TINYBMS_AUTONOMOUS) {
            return ESP_ERR_INVALID_STATE;
        }
        *out_mode = (hmi_operation_mode_t) stored;
    }
    return err;
}

static esp_err_t save_mode_to_nvs(hmi_operation_mode_t mode)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_i8(handle, NVS_KEY_MODE, (int8_t) mode);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t operation_mode_init(void)
{
    hmi_operation_mode_t mode = (hmi_operation_mode_t) CONFIG_HMI_DEFAULT_OPERATION_MODE;

    esp_err_t err = load_mode_from_nvs(&mode);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded persisted mode: %d", mode);
    } else {
        ESP_LOGW(TAG, "Using default mode (%d) (reason=%s)",
                 mode, esp_err_to_name(err));
        // Persist the default so next boot reads it without warning
        esp_err_t persist_err = save_mode_to_nvs(mode);
        if (persist_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist default mode %d: %s", mode, esp_err_to_name(persist_err));
        }
    }

    s_operation_mode = mode;
    return ESP_OK;
}

hmi_operation_mode_t operation_mode_get(void)
{
    return s_operation_mode;
}

esp_err_t operation_mode_set(hmi_operation_mode_t mode)
{
    if (mode < HMI_MODE_CONNECTED_S3 || mode > HMI_MODE_TINYBMS_AUTONOMOUS) {
        return ESP_ERR_INVALID_ARG;
    }

    s_operation_mode = mode;
    esp_err_t err = save_mode_to_nvs(mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist mode %d: %s", mode, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Operation mode updated to %d", mode);
    return ESP_OK;
}
