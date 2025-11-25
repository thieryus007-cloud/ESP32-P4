# Clarification : Ordre des octets MODBUS dans TinyBMS Rev D

## 🚨 Incohérence Documentation vs Implémentation

Ce document explique une **incohérence majeure** découverte entre la documentation officielle TinyBMS Rev D et l'implémentation de référence.

### Date de découverte
2025-11-25

### Version de la documentation
TinyBMS Communication Protocols Revision D, 2025-07-04

---

## 📋 Résumé du problème

La documentation PDF officielle TinyBMS Rev D indique que les commandes MODBUS (0x03 et 0x10) utilisent **BIG ENDIAN** (MSB, LSB) pour les adresses de registres, **MAIS** l'implémentation de référence en C++ (`components/tinybms_client/tinybms_protocol.cpp`) utilise **LITTLE ENDIAN** (LSB, MSB).

### Verdict : L'implémentation C++ est correcte

L'implémentation C++ est la **source de vérité** car elle fonctionne avec le hardware réel TinyBMS.

---

## 📄 Détails de l'incohérence

### 1. Commande 0x03 (MODBUS Read)

#### Documentation PDF (Page 6, Section 1.1.6) - ❌ INCORRECT
```
Request to BMS:
Byte1  Byte2  Byte3      Byte4      Byte5  Byte6  Byte7    Byte8
0xAA   0x03   ADDR:MSB   ADDR:LSB   0x00   RL     CRC:LSB  CRC:MSB
               ^^^^^^^^   ^^^^^^^^
               BIG ENDIAN (incorrect)
```

#### Implémentation C++ - ✅ CORRECT
```cpp
// components/tinybms_client/tinybms_protocol.cpp (lignes 198-218)
esp_err_t tinybms_build_modbus_read_frame(uint8_t *frame,
                                           uint16_t start_address,
                                           uint16_t quantity)
{
    frame[3] = start_address & 0xFF;           // LSB first
    frame[4] = (start_address >> 8) & 0xFF;    // MSB second = LITTLE ENDIAN ✅
    frame[5] = quantity & 0xFF;                // LSB first
    frame[6] = (quantity >> 8) & 0xFF;         // MSB second = LITTLE ENDIAN ✅
```

#### Exemple TypeScript corrigé
```typescript
export function buildReadRegisterCommand(startAddr: number, count: number): Uint8Array {
    const buf = [
        0xAA,
        0x03,
        startAddr & 0xFF,        // Address LSB (Little Endian) ✅
        (startAddr >> 8) & 0xFF, // Address MSB ✅
        0x00,
        count & 0xFF
    ];
    // ...
}
```

---

### 2. Commande 0x10 (MODBUS Write)

#### Documentation PDF (Page 6, Section 1.1.7) - ❌ INCORRECT
```
Request to BMS:
Byte1  Byte2  Byte3      Byte4      Byte5  Byte6  Byte7  Byte8       Byte9
0xAA   0x10   ADDR:MSB   ADDR:LSB   0x00   RL     PL     DATA:MSB    DATA:LSB
               ^^^^^^^^   ^^^^^^^^                        ^^^^^^^^    ^^^^^^^^
               BIG ENDIAN (incorrect)                     BIG ENDIAN (correct)
```

#### Implémentation C++ - ✅ CORRECT
```cpp
// components/tinybms_client/tinybms_protocol.cpp (lignes 227-250)
esp_err_t tinybms_build_modbus_write_frame(...)
{
    frame[3] = start_address & 0xFF;           // Address LSB
    frame[4] = (start_address >> 8) & 0xFF;    // Address MSB = LITTLE ENDIAN ✅
    frame[5] = quantity & 0xFF;                // Quantity LSB
    frame[6] = (quantity >> 8) & 0xFF;         // Quantity MSB = LITTLE ENDIAN ✅
    // ...
    // Les DONNÉES utilisent BIG ENDIAN (correct pour MODBUS standard)
    frame[offset] = (values[i] >> 8) & 0xFF;   // Data MSB ✅
    frame[offset + 1] = values[i] & 0xFF;      // Data LSB ✅
}
```

---

## 📊 Tableau récapitulatif

| Commande | Type | Documentation PDF | Implémentation C++ | Statut |
|----------|------|-------------------|-------------------|---------|
| **0x03 MODBUS Read** | Adresse | ❌ BIG ENDIAN | ✅ LITTLE ENDIAN | **C++ correct** |
| **0x03 MODBUS Read** | Quantité | ❌ BIG ENDIAN | ✅ LITTLE ENDIAN | **C++ correct** |
| **0x10 MODBUS Write** | Adresse | ❌ BIG ENDIAN | ✅ LITTLE ENDIAN | **C++ correct** |
| **0x10 MODBUS Write** | Quantité | ❌ BIG ENDIAN | ✅ LITTLE ENDIAN | **C++ correct** |
| **0x10 MODBUS Write** | Données | ✅ BIG ENDIAN | ✅ BIG ENDIAN | **Cohérent** |
| **0x07 Read Block** | Tout | ✅ LITTLE ENDIAN | ✅ LITTLE ENDIAN | **Cohérent** |
| **0x09 Read Individual** | Tout | ✅ LITTLE ENDIAN | ✅ LITTLE ENDIAN | **Cohérent** |
| **0x0B Write Block** | Tout | ✅ LITTLE ENDIAN | ✅ LITTLE ENDIAN | **Cohérent** |
| **0x0D Write Individual** | Tout | ✅ LITTLE ENDIAN | ✅ LITTLE ENDIAN | **Cohérent** |

---

## ✅ Règle à suivre (Correcte)

### Pour les commandes MODBUS (0x03, 0x10) :
- **Adresses et quantités** : LITTLE ENDIAN (LSB, MSB)
- **Données (valeurs)** : BIG ENDIAN (MSB, LSB) - standard MODBUS

### Pour les commandes propriétaires (0x07, 0x09, 0x0B, 0x0D) :
- **Tout** : LITTLE ENDIAN (LSB, MSB)

---

## 🔧 Fichiers corrigés dans ce projet

Les fichiers suivants ont été corrigés pour utiliser le bon ordre des octets (LITTLE ENDIAN pour les adresses) :

1. ✅ `Exemple/TinyBMS-web/docs/TinyBMS_service.ts`
2. ✅ `Exemple/Gemini/TinyBMS_service.ts`
3. ✅ `Exemple/Gemini/TinyBmsService.ts`
4. ✅ `Exemple/TinyBMS-web/tinybms.js`

### Fichier déjà conforme :
- ✅ `Exemple/mac-local/src/serial.js` (déjà correct)

---

## 📝 Exemple pratique

### Lire le registre 300 (Fully Charged Voltage)

#### ❌ INCORRECT (selon documentation PDF erronée)
```typescript
const buf = [
    0xAA, 0x03,
    0x01, 0x2C,  // 0x012C = 300 en BIG ENDIAN (INCORRECT)
    0x00, 0x01   // Lire 1 registre
];
```

#### ✅ CORRECT (selon implémentation C++)
```typescript
const buf = [
    0xAA, 0x03,
    0x2C, 0x01,  // 0x012C = 300 en LITTLE ENDIAN (CORRECT)
    0x00, 0x01   // Lire 1 registre
];
```

---

## 🎯 Impact du bug

Si vous utilisez **BIG ENDIAN** pour les adresses comme indiqué dans la documentation PDF :
- ❌ Les lectures/écritures de registres **échoueront**
- ❌ Le BMS ne répondra pas correctement
- ❌ Les trames seront rejetées par le firmware

---

## 📧 Contact Enepaq

Cette incohérence devrait être signalée à Enepaq pour corriger la documentation PDF Rev D (pages 6-7, sections 1.1.6 et 1.1.7).

**Site web** : https://www.enepaq.com

---

## 📚 Références

- **Documentation officielle** : `TinyBMS_Communication_Protocols_Rev_D 3.pdf`
- **Implémentation de référence** : `components/tinybms_client/tinybms_protocol.cpp`
- **Commit de correction** : Voir l'historique Git de ce fichier

---

## ⚠️ Note importante

**Toujours suivre l'implémentation C++ de référence** plutôt que la documentation PDF pour les détails techniques d'implémentation du protocole.

---

*Document créé le 2025-11-25 suite à l'analyse de conformité du projet ESP32-P4.*
