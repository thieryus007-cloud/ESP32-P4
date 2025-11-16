// components/event_bus/event_bus.c

#include "event_bus.h"
#include "esp_log.h"
#include <string.h>

#define MAX_SUBSCRIBERS 32

struct event_bus {
    struct {
        event_type_t     type;
        event_callback_t callback;
        void            *user_ctx;
        bool             in_use;
    } subscribers[MAX_SUBSCRIBERS];
};

static const char *TAG = "EVENT_BUS";

void event_bus_init(event_bus_t *bus)
{
    if (!bus) {
        return;
    }
    memset(bus, 0, sizeof(*bus));
    ESP_LOGI(TAG, "EventBus initialized");
}

bool event_bus_subscribe(event_bus_t *bus,
                         event_type_t type,
                         event_callback_t callback,
                         void *user_ctx)
{
    if (!bus || !callback || type <= EVENT_TYPE_NONE || type >= EVENT_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid subscribe params");
        return false;
    }

    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (!bus->subscribers[i].in_use) {
            bus->subscribers[i].type      = type;
            bus->subscribers[i].callback  = callback;
            bus->subscribers[i].user_ctx  = user_ctx;
            bus->subscribers[i].in_use    = true;
            ESP_LOGI(TAG, "Subscriber added for event type %d (slot %d)", type, i);
            return true;
        }
    }

    ESP_LOGE(TAG, "No free subscriber slots for event type %d", type);
    return false;
}

bool event_bus_publish(event_bus_t *bus, const event_t *event)
{
    if (!bus || !event || event->type <= EVENT_TYPE_NONE || event->type >= EVENT_TYPE_MAX) {
        ESP_LOGE(TAG, "Invalid publish params");
        return false;
    }

    // Dispatch synchrone très simple : tous les subscribers du type reçoivent l'event
    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (bus->subscribers[i].in_use && bus->subscribers[i].type == event->type) {
            if (bus->subscribers[i].callback) {
                bus->subscribers[i].callback(bus, event, bus->subscribers[i].user_ctx);
            }
        }
    }

    return true;
}
