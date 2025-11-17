// components/event_bus/event_bus.h
#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_TYPE_NONE = 0,

    // Événements "bruts" venant du S3 via JSON
    EVENT_REMOTE_TELEMETRY_UPDATE,
    EVENT_REMOTE_SYSTEM_EVENT,
    EVENT_REMOTE_CONFIG_SNAPSHOT,
    EVENT_REMOTE_CMD_RESULT,

    // Modèle interne pour la GUI
    EVENT_BATTERY_STATUS_UPDATED,
    EVENT_PACK_STATS_UPDATED,
    EVENT_SYSTEM_STATUS_UPDATED,
    EVENT_CONFIG_UPDATED,

    // Actions utilisateur depuis la GUI
    EVENT_USER_INPUT_SET_TARGET_SOC,
    EVENT_USER_INPUT_CHANGE_MODE,
    EVENT_USER_INPUT_ACK_ALARM,
    EVENT_USER_INPUT_WRITE_CONFIG,
    EVENT_USER_INPUT_RELOAD_CONFIG,

    EVENT_TYPE_MAX
} event_type_t;

/**
 * @brief Payload générique d'événement.
 *
 * On reste générique : le payload est un pointeur vers une structure
 * typée définie dans event_types.h (ou ailleurs). Tu peux aussi utiliser
 * un union si tu veux quelque chose de plus strict.
 */
typedef struct {
    event_type_t type;
    void        *data;   // pointeur vers struct spécifique (lifetime gérée par l'émetteur ou le bus)
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

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
