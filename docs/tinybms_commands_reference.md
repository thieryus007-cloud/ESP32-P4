# Référence Complète des Commandes TinyBMS

**Version du protocole:** TinyBMS Communication Protocols Revision D, 2025-07-04
**Date de mise à jour:** 2025-11-20

---

## Table des Matières

1. [Vue d'ensemble du protocole](#vue-densemble-du-protocole)
2. [Commandes disponibles](#commandes-disponibles)
3. [Format des trames](#format-des-trames)
4. [Implémentation](#implémentation)
5. [Tests et validation](#tests-et-validation)

---

## Vue d'ensemble du protocole

### Configuration UART

| Paramètre | Valeur |
|-----------|--------|
| **Baud rate** | 115200 |
| **Data bits** | 8 |
| **Parity** | None |
| **Stop bits** | 1 |
| **Flow control** | None |

### Format général de trame

Toutes les trames TinyBMS suivent ce format de base :

```
[Preamble] [Command/Response] [Payload Length] [Payload...] [CRC:LSB] [CRC:MSB]
    0xAA         1 byte            1 byte         N bytes      2 bytes
```

### CRC-16 (MODBUS)

- **Algorithme:** CRC-16 MODBUS
- **Polynomial:** 0xA001 (réfléchi de 0x8005)
- **Initial value:** 0xFFFF
- **Calcul:** Sur tous les bytes de la trame SAUF les 2 derniers bytes CRC
- **Byte order:** Little-endian (LSB first, then MSB)

---

## Commandes disponibles

### 1. Read Individual Registers (0x09) ✅ IMPLÉMENTÉ

**Description:** Lit un registre individuel du TinyBMS.

**Cas d'usage:**
- Lecture de valeurs de configuration
- Récupération de télémétrie en temps réel
- Diagnostic et monitoring

**Request (7 bytes):**
```
┌──────────┬─────────┬────┬──────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Addr:LSB │ Addr:MSB │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x09   │ 02 │    1B    │    1B    │   1B    │   1B    │
└──────────┴─────────┴────┴──────────┴──────────┴─────────┴─────────┘
```

**Response (9 bytes):**
```
┌──────────┬─────────┬────┬──────────┬──────────┬──────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Addr:LSB │ Addr:MSB │ Data:LSB │ Data:MSB │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x09   │ 04 │    1B    │    1B    │    1B    │    1B    │   1B    │   1B    │
└──────────┴─────────┴────┴──────────┴──────────┴──────────┴──────────┴─────────┴─────────┘
```

**Exemple - Lecture du registre 0x012C (Fully Charged Voltage):**
```
TX: AA 09 02 2C 01 [CRC]
RX: AA 09 04 2C 01 68 10 [CRC]
    → Data = 0x1068 = 4200 (4200 mV)
```

**Implémentation:**
- **ESP32-P4:** `tinybms_read_register()` dans `tinybms_client.cpp`
- **mac-local:** `readRegister()` dans `serial.js`
- **HMI:** Utilisé par `screen_tinybms_config.cpp` pour lire les registres

---

### 2. Write Individual Registers (0x0D) ✅ IMPLÉMENTÉ

**Description:** Écrit une valeur dans un registre du TinyBMS.

**Cas d'usage:**
- Configuration des paramètres de batterie
- Ajustement des seuils de protection
- Personnalisation du comportement du BMS

**Request (9 bytes):**
```
┌──────────┬─────────┬────┬──────────┬──────────┬──────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Addr:LSB │ Addr:MSB │ Data:LSB │ Data:MSB │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x0D   │ 04 │    1B    │    1B    │    1B    │    1B    │   1B    │   1B    │
└──────────┴─────────┴────┴──────────┴──────────┴──────────┴──────────┴─────────┴─────────┘
```

**Response ACK (5 bytes):**
```
┌──────────┬─────────┬──────┬─────────┬─────────┐
│ Preamble │   ACK   │ CMD  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x01   │ 0x0D │   1B    │   1B    │
└──────────┴─────────┴──────┴─────────┴─────────┘
```

**Response NACK (6 bytes):**
```
┌──────────┬──────┬──────┬────────────┬─────────┬─────────┐
│ Preamble │ NACK │ CMD  │ Error Code │ CRC:LSB │ CRC:MSB │
│   0xAA   │ 0x00 │ 0x0D │     1B     │   1B    │   1B    │
└──────────┴──────┴──────┴────────────┴─────────┴─────────┘
```

**Codes d'erreur NACK:**
- `0x01`: Invalid register address
- `0x02`: Read-only register
- `0x03`: Value out of range
- `0x04`: CRC error
- `0xFF`: Unknown error

**Exemple - Écriture de 4200mV dans le registre 0x012C:**
```
TX: AA 0D 04 2C 01 68 10 [CRC]
RX: AA 01 0D [CRC]  (ACK)
```

**Registres modifiables:**
- Plage: `0x012C` à `0x018F`
- Voir `tinybms_registers.h` pour la liste complète

**Implémentation:**
- **ESP32-P4:** `tinybms_write_register()` dans `tinybms_client.cpp`
- **mac-local:** `writeRegister()` dans `serial.js`
- **HMI:** Utilisé par `screen_tinybms_config.cpp` pour configurer les registres

---

### 3. Read Registers Block (0x07) ⚠️ NON IMPLÉMENTÉ

**Description:** Lit un bloc de registres consécutifs (protocole propriétaire TinyBMS).

**Cas d'usage:**
- Lecture rapide de plusieurs registres consécutifs
- Optimisation de la bande passante UART
- Récupération de télémétrie complète

**Request (8 bytes):**
```
┌──────────┬─────────┬────┬──────────────┬──────────────┬───────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Start:LSB    │ Start:MSB    │ Count │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x07   │ 03 │      1B      │      1B      │  1B   │   1B    │   1B    │
└──────────┴─────────┴────┴──────────────┴──────────────┴───────┴─────────┴─────────┘
```

**Response (variable):**
```
┌──────────┬─────────┬────┬──────────────┬──────────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Start:LSB    │ Start:MSB    │ Data...  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x07   │ N  │      1B      │      1B      │  N bytes │   1B    │   1B    │
└──────────┴─────────┴────┴──────────────┴──────────────┴──────────┴─────────┴─────────┘
```

**Exemple - Lecture de 3 registres à partir de 0x012C:**
```
TX: AA 07 03 2C 01 03 [CRC]
RX: AA 07 08 2C 01 68 10 69 10 6A 10 [CRC]
    → Reg 0x012C = 0x1068
    → Reg 0x012D = 0x1069
    → Reg 0x012E = 0x106A
```

**Avantages:**
- Réduit le nombre de transactions UART
- Réduit la latence globale
- Économise de la bande passante

**Implémentation recommandée:**
- Ajouter `tinybms_read_block()` dans `tinybms_client.cpp`
- Ajouter `readBlock()` dans `serial.js`

---

### 4. Write Registers Block (0x0B) ⚠️ NON IMPLÉMENTÉ

**Description:** Écrit un bloc de registres consécutifs (protocole propriétaire TinyBMS).

**Cas d'usage:**
- Configuration rapide de plusieurs paramètres
- Mise à jour batch de configuration
- Optimisation de la bande passante UART

**Request (variable):**
```
┌──────────┬─────────┬────┬──────────────┬──────────────┬───────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Start:LSB    │ Start:MSB    │ Count │ Data...  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x0B   │ N  │      1B      │      1B      │  1B   │  N bytes │   1B    │   1B    │
└──────────┴─────────┴────┴──────────────┴──────────────┴───────┴──────────┴─────────┴─────────┘
```

**Response ACK/NACK:** Identique à Write Individual (0x0D)

**Exemple - Écriture de 3 registres à partir de 0x012C:**
```
TX: AA 0B 09 2C 01 03 68 10 69 10 6A 10 [CRC]
RX: AA 01 0B [CRC]  (ACK)
```

**Implémentation recommandée:**
- Ajouter `tinybms_write_block()` dans `tinybms_client.cpp`
- Ajouter `writeBlock()` dans `serial.js`

---

### 5. MODBUS Read (0x03) ⚠️ NON IMPLÉMENTÉ

**Description:** Lit des registres en utilisant le protocole MODBUS standard.

**Cas d'usage:**
- Compatibilité avec systèmes MODBUS existants
- Intégration avec outils MODBUS standard
- Interopérabilité

**Request (8 bytes):**
```
┌──────────┬─────────┬────┬──────────────┬──────────────┬──────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Start:LSB    │ Start:MSB    │ Qty:LSB  │ Qty:MSB  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x03   │ 04 │      1B      │      1B      │    1B    │    1B    │   1B    │   1B    │
└──────────┴─────────┴────┴──────────────┴──────────────┴──────────┴──────────┴─────────┴─────────┘
```

**Response (variable):**
```
┌──────────┬─────────┬────┬────────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Byte Count │ Data...  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x03   │ N  │     1B     │  N bytes │   1B    │   1B    │
└──────────┴─────────┴────┴────────────┴──────────┴─────────┴─────────┘
```

**Implémentation recommandée:**
- Ajouter `tinybms_modbus_read()` dans `tinybms_client.cpp`
- Ajouter `modbusRead()` dans `serial.js`

---

### 6. MODBUS Write (0x10) ⚠️ NON IMPLÉMENTÉ

**Description:** Écrit des registres en utilisant le protocole MODBUS standard.

**Cas d'usage:**
- Compatibilité avec systèmes MODBUS existants
- Intégration avec outils MODBUS standard
- Interopérabilité

**Request (variable):**
```
┌──────────┬─────────┬────┬──────────────┬──────────────┬──────────┬──────────┬────────────┬──────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Start:LSB    │ Start:MSB    │ Qty:LSB  │ Qty:MSB  │ Byte Count │ Data...  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x10   │ N  │      1B      │      1B      │    1B    │    1B    │     1B     │  N bytes │   1B    │   1B    │
└──────────┴─────────┴────┴──────────────┴──────────────┴──────────┴──────────┴────────────┴──────────┴─────────┴─────────┘
```

**Response ACK/NACK:** Identique aux autres commandes Write

**Implémentation recommandée:**
- Ajouter `tinybms_modbus_write()` dans `tinybms_client.cpp`
- Ajouter `modbusWrite()` dans `serial.js`

---

### 7. System Reset (Command 0x02) ✅ IMPLÉMENTÉ

**Description:** Redémarre le TinyBMS en utilisant la commande UART dédiée.

**Cas d'usage:**
- Redémarrage du BMS suite à une configuration
- Récupération après une erreur système
- Réinitialisation du firmware

**Request (6 bytes):**
```
┌──────────┬─────────┬────┬────────┬─────────┬─────────┐
│ Preamble │ Command │ PL │ Option │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x02   │ 01 │  0x05  │   1B    │   1B    │
└──────────┴─────────┴────┴────────┴─────────┴─────────┘
```

**Options disponibles:**
- `0x05`: Reset BMS (redémarrage du système)

**Response ACK (5 bytes):**
```
┌──────────┬─────────┬──────┬─────────┬─────────┐
│ Preamble │   ACK   │ CMD  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x01   │ 0x02 │   1B    │   1B    │
└──────────┴─────────┴──────┴─────────┴─────────┘
```

**Exemple - Redémarrage du BMS:**
```
TX: AA 02 01 05 [CRC]
RX: AA 01 02 [CRC]  (ACK)
```

**Notes importantes:**
- Conforme à la section 1.1.8 du protocole TinyBMS Rev D
- Après le redémarrage, le BMS sera temporairement déconnecté
- Il est recommandé d'attendre quelques secondes avant de tenter une reconnexion

**Implémentation:**
- **ESP32-P4:** `tinybms_restart()` dans `tinybms_client.cpp`
- **Protocol:** `tinybms_build_reset_frame()` dans `tinybms_protocol.cpp`
- **mac-local:** `buildRestartFrame()` dans `serial.js`
- **HMI:** Boutons "Restart" dans `screen_tinybms_config.cpp` et `screen_bms_control.cpp`

---

## Format des trames

### ACK (Acknowledgement) - 5 bytes

```
┌──────────┬─────────┬──────────┬─────────┬─────────┐
│ Preamble │   ACK   │ Command  │ CRC:LSB │ CRC:MSB │
│   0xAA   │  0x01   │   Cmd    │   1B    │   1B    │
└──────────┴─────────┴──────────┴─────────┴─────────┘
```

**Signification:** La commande a été reçue et exécutée avec succès.

### NACK (Negative Acknowledgement) - 6 bytes

```
┌──────────┬──────┬──────────┬────────────┬─────────┬─────────┐
│ Preamble │ NACK │ Command  │ Error Code │ CRC:LSB │ CRC:MSB │
│   0xAA   │ 0x00 │   Cmd    │     1B     │   1B    │   1B    │
└──────────┴──────┴──────────┴────────────┴─────────┴─────────┘
```

**Signification:** La commande a été reçue mais n'a PAS pu être exécutée.

---

## Implémentation

### Implémentation ESP32-P4 (Firmware)

**Fichiers principaux:**
- `components/tinybms_client/tinybms_protocol.h` - Définitions du protocole
- `components/tinybms_client/tinybms_protocol.cpp` - Fonctions de construction/parsing de trames
- `components/tinybms_client/tinybms_client.h` - API client haut niveau
- `components/tinybms_client/tinybms_client.cpp` - Implémentation client UART

**API publique:**
```cpp
// Lecture d'un registre
esp_err_t tinybms_read_register(uint16_t address, uint16_t *value);

// Écriture d'un registre (avec vérification)
esp_err_t tinybms_write_register(uint16_t address, uint16_t value, uint16_t *verified_value);

// Redémarrage du système
esp_err_t tinybms_restart(void);

// État et statistiques
tinybms_state_t tinybms_get_state(void);
esp_err_t tinybms_get_stats(tinybms_stats_t *stats);
```

### Implémentation mac-local (Node.js)

**Fichiers principaux:**
- `Exemple/mac-local/src/serial.js` - Client UART TinyBMS
- `Exemple/mac-local/src/server.js` - API REST Express
- `Exemple/mac-local/src/registers.js` - Parser de registres

**API REST:**
```
GET  /api/ports                 - Liste des ports série
POST /api/connection/open       - Ouvrir connexion
POST /api/connection/close      - Fermer connexion
GET  /api/connection/status     - État connexion

GET  /api/registers             - Lire tous les registres
POST /api/registers             - Écrire un registre
POST /api/system/restart        - Redémarrer TinyBMS
```

**API JavaScript:**
```javascript
const serial = new TinyBmsSerial();

// Lecture
const rawValue = await serial.readRegister(0x012C);

// Écriture
const verifiedValue = await serial.writeRegister(0x012C, 4200);

// Redémarrage
await serial.restartTinyBms();

// Lecture catalogue
const results = await serial.readCatalogue(descriptors);
```

### Implémentation HMI (LVGL)

**Écrans utilisant les commandes:**

1. **`screen_tinybms_config.cpp`** - Configuration des registres
   - Lecture: `tinybms_model_read_all()`
   - Écriture: `tinybms_model_write_register()`
   - Redémarrage: `tinybms_restart()`

2. **`screen_bms_control.cpp`** - Contrôle BMS
   - Affichage des limites CVL/CCL/DCL
   - Affichage de l'état de protection

3. **`screen_tinybms_status.cpp`** - État TinyBMS
   - Affichage de l'état de connexion
   - Statistiques UART

---

## Tests et validation

### Tests unitaires recommandés

1. **Test de lecture d'un registre valide**
   ```
   → Lire 0x012C (Fully Charged Voltage)
   ✓ Réponse reçue
   ✓ Valeur cohérente (2500-5000 mV)
   ```

2. **Test d'écriture d'un registre valide**
   ```
   → Écrire 4200 dans 0x012C
   ✓ ACK reçu
   ✓ Relecture = 4200
   ```

3. **Test d'écriture dans un registre read-only**
   ```
   → Écrire dans 0x0064 (tension cellule)
   ✓ NACK reçu
   ✓ Error code = 0x02
   ```

4. **Test CRC invalide**
   ```
   → Envoyer trame avec CRC corrompu
   ✓ Pas de réponse (timeout)
   ```

5. **Test de redémarrage**
   ```
   → Envoyer commande restart
   ✓ ACK reçu
   ✓ Connexion perdue temporairement
   ✓ Reconnexion réussie
   ```

### Tests d'intégration

1. **Test HMI → Firmware → TinyBMS**
   - Interface GUI pour lire/écrire registres
   - Vérifier cohérence des valeurs affichées

2. **Test mac-local → TinyBMS**
   - Utiliser l'interface web
   - Tester toutes les routes API

3. **Test de performance**
   - Mesurer la latence de lecture (attendu: < 100ms)
   - Mesurer la latence d'écriture (attendu: < 200ms)

---

## Résumé des commandes

| Code | Nom | Status | ESP32-P4 | mac-local | HMI |
|------|-----|--------|----------|-----------|-----|
| 0x02 | System Reset | ✅ Implémenté | ✅ | ✅ | ✅ |
| 0x09 | Read Individual | ✅ Implémenté | ✅ | ✅ | ✅ |
| 0x0D | Write Individual | ✅ Implémenté | ✅ | ✅ | ✅ |
| 0x07 | Read Block | ⚠️ Non implémenté | ❌ | ❌ | ❌ |
| 0x0B | Write Block | ⚠️ Non implémenté | ❌ | ❌ | ❌ |
| 0x03 | MODBUS Read | ⚠️ Non implémenté | ❌ | ❌ | ❌ |
| 0x10 | MODBUS Write | ⚠️ Non implémenté | ❌ | ❌ | ❌ |

---

## Prochaines étapes

Pour avoir une implémentation **complète** conforme à la documentation TinyBMS Rev D:

1. ✅ **Commande Reset** (0x02) - FAIT
2. ✅ **Commandes individuelles** (0x09, 0x0D) - FAIT
3. ⚠️ **Commandes bloc** (0x07, 0x0B) - À IMPLÉMENTER
4. ⚠️ **Commandes MODBUS** (0x03, 0x10) - À IMPLÉMENTER
5. ✅ **Documentation** - FAIT

---

## Références

- **Document officiel:** TinyBMS Communication Protocols Revision D, 2025-07-04
- **Fichier:** `/home/user/ESP32-P4/docs/TinyBMS_Communication_Protocols_Rev_D 2.pdf`
- **Corrections appliquées:** `/home/user/ESP32-P4/UART_PROTOCOL_FIXES.md`
