# Plan d'intégration CAN BMS sur ESP32-P4

**Projet**: ESP32-P4 BMS Autonome avec interface CAN
**Base**: Projet BMS de référence (Exemple/mac-local/BMS)
**Objectif**: Porter la fonctionnalité CAN BMS vers ESP32-P4 en respectant EXACTEMENT l'implémentation existante

---

## 🎯 Vue d'ensemble

Le projet BMS de référence (dans `Exemple/mac-local/BMS`) implémente un **gateway UART-vers-CAN** qui:
- Communique avec TinyBMS via **UART** (115200 baud)
- Synthétise des messages **CAN Victron Energy** à partir des données UART
- Broadcast vers onduleurs/chargeurs compatibles Victron
- Implémente une **state machine CVL** pour protection batterie
- Fournit une interface web pour monitoring et configuration

**Point critique**: TinyBMS **N'UTILISE PAS** CAN nativement. Toute communication CAN est générée par l'ESP32 à partir des données UART.

**Architecture de référence**:
```
Exemple/mac-local/BMS/main/
├── can_victron/          # Driver CAN TWAI bas niveau
├── can_publisher/        # Orchestrateur + CVL state machine
├── uart_bms/             # Client UART TinyBMS
├── event_bus/            # Bus d'événements (similaire à ESP32-P4)
├── monitoring/           # Métriques système
├── web_server/           # Interface web
└── app_main.c            # Orchestration globale
```

### État d'avancement rapide

- ✅ Phase 1-3 répliquées : adaptation TinyBMS → UART, encodeurs CAN Victron, state machine CVL.
- ✅ Phase 4 : orchestrateur CAN côté ESP32-P4 avec EventBus partagé.
- ✅ Phase 5 (en cours de finalisation) : publication d'événements CAN (`EVENT_CAN_*`) et intégration avec la GUI/diagnostics.
- 🚧 Reste à faire : validations terrain 500 kbps, intégration complète dans `hmi_main`, sécurisation NVS pour les compteurs d'énergie.

---

## 📋 Spécifications CAN (du projet BMS existant)

### Protocole: Victron Energy CAN

**Configuration matérielle**:
```c
// Vitesse CAN
#define CAN_BITRATE_BPS 500000U          // 500 kbps (OBLIGATOIRE)

// GPIO ESP32-P4 (selon votre README)
#define CAN_TX_GPIO 22                    // GPIO22 (vs GPIO7 sur BMS original)
#define CAN_RX_GPIO 21                    // GPIO21 (vs GPIO6 sur BMS original)

// Format
Frame Type: Standard 11-bit IDs (max 0x7FF)
Max DLC: 8 bytes per frame
```

### Messages CAN Victron (19 messages)

| CAN ID | Description | Période (ms) | DLC | Priorité |
|--------|-------------|--------------|-----|----------|
| **0x305** | **Keepalive** (handshake GX) | 1000 | 8 | CRITIQUE |
| **0x307** | **Handshake Response** (RX from GX) | On receive | 8 | CRITIQUE |
| **0x351** | **CVL/CCL/DCL** (Charge limits) | 1000 | 8 | CRITIQUE |
| **0x355** | **SOC/SOH** (State of Charge/Health) | 1000 | 8 | CRITIQUE |
| **0x356** | **Voltage/Current/Temp** | 1000 | 8 | CRITIQUE |
| **0x35A** | **Alarm Status** | 1000 | 8 | HAUTE |
| **0x35E** | **Manufacturer Info** | 2000 | 8 | BASSE |
| **0x35F** | **Battery ID** (firmware, capacity) | 2000 | 8 | BASSE |
| **0x370** | **Battery Name Part 1** | 2000 | 8 | BASSE |
| **0x371** | **Battery Name Part 2** | 2000 | 8 | BASSE |
| **0x372** | **Module Status Counts** | 1000 | 8 | MOYENNE |
| **0x373** | **Cell Voltage/Temp Extremes** | 1000 | 8 | HAUTE |
| **0x374** | **Min Cell Identifier** | 1000 | 8 | MOYENNE |
| **0x375** | **Max Cell Identifier** | 1000 | 8 | MOYENNE |
| **0x376** | **Min Temp Identifier** | 1000 | 8 | MOYENNE |
| **0x377** | **Max Temp Identifier** | 1000 | 8 | MOYENNE |
| **0x378** | **Energy Counters** (Wh in/out) | 1000 | 8 | HAUTE |
| **0x379** | **Installed Capacity** | 5000 | 8 | BASSE |
| **0x380** | **Serial Number Part 1** | 5000 | 8 | BASSE |
| **0x381** | **Serial Number Part 2** | 5000 | 8 | BASSE |
| **0x382** | **Battery Family** | 5000 | 8 | BASSE |

---

## 📦 Formats de messages (EXACTS du projet BMS)

### 0x305 - Keepalive (CRITIQUE)

**Fonction**: Heartbeat pour maintenir connexion avec GX device

```c
// Tous les 1000ms
uint8_t data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// Implémentation spécifique (voir conversion_table.c)
```

---

### 0x307 - Handshake Response (RX ONLY)

**Fonction**: Réponse du GX device pour confirmer connexion

```c
// RÉCEPTION uniquement
// Bytes 4-6: Signature "VIC" (0x56 0x49 0x43)
if (data[4] == 'V' && data[5] == 'I' && data[6] == 'C') {
    // Victron GX device détecté
    connection_established = true;
}
```

---

### 0x351 - CVL/CCL/DCL (CRITIQUE)

**Fonction**: Limites de charge/décharge

```c
// Bytes 0-1: CVL (Charge Voltage Limit)
uint16_t cvl_raw = (uint16_t)(cvl_volts * 10.0);  // Scale: 0.1V
data[0] = cvl_raw & 0xFF;
data[1] = (cvl_raw >> 8) & 0xFF;

// Bytes 2-3: CCL (Max Charge Current)
uint16_t ccl_raw = (uint16_t)(ccl_amps * 10.0);  // Scale: 0.1A
data[2] = ccl_raw & 0xFF;
data[3] = (ccl_raw >> 8) & 0xFF;

// Bytes 4-5: DCL (Max Discharge Current)
uint16_t dcl_raw = (uint16_t)(dcl_amps * 10.0);  // Scale: 0.1A
data[4] = dcl_raw & 0xFF;
data[5] = (dcl_raw >> 8) & 0xFF;

// Bytes 6-7: Reserved
data[6] = 0x00;
data[7] = 0x00;

// Source des valeurs:
// - CVL: Calculé par state machine CVL (voir section State Machine)
// - CCL: Calculé par state machine + TinyBMS register 103
// - DCL: TinyBMS register 102 (max_discharge_current_a)
```

**Exemple**:
- CVL = 54.4V → 544 (0x0220) → `[0x20, 0x02, ...]`
- CCL = 50.0A → 500 (0x01F4) → `[..., 0xF4, 0x01, ...]`
- DCL = 100.0A → 1000 (0x03E8) → `[..., 0xE8, 0x03, 0x00, 0x00]`

---

### 0x355 - SOC/SOH (CRITIQUE)

**Fonction**: État de charge et santé de la batterie

```c
// Bytes 0-1: SOC (State of Charge)
// Source: TinyBMS register 46 (0x002E)
// Scale TinyBMS: 0.000001 (1ppm) → Scale CAN: 1% (0-10000 = 0-100.00%)
uint16_t soc_raw = (uint16_t)round(soc_pct * 100.0);
data[0] = soc_raw & 0xFF;
data[1] = (soc_raw >> 8) & 0xFF;

// Bytes 2-3: SOH (State of Health)
// Source: TinyBMS register 45 (0x002D)
// Scale TinyBMS: 0.002% → Scale CAN: 1% (0-10000)
uint16_t soh_raw = (uint16_t)round(soh_pct * 100.0);
data[2] = soh_raw & 0xFF;
data[3] = (soh_raw >> 8) & 0xFF;

// Bytes 4-5: High-resolution SOC
// Scale: 0.01% (0-10000 = 0-100.00%)
uint16_t soc_hires = (uint16_t)(soc_pct * 100.0);
data[4] = soc_hires & 0xFF;
data[5] = (soc_hires >> 8) & 0xFF;

// Bytes 6-7: Reserved
data[6] = 0x00;
data[7] = 0x00;
```

**Exemple**:
- SOC = 85.53% → 8553 (0x2169) → `[0x69, 0x21, ..., 0x69, 0x21, 0x00, 0x00]`

---

### 0x356 - Voltage/Current/Temperature (CRITIQUE)

**Fonction**: Mesures principales de la batterie

```c
// Bytes 0-1: Pack Voltage
// Source: TinyBMS register 36 (0x0024) FLOAT
// Scale: 0.01V (multiply by 100)
int16_t voltage_raw = (int16_t)lrint(voltage_v * 100.0);
data[0] = voltage_raw & 0xFF;
data[1] = (voltage_raw >> 8) & 0xFF;

// Bytes 2-3: Pack Current (SIGNED!)
// Source: TinyBMS register 38 (0x0026) FLOAT
// Scale: 0.1A (multiply by 10)
// Convention: Positive = charge, Negative = discharge
int16_t current_raw = (int16_t)lrint(current_a * 10.0);
data[2] = current_raw & 0xFF;
data[3] = (current_raw >> 8) & 0xFF;

// Bytes 4-5: Pack Temperature (SIGNED!)
// Source: TinyBMS register 48 (0x0030)
// Scale: 0.1°C (multiply by 10)
int16_t temp_raw = (int16_t)lrint(temp_c * 10.0);
data[4] = temp_raw & 0xFF;
data[5] = (temp_raw >> 8) & 0xFF;

// Bytes 6-7: Reserved
data[6] = 0x00;
data[7] = 0x00;
```

**Exemple**:
- Voltage = 51.2V → 5120 (0x1400) → `[0x00, 0x14, ...]`
- Current = -25.3A → -253 (0xFF03) → `[..., 0x03, 0xFF, ...]`
- Temp = 23.5°C → 235 (0x00EB) → `[..., 0xEB, 0x00, 0x00, 0x00]`

---

### 0x35A - Alarm Status (Format complexe)

**Fonction**: États d'alarmes et warnings

**Structure**: 2 champs parallèles de 4 bytes
- Bytes 0-3: ALARM (niveau 2 uniquement)
- Bytes 4-7: WARNING (niveau 1+2)

```c
// Encodage: 00 = unsupported, 01 = OK, 10 = active

// Byte 0 (Alarms):
data[0] = 0x00;
data[0] |= (overall_alarm & 0x3) << 0;      // Bits 0-1: État général
data[0] |= (overvoltage_alarm & 0x3) << 2;  // Bits 2-3: Surtension pack
data[0] |= (undervoltage_alarm & 0x3) << 4; // Bits 4-5: Sous-tension pack
data[0] |= (overtemp_alarm & 0x3) << 6;     // Bits 6-7: Surchauffe (≥65°C)

// Byte 1 (Alarms):
data[1] = 0x00;
data[1] |= (undertemp_alarm & 0x3) << 0;    // Bits 0-1: Trop froid (≤-10°C)
data[1] |= (high_temp_charge_alarm & 0x3) << 2;  // Bits 2-3: Temp charge élevée
data[1] |= (0x3) << 4;                      // Bits 4-5: Reserved
data[1] |= (discharge_oc_alarm & 0x3) << 6; // Bits 6-7: Surintensité décharge

// Byte 2 (Alarms):
data[2] = 0x00;
data[2] |= (charge_oc_alarm & 0x3) << 0;    // Bits 0-1: Surintensité charge
data[2] |= (0x3F) << 2;                     // Bits 2-7: Reserved

// Byte 3 (Alarms):
data[3] = 0x00;
data[3] |= (cell_imbalance_alarm & 0x3) << 0;  // Bits 0-1: Déséquilibre (≥40mV)
data[3] |= (0x3F) << 2;                     // Bits 2-7: Reserved

// Bytes 4-7: Mirror structure pour WARNING (mêmes positions)
data[4] = warning_byte_0;  // Seuils plus bas que alarmes
data[5] = warning_byte_1;
data[6] = warning_byte_2;
data[7] = warning_byte_3;

// Byte 5 spécial (Warnings uniquement):
data[5] |= (low_temp_charge_warning & 0x3) << 4;  // Bits 4-5

// Byte 7 spécial (Status):
data[7] |= (0x1) << 2;  // Bits 2-3: System online indicator
```

**Seuils d'alarmes** (du projet BMS):
- Overvoltage: `pack_voltage ≥ overvoltage_cutoff`
- Undervoltage: `pack_voltage ≤ undervoltage_cutoff`
- Overtemp: `max_temp ≥ 65°C` (défaut)
- Undertemp: `min_temp ≤ -10°C`
- Discharge OC: `current ≥ 80% × DCL`
- Charge OC: `current ≥ 80% × CCL`
- Cell imbalance: `max_cell_mv - min_cell_mv ≥ 40mV`

---

### 0x35E - Manufacturer Info

```c
// ASCII string "Enepaq" (null-padded to 8 bytes)
strcpy((char *)data, "Enepaq");
data[6] = 0x00;
data[7] = 0x00;

// Source: TinyBMS register 500 reference
```

---

### 0x35F - Battery Identification

```c
// Bytes 0-3: Firmware Version
// Source: TinyBMS register 501
uint32_t fw_version = tinybms_firmware_version;
data[0] = fw_version & 0xFF;
data[1] = (fw_version >> 8) & 0xFF;
data[2] = (fw_version >> 16) & 0xFF;
data[3] = (fw_version >> 24) & 0xFF;

// Bytes 4-7: Battery Capacity
// Source: TinyBMS register 306 (0x0132)
// Scale: 0.01Ah (multiply by 100)
uint32_t capacity_raw = (uint32_t)(capacity_ah * 100.0);
data[4] = capacity_raw & 0xFF;
data[5] = (capacity_raw >> 8) & 0xFF;
data[6] = (capacity_raw >> 16) & 0xFF;
data[7] = (capacity_raw >> 24) & 0xFF;

// Exemple: 100.0Ah → 10000 (0x00002710)
```

---

### 0x373 - Cell Voltage/Temperature Extremes

```c
// Bytes 0-1: Min cell voltage
// Scale: 0.001V = 1mV (multiply by 1000)
uint16_t min_cell_raw = (uint16_t)(min_cell_mv);
data[0] = min_cell_raw & 0xFF;
data[1] = (min_cell_raw >> 8) & 0xFF;

// Bytes 2-3: Max cell voltage
uint16_t max_cell_raw = (uint16_t)(max_cell_mv);
data[2] = max_cell_raw & 0xFF;
data[3] = (max_cell_raw >> 8) & 0xFF;

// Bytes 4-5: Min temperature
// Scale: 0.1°C (multiply by 10)
int16_t min_temp_raw = (int16_t)lrint(pack_temp_min_c * 10.0);
data[4] = min_temp_raw & 0xFF;
data[5] = (min_temp_raw >> 8) & 0xFF;

// Bytes 6-7: Max temperature
int16_t max_temp_raw = (int16_t)lrint(pack_temp_max_c * 10.0);
data[6] = max_temp_raw & 0xFF;
data[7] = (max_temp_raw >> 8) & 0xFF;

// Exemple: min_cell=3250mV, max_cell=3280mV, min_temp=22.5°C, max_temp=24.8°C
// → [0xBA, 0x0C, 0xD0, 0x0C, 0xE1, 0x00, 0xF8, 0x00]
```

---

### 0x378 - Energy Counters (Thread-Protected!)

**Fonction**: Compteurs d'énergie chargée/déchargée

```c
// CRITIQUE: Protection mutex obligatoire!
xSemaphoreTake(s_energy_mutex, portMAX_DELAY);

// Bytes 0-3: Energy Charged
// Scale: 100Wh units (divide kWh by 100)
// Source: Accumulateur interne thread-safe
uint32_t charged_raw = (uint32_t)(energy_charged_wh / 100.0);
data[0] = charged_raw & 0xFF;
data[1] = (charged_raw >> 8) & 0xFF;
data[2] = (charged_raw >> 16) & 0xFF;
data[3] = (charged_raw >> 24) & 0xFF;

// Bytes 4-7: Energy Discharged
uint32_t discharged_raw = (uint32_t)(energy_discharged_wh / 100.0);
data[4] = discharged_raw & 0xFF;
data[5] = (discharged_raw >> 8) & 0xFF;
data[6] = (discharged_raw >> 16) & 0xFF;
data[7] = (discharged_raw >> 24) & 0xFF;

xSemaphoreGive(s_energy_mutex);

// Exemple: 12.5kWh charged → 12500Wh → 125 units → [0x7D, 0x00, 0x00, 0x00, ...]
```

**Accumulation**:
```c
// Appelé à chaque échantillon BMS
void accumulate_energy(float voltage_v, float current_a, uint64_t delta_time_ms) {
    float power_w = voltage_v * current_a;
    double delta_energy_wh = power_w * (delta_time_ms / 3600000.0);

    xSemaphoreTake(s_energy_mutex, portMAX_DELAY);
    if (delta_energy_wh > 0)
        s_energy_charged_wh += delta_energy_wh;
    else
        s_energy_discharged_wh += fabs(delta_energy_wh);
    xSemaphoreGive(s_energy_mutex);
}
```

**Persistance NVS**:
```c
// Sauvegarder toutes les 60s SI delta ≥ 10Wh
#define ENERGY_PERSIST_MIN_DELTA_WH 10.0
#define ENERGY_PERSIST_MIN_INTERVAL_MS 60000

if (time_since_last_save >= 60000 && fabs(current - last_saved) >= 10.0) {
    nvs_set_blob(handle, "energy_charged", &s_energy_charged_wh, sizeof(double));
    nvs_set_blob(handle, "energy_discharged", &s_energy_discharged_wh, sizeof(double));
    nvs_commit(handle);
}
```

---

### 0x379 - Installed Capacity

```c
// Bytes 0-3: Capacity in 0.01Ah units
// Source: TinyBMS register 306 (0x0132)
// Scale: 0.01Ah (multiply by 100)
uint32_t capacity_raw = (uint32_t)(battery_capacity_ah * 100.0);
data[0] = capacity_raw & 0xFF;
data[1] = (capacity_raw >> 8) & 0xFF;
data[2] = (capacity_raw >> 16) & 0xFF;
data[3] = (capacity_raw >> 24) & 0xFF;

// Bytes 4-7: Reserved
data[4] = 0x00;
data[5] = 0x00;
data[6] = 0x00;
data[7] = 0x00;

// Exemple: 200.0Ah → 20000 (0x00004E20) → [0x20, 0x4E, 0x00, 0x00, 0x00...]
```

---

### 0x380/0x381 - Serial Number (2 frames)

```c
// Serial number total: 16 caractères ASCII

// 0x380 - Caractères 0-7
memcpy(data, &serial_number[0], 8);

// 0x381 - Caractères 8-15
memcpy(data, &serial_number[8], 8);

// Source: TinyBMS serial_number[17] field
```

---

### 0x382 - Battery Family

```c
// ASCII string (null-padded to 8 bytes)
strcpy((char *)data, battery_family);
// Exemple: "LiFePO4" → [0x4C, 0x69, 0x46, 0x65, 0x50, 0x4F, 0x34, 0x00]

// Source: TinyBMS register 502 reference
```

---

## ⚙️ State Machine CVL (CRITIQUE pour protection batterie)

### États (6 états)

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

### Transitions basées sur SOC

```c
// Seuils de configuration (depuis BMS cvl_logic.c)
float bulk_soc_threshold;         // Exemple: 80%
float transition_soc_threshold;   // Exemple: 90%
float float_soc_threshold;        // Exemple: 95%
float sustain_soc_entry_percent;  // Exemple: 30%
float sustain_soc_exit_percent;   // Exemple: 40%

// Logique de transition
if (soc < bulk_soc_threshold)
    state = CVL_STATE_BULK;
else if (soc < transition_soc_threshold)
    state = CVL_STATE_TRANSITION;
else if (soc < float_soc_threshold)
    state = CVL_STATE_FLOAT_APPROACH;
else
    state = CVL_STATE_FLOAT;

// Override sustain (maintenance bas SOC)
if (soc <= sustain_soc_entry_percent)
    state = CVL_STATE_SUSTAIN;
// Sort si soc >= sustain_soc_exit_percent
```

### Calcul CVL par état

```c
// Tensions de base
float cell_max_voltage_v = overvoltage_cutoff_mv / 1000.0;
float pack_max_voltage_v = cell_max_voltage_v × series_cell_count;
float min_float_voltage_v = cell_min_float_voltage_v × series_cell_count;

// CVL selon l'état
switch (state) {
    case CVL_STATE_BULK:
    case CVL_STATE_TRANSITION:
    case CVL_STATE_FLOAT_APPROACH:
        cvl = bulk_target_voltage_v;  // Tension de charge rapide
        break;

    case CVL_STATE_FLOAT:
        cvl = float_voltage_v;  // Tension de maintien (plus basse)
        break;

    case CVL_STATE_SUSTAIN:
        cvl = sustain_voltage_v;  // Tension maintenance (encore plus basse)
        break;

    case CVL_STATE_IMBALANCE_HOLD:
        // Réduction proportionnelle au déséquilibre
        over_threshold = cell_imbalance_mv - imbalance_hold_threshold_mv;
        drop = fminf(imbalance_drop_max_v,
                    over_threshold × imbalance_drop_per_mv);
        cvl = bulk_target_voltage_v - drop;
        cvl = fmaxf(cvl, min_float_voltage_v);  // Minimum absolu
        break;
}

// Plafond de sécurité absolu
cvl = fminf(cvl, pack_max_voltage_v);
```

### Protection cellule haute tension

```c
// Activation avec hystéresis
if (max_cell_voltage_v >= cell_safety_threshold_v)
    protection_active = true;
if (max_cell_voltage_v <= cell_safety_release_v)
    protection_active = false;

// Réduction dynamique de CVL si protection active
if (protection_active) {
    float nominal_current = battery_capacity_ah;  // Courant 1C
    float reduction = cell_protection_kp ×
                     (1.0 + charge_current / nominal_current) ×
                     (max_cell_voltage_v - cell_safety_threshold_v);

    reduction = fminf(reduction, imbalance_drop_max_v);
    cvl = fmaxf(min_float_voltage_v, pack_max_voltage_v - reduction);
}

// Limitation du taux de remontée (anti-oscillation)
float max_step = max_recovery_step_v;  // Exemple: 0.1V par cycle
if (cvl > previous_cvl + max_step)
    cvl = previous_cvl + max_step;
```

### Ajustements CCL/DCL

```c
// Limites de base depuis TinyBMS
float base_ccl = max_charge_current_a;     // Register 103 (0x0067)
float base_dcl = max_discharge_current_a;  // Register 102 (0x0066)

// Réduction en mode float
if (state == CVL_STATE_FLOAT) {
    ccl = fminf(base_ccl, minimum_ccl_in_float_a);
}

// Restrictions en mode sustain
if (state == CVL_STATE_SUSTAIN) {
    ccl = fminf(base_ccl, sustain_ccl_limit_a);
    dcl = fminf(base_dcl, sustain_dcl_limit_a);
}

// Imbalance hold (même restriction que float)
if (state == CVL_STATE_IMBALANCE_HOLD) {
    ccl = fminf(base_ccl, minimum_ccl_in_float_a);
}
```

### Détection déséquilibre

```c
// Calcul delta
uint16_t cell_imbalance_mv = max_cell_mv - min_cell_mv;

// Hystéresis entrée/sortie
if (cell_imbalance_mv > imbalance_hold_threshold_mv) {
    enter_imbalance_hold_state();
}
if (cell_imbalance_mv <= imbalance_release_threshold_mv) {
    exit_imbalance_hold_state();
}

// Seuils typiques (du projet BMS):
// imbalance_hold_threshold_mv = 40;      // Entrée à >40mV
// imbalance_release_threshold_mv = 30;   // Sortie à ≤30mV
```

---

## 🔗 Mapping TinyBMS UART → CAN

### Registres TinyBMS utilisés

| Adresse | Type | Description | Utilisé dans CAN ID |
|---------|------|-------------|---------------------|
| 0x0000-0x000F | UINT16 | Tensions cellules 1-16 (0.1mV) | 0x373 (min/max) |
| 0x0024 (36) | FLOAT | Tension pack (V) | 0x356 |
| 0x0026 (38) | FLOAT | Courant pack (A) | 0x356 |
| 0x002D (45) | UINT16 | SOH (0.002%) | 0x355 |
| 0x002E (46) | UINT32 | SOC (0.000001) | 0x355 |
| 0x0030 (48) | INT16 | Température moyenne (0.1°C) | 0x356 |
| 0x0066 (102) | UINT16 | Courant décharge max (0.1A) | 0x351 (DCL) |
| 0x0067 (103) | UINT16 | Courant charge max (0.1A) | 0x351 (CCL) |
| 0x0132 (306) | UINT16 | Capacité batterie (0.01Ah) | 0x35F, 0x379 |
| 0x01F4 (500) | - | Nom fabricant | 0x35E |
| 0x01F5 (501) | UINT32 | Version firmware | 0x35F |
| 0x01F6 (502) | - | Famille batterie | 0x382 |
| 0x01F8-01F9 | - | Numéro série (2 parts) | 0x380, 0x381 |

**Total**: 45 registres, 59 mots

### Configuration UART (IDENTIQUE à ESP32-P4)

```c
// Déjà implémenté dans votre tinybms_client !
#define UART_PORT         UART_NUM_1
#define UART_TX_GPIO      26           // ✅ IDENTIQUE
#define UART_RX_GPIO      27           // ✅ IDENTIQUE
#define UART_BAUD_RATE    115200       // ✅ IDENTIQUE
#define UART_DATA_BITS    UART_DATA_8_BITS
#define UART_PARITY       UART_PARITY_DISABLE
#define UART_STOP_BITS    UART_STOP_BITS_1
```

**✅ AUCUN CHANGEMENT** nécessaire sur la partie UART TinyBMS !

---

## 📂 Architecture modulaire (ESP32-P4)

### Composants à créer

```
components/
├── can_victron/          # Driver CAN TWAI (NOUVEAU)
│   ├── CMakeLists.txt
│   ├── can_victron.c     # Wrapper TWAI ESP32-P4
│   └── can_victron.h     # API publique
│
├── can_publisher/        # Encodeur messages CAN (NOUVEAU)
│   ├── CMakeLists.txt
│   ├── can_publisher.c   # Logique de publication
│   ├── conversion_table.c # 🔴 COPIER depuis BMS repo
│   ├── conversion_table.h # 🔴 COPIER depuis BMS repo
│   ├── cvl_controller.c  # 🔴 COPIER depuis BMS repo
│   ├── cvl_logic.c       # 🔴 COPIER depuis BMS repo
│   └── cvl_types.h       # 🔴 COPIER depuis BMS repo
│
├── tinybms_client/       # ✅ DÉJÀ EXISTANT
├── tinybms_model/        # ✅ DÉJÀ EXISTANT
├── event_bus/            # ✅ DÉJÀ EXISTANT
└── gui_lvgl/             # ✅ DÉJÀ EXISTANT
    ├── screen_can_status.c   # NOUVEAU
    ├── screen_can_config.c   # NOUVEAU
    └── screen_bms_control.c  # NOUVEAU
```

### Flux de données

```
┌───────────┐  UART   ┌─────────────────┐  Event  ┌──────────────┐
│  TinyBMS  │◄───────►│ tinybms_client  │────────►│  event_bus   │
│           │ 115200  │ (✅ existant)   │   Bus   │ (✅ existant)│
└───────────┘         └─────────────────┘         └──────────────┘
   GPIO 26/27                 │                            │
                              ▼                            ▼
                   ┌──────────────────┐       ┌──────────────────────────┐
                   │  tinybms_model   │       │ EVENT_TINYBMS_REGISTER_  │
                   │  (✅ existant)   │       │ UPDATED / CONFIG_CHANGED │
                   │  Cache registres │       └──────────────────────────┘
                   └──────────────────┘                    │
                              │                            │
                              └────────────┬───────────────┘
                                           │
                   ┌───────────────────────┴────────────────────────┐
                   ▼                                                ▼
       ┌─────────────────────────┐                    ┌──────────────────┐
       │   tinybms_adapter       │                    │  gui_lvgl        │
       │   (🔴 NOUVEAU)          │                    │  (✅ existant)   │
       │ Convertit tinybms_model │                    │  + 3 new screens │
       │ → uart_bms_live_data_t  │                    └──────────────────┘
       └─────────────────────────┘
                   │
                   ▼
       ┌─────────────────────────┐
       │   can_publisher         │
       │   (🔴 NOUVEAU)          │
       │ - CVL state machine     │
       │ - Message encoders      │
       │ - Conversion table      │
       └─────────────────────────┘
                   │
                   ▼
       ┌─────────────────────────┐
       │    can_victron          │  CAN   ┌──────────────┐
       │   (🔴 NOUVEAU)          │───────►│   Victron    │
       │   TWAI driver ESP32-P4  │ 500kbps│   GX Device  │
       └─────────────────────────┘        └──────────────┘
             GPIO 22/21
```

**Légende**:
- ✅ Modules déjà implémentés dans ESP32-P4
- 🔴 Modules à copier/adapter depuis Exemple/mac-local/BMS
- GPIO 26/27: UART TinyBMS (inchangé)
- GPIO 22/21: CAN Bus (nouveau)

---

## 🛠️ Fichiers à copier EXACTEMENT du projet BMS

### Fichiers critiques (depuis Exemple/mac-local/BMS)

| Fichier source (local) | Destination (ESP32-P4) | Adaptations |
|------------------------|------------------------|-------------|
| `Exemple/mac-local/BMS/main/can_victron/can_victron.c` | `components/can_victron/can_victron.c` | GPIO adaptés (voir ci-dessous) |
| `Exemple/mac-local/BMS/main/can_victron/can_victron.h` | `components/can_victron/can_victron.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_victron/CMakeLists.txt` | `components/can_victron/CMakeLists.txt` | Adapter dépendances |
| `Exemple/mac-local/BMS/main/can_publisher/can_publisher.c` | `components/can_publisher/can_publisher.c` | Adapter interfaces |
| `Exemple/mac-local/BMS/main/can_publisher/can_publisher.h` | `components/can_publisher/can_publisher.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/conversion_table.c` | `components/can_publisher/conversion_table.c` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/conversion_table.h` | `components/can_publisher/conversion_table.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/cvl_controller.c` | `components/can_publisher/cvl_controller.c` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/cvl_controller.h` | `components/can_publisher/cvl_controller.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/cvl_logic.c` | `components/can_publisher/cvl_logic.c` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/cvl_logic.h` | `components/can_publisher/cvl_logic.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/cvl_types.h` | `components/can_publisher/cvl_types.h` | ✅ IDENTIQUE |
| `Exemple/mac-local/BMS/main/can_publisher/CMakeLists.txt` | `components/can_publisher/CMakeLists.txt` | Adapter dépendances |

### Adaptations nécessaires

#### 1. GPIO CAN (can_victron.c)

```c
// AVANT (BMS référence):
// GPIO définis via menuconfig ou defines
#define CONFIG_TINYBMS_CAN_VICTRON_TX_GPIO 7   // Exemple
#define CONFIG_TINYBMS_CAN_VICTRON_RX_GPIO 6

// APRÈS (ESP32-P4):
#define CONFIG_TINYBMS_CAN_VICTRON_TX_GPIO 22
#define CONFIG_TINYBMS_CAN_VICTRON_RX_GPIO 21
```

#### 2. Interface UART BMS

Le projet BMS de référence utilise `uart_bms.cpp/h` (C++) avec la structure:
```c
typedef struct {
    float pack_voltage_v;
    float pack_current_a;
    float pack_temp_c;
    uint16_t cell_voltages_mv[16];
    // ... autres champs
} uart_bms_live_data_t;
```

ESP32-P4 utilise déjà `tinybms_client` + `tinybms_model` (C pur). Il faut créer un **adaptateur** dans `can_publisher.c`:

```c
// Nouveau fichier: components/can_publisher/tinybms_adapter.c
// Convertit tinybms_model → uart_bms_live_data_t pour compatibilité
void tinybms_to_uart_bms_data(const tinybms_model_t *src, uart_bms_live_data_t *dst);
```

#### 3. EventBus

Le projet BMS utilise un EventBus similaire, mais avec des IDs différents. Adapter:
- `event_bus_publish_fn_t` → Compatible (déjà identique)
- Event IDs → Mapper vers `event_types.h` d'ESP32-P4

**Tout le reste (conversion_table, cvl_logic) = COPIE EXACTE !**

---

## 📋 Événements à ajouter

```c
// Dans components/event_types/event_types.h

typedef enum {
    // ... événements existants ...

    // CAN Bus events
    EVENT_CAN_BUS_STARTED,
    EVENT_CAN_BUS_STOPPED,
    EVENT_CAN_MESSAGE_TX,              // Message transmis
    EVENT_CAN_MESSAGE_RX,              // Message reçu (0x307 handshake)
    EVENT_CAN_KEEPALIVE_TIMEOUT,       // Pas de réponse GX
    EVENT_CAN_ERROR,                   // Erreur bus CAN

    // CVL State Machine
    EVENT_CVL_STATE_CHANGED,           // Changement d'état CVL
    EVENT_CVL_LIMITS_UPDATED,          // CVL/CCL/DCL recalculés

    // Energy
    EVENT_ENERGY_COUNTERS_UPDATED,     // 0x378 mis à jour

    EVENT_TYPE_MAX
} event_type_t;
```

---

## 🖥️ Nouveaux écrans GUI

### screen_can_status.c

**Fonctionnalités**:
- État connexion CAN (bus OK, erreurs)
- État Victron GX (handshake 0x307 reçu)
- Statistiques TX/RX (messages/sec, erreurs)
- État CVL (état actuel de la state machine)
- Boutons:
  - "Start CAN" / "Stop CAN"
  - "Force Keepalive" (envoi 0x305 manuel)

### screen_can_config.c

**Fonctionnalités**:
- Configuration GPIO TX/RX (lecture seule, info)
- Vitesse CAN: 500 kbps (lecture seule)
- Intervalle keepalive (slider 100-5000ms)
- Timeout keepalive (slider 1000-60000ms)
- Battery ID (affichage seulement)
- Messages actifs (liste des 19 ID avec période)

### screen_bms_control.c

**Fonctionnalités**:
- État CVL actuel (BULK/TRANSITION/FLOAT/etc.)
- Valeurs CVL/CCL/DCL en temps réel
- Graphique historique CVL (dernières 5 min)
- Indicateurs de protection:
  - Cell protection active
  - Imbalance hold active
- Paramètres CVL (édition):
  - Seuils SOC (bulk/transition/float)
  - Seuils déséquilibre
  - Constantes PID protection cellule

---

## 🔄 Intégration dans hmi_main.cpp

```cpp
// main/hmi_main.cpp

#include "can_victron.h"
#include "can_publisher.h"
#include "gui_init.hpp"

void hmi_main_init(void) {
    // ... initialisations existantes ...

    // 3c) Init CAN
    can_victron_init(CAN_TX_GPIO, CAN_RX_GPIO);        // NOUVEAU
    can_publisher_init(&s_event_bus);                  // NOUVEAU

    // 4) Init GUI (LVGL + écrans)
    s_gui_root = std::make_unique<gui::GuiRoot>(&s_event_bus);
    s_gui_root->init();
}

void hmi_main_start(void) {
    // ... démarrages existants ...

    // 2c) Démarrer CAN
    can_victron_start();                               // NOUVEAU
    can_publisher_start();                             // NOUVEAU

    if (s_gui_root) {
        s_gui_root->start();
    }
}
```

---

## 🧪 Séquence de développement (7 phases)

### Phase 1: Adaptateur TinyBMS (1-2 jours)
- [ ] Analyser la structure `uart_bms_live_data_t` du projet BMS
- [ ] Créer `components/can_publisher/tinybms_adapter.c/h`
- [ ] Implémenter conversion `tinybms_model_t` → `uart_bms_live_data_t`
- [ ] Mapper les 34 registres TinyBMS vers la structure unifiée
- [ ] Tests unitaires de conversion

### Phase 2: Driver CAN (2-3 jours)
- [ ] Créer `components/can_victron/`
- [ ] Copier `can_victron.c/h` depuis `Exemple/mac-local/BMS/main/can_victron/`
- [ ] Adapter GPIO 22/21 pour ESP32-P4
- [ ] Adapter CMakeLists.txt (dépendances ESP-IDF)
- [ ] Tester transmission/réception basique
- [ ] Vérifier 500 kbps avec analyseur CAN

### Phase 3: Encodeurs messages (2-3 jours)
- [ ] Créer `components/can_publisher/`
- [ ] Copier `conversion_table.c/h` depuis `Exemple/mac-local/BMS/main/can_publisher/`
- [ ] Copier `cvl_*.c/h` (4 fichiers CVL)
- [ ] Copier `can_publisher.c/h`
- [ ] Adapter CMakeLists.txt
- [ ] Compiler et résoudre dépendances
- [ ] Tester encodage message 0x351/0x355/0x356

### Phase 4: Intégration EventBus (1-2 jours)
- [ ] Ajouter nouveaux types d'événements (CAN/CVL)
- [ ] Créer glue code dans `can_publisher.c`
- [ ] Abonner can_publisher à `EVENT_TINYBMS_REGISTER_UPDATED`
- [ ] Intégrer `tinybms_adapter` dans le flux
- [ ] Mapper données TinyBMS → messages CAN via adaptateur
- [ ] Tester flux complet: UART → tinybms_model → adapter → CAN

### Phase 5: Keepalive et handshake (1-2 jours)
- [ ] Implémenter transmission périodique 0x305 (1000ms)
- [ ] Implémenter réception 0x307 (handshake Victron GX)
- [ ] Gérer timeout keepalive et reconnexion
- [ ] Publier événements `EVENT_CAN_KEEPALIVE_TIMEOUT`
- [ ] Tester avec GX device réel ou simulateur
- [ ] Valider connexion stable

### Phase 6: State Machine CVL (2-3 jours)
- [ ] Intégrer `cvl_controller.c` + `cvl_logic.c`
- [ ] Configurer seuils SOC/tension (fichier config ou NVS)
- [ ] Tester transitions d'états (BULK→TRANSITION→FLOAT)
- [ ] Vérifier protection cellule haute tension
- [ ] Valider limitation CCL/DCL dynamique
- [ ] Tester détection déséquilibre avec hystérésis

### Phase 7: GUI et finalisation (2-3 jours)
- [ ] Créer `screen_can_status.c` (état bus CAN + stats)
- [ ] Créer `screen_can_config.c` (configuration CAN)
- [ ] Créer `screen_bms_control.c` (CVL state + limites)
- [ ] Intégrer 3 nouveaux onglets dans `gui_init.cpp`
- [ ] Implémenter affichage temps réel CVL/CCL/DCL
- [ ] Tests complets avec onduleur Victron
- [ ] Documentation utilisateur

**Durée totale estimée**: 11-18 jours

---

## ✅ Checklist de conformité

### Protocole CAN
- [ ] Vitesse CAN = 500 kbps (NON NÉGOCIABLE)
- [ ] Format = Standard 11-bit (NON Extended 29-bit)
- [ ] GPIO ESP32-P4 = 22 (TX), 21 (RX)
- [ ] 19 messages CAN implémentés
- [ ] Périodes de broadcast respectées

### Messages critiques
- [ ] 0x305 Keepalive toutes les 1000ms
- [ ] 0x307 Handshake détection
- [ ] 0x351 CVL/CCL/DCL avec state machine
- [ ] 0x355 SOC/SOH exact (scale 0.01%)
- [ ] 0x356 V/I/T avec types signés
- [ ] 0x35A Alarmes avec bit encoding exact
- [ ] 0x378 Energy avec mutex protection

### State Machine CVL
- [ ] 6 états implémentés
- [ ] Transitions basées SOC
- [ ] Protection cellule haute tension
- [ ] Détection déséquilibre avec hystérésis
- [ ] Limitation taux de remontée CVL

### Intégration
- [ ] TinyBMS UART inchangé (GPIO 26/27)
- [ ] EventBus pour communication inter-modules
- [ ] GUI avec 3 nouveaux écrans CAN
- [ ] NVS persistence energy counters
- [ ] Logs ESP_LOG cohérents

---

## 📚 Références

### Documentation projet BMS (Local)
- **Emplacement**: `Exemple/mac-local/BMS/`
- **Fichiers clés**:
  - `main/can_victron/can_victron.c/h` - Driver TWAI bas niveau
  - `main/can_publisher/can_publisher.c/h` - Orchestrateur CAN
  - `main/can_publisher/conversion_table.c/h` - Encodeurs des 19 messages
  - `main/can_publisher/cvl_logic.c/h` - Logique state machine CVL
  - `main/can_publisher/cvl_controller.c/h` - Contrôleur CVL
  - `main/can_publisher/cvl_types.h` - Types CVL
  - `main/uart_bms/uart_bms.cpp/h` - Référence client UART
  - `main/event_bus/event_bus.c/h` - Référence EventBus
  - `main/app_main.c` - Séquence d'initialisation
- **Interface Web**: `web/` - Application SPA pour monitoring

### Protocole Victron
- Victron Energy CAN-bus BMS documentation
- Standard J1939-like avec 11-bit IDs (non Extended 29-bit)
- Compatible: MultiPlus, Quattro, GX devices (Venus OS)
- 19 messages standardisés avec périodes fixes

### Hardware ESP32-P4
- **Plateforme**: ESP32-P4-WIFI6-Touch-LCD-7B (Waveshare)
- **GPIO CAN**: 22 (TX), 21 (RX)
- **GPIO UART**: 26 (TX), 27 (RX) - TinyBMS
- **TWAI peripheral**: ESP32-P4 CAN controller
- **Référence Waveshare**: Exemple 14_TWAItransmit

### TinyBMS
- **Communication**: UART uniquement (115200 baud, 8N1)
- **Protocole**: Modbus-like avec CRC16
- **Registres**: 34 registres documentés dans tinybms_registers.c
- **Pas de CAN natif**: L'ESP32 synthétise les messages CAN

---

## ⚠️ Points critiques (NE PAS DÉVIER)

1. **Vitesse CAN**: TOUJOURS 500 kbps (Victron standard)
2. **Message 0x351**: CVL DOIT utiliser la state machine (pas de valeur fixe)
3. **Message 0x356**: Current DOIT être signé (+ charge, - décharge)
4. **Message 0x35A**: Bit encoding EXACT (alarmes vs warnings)
5. **Message 0x378**: MUTEX OBLIGATOIRE (thread safety)
6. **Keepalive 0x305**: CRITIQUE pour connexion GX
7. **State Machine CVL**: Protection cellule = priorité absolue
8. **GPIO**: 22/21 pour CAN, 26/27 pour UART (NON MODIFIABLES)

---

## 🎯 Résultat attendu

**ESP32-P4 fonctionnant comme**:
- ✅ Gateway UART TinyBMS → CAN Victron
- ✅ Interface tactile 7" pour monitoring/config
- ✅ Compatible avec onduleurs Victron (MultiPlus, Quattro, etc.)
- ✅ Protection batterie via state machine CVL
- ✅ Architecture modulaire ESP-IDF
- ✅ Exactement conforme au projet BMS existant

**Différences vs projet BMS original**:
- ➕ Écran tactile 7 pouces intégré
- ➕ Interface graphique LVGL
- ➕ Configuration interactive
- ➕ WiFi 6 pour monitoring distant
- ✅ Même protocole CAN Victron
- ✅ Même state machine CVL
- ✅ Même encodage messages

---

---

## 📝 Notes d'implémentation

### Utilisation du projet BMS de référence

Le projet BMS complet se trouve dans `Exemple/mac-local/BMS/`. Ce projet est la **référence absolue** pour l'implémentation CAN:

1. **Ne rien inventer**: Tous les algorithmes (CVL, encodage messages) doivent être copiés EXACTEMENT
2. **Copie directe**: Les fichiers `conversion_table.c`, `cvl_logic.c`, etc. sont à copier sans modification
3. **Seules adaptations autorisées**:
   - GPIO (22/21 au lieu de 7/6)
   - Interface tinybms_model (via adaptateur)
   - Event IDs (mapping vers event_types.h d'ESP32-P4)
   - CMakeLists.txt (dépendances ESP-IDF)

### Architecture modulaire ESP32-P4

L'ESP32-P4 possède déjà une architecture solide:
- ✅ `event_bus/` - Identique au BMS de référence
- ✅ `tinybms_client/` - Équivalent de `uart_bms/`
- ✅ `tinybms_model/` - Cache des registres TinyBMS
- ✅ `gui_lvgl/` - Interface graphique 7"

Il faut **compléter** avec:
- 🔴 `can_victron/` - Driver TWAI (copie)
- 🔴 `can_publisher/` - Encodeurs + CVL (copie + adaptateur)

### Différences clés ESP32-P4 vs BMS référence

| Aspect | BMS référence | ESP32-P4 |
|--------|---------------|----------|
| **UART Client** | `uart_bms.cpp` (C++) | `tinybms_client` (C) |
| **Structure données** | `uart_bms_live_data_t` | `tinybms_model_t` |
| **Interface** | Web SPA | LVGL tactile 7" |
| **WiFi** | ESP32 standard | ESP32-P4 WiFi 6 |
| **Display** | Aucun | 800x480 tactile |
| **GPIO CAN** | 7/6 | 22/21 |
| **GPIO UART** | Variable | 26/27 |

**Solution**: L'adaptateur `tinybms_adapter.c` fait le pont entre les deux structures.

### Avantages ESP32-P4

- **Interface tactile intégrée**: Contrôle direct sans application web externe
- **Monitoring temps réel**: Graphiques CVL/CCL/DCL sur écran
- **Configuration interactive**: Pas besoin de recompilation pour ajuster seuils
- **WiFi 6**: Meilleure performance pour monitoring distant
- **Même fiabilité**: Protocole CAN et CVL identiques au BMS éprouvé

Implement professionel dashboard:
https://docs.lvgl.io/master/details/widgets/bar.html

