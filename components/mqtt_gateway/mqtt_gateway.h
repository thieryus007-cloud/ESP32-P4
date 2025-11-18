#pragma once

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_gateway_init(event_bus_t *bus);
void mqtt_gateway_start(void);
void mqtt_gateway_stop(void);

#ifdef __cplusplus
}
#endif
