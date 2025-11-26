# Corrections TinyBMS pour ESP32-P4

## Date
2025-11-26

## Vue d'ensemble

Ce document présente les corrections appliquées au projet ESP32-P4 pour garantir la conformité totale avec le protocole TinyBMS Communication Protocols Rev D (2025-07-04).

## Corrections Appliquées

### 1. Registre Total Distance Incorrect

**Fichier:** `docs/tinybms_protocol_full.h`

**Problème:** Le registre Total Distance était mappé au registre 101, alors que selon le protocole (page 23), il doit commencer au registre 100.

**Référence Protocole Rev D (page 23):**
```
Reg 100-101: Total Distance [UINT_32] / Resolution 0.01 km
```

**Avant Correction:**
```c
#define REG_STATS_TOTAL_DISTANCE            101 // UINT32, 0.01km
```

**Après Correction:**
```c
#define REG_STATS_TOTAL_DISTANCE            100 // UINT32, 0.01km (occupies registers 100-101)
```

**Explication:** Un registre UINT32 occupe 2 registres consécutifs (16 bits chacun). Le registre Total Distance commence donc au registre 100 et s'étend jusqu'au registre 101.

**Impact:** Cette correction garantit que toute lecture du registre Total Distance dans le code ESP32-P4 utilise la bonne adresse de départ.

### 2. Code de Statut REGENERATION

**Fichier:** `docs/tinybms_protocol_full.h`

**Statut:** ✅ Déjà correct

**Vérification:** Le code de statut BMS 0x96 (REGENERATION) est correctement défini dans le fichier tinybms_protocol_full.h:

```c
// Codes Status (Reg 50) [cite: 190]
#define STATUS_CHARGING                     0x91
#define STATUS_FULLY_CHARGED                0x92
#define STATUS_DISCHARGING                  0x93
#define STATUS_REGENERATION                 0x96
#define STATUS_IDLE                         0x97
#define STATUS_FAULT                        0x9B
```

**Référence Protocole Rev D:**
Le protocole définit les codes de statut suivants pour le registre 50 (Online Status):
- 0x91: Charging
- 0x92: Fully Charged
- 0x93: Discharging
- 0x96: Regeneration (freinage régénératif)
- 0x97: Idle
- 0x9B: Fault

## Fichiers Modifiés

### docs/tinybms_protocol_full.h
- Ligne 100: Correction du registre REG_STATS_TOTAL_DISTANCE de 101 à 100
- Ajout d'un commentaire explicatif indiquant que le registre occupe les adresses 100-101

## Tests Recommandés

### Test 1: Lecture Total Distance
```c
// Lire le registre Total Distance
uint16_t values[2];
esp_err_t ret = tinybms_read_block(100, 2, values);

// Reconstituer la valeur UINT32
uint32_t total_distance_raw = ((uint32_t)values[1] << 16) | values[0];
float total_distance_km = total_distance_raw * 0.01f;

// Vérifier que la valeur est cohérente
assert(ret == ESP_OK);
assert(total_distance_km >= 0.0f);
```

### Test 2: Vérification Statut Regeneration
```c
// Lire le registre Online Status
uint16_t status;
esp_err_t ret = tinybms_read_online_status(&status);

// Vérifier tous les codes de statut valides
assert(ret == ESP_OK);
assert(status == STATUS_CHARGING ||
       status == STATUS_FULLY_CHARGED ||
       status == STATUS_DISCHARGING ||
       status == STATUS_REGENERATION ||  // 0x96
       status == STATUS_IDLE ||
       status == STATUS_FAULT);
```

## Conformité Protocole

Après ces corrections, le projet ESP32-P4 est maintenant **100% conforme** au protocole TinyBMS Communication Protocols Rev D pour les points suivants:

| Aspect | Avant | Après | Conforme |
|--------|-------|-------|----------|
| Registre Total Distance | 101 | 100 | ✅ |
| Code Statut Regeneration (0x96) | Présent | Présent | ✅ |
| Format UINT32 Total Distance | 2 registres consécutifs | 2 registres consécutifs | ✅ |

## Références

- **Document source:** TinyBMS Communication Protocols Rev D
- **Date:** 2025-07-04
- **Sections concernées:**
  - Section 3.2: Statistics Data (page 23)
  - Section 3.1: Live Data - Registre 50 Online Status (page 22)

## Notes de Mise en Œuvre

### Registres UINT32
Un registre UINT32 dans le protocole TinyBMS occupe **2 registres consécutifs de 16 bits**:
- Premier registre (adresse N): LSW (Least Significant Word)
- Second registre (adresse N+1): MSW (Most Significant Word)

Exemple pour Total Distance au registre 100:
- Registre 100: Bits 0-15 de la valeur
- Registre 101: Bits 16-31 de la valeur

### Lecture d'un UINT32
```c
// Méthode recommandée: utiliser tinybms_read_block
uint16_t values[2];
tinybms_read_block(100, 2, values);  // Lire registres 100-101
uint32_t total = ((uint32_t)values[1] << 16) | values[0];
```

---

**Document généré le:** 2025-11-26
**Version du protocole:** TinyBMS Communication Protocols Rev D
**Projet:** ESP32-P4 Battery Management System
