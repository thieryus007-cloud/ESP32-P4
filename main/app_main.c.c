// main/app_main.c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "hmi_main.h"

static const char *TAG = "APP_MAIN";

void app_main(void)
{
    esp_err_t ret;

    // --- NVS ---
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting HMI firmware (ESP32-P4 + LVGL)");

    // --- HMI global init ---
    hmi_main_init();

    // --- Start tasks/modules ---
    hmi_main_start();

    // app_main ne doit pas faire plus, on laisse les tasks tourner
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
