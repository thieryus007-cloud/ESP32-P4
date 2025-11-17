# CAN Publisher Component

## Description

Ce composant fait partie du plan d'intégration CAN BMS pour ESP32-P4 (voir `PLAN_BMS_CAN.md` à la racine).

Il contient les modules nécessaires pour:
1. Adapter les données TinyBMS au format attendu par le projet BMS de référence
2. Encoder et publier les 19 messages CAN Victron Energy
3. Gérer la state machine CVL (Charge Voltage Limit)
4. Orchestrer la publication périodique des messages CAN

## Guide d'intégration rapide

- **Initialisation** : créer/initialiser le bus d'événements applicatif puis appeler `can_victron_init()` suivi de `can_publisher_init()`.
- **Flux entrant** : publier `EVENT_TINYBMS_REGISTER_UPDATED` depuis `tinybms_client` pour que l'orchestrateur convertisse automatiquement les données UART.
- **Flux sortant** : les 19 trames Victron sont envoyées via `can_victron_publish_frame()` avec un throttling à 1000 ms par défaut.
- **Persistance énergie** : appeler `can_publisher_conversion_restore_energy_state()` au boot et `can_publisher_conversion_persist_energy_state()` lors de la mise en veille pour conserver les compteurs Wh.
- **Observabilité** : récupérer les statistiques avec `can_publisher_get_stats()` et surveiller `EVENT_CVL_LIMITS_UPDATED` pour afficher les limites calculées côté GUI.

## Structure (après Phase 4)

```
can_publisher/
├── tinybms_adapter.c/h    # ✅ Phase 1: Adaptateur tinybms_model → uart_bms_live_data_t
├── conversion_table.c/h   # ✅ Phase 3: Encodeurs des 19 messages CAN Victron
├── cvl_types.h            # ✅ Phase 3: Types pour state machine CVL (6 états)
├── cvl_logic.c/h          # ✅ Phase 3: Logique state machine CVL
├── cvl_controller.c/h     # ✅ Phase 3: Contrôleur CVL (orchestration)
├── can_publisher.h        # ✅ Phase 4: API complète orchestrateur
└── can_publisher.c        # ✅ Phase 4: Orchestrateur complet EventBus
```

**Statistiques**:
- **Lignes de code**: ~2350 lignes (C + headers)
- **Messages CAN**: 19 encodeurs Victron
- **State machine CVL**: 6 états
- **Orchestrateur**: EventBus intégré (Phase 4)

## Phase 1: Adaptateur TinyBMS ✅

### Fonctionnalités
- Structure `uart_bms_live_data_t` compatible BMS référence
- Conversion `tinybms_model` → `uart_bms_live_data_t`
- Mapping des 34 registres de configuration (0x012C-0x0157)
- API thread-safe avec statistiques

### Utilisation

```c
#include "tinybms_adapter.h"

// Convertir les données TinyBMS
uart_bms_live_data_t bms_data;
if (tinybms_adapter_convert(&bms_data) == ESP_OK) {
    // Données prêtes pour encodeurs CAN
}
```

## Phase 3: Encodeurs CAN + CVL ✅

### conversion_table.c/h - Encodeurs Victron

**19 messages CAN Victron** encodés depuis BMS référence:

| CAN ID | Description | Période | Fonction encoder |
|--------|-------------|---------|------------------|
| 0x305 | Keepalive | 1000ms | `encode_keepalive()` |
| 0x307 | Handshake Response | RX only | - |
| 0x351 | CVL/CCL/DCL | 1000ms | `encode_cvl_ccl_dcl()` |
| 0x355 | SOC/SOH | 1000ms | `encode_soc_soh()` |
| 0x356 | Voltage/Current/Temp | 1000ms | `encode_voltage_current_temp()` |
| 0x35A | Alarm Status | 1000ms | `encode_alarm_status()` |
| 0x35E | Manufacturer Info | 2000ms | `encode_manufacturer()` |
| 0x35F | Battery ID | 2000ms | `encode_battery_id()` |
| 0x370-0x371 | Battery Name (2 parts) | 2000ms | `encode_battery_name_*()` |
| 0x372 | Module Status | 1000ms | `encode_module_status()` |
| 0x373 | Cell V/T Extremes | 1000ms | `encode_cell_extremes()` |
| 0x374-0x377 | Min/Max Cell/Temp IDs | 1000ms | `encode_*_identifier()` |
| 0x378 | Energy Counters | 1000ms | `encode_energy_counters()` |
| 0x379 | Installed Capacity | 5000ms | `encode_installed_capacity()` |
| 0x380-0x381 | Serial Number (2 parts) | 5000ms | `encode_serial_number_*()` |
| 0x382 | Battery Family | 5000ms | `encode_battery_family()` |

**Caractéristiques**:
- ✅ Thread-safe (mutex pour energy counters)
- ✅ Gestion NVS pour persistance énergie
- ✅ Intégration puissance (V × I × Δt)
- ✅ Compteurs charged/discharged (Wh)
- ✅ Catalogue `g_can_publisher_channels[]`

**API Énergie**:
```c
// Intégrer échantillon BMS (accumulation énergie)
can_publisher_conversion_ingest_sample(const uart_bms_live_data_t *sample);

// Lire compteurs énergie
double charged_wh, discharged_wh;
can_publisher_conversion_get_energy_state(&charged_wh, &discharged_wh);

// Sauvegarder/restaurer depuis NVS
can_publisher_conversion_persist_energy_state();
can_publisher_conversion_restore_energy_state();
```

### cvl_logic.c/h - State Machine CVL

**6 états CVL** pour protection batterie:

```c
typedef enum {
    CVL_STATE_BULK = 0,              // Charge rapide initiale
    CVL_STATE_TRANSITION = 1,         // Transition vers float
    CVL_STATE_FLOAT_APPROACH = 2,     // Approche du float
    CVL_STATE_FLOAT = 3,              // Charge de maintien
    CVL_STATE_IMBALANCE_HOLD = 4,     // Protection déséquilibre
    CVL_STATE_SUSTAIN = 5,            // Mode maintenance bas SOC
} cvl_state_t;
```

**Logique de calcul**:
- Transitions basées sur SOC (bulk_soc_threshold, float_soc_threshold, etc.)
- Protection cellule haute tension (hystérésis)
- Réduction CVL dynamique si déséquilibre
- Limitation CCL/DCL selon l'état
- Anti-oscillation (max_recovery_step_v)

**API**:
```c
void cvl_compute_limits(
    const cvl_inputs_t *input,           // SOC, voltages, températures
    const cvl_config_snapshot_t *config, // Seuils de configuration
    const cvl_runtime_state_t *previous, // État précédent
    cvl_computation_result_t *result     // CVL/CCL/DCL calculés
);
```

### cvl_controller.c/h - Contrôleur CVL

Orchestration de la state machine CVL:
- Initialisation de la configuration
- Préparation des données BMS pour CVL
- Récupération des résultats CVL

**API**:
```c
// Initialiser contrôleur CVL
can_publisher_cvl_init();

// Préparer données BMS pour calcul CVL
can_publisher_cvl_prepare(const uart_bms_live_data_t *bms_data);

// Récupérer derniers résultats CVL
bool can_publisher_cvl_get_latest(can_publisher_cvl_result_t *result);
```

### cvl_types.h - Types CVL

Définitions de types pour la state machine CVL (cvl_state_t).

## Phase 2: Driver CAN ✅ (completed)

Driver TWAI bas niveau dans `components/can_victron/`:
- GPIO 22/21 pour ESP32-P4
- 500 kbps, Standard 11-bit IDs
- Keepalive automatique 0x305/0x307
- Thread-safe avec 3 mutex

## Phase 4: Intégration EventBus ✅

### Orchestrateur can_publisher.c

**Implémentation complète** de l'orchestrateur de publication CAN:

**Fonctionnalités**:
- ✅ Abonnement à `EVENT_TINYBMS_REGISTER_UPDATED`
- ✅ Conversion automatique via `tinybms_adapter`
- ✅ Encodage via `conversion_table` (19 messages)
- ✅ Publication vers `can_victron`
- ✅ Gestion state machine CVL
- ✅ Persistance NVS des compteurs énergie
- ✅ Publication événements `EVENT_CVL_LIMITS_UPDATED`
- ✅ Throttle 1000ms (évite surcharge bus)

**API**:
```c
// Initialiser l'orchestrateur
void can_publisher_init(void);

// Arrêter l'orchestrateur
void can_publisher_deinit(void);

// Récupérer statistiques
void can_publisher_get_stats(uint32_t *publish_count, uint64_t *last_publish_ms);
```

**Flux de données**:
```
EVENT_TINYBMS_REGISTER_UPDATED
    ↓
tinybms_adapter_convert()
    ↓
can_publisher_cvl_prepare()
    ↓
can_publisher_conversion_ingest_sample()
    ↓
conversion_table encoders (19 messages)
    ↓
can_victron_publish_frame()
    ↓
EVENT_CVL_LIMITS_UPDATED (publication)
```

**Thread-safety**: Mutex pour statistiques et publication

**Nouveaux événements ajoutés** (event_types.h):
- `EVENT_CAN_BUS_STARTED/STOPPED`
- `EVENT_CAN_MESSAGE_TX/RX`
- `EVENT_CAN_KEEPALIVE_TIMEOUT/ERROR`
- `EVENT_CVL_STATE_CHANGED`
- `EVENT_CVL_LIMITS_UPDATED`
- `EVENT_ENERGY_COUNTERS_UPDATED`

## Phases suivantes

### Phase 5: Keepalive complet
- Déjà partiellement dans can_victron (Phase 2)
- Intégration complète avec can_publisher

### Phase 6: Tests et validation
- Tests encodeurs CAN
- Validation state machine CVL
- Tests avec GX device réel

### Phase 7: GUI
- Écrans CAN status, config, BMS control

## Limitations actuelles

⚠️ **Registres temps réel manquants** dans `tinybms_model`:
- `0x0000-0x000F`: Tensions cellules
- `0x0024/0x0026`: Tension/courant pack
- `0x002D/0x002E`: SOH/SOC
- `0x0030`: Température
- `0x0066/0x0067`: Courants max

**Workaround**: L'adaptateur utilise des valeurs par défaut temporaires.
**Solution**: Étendre `tinybms_model` pour lire ces registres (Phase 4+).

## Dépendances

- `tinybms_model`: Cache des registres TinyBMS
- `esp_timer`: Timestamps
- `event_bus`: Bus d'événements (pub/sub)
- `event_types`: Définitions événements CAN/CVL
- `can_victron`: Driver CAN TWAI
- `freertos`: Mutex, tâches
- `nvs_flash`: Persistance compteurs énergie

## Référence

- **Plan complet**: `PLAN_BMS_CAN.md` à la racine
- **Source originale**: `Exemple/mac-local/BMS/main/can_publisher/`
- **Protocole Victron**: Victron Energy CAN-bus BMS specification

## Notes d'implémentation Phase 3

✅ **Copie exacte** depuis BMS référence:
- `conversion_table.c/h` - AUCUNE modification
- `cvl_logic.c/h` - AUCUNE modification
- `cvl_controller.c/h` - AUCUNE modification
- `cvl_types.h` - AUCUNE modification

⚠️ **Stub créé**:
- `can_publisher.h` - Types minimaux pour compilation (sera complété en Phase 4)

🎯 **Respect du principe**: "Ne rien inventer de nouveau"

## Notes d'implémentation Phase 4

✅ **Orchestrateur complet**:
- `can_publisher.c` - 242 lignes, orchestration EventBus
- `can_publisher.h` - API complète (was stub)
- `event_types.h` - 9 nouveaux événements CAN/CVL

✅ **Intégration**:
- Abonnement à `EVENT_TINYBMS_REGISTER_UPDATED`
- Utilisation encodeurs conversion_table
- Publication vers can_victron
- Gestion CVL via cvl_controller
- Persistance NVS pour énergie

✅ **Thread-safety**:
- Mutex pour statistiques publication
- Throttle 1000ms pour éviter surcharge

🎯 **Architecture événementielle**: Découplage complet entre modules via EventBus
