# CAN Victron Component

## Description

Driver CAN bas niveau pour ESP32-P4 utilisant le périphérique TWAI (Two-Wire Automotive Interface).
Ce composant fait partie du plan d'intégration CAN BMS (voir `PLAN_BMS_CAN.md` Phase 2).

**Source**: Copié depuis `Exemple/mac-local/BMS/main/can_victron/` avec adaptations pour ESP32-P4.

## Caractéristiques

- **Protocole**: Victron Energy CAN (Standard 11-bit IDs)
- **Vitesse**: 500 kbps (obligatoire pour Victron)
- **GPIO ESP32-P4**: TX=22, RX=21
- **Keepalive**: Messages 0x305/0x307 pour handshake avec GX device
- **Thread-safe**: Double mutex (TWAI hardware + driver state)
- **Event bus**: Intégration avec le bus d'événements ESP32-P4

## Mise en route express

1. Initialiser le bus d'événements global (`event_bus_init()`).
2. Appeler `can_victron_init()` puis `can_victron_set_event_bus(&event_bus)` avant de lancer les tâches applicatives.
3. Connecter les broches GPIO22 (TX) et GPIO21 (RX) au réseau CAN 500 kbps.
4. Vérifier la réception du handshake 0x307 (événement `EVENT_CAN_MESSAGE_RX`) pour confirmer la présence du GX device.
5. Démarrer `can_publisher_init()` si vous souhaitez publier les 19 trames Victron.

## Configuration GPIO

```c
// ESP32-P4 (Waveshare ESP32-P4-WIFI6-Touch-LCD-7B)
#define CAN_TX_GPIO  22
#define CAN_RX_GPIO  21
```

Ces GPIO sont **fixes** pour ESP32-P4 selon le plan d'intégration CAN.

## Adaptations depuis BMS référence

### Changements GPIO
- **BMS référence**: GPIO 7 (TX) / 6 (RX)
- **ESP32-P4**: GPIO 22 (TX) / 21 (RX)

### Configuration statique
Le composant utilise des valeurs de configuration **statiques** au lieu de `config_manager`:

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| `keepalive_interval_ms` | 1000 | Intervalle keepalive (0x305) |
| `keepalive_timeout_ms` | 5000 | Timeout sans réponse GX |
| `keepalive_retry_ms` | 2000 | Intervalle entre retries |
| `manufacturer` | "Enepaq" | Nom fabricant (0x35E) |
| `battery_name` | "ESP32-P4-BMS" | Nom batterie (0x370/371) |
| `battery_family` | "LiFePO4" | Famille batterie (0x382) |
| `serial_number` | "ESP32P4-00000001" | Numéro de série (0x380/381) |

### Dépendances supprimées/modifiées
- ❌ `config_manager.h` → Configuration statique
- ✅ `app_events.h` → Event IDs importés depuis `event_types.h` (Phase 5)
- ❌ `can_config_defaults.h` → Constantes définies localement

## API

### Initialisation

```c
#include "can_victron.h"

// 1. Initialiser le driver CAN
can_victron_init();

// 2. Enregistrer le callback d'événements (optionnel)
can_victron_set_event_publisher(my_event_publisher);
```

### Publication de frames

```c
// Publier un message CAN
uint8_t data[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
esp_err_t err = can_victron_publish_frame(
    0x351,           // CAN ID (ex: CVL/CCL/DCL)
    data,            // Données (max 8 bytes)
    8,               // DLC (Data Length Code)
    "CVL/CCL/DCL"    // Description (pour logs)
);

if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to publish CAN frame: %s", esp_err_to_name(err));
}
```

### Récupération du statut

```c
can_victron_status_t status;
if (can_victron_get_status(&status) == ESP_OK) {
    printf("Driver started: %d\n", status.driver_started);
    printf("Keepalive OK: %d\n", status.keepalive_ok);
    printf("TX frames: %llu\n", status.tx_frame_count);
    printf("RX frames: %llu\n", status.rx_frame_count);
    printf("Bus state: %d\n", status.bus_state);
}
```

### Arrêt

```c
// Arrêter le driver CAN proprement
can_victron_deinit();
```

## Messages CAN Victron

Le driver gère automatiquement:

### 0x305 - Keepalive (TX)
- **Période**: 1000 ms
- **Fonction**: Maintenir connexion avec GX device
- **Format**: 8 bytes à 0x00

### 0x307 - Handshake (RX)
- **Réception uniquement**
- **Fonction**: Confirmation de connexion depuis GX device
- **Signature**: Bytes 4-6 = "VIC" (0x56 0x49 0x43)

## Thread Safety

Le composant utilise 3 mutex:
- **s_twai_mutex**: Protège l'accès au hardware TWAI
- **s_driver_state_mutex**: Protège le flag `s_driver_started`
- **s_keepalive_mutex**: Protège les variables keepalive

Toutes les fonctions publiques sont **thread-safe** avec timeout de 100ms.

## Task CAN

Une task FreeRTOS dédiée (`can_victron_task`) gère:
- Réception des messages CAN (queue RX)
- Envoi périodique du keepalive 0x305
- Détection des timeouts keepalive
- Récupération automatique des erreurs bus

**Priorité**: `tskIDLE_PRIORITY + 6`
**Stack**: 4096 bytes

## Événements

Le driver publie les événements suivants (depuis `event_types.h` - Phase 5):

| Event ID | Description |
|----------|-------------|
| `EVENT_CAN_BUS_STARTED` | Driver CAN démarré |
| `EVENT_CAN_BUS_STOPPED` | Driver CAN arrêté |
| `EVENT_CAN_MESSAGE_TX` | Message CAN transmis |
| `EVENT_CAN_MESSAGE_RX` | Message CAN reçu (handshake 0x307) |
| `EVENT_CAN_KEEPALIVE_TIMEOUT` | Timeout keepalive (pas de réponse GX) |
| `EVENT_CAN_ERROR` | Erreur bus CAN |

**Note Phase 5**: Les Event IDs sont maintenant importés depuis `event_types.h` (partagés avec can_publisher).
Publication complète via event_bus en cours d'intégration (TODO).

## Statistiques

Le driver collecte les statistiques suivantes:
- Compteurs TX/RX (frames et bytes)
- Erreurs TX/RX
- Arbitration lost, bus errors, bus off
- Occupancy bus (%)
- État TWAI (running, stopped, bus off, recovering)

## Phases complétées

✅ **Phase 2**: Driver CAN TWAI complet
- Transmission/réception CAN 500 kbps
- Keepalive 0x305 automatique (1000ms)
- Handshake 0x307 avec validation signature "VIC"
- Gestion timeout et reconnexion
- Thread-safe avec 3 mutex
- Task FreeRTOS dédiée

✅ **Phase 3**: Encodeurs messages CAN (dans can_publisher)
✅ **Phase 4**: Orchestrateur EventBus (dans can_publisher)
✅ **Phase 5**: Intégration event_types.h + Publication événements
- Event IDs partagés (EVENT_CAN_*)
- Détection et logging timeout keepalive
- Détection et logging handshake 0x307
- ✅ Publication EVENT_CAN_KEEPALIVE_TIMEOUT
- ✅ Publication EVENT_CAN_MESSAGE_RX (handshake 0x307)
- API: `can_victron_set_event_bus(event_bus_t *bus)`

## Publication événements

Le driver publie maintenant les événements via event_bus:

**EVENT_CAN_KEEPALIVE_TIMEOUT**:
- Publié lors du timeout keepalive (> 5000ms sans réponse GX)
- Permet à l'interface GUI d'afficher l'état de connexion

**EVENT_CAN_MESSAGE_RX**:
- Publié lors de la réception d'un handshake 0x307 valide
- Signature "VIC" validée (bytes 4-6)
- Confirme la connexion avec le GX device

**Utilisation**:
```c
// Dans hmi_main.cpp ou similaire
can_victron_init();
can_victron_set_event_bus(&event_bus);
```

## Dépendances

- `driver` (TWAI driver ESP-IDF)
- `esp_timer` (Timestamps)
- `freertos` (Tasks, mutex)
- `event_bus` (Publication d'événements)
- `event_types` (Event IDs CAN/CVL - Phase 5)

## Référence

- **Plan complet**: `PLAN_BMS_CAN.md` à la racine
- **Source originale**: `Exemple/mac-local/BMS/main/can_victron/`
- **Protocole Victron**: Victron Energy CAN-bus BMS specification
- **Hardware**: ESP32-P4-WIFI6-Touch-LCD-7B (Waveshare)
