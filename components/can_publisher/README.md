# CAN Publisher Component

## Description

Ce composant fait partie du plan d'intégration CAN BMS pour ESP32-P4 (voir `PLAN_BMS_CAN.md` à la racine).

Il contient les modules nécessaires pour:
1. Adapter les données TinyBMS au format attendu par le projet BMS de référence
2. Publier des messages CAN Victron Energy (à venir)
3. Gérer la state machine CVL (Charge Voltage Limit) (à venir)

## Structure

```
can_publisher/
├── tinybms_adapter.c/h    # ✅ Phase 1: Adaptateur tinybms_model → uart_bms_live_data_t
├── conversion_table.c/h   # 🔴 Phase 3: Encodeurs de messages CAN (à copier)
├── cvl_controller.c/h     # 🔴 Phase 6: Contrôleur CVL (à copier)
├── cvl_logic.c/h          # 🔴 Phase 6: Logique state machine CVL (à copier)
├── cvl_types.h            # 🔴 Phase 6: Types CVL (à copier)
└── can_publisher.c/h      # 🔴 Phase 4: Orchestrateur (à créer)
```

## Phase 1: Adaptateur TinyBMS (ACTUEL)

### Objectif

Créer un pont entre:
- **ESP32-P4**: `tinybms_model` (cache de 34 registres de configuration 0x012C-0x0157)
- **BMS référence**: `uart_bms_live_data_t` (structure unifiée utilisée par can_publisher)

### État actuel

✅ **Implémenté**:
- Structure `uart_bms_live_data_t` définie dans `tinybms_adapter.h`
- Fonction `tinybms_adapter_convert()` pour la conversion
- Mapping des registres de configuration disponibles (capacité, seuils de sécurité, etc.)

⚠️ **Limitations actuelles**:
Les registres de **mesures temps réel** ne sont pas encore disponibles dans `tinybms_model`:
- `0x0000-0x000F`: Tensions des cellules (16 valeurs)
- `0x0024 (36)`: Tension pack (FLOAT)
- `0x0026 (38)`: Courant pack (FLOAT)
- `0x002D (45)`: SOH - State of Health (UINT16)
- `0x002E (46)`: SOC - State of Charge (UINT32)
- `0x0030 (48)`: Température moyenne (INT16)
- `0x0066 (102)`: Courant de décharge max (UINT16)
- `0x0067 (103)`: Courant de charge max (UINT16)

Ces registres devront être ajoutés à `tinybms_model` avant les phases CAN.

### Utilisation

```c
#include "tinybms_adapter.h"

// S'assurer que les registres sont cachés
tinybms_model_read_all();

// Vérifier que l'adaptateur est prêt
if (tinybms_adapter_is_ready()) {
    uart_bms_live_data_t bms_data;

    // Convertir les données
    if (tinybms_adapter_convert(&bms_data) == ESP_OK) {
        // Utiliser bms_data pour CAN publisher
        // can_publisher_update(&bms_data);
    }
}
```

## Phases suivantes

### Phase 2: Driver CAN (à venir)
Copier `can_victron.c/h` depuis `Exemple/mac-local/BMS/main/can_victron/`

### Phase 3: Encodeurs messages (à venir)
Copier `conversion_table.c/h` depuis `Exemple/mac-local/BMS/main/can_publisher/`

### Phase 4: Intégration EventBus (à venir)
Créer `can_publisher.c/h` pour orchestrer la conversion et la publication

### Phase 5: Keepalive et handshake (à venir)
Implémenter messages 0x305 (keepalive) et 0x307 (handshake)

### Phase 6: State Machine CVL (à venir)
Copier les fichiers CVL depuis `Exemple/mac-local/BMS/main/can_publisher/`

### Phase 7: GUI (à venir)
Créer les écrans GUI pour CAN status, config et BMS control

## TODOs critiques avant Phase 2

1. **Étendre tinybms_model** pour supporter les registres de mesures temps réel (0x0000-0x004F)
2. **Implémenter la lecture périodique** des mesures temps réel dans tinybms_client
3. **Publier des événements** pour les mises à jour de mesures (pas seulement config)

## Dépendances

- `tinybms_model`: Pour accéder au cache des registres
- `esp_timer`: Pour les timestamps

## Référence

Voir `PLAN_BMS_CAN.md` à la racine du projet pour le plan complet d'intégration.
