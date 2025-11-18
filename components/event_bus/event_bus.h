// components/event_bus/event_bus.h
#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Payload générique d'événement.
 *
 * On reste générique : le payload est un pointeur vers une structure
 * typée définie dans event_types.h (ou ailleurs). Tu peux aussi utiliser
 * un union si tu veux quelque chose de plus strict.
 */
typedef struct {
    event_type_t type;
    void        *data;       // pointeur vers struct spécifique (lifetime gérée par l'émetteur ou le bus)
    size_t       data_size;  // taille du payload si le bus doit en faire une copie
} event_t;

struct event_bus;

/**
 * @brief Prototype callback de subscriber
 */
typedef void (*event_callback_t)(struct event_bus *bus, const event_t *event, void *user_ctx);

/**
 * @brief Handle interne du bus (implémentation dans event_bus.c)
 */
typedef struct event_bus event_bus_t;

typedef struct {
    uint32_t subscribers;     // slots occupés
    uint32_t published_total; // nombre d'événements dispatchés
} event_bus_metrics_t;

typedef struct {
    uint32_t queue_capacity;     // taille de la queue principale
    uint32_t messages_waiting;   // nb d'events en attente dans la queue
    uint32_t dropped_events;     // nb d'events perdus faute de place
} event_bus_queue_metrics_t;

/**
 * @brief Initialiser l'EventBus
 */
void event_bus_init(event_bus_t *bus);

/**
 * @brief S'abonner à un type d'événement
 *
 * @param bus        Bus sur lequel s'abonner
 * @param type       Type d'événement
 * @param callback   Fonction appelée lors de la réception
 * @param user_ctx   Contexte utilisateur (pointeur opaque)
 * @return true si succès
 */
bool event_bus_subscribe(event_bus_t *bus,
                         event_type_t type,
                         event_callback_t callback,
                         void *user_ctx);

/**
 * @brief Publier un événement sur le bus
 *
 * @note L'implémentation peut être synchrone ou via une queue + task dédiée.
 */
bool event_bus_publish(event_bus_t *bus, const event_t *event);

event_bus_metrics_t event_bus_get_metrics(const event_bus_t *bus);

bool event_bus_get_queue_metrics(const event_bus_t *bus, event_bus_queue_metrics_t *out);

void event_bus_dispatch_task(void *ctx);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
