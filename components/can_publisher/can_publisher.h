#pragma once

/**
 * @file can_publisher.h
 * @brief CAN Publisher - Orchestrateur Phase 4
 *
 * Orchestre la publication des messages CAN Victron depuis les données TinyBMS:
 * - S'abonne aux événements TinyBMS
 * - Convertit via tinybms_adapter
 * - Encode via conversion_table (19 messages Victron)
 * - Gère la state machine CVL
 * - Publie via can_victron
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "tinybms_adapter.h"  // Pour uart_bms_live_data_t

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of buffered CAN frames. */
#define CAN_PUBLISHER_MAX_BUFFER_SLOTS 8U

/**
 * @brief Lightweight CAN frame representation
 */
typedef struct {
    uint32_t id;          /**< CAN identifier (11-bit for Victron) */
    uint8_t dlc;          /**< Data length code (max 8) */
    uint8_t data[8];      /**< Frame payload */
    uint64_t timestamp_ms;/**< Timestamp */
} can_publisher_frame_t;

/**
 * @brief Encoder function signature
 *
 * Converts uart_bms_live_data_t to CAN frame.
 */
typedef bool (*can_publisher_fill_frame_fn_t)(const uart_bms_live_data_t *bms_data,
                                               can_publisher_frame_t *out_frame);

/**
 * @brief CAN channel descriptor
 */
typedef struct {
    uint16_t pgn;                            /**< Victron PGN (11-bit) */
    uint32_t can_id;                         /**< CAN ID */
    uint8_t dlc;                             /**< Expected DLC */
    can_publisher_fill_frame_fn_t fill_fn;   /**< Encoder function */
    const char *description;                 /**< Description */
    uint32_t period_ms;                      /**< Transmission period (ms) */
} can_publisher_channel_t;

/**
 * @brief Frame publisher function signature
 */
typedef esp_err_t (*can_publisher_frame_publish_fn_t)(uint32_t can_id,
                                                      const uint8_t *data,
                                                      size_t length,
                                                      const char *description);

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize CAN publisher
 *
 * - Initialise le contrôleur CVL
 * - Restaure les compteurs d'énergie depuis NVS
 * - S'abonne à EVENT_TINYBMS_REGISTER_UPDATED
 * - Prépare la publication des 19 messages CAN Victron
 *
 * @note Doit être appelé après can_victron_init() et tinybms_model_init()
 */
void can_publisher_init(void);

/**
 * @brief Deinitialize CAN publisher
 *
 * - Se désabonne des événements
 * - Sauvegarde les compteurs d'énergie dans NVS
 * - Libère les ressources
 */
void can_publisher_deinit(void);

/**
 * @brief Get publisher statistics
 *
 * @param publish_count Output: nombre de publications CAN réalisées
 * @param last_publish_ms Output: timestamp dernière publication (ms)
 */
void can_publisher_get_stats(uint32_t *publish_count, uint64_t *last_publish_ms);

#ifdef __cplusplus
}
#endif
