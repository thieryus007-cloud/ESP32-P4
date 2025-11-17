// components/tinybms_model/tinybms_rules.h
#ifndef TINYBMS_RULES_H
#define TINYBMS_RULES_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le moteur de règles TinyBMS (seuil + hystérésis + délai).
 */
void tinybms_rules_init(event_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif // TINYBMS_RULES_H
