# Corrections du Protocole UART TinyBMS

**Date:** 2025-11-20
**Référence:** TinyBMS Communication Protocols Revision D, 2025-07-04

## Résumé

Mise en conformité complète du module UART avec les spécifications officielles du protocole TinyBMS.

---

## 📋 Problèmes Identifiés et Corrigés

### 1. ❌ Format de Trame READ Incorrect

**Avant (INCORRECT):**
```
[0xAA] [0x07] [0x01] [Addr_LO] [Addr_HI] [CRC_LO] [CRC_HI]
  ↑      ↑      ↑
  OK   Length?  READ cmd = 0x01 (FAUX)
```

**Après (CONFORME - Section 1.1.3):**
```
[0xAA] [0x09] [0x02] [Addr_LO] [Addr_HI] [CRC_LO] [CRC_HI]
  ↑      ↑      ↑
  OK    CMD     PL (payload length = 2 bytes d'adresse)
```

### 2. ❌ Codes de Commande Incorrects

| Type | Ancien Code | Nouveau Code | Référence |
|------|-------------|--------------|-----------|
| READ Individual | 0x01 ❌ | **0x09** ✅ | Section 1.1.3 |
| WRITE Individual | 0x04 ❌ | **0x0D** ✅ | Section 1.1.5 |
| READ Response | 0x02 ❌ | **0x09** ✅ | Section 1.1.3 |
| ACK (Byte2) | 0x01 ❌ | **0x01** ✅ | Section 1.1.1 |
| NACK (Byte2) | 0x81 ❌ | **0x00** ✅ | Section 1.1.1 |

### 3. ❌ Format ACK/NACK Incorrect

**Avant:**
- Vérifiait `frame[2]` (Byte3) pour ACK/NACK
- Codes: ACK=0x01, NACK=0x81

**Après (CONFORME - Section 1.1.1):**
```
ACK (5 bytes):
[0xAA] [0x01] [CMD] [CRC:LSB] [CRC:MSB]
        ↑
      ACK indicator

NACK (6 bytes):
[0xAA] [0x00] [CMD] [ERROR] [CRC:LSB] [CRC:MSB]
        ↑             ↑
    NACK ind.     Error code
```

### 4. ❌ Parsing de Réponse READ Incorrect

**Avant:**
- Attendait 7 bytes
- Lisait la valeur directement aux bytes 3-4

**Après (CONFORME):**
```
Response (9 bytes):
[0xAA] [0x09] [PL] [Addr:LSB] [Addr:MSB] [Data:LSB] [Data:MSB] [CRC:LSB] [CRC:MSB]
                                           ↑         ↑
                                         Bytes 5-6 (valeur)
```

### 5. ✅ Éléments Corrects (Non Modifiés)

- **CRC-16 MODBUS:** Polynomial 0xA001 (réfléchi de 0x8005) ✅
- **Configuration UART:** 115200 baud, 8N1, no flow control ✅
- **Little-endian encoding** pour adresses et valeurs ✅

---

## 🔧 Fichiers Modifiés

### 1. `components/tinybms_client/tinybms_protocol.h`

**Modifications:**
- Redéfinition des codes de commande selon spécification officielle
- Ajout de constantes pour MODBUS (0x03, 0x10)
- Correction des longueurs de trame
- Documentation complète du format de trame

**Nouveaux Defines:**
```c
#define TINYBMS_CMD_READ_BLOCK          0x07  // Read registers block (proprietary)
#define TINYBMS_CMD_READ_INDIVIDUAL     0x09  // Read individual registers
#define TINYBMS_CMD_WRITE_BLOCK         0x0B  // Write registers block (proprietary)
#define TINYBMS_CMD_WRITE_INDIVIDUAL    0x0D  // Write individual registers
#define TINYBMS_CMD_MODBUS_READ         0x03  // MODBUS read
#define TINYBMS_CMD_MODBUS_WRITE        0x10  // MODBUS write
#define TINYBMS_RESP_ACK                0x01  // Byte2 in ACK response
#define TINYBMS_RESP_NACK               0x00  // Byte2 in NACK response
```

### 2. `components/tinybms_client/tinybms_protocol.cpp`

**Fonctions modifiées:**

#### `tinybms_build_read_frame()`
- Utilise la commande **0x09** (Read Individual)
- Ajoute le champ **PL** (Payload Length) = 0x02
- Format: `[0xAA] [0x09] [0x02] [Addr:LSB] [Addr:MSB] [CRC:LSB] [CRC:MSB]`

#### `tinybms_build_write_frame()`
- Utilise la commande **0x0D** (Write Individual)
- Ajoute le champ **PL** = 0x04
- Format: `[0xAA] [0x0D] [0x04] [Addr:LSB] [Addr:MSB] [Data:LSB] [Data:MSB] [CRC:LSB] [CRC:MSB]`

#### `tinybms_parse_read_response()`
- Attend **9 bytes** minimum
- Vérifie que Byte2 = **0x09**
- Extrait la valeur des **Bytes 5-6** (au lieu de 3-4)
- Valide le champ PL

#### `tinybms_parse_ack()`
- Vérifie **Byte2** pour ACK/NACK (au lieu de Byte3)
- ACK: Byte2 = **0x01**
- NACK: Byte2 = **0x00**, error code en Byte4

---

## 📊 Comparaison Avant/Après

### Exemple: Lecture du registre 0x012C (Fully Charged Voltage)

**AVANT (Incorrect):**
```
TX: AA 07 01 2C 01 [CRC]  ← Commande 0x01 incorrecte
     └─┘ │  └──┬──┘
  Preamb│    Addr
      "Length"?
```

**APRÈS (Conforme):**
```
TX: AA 09 02 2C 01 [CRC]  ← Commande 0x09 + PL=02
     └─┘ │  └──┬──┘
  Preamb│    Addr
      CMD PL

RX: AA 09 04 2C 01 68 10 [CRC]
     └─┘ │  └──┬──┘ └──┬──┘
  Preamb│   Addr    Data=0x1068 (4200mV)
      CMD PL
```

---

## ✅ Tests Recommandés

1. **Test de lecture:**
   - Lire le registre 0x012C (Fully Charged Voltage)
   - Vérifier que la réponse est bien parsée
   - Confirmer que la valeur est cohérente

2. **Test d'écriture:**
   - Écrire une valeur dans un registre de configuration
   - Vérifier la réception de l'ACK
   - Relire le registre pour validation

3. **Test d'erreur:**
   - Écriture dans une adresse invalide (< 0x012C)
   - Vérifier la réception du NACK avec code d'erreur

4. **Test CRC:**
   - Envoyer une trame avec CRC invalide
   - Vérifier que le BMS ne répond pas

---

## 📚 Références

- **Document:** TinyBMS Communication Protocols Revision D, 2025-07-04
- **Sections clés:**
  - 1.1.1: Acknowledgement (ACK/NACK)
  - 1.1.2: Read registers block (cmd 0x07)
  - 1.1.3: Read individual registers (cmd 0x09) ← **Utilisée**
  - 1.1.4: Write registers block (cmd 0x0B)
  - 1.1.5: Write individual registers (cmd 0x0D) ← **Utilisée**
  - 1.1.6: MODBUS Read (cmd 0x03)
  - 1.1.7: MODBUS Write (cmd 0x10)
  - 1.2: CRC checksum calculation

---

## ⚠️ Notes Importantes

1. **Sleep Mode:** Le PDF mentionne que si le TinyBMS est en mode sleep, la première commande doit être envoyée **deux fois**. Implémenter si nécessaire.

2. **Timeout:** Le timeout actuel de 750ms devrait être suffisant selon les spécifications.

3. **MODBUS Alternative:** Les commandes 0x03 (read) et 0x10 (write) sont aussi disponibles pour compatibilité MODBUS. Envisager de les supporter à l'avenir.

4. **Registres writable:** Seuls les registres dans la plage **0x012C à 0x018F** peuvent être écrits selon la spécification.

---

**Status:** ✅ Corrections appliquées et code conforme au protocole officiel TinyBMS Rev D
