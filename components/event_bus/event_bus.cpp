// components/event_bus/event_bus.c

#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <string.h>

#ifndef CONFIG_EVENT_BUS_QUEUE_LENGTH
#define CONFIG_EVENT_BUS_QUEUE_LENGTH 32
#endif

#define MAX_SUBSCRIBERS 32

struct event_bus {
    struct {
        event_type_t     type;
        event_callback_t callback;
        void            *user_ctx;
        bool             in_use;
    } subscribers[MAX_SUBSCRIBERS];
    QueueHandle_t queue;
    uint32_t      queue_length;
    uint32_t published_total;
    uint32_t dropped_events;
};

typedef struct {
    event_t event;
    void   *payload_copy;
    size_t  payload_size;
} event_bus_queue_item_t;

static const char *TAG = "EVENT_BUS";

static void dispatch_to_subscribers(event_bus_t *bus, const event_t *event)
{
    if (!bus || !event) {
        return;
    }

    bus->published_total++;
    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (bus->subscribers[i].in_use && bus->subscribers[i].type == event->type) {
            if (bus->subscribers[i].callback) {
                bus->subscribers[i].callback(bus, event, bus->subscribers[i].user_ctx);
            }
        }
    }
}

void event_bus_init(event_bus_t *bus)
{
    if (!bus) {
        return;
    }
    memset(bus, 0, sizeof(*bus));
    bus->queue_length = CONFIG_EVENT_BUS_QUEUE_LENGTH;
    bus->queue        = xQueueCreate(bus->queue_length, sizeof(event_bus_queue_item_t));
    if (!bus->queue) {
        ESP_LOGE(TAG, "Failed to create EventBus queue (len=%u)", bus->queue_length);
    }
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

    if (bus->queue) {
        event_bus_queue_item_t item = {
            .event        = *event,
            .payload_copy = NULL,
            .payload_size = 0,
        };

        if (event->data && event->data_size > 0) {
            item.payload_copy = pvPortMalloc(event->data_size);
            if (!item.payload_copy) {
                bus->dropped_events++;
                ESP_LOGE(TAG, "Failed to allocate event payload copy (size=%zu)", event->data_size);
                return false;
            }
            memcpy(item.payload_copy, event->data, event->data_size);
            item.payload_size = event->data_size;
            item.event.data   = item.payload_copy;
        }

        if (xQueueSend(bus->queue, &item, 0) != pdTRUE) {
            if (item.payload_copy) {
                vPortFree(item.payload_copy);
            }
            bus->dropped_events++;
            if ((bus->dropped_events & (bus->dropped_events - 1U)) == 0U) {
                ESP_LOGW(TAG,
                         "EventBus queue saturated: %" PRIu32 " drops (capacity=%" PRIu32 ")",
                         bus->dropped_events,
                         bus->queue_length);
            }
            return false;
        }
        return true;
    }

    dispatch_to_subscribers(bus, event);
    return true;
}

event_bus_metrics_t event_bus_get_metrics(const event_bus_t *bus)
{
    event_bus_metrics_t m = {0};
    if (!bus) {
        return m;
    }

    for (int i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (bus->subscribers[i].in_use) {
            m.subscribers++;
        }
    }
    m.published_total = bus->published_total;
    return m;
}

bool event_bus_get_queue_metrics(const event_bus_t *bus, event_bus_queue_metrics_t *out)
{
    if (!bus || !out || !bus->queue) {
        if (out) {
            memset(out, 0, sizeof(*out));
        }
        return false;
    }

    out->queue_capacity   = bus->queue_length;
    out->messages_waiting = (uint32_t) uxQueueMessagesWaiting(bus->queue);
    out->dropped_events   = bus->dropped_events;
    return true;
}

void event_bus_dispatch_task(void *ctx)
{
    event_bus_t *bus = (event_bus_t *) ctx;

    if (!bus || !bus->queue) {
        ESP_LOGE(TAG, "EventBus dispatch task aborted: invalid context");
        vTaskDelete(NULL);
        return;
    }

    event_bus_queue_item_t item;
    while (true) {
        if (xQueueReceive(bus->queue, &item, portMAX_DELAY) == pdTRUE) {
            dispatch_to_subscribers(bus, &item.event);
            if (item.payload_copy && item.payload_size > 0) {
                vPortFree(item.payload_copy);
            }
        }
    }
}
