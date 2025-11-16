#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Nombre max de cellules supportées dans les structures.
 * Doit être cohérent avec ton pack réel (ex : 16 ou 32).
 */
#define PACK_MAX_CELLS 32

/**
 * ===========================================================
 *  TYPES D’ÉVÉNEMENTS PUBLIÉS SUR L’EVENTBUS
 * ===========================================================
 */

typedef enum {
    EVENT_TYPE_NONE = 0,

    // --- Événements provenant du S3 / WebSocket / JSON ---
    EVENT_REMOTE_TELEMETRY_UPDATE,      // JSON brut → batterie globale
    EVENT_REMOTE_SYSTEM_EVENT,          // JSON brut → événements système
    EVENT_REMOTE_CONFIG_SNAPSHOT,       // non utilisé pour l'instant
    EVENT_REMOTE_CMD_RESULT,            // résultat retour /api ou WS commande

    // --- Événements "propres" (modèle traité/local) ---
    EVENT_BATTERY_STATUS_UPDATED,       // battery_status_t
    EVENT_PACK_STATS_UPDATED,           // pack_stats_t
    EVENT_SYSTEM_STATUS_UPDATED,        // system_status_t
    EVENT_CONFIG_UPDATED,               // si on gère la config locale

    // --- Événements émis par la GUI (user actions) ---
    EVENT_USER_INPUT_SET_TARGET_SOC,    // user_input_set_target_soc_t
    EVENT_USER_INPUT_CHANGE_MODE,       // futur
    EVENT_USER_INPUT_ACK_ALARM,         // futur
    EVENT_USER_INPUT_WRITE_CONFIG,      // futur

    EVENT_TYPE_MAX
} event_type_t;

/**
 * ===========================================================
 *  STRUCTURES DES PAYLOADS
 * ===========================================================
 */

/**
 * @brief  Statut batterie global (dérivé du JSON telemetry)
 *         utilisé pour l'écran "Home" et résumé "Pack" & "Power".
 */
typedef struct {
    float soc;            // State of Charge (%)
    float soh;            // State of Health (%)
    float voltage;        // V (pack_voltage_v)
    float current;        // A (pack_current_a)
    float power;          // W (V * A ou champ JSON dédié)
    float temperature;    // °C moyenne du pack

    // Flags “health” / état système dérivés des infos telemetry
    bool  bms_ok;         // TinyBMS/pack_voltage OK
    bool  can_ok;         // Energie CAN présente
    bool  mqtt_ok;        // à affiner avec /ws/events
    bool  tinybms_ok;     // BMS réel OK
} battery_status_t;

/**
 * @brief  Statut système global (dérivé de /ws/events)
 *         utilisé pour les LED état WiFi/Storage/Errors.
 */
typedef struct {
    bool wifi_connected;       // WiFi STA connecté
    bool server_reachable;     // HMI → S3 OK
    bool storage_ok;           // stockage interne OK
    bool has_error;            // erreur globale (à affiner selon events)
} system_status_t;

/**
 * @brief  Stats des cellules et du pack, utilisées pour les écrans "Pack" et "Cells".
 *
 * Le balancing est modélisé par :
 *  - balancing[i] : true si la cellule i est en équilibrage actif
 *  - bal_start_mv / bal_stop_mv : seuils (mV) si le JSON les fournit
 */
typedef struct {
    uint8_t cell_count;                // nombre de cellules détectées

    float   cell_min;                  // mV (cellule la plus basse)
    float   cell_max;                  // mV (cellule la plus haute)
    float   cell_delta;                // mV (max - min)
    float   cell_avg;                  // mV moyenne

    float   cells[PACK_MAX_CELLS];     // tensions individuelles mV
    bool    balancing[PACK_MAX_CELLS]; // true si balancing actif sur la cellule

    float   bal_start_mv;              // seuil de démarrage balancing (mV) si dispo
    float   bal_stop_mv;               // seuil d’arrêt balancing (mV) si dispo
} pack_stats_t;

/**
 * @brief  Résultat d’une commande envoyée par l’HMI
 *         (via WS commands ou /api/*).
 */
typedef struct {
    bool  success;             // OK / ERROR
    int   error_code;          // code interne / HTTP
    char  message[64];         // texte utilisateur
} cmd_result_t;

/**
 * @brief  Commande : changer le target SOC
 */
typedef struct {
    float target_soc;          // %
} user_input_set_target_soc_t;

/**
 * @brief  Payload générique d'événement EventBus
 */
typedef struct {
    event_type_t type;
    void        *data;         // pointeur vers une des structs ci-dessus
} event_t;

#ifdef __cplusplus
}
#endif

#endif // EVENT_TYPES_H
