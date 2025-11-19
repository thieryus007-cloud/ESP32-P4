#pragma once

#include <cstdint>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

namespace tinybms {

class TimeUtils {
public:
    static uint32_t now_ms() {
        return static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }

    static uint64_t now_ms_64() {
        return static_cast<uint64_t>(xTaskGetTickCount()) *
               static_cast<uint64_t>(portTICK_PERIOD_MS);
    }
};

}  // namespace tinybms
