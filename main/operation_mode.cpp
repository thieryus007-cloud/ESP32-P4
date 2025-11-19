// main/operation_mode.cpp
#include "operation_mode.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {

constexpr const char *TAG = "OP_MODE";
constexpr const char *NVS_NAMESPACE = "hmi_mode";
constexpr const char *NVS_KEY_MODE = "mode";

#ifndef CONFIG_HMI_DEFAULT_OPERATION_MODE
#define CONFIG_HMI_DEFAULT_OPERATION_MODE 0
#endif

class ScopedNvsHandle {
public:
    ScopedNvsHandle() = default;
    ~ScopedNvsHandle()
    {
        reset();
    }

    ScopedNvsHandle(const ScopedNvsHandle &) = delete;
    ScopedNvsHandle &operator=(const ScopedNvsHandle &) = delete;

    nvs_handle_t *out_ptr()
    {
        return &handle_;
    }

    nvs_handle_t get() const
    {
        return handle_;
    }

    void reset(nvs_handle_t new_handle = 0)
    {
        if (handle_ != 0) {
            nvs_close(handle_);
        }
        handle_ = new_handle;
    }

private:
    nvs_handle_t handle_ = 0;
};

bool is_valid_mode(hmi_operation_mode_t mode)
{
    return mode >= HMI_MODE_CONNECTED_S3 && mode <= HMI_MODE_TINYBMS_AUTONOMOUS;
}

bool is_valid_raw_value(int8_t value)
{
    return value >= static_cast<int8_t>(HMI_MODE_CONNECTED_S3) &&
           value <= static_cast<int8_t>(HMI_MODE_TINYBMS_AUTONOMOUS);
}

class OperationModeManager {
public:
    static OperationModeManager &instance()
    {
        static OperationModeManager inst;
        return inst;
    }

    esp_err_t init()
    {
        hmi_operation_mode_t mode = static_cast<hmi_operation_mode_t>(CONFIG_HMI_DEFAULT_OPERATION_MODE);

        esp_err_t err = load_from_nvs(&mode);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded persisted mode: %d", mode);
        } else {
            ESP_LOGW(TAG, "Using default mode (%d) (reason=%s)", mode, esp_err_to_name(err));
            esp_err_t persist_err = save_to_nvs(mode);
            if (persist_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to persist default mode %d: %s", mode, esp_err_to_name(persist_err));
            }
        }

        operation_mode_ = mode;
        return ESP_OK;
    }

    hmi_operation_mode_t get() const
    {
        return operation_mode_;
    }

    esp_err_t set(hmi_operation_mode_t mode)
    {
        if (!is_valid_mode(mode)) {
            return ESP_ERR_INVALID_ARG;
        }

        operation_mode_ = mode;
        esp_err_t err = save_to_nvs(mode);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist mode %d: %s", mode, esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG, "Operation mode updated to %d", mode);
        return ESP_OK;
    }

private:
    esp_err_t load_from_nvs(hmi_operation_mode_t *out_mode)
    {
        if (!out_mode) {
            return ESP_ERR_INVALID_ARG;
        }

        ScopedNvsHandle handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, handle.out_ptr());
        if (err != ESP_OK) {
            return err;
        }

        int8_t stored = 0;
        err = nvs_get_i8(handle.get(), NVS_KEY_MODE, &stored);
        if (err == ESP_OK) {
            if (!is_valid_raw_value(stored)) {
                return ESP_ERR_INVALID_STATE;
            }
            *out_mode = static_cast<hmi_operation_mode_t>(stored);
        }
        return err;
    }

    esp_err_t save_to_nvs(hmi_operation_mode_t mode)
    {
        ScopedNvsHandle handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, handle.out_ptr());
        if (err != ESP_OK) {
            return err;
        }

        err = nvs_set_i8(handle.get(), NVS_KEY_MODE, static_cast<int8_t>(mode));
        if (err == ESP_OK) {
            err = nvs_commit(handle.get());
        }
        return err;
    }

    hmi_operation_mode_t operation_mode_ = HMI_MODE_CONNECTED_S3;
};

} // namespace

extern "C" {

esp_err_t operation_mode_init(void)
{
    return OperationModeManager::instance().init();
}

hmi_operation_mode_t operation_mode_get(void)
{
    return OperationModeManager::instance().get();
}

esp_err_t operation_mode_set(hmi_operation_mode_t mode)
{
    return OperationModeManager::instance().set(mode);
}

} // extern "C"
