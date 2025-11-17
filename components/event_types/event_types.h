#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HMI_MODE_CONNECTED_S3 = 0,   // Mode connecté au backend S3 / télémétrie réseau attendue
    HMI_MODE_TINYBMS_AUTONOMOUS, // Mode autonome : UART TinyBMS uniquement, pas de réseau
} hmi_operation_mode_t;

typedef enum {
    NETWORK_STATE_NOT_CONFIGURED = 0,  // réseau non attendu ou non configuré
    NETWORK_STATE_ERROR,               // tentative en échec
    NETWORK_STATE_ACTIVE,              // WiFi et bridge accessibles
} network_state_t;

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
    EVENT_NETWORK_REQUEST_STARTED,      // début requête réseau (HTTP/WS)
    EVENT_NETWORK_REQUEST_FINISHED,     // fin requête réseau
    EVENT_ALERTS_ACTIVE_UPDATED,        // Liste des alertes actives (alert_list_t)
    EVENT_ALERTS_HISTORY_UPDATED,       // Historique des alertes (alert_list_t)
    EVENT_ALERT_FILTERS_UPDATED,        // Filtres/seuils appliqués aux alertes (alert_filters_t)
    EVENT_HISTORY_UPDATED,              // Historique batterie (history_snapshot_t)
    EVENT_HISTORY_EXPORTED,             // Résultat export CSV (history_export_result_t)

    // --- Événements "propres" (modèle traité/local) ---
    EVENT_BATTERY_STATUS_UPDATED,       // battery_status_t
    EVENT_PACK_STATS_UPDATED,           // pack_stats_t
    EVENT_SYSTEM_STATUS_UPDATED,        // system_status_t
    EVENT_NETWORK_FAILOVER_ACTIVATED,   // network_failover_event_t
    EVENT_OPERATION_MODE_CHANGED,       // operation_mode_event_t
    EVENT_CONFIG_UPDATED,               // si on gère la config locale

    // --- Événements émis par la GUI (user actions) ---
    EVENT_USER_INPUT_SET_TARGET_SOC,    // user_input_set_target_soc_t
    EVENT_USER_INPUT_CHANGE_MODE,       // user_input_change_mode_t
    EVENT_USER_INPUT_ACK_ALARM,         // futur
    EVENT_USER_INPUT_ACK_ALERT,         // user_input_ack_alert_t
    EVENT_USER_INPUT_REFRESH_ALERT_HISTORY, // requête GET /api/alerts/history
    EVENT_USER_INPUT_UPDATE_ALERT_FILTERS,  // alert_filters_t
    EVENT_USER_INPUT_REQUEST_HISTORY,   // user_input_history_request_t
    EVENT_USER_INPUT_EXPORT_HISTORY,    // user_input_history_export_t
    EVENT_USER_INPUT_WRITE_CONFIG,      // futur
    EVENT_USER_INPUT_RELOAD_CONFIG,     // recharger /api/config

    // --- Événements TinyBMS (UART direct) ---
    EVENT_TINYBMS_CONNECTED,            // TinyBMS connecté via UART
    EVENT_TINYBMS_DISCONNECTED,         // TinyBMS déconnecté
    EVENT_TINYBMS_REGISTER_UPDATED,     // tinybms_register_update_t
    EVENT_TINYBMS_CONFIG_CHANGED,       // configuration TinyBMS modifiée
    EVENT_USER_INPUT_TINYBMS_WRITE_REG, // user_input_tinybms_write_t
    EVENT_TINYBMS_UART_LOG,             // tinybms_uart_log_entry_t
    EVENT_TINYBMS_STATS_UPDATED,        // tinybms_stats_event_t

    // --- Événements CAN Bus (Phase 2+) ---
    EVENT_CAN_BUS_STARTED,              // Driver CAN démarré
    EVENT_CAN_BUS_STOPPED,              // Driver CAN arrêté
    EVENT_CAN_MESSAGE_TX,               // Message CAN transmis
    EVENT_CAN_MESSAGE_RX,               // Message CAN reçu (0x307)
    EVENT_CAN_KEEPALIVE_TIMEOUT,        // Timeout keepalive (pas de réponse GX)
    EVENT_CAN_ERROR,                    // Erreur bus CAN

    // --- Événements CVL State Machine (Phase 3+) ---
    EVENT_CVL_STATE_CHANGED,            // Changement d'état CVL (cvl_state_event_t)
    EVENT_CVL_LIMITS_UPDATED,           // CVL/CCL/DCL recalculés (cvl_limits_event_t)

    // --- Événements Energy Counters (Phase 3+) ---
    EVENT_ENERGY_COUNTERS_UPDATED,      // Compteurs énergie mis à jour

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
    network_state_t network_state; // État réseau global
    hmi_operation_mode_t operation_mode; // Mode courant : connecté S3 ou autonome TinyBMS
    bool telemetry_expected;   // true si on attend des flux /ws/* (mode connecté)
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

typedef struct {
    char path[64];
    char method[8];
} network_request_t;

typedef struct {
    network_request_t request; // requête concernée
    bool success;              // true si requête terminée avec succès
    int  status;               // code HTTP ou erreur interne
} network_request_status_t;

/**
 * @brief  Entrée d'alerte (active ou historique)
 */
#define ALERT_MAX_ENTRIES        32
#define ALERT_MESSAGE_MAX_LEN    96
#define ALERT_SOURCE_MAX_LEN     32
#define ALERT_STATUS_MAX_LEN     16

typedef struct {
    int      id;                                // identifiant unique de l'alerte
    int      code;                              // code optionnel (event_id)
    int      severity;                          // niveau de sévérité (0=info, 4=critique)
    uint64_t timestamp_ms;                      // horodatage
    bool     acknowledged;                      // true si acquittée
    char     message[ALERT_MESSAGE_MAX_LEN];    // texte
    char     source[ALERT_SOURCE_MAX_LEN];      // source (module)
    char     status[ALERT_STATUS_MAX_LEN];      // statut (active/resolved)
} alert_entry_t;

typedef struct {
    alert_entry_t entries[ALERT_MAX_ENTRIES];
    uint8_t       count;
} alert_list_t;

typedef struct {
    int  min_severity;                          // seuil minimal à afficher
    bool hide_acknowledged;                     // masque les alertes acquittées
    char source_filter[ALERT_SOURCE_MAX_LEN];   // filtre optionnel sur la source
} alert_filters_t;

/**
 * @brief  Commande : changer le target SOC
 */
typedef struct {
    float target_soc;          // %
} user_input_set_target_soc_t;

typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char static_ip[16];
    char mqtt_broker[64];
    char mqtt_topic_pub[64];
    char mqtt_topic_sub[64];
    int  can_bitrate;
    int  uart_baudrate;
    char uart_parity[2];
} hmi_config_t;

typedef struct {
    hmi_config_t config;
    bool mqtt_only;            // true -> POST /api/mqtt/config uniquement
} user_input_write_config_t;

typedef struct {
    bool include_mqtt;         // true -> charger aussi /api/mqtt/config
} user_input_reload_config_t;

/**
 * @brief Échantillon pour l'historique batterie
 */
typedef struct {
    uint64_t timestamp_ms;  // Horodatage en millisecondes
    float    voltage;       // V
    float    current;       // A
    float    temperature;   // °C
    float    soc;           // %
} history_sample_t;

typedef enum {
    HISTORY_RANGE_LAST_HOUR = 0,
    HISTORY_RANGE_LAST_DAY,
    HISTORY_RANGE_LAST_WEEK,
} history_range_t;

#define HISTORY_SNAPSHOT_MAX 512

typedef struct {
    history_range_t   range;                       // fenêtre demandée
    uint16_t          count;                       // nombre d'échantillons valides
    bool              from_backend;                // true si issu du backend
    history_sample_t  samples[HISTORY_SNAPSHOT_MAX];
} history_snapshot_t;

typedef struct {
    history_range_t range;
} user_input_history_request_t;

typedef struct {
    history_range_t range;
} user_input_history_export_t;

typedef struct {
    hmi_operation_mode_t mode;      // Mode demandé par l'utilisateur
} user_input_change_mode_t;

typedef struct {
    hmi_operation_mode_t mode;      // Mode courant
    bool telemetry_expected;        // Aligné avec system_status_t.telemetry_expected
} operation_mode_event_t;

typedef struct {
    int                  fail_count;       // Nombre de tentatives WiFi échouées
    int                  fail_threshold;   // Seuil ayant déclenché la bascule
    hmi_operation_mode_t new_mode;         // Mode choisi après bascule
} network_failover_event_t;

typedef struct {
    bool   success;
    char   path[64];
    size_t exported_count;
} history_export_result_t;

typedef struct {
    int alert_id;              // identifiant à acquitter
} user_input_ack_alert_t;

typedef struct {
    alert_filters_t filters;   // filtres/thresholds souhaités
} user_input_alert_filters_t;

/**
 * @brief  Mise à jour d'un registre TinyBMS
 */
typedef struct {
    uint16_t address;          // Adresse du registre
    uint16_t raw_value;        // Valeur brute
    float user_value;          // Valeur convertie pour affichage
    char key[32];              // Clé du registre (ex: "fully_charged_voltage_mv")
} tinybms_register_update_t;

typedef struct {
    char action[16];           // read / write / restart
    uint16_t address;          // Adresse concernée (0 si non applicable)
    int result;                // esp_err_t code
    bool success;              // true si succès
    char message[96];          // Résumé pour UI
} tinybms_uart_log_entry_t;

/**
 * @brief  Statistiques de communication TinyBMS
 */
typedef struct {
    uint32_t reads_ok;
    uint32_t reads_failed;
    uint32_t writes_ok;
    uint32_t writes_failed;
    uint32_t crc_errors;
    uint32_t timeouts;
    uint32_t nacks;
    uint32_t retries;
} tinybms_stats_t;

typedef struct {
    tinybms_stats_t stats;
    uint64_t        timestamp_ms;
} tinybms_stats_event_t;

/**
 * @brief  Commande : écrire un registre TinyBMS
 */
typedef struct {
    uint16_t address;          // Adresse du registre
    uint16_t value;            // Valeur à écrire
    char key[32];              // Clé du registre
} user_input_tinybms_write_t;

/**
 * @brief  Événement: Changement d'état CVL
 */
typedef struct {
    uint8_t previous_state;    // État CVL précédent (cvl_state_t)
    uint8_t new_state;         // Nouvel état CVL
    float soc_percent;         // SOC au moment du changement (%)
    uint64_t timestamp_ms;     // Timestamp
} cvl_state_event_t;

/**
 * @brief  Événement: Limites CVL/CCL/DCL mises à jour
 */
typedef struct {
    float cvl_voltage_v;       // Charge Voltage Limit (V)
    float ccl_current_a;       // Charge Current Limit (A)
    float dcl_current_a;       // Discharge Current Limit (A)
    uint8_t cvl_state;         // État CVL actuel (cvl_state_t)
    bool imbalance_hold_active; // Protection déséquilibre active
    bool cell_protection_active; // Protection cellule active
    uint64_t timestamp_ms;     // Timestamp
} cvl_limits_event_t;

#ifdef __cplusplus
}
#endif

#endif // EVENT_TYPES_H
