Voici le rapport détaillé reprenant exactement la présentation précédente, enrichi avec les détails techniques précis (Unités, Échelles, Types de données, Bits) extraits de la documentation PDF fournie.

---

Voici un rapport détaillé comparant les spécifications du protocole UART de TinyBMS (Révision D) avec le code source fourni.

Le code se concentre principalement sur les commandes de gestion des registres (lecture/écriture générique et Modbus)
et la commande de réinitialisation. La grande majorité des commandes de "raccourci" (shortcut) permettant de lire 
des valeurs spécifiques (comme la tension ou le courant) sans spécifier d'adresse de registre ne sont pas implémentées,
le code préférant probablement utiliser la commande générique `Read individual registers (0x09)` pour obtenir 
ces mêmes informations.

### Tableau de synthèse des commandes

| Commande | Description | Statut dans le code |
| :--- | :--- | :--- |
| **0x02** | Reset / Clear Events & Stats | ✅ **Présente** |
| **0x03** | Read Register Block (Modbus) | ✅ **Présente** |
| **0x07** | Read Register Block (Proprietary) | ✅ **Présente** |
| **0x09** | Read Individual Registers | ✅ **Présente** |
| **0x0B** | Write Register Block (Proprietary)| ✅ **Présente** |
| **0x0D** | Write Individual Registers | ✅ **Présente** |
| **0x10** | Write Register Block (Modbus) | ✅ **Présente** |
| **0x11** | Read Newest Events | ❌ Pas présente |
| **0x12** | Read All Events | ❌ Pas présente |
| **0x14** | Read Battery Pack Voltage | ❌ Pas présente |
| **0x15** | Read Battery Pack Current | ❌ Pas présente |
| **0x16** | Read Max Cell Voltage | ❌ Pas présente |
| **0x17** | Read Min Cell Voltage | ❌ Pas présente |
| **0x18** | Read Online Status | ❌ Pas présente |
| **0x19** | Read Lifetime Counter | ❌ Pas présente |
| **0x1A** | Read Estimated SOC | ❌ Pas présente |
| **0x1B** | Read Temperatures | ❌ Pas présente |
| **0x1C** | Read Cell Voltages | ❌ Pas présente |
| **0x1D** | Read Settings Values | ❌ Pas présente |
| **0x1E** | Read Version | ❌ Pas présente |
| **0x1F** | Read Extended Version | ❌ Pas présente |
| **0x20** | Read Speed/Distance | ❌ Pas présente |

---

### Détails et Formats Attendus

Voici le détail pour chaque commande, basé sur la documentation "TinyBMS Communication Protocols".

#### 1. Reset Tiny BMS, clear Events and Statistics (0x02)
[cite_start]Cette commande sert à redémarrer le BMS ou effacer ses statistiques/événements[cite: 134].
* **Statut Code :** **Présente** (`TINYBMS_CMD_RESET` dans `tinybms_protocol.h` et implémentée dans `tinybms_build_reset_frame`).
* **Format Document :**
    [cite_start]`0xAA 0x02 OPTION CRC:LSB CRC:MSB` [cite: 135]
* **Détails des Données :**
    * **OPTION (Byte 3) :**
        * [cite_start]`0x01` : Clear Events[cite: 136].
        * [cite_start]`0x02` : Clear Statistics[cite: 137].
        * [cite_start]`0x05` : Reset BMS[cite: 138].

#### 2. Read Tiny BMS registers block (0x07)
[cite_start]Lecture propriétaire d'un bloc de registres[cite: 55].
* **Statut Code :** **Présente** (`TINYBMS_CMD_READ_BLOCK`).
* **Format Document :**
    [cite_start]`0xAA 0x07 RL ADDR:LSB ADDR:MSB CRC:LSB CRC:MSB` [cite: 56]
* **Détails des Données :**
    * [cite_start]**RL (Byte 3) :** Nombre de registres à lire (Registers to read)[cite: 61].
    * [cite_start]**ADDR (Byte 4-5) :** Adresse du premier registre du bloc[cite: 62].
    * [cite_start]**DATA (Réponse) :** Valeurs des registres au format `[UINT_16]`[cite: 59].
    * [cite_start]**PL (Réponse Byte 3) :** Payload length byte[cite: 64].
        * [cite_start]Bit 0 : Payload size in bytes (dernier paquet)[cite: 65].
        * [cite_start]Bit 1 : Current packet ID[cite: 65].

#### 3. Read Tiny BMS individual registers (0x09)
[cite_start]Lecture de registres individuels (commande la plus utilisée dans le code client)[cite: 72].
* **Statut Code :** **Présente** (`TINYBMS_CMD_READ_INDIVIDUAL`).
* **Format Document :**
    [cite_start]`0xAA 0x09 PL ADDR:LSB ADDR:MSB CRC:LSB CRC:MSB` [cite: 74]
* **Détails des Données :**
    * [cite_start]**PL (Byte 3) :** Payload length in bytes[cite: 75].
    * [cite_start]**ADDR :** Adresses des registres individuels au format `[UINT_16]`[cite: 74, 80].
    * [cite_start]**DATA (Réponse) :** Valeurs des registres au format `[UINT_16]`[cite: 79].

#### 4. Write Tiny BMS registers block (0x0B)
[cite_start]Écriture propriétaire d'un bloc de registres[cite: 85].
* **Statut Code :** **Présente** (`TINYBMS_CMD_WRITE_BLOCK`).
* **Format Document :**
    [cite_start]`0xAA 0x0B PL ADDR:LSB ADDR:MSB DATA1... DATAn CRC:LSB CRC:MSB` [cite: 87, 88]
* **Détails des Données :**
    * **ADDR (Byte 4-5) :** Adresse du premier registre du bloc. [cite_start]Adresses valides pour l'écriture : `0x012C` à `0x018F`[cite: 89].
    * [cite_start]**DATA :** Valeurs du bloc de registres à écrire au format `[UINT_16]`[cite: 87, 90].
    * [cite_start]**PL :** Payload length byte[cite: 91].

#### 5. Write Tiny BMS individual registers (0x0D)
[cite_start]Écriture d'un registre unique[cite: 94].
* **Statut Code :** **Présente** (`TINYBMS_CMD_WRITE_INDIVIDUAL`).
* **Format Document :**
    [cite_start]`0xAA 0x0D PL ADDR:LSB ADDR:MSB DATA:LSB DATA:MSB CRC:LSB CRC:MSB` [cite: 95, 100]
* **Détails des Données :**
    * **ADDR :** Adresse du registre individuel. [cite_start]Adresses valides pour l'écriture : `0x012C` à `0x018F`[cite: 101].
    * [cite_start]**DATA :** Valeur à écrire au format `[UINT_16]`[cite: 95].
    * [cite_start]**PL :** Payload length byte[cite: 103].

#### 6. Read registers block - MODBUS compatible (0x03)
[cite_start]Commande standard Modbus (Function 03) encapsulée[cite: 109].
* **Statut Code :** **Présente** (`TINYBMS_CMD_MODBUS_READ`).
* **Format Document :**
    [cite_start]`0xAA 0x03 ADDR:MSB ADDR:LSB 0x00 RL CRC:LSB CRC:MSB` [cite: 111]
* **Détails des Données :**
    * [cite_start]**ADDR (Byte 3-4) :** Adresse du premier registre du bloc[cite: 113]. Notez l'ordre MSB/LSB inversé par rapport aux commandes propriétaires.
    * **RL (Byte 6) :** Nombre de registres à lire. Max. [cite_start]127 registres (`0x7F`)[cite: 114].
    * [cite_start]**DATA (Réponse) :** Valeurs au format `[UINT_16]`[cite: 116].

#### 7. Write registers block - MODBUS compatible (0x10)
[cite_start]Commande standard Modbus (Function 16) encapsulée[cite: 119].
* **Statut Code :** **Présente** (`TINYBMS_CMD_MODBUS_WRITE`).
* **Format Document :**
    [cite_start]`0xAA 0x10 ADDR:MSB ADDR:LSB 0x00 RL PL DATA... CRC:LSB CRC:MSB` [cite: 120, 121]
* **Détails des Données :**
    * [cite_start]**ADDR (Byte 3-4) :** Adresse du premier registre du bloc[cite: 125].
    * **RL (Byte 6) :** Nombre de registres à écrire. Max. [cite_start]100 registres (`0x64`)[cite: 126].
    * [cite_start]**PL (Byte 7) :** Payload length in bytes[cite: 127].
    * [cite_start]**DATA :** Valeurs à écrire au format `[UINT_16]`[cite: 120].

#### 8. Commandes de raccourci (0x11 à 0x20)
Ces commandes permettent de récupérer des données formatées (souvent des `float` ou des structures) sans avoir à connaître l'adresse mémoire du registre. 
**Aucune de ces commandes n'est implémentée dans le code fourni.**

* [cite_start]**0x11 Read newest Events :** `0xAA 0x11 CRC CRC` [cite: 143]
    * [cite_start]**Réponse :** `BTSP` (BMS Timestamp, `[UINT_32]`, sec), `TSP` (Event Timestamp, `[UINT_24]`, sec), `EVENT` (Event ID, `[UINT_8]`)[cite: 145, 149, 150].

* [cite_start]**0x12 Read all Events :** `0xAA 0x12 CRC CRC` [cite: 153]
    * [cite_start]**Réponse :** Identique à 0x11 (`BTSP`, `TSP`, `EVENT`)[cite: 154, 157].

* [cite_start]**0x14 Read Battery Voltage :** `0xAA 0x14 CRC CRC` [cite: 165]
    * [cite_start]**Reg :** 36[cite: 163].
    * [cite_start]**Réponse :** `DATA` au format `[FLOAT]`[cite: 167].

* [cite_start]**0x15 Read Battery Current :** `0xAA 0x15 CRC CRC` [cite: 171]
    * [cite_start]**Reg :** 38[cite: 170].
    * [cite_start]**Réponse :** `DATA` au format `[FLOAT]`[cite: 173].

* [cite_start]**0x16 Read Max Cell Voltage :** `0xAA 0x16 CRC CRC` [cite: 177]
    * [cite_start]**Reg :** 41[cite: 176].
    * [cite_start]**Réponse :** `DATA` au format `[UINT_16]`[cite: 178].

* [cite_start]**0x17 Read Min Cell Voltage :** `0xAA 0x17 CRC CRC` [cite: 181]
    * [cite_start]**Reg :** 40[cite: 180].
    * [cite_start]**Réponse :** `DATA` au format `[UINT_16]`[cite: 182].

* [cite_start]**0x18 Read Online Status :** `0xAA 0x18 CRC CRC` [cite: 186]
    * [cite_start]**Reg :** 50[cite: 185].
    * [cite_start]**Réponse :** `DATA`[cite: 191]:
        * `0x91` : Charging [INFO]
        * `0x92` : Fully Charged [INFO]
        * `0x93` : Discharging [INFO]
        * `0x96` : Regeneration [INFO]
        * `0x97` : Idle [INFO]
        * `0x9B` : Fault [ERROR]

* [cite_start]**0x19 Read Lifetime Counter :** `0xAA 0x19 CRC CRC` [cite: 197]
    * [cite_start]**Reg :** 32[cite: 194].
    * [cite_start]**Réponse :** `DATA` au format `[UINT_32]`[cite: 199].

* [cite_start]**0x1A Read Estimated SOC :** `0xAA 0x1A CRC CRC` [cite: 203]
    * [cite_start]**Reg :** 46[cite: 202].
    * [cite_start]**Réponse :** `DATA` au format `[UINT_32]`[cite: 205].

* [cite_start]**0x1B Read Temperatures :** `0xAA 0x1B CRC CRC` [cite: 209]
    * [cite_start]**Regs :** 48, 42, 43[cite: 208].
    * [cite_start]**Réponse :** 3 valeurs au format `[INT_16]`[cite: 211].
        * [cite_start]`DATA1` : Température interne[cite: 213].
        * [cite_start]`DATA2` : Capteur externe #1 (`-32768` si non connecté)[cite: 214].
        * [cite_start]`DATA3` : Capteur externe #2 (`-32768` si non connecté)[cite: 214].

* [cite_start]**0x1C Read Cell Voltages :** `0xAA 0x1C CRC CRC` [cite: 222]
    * [cite_start]**Réponse :** Série de valeurs `DATA` au format `[UINT_16]` pour chaque cellule[cite: 224].

* [cite_start]**0x1D Read Settings Values :** `0xAA 0x1D OPTION 0x00 RL CRC CRC` [cite: 229]
    * [cite_start]**OPTION (Byte 3) :** `0x01` (Min), `0x02` (Max), `0x03` (Default), `0x04` (Current)[cite: 230].
    * [cite_start]**RL (Byte 5) :** Max 100 registres[cite: 231].
    * [cite_start]**Réponse :** `DATA` au format `[UINT_16]`[cite: 232].

* [cite_start]**0x1E Read Version :** `0xAA 0x1E CRC CRC` [cite: 236]
    * **Réponse :**
        * [cite_start]`DATA1` : Hardware version `[UINT_8]`[cite: 238].
        * [cite_start]`DATA2` : Hardware changes version `[UINT_8]`[cite: 239].
        * [cite_start]`DATA3` : Firmware public version `[UINT_8]`[cite: 240].
        * [cite_start]`DATA4` : Firmware internal version `[UINT_16]`[cite: 241].

* [cite_start]**0x1F Read Extended Version :** `0xAA 0x1F CRC CRC` [cite: 244]
    * **Réponse :**
        * [cite_start]Ajoute `DATA5` (Bootloader version, `[UINT_8]`) et `DATA6` (Register map version, `[UINT_8]`) aux données de la commande 0x1E[cite: 248].

* [cite_start]**0x20 Read Speed/Distance :** `0xAA 0x20 CRC CRC` [cite: 250]
    * **Réponse :**
        * [cite_start]`DATA1` : Speed (km/h) `[FLOAT]`[cite: 250].
        * [cite_start]`DATA2` : Left distance to empty (km) `[UINT_32]`[cite: 250].
        * [cite_start]`DATA3` : Estimated time left (seconds) `[UINT_32]`[cite: 250].

### Conclusion
Le code implémente une couche "bas niveau" robuste permettant l'accès complet à la mémoire du BMS via les commandes `0x09` (lecture) et `0x0D` (écriture). Cependant,
il n'implémente pas les commandes de haut niveau (shortcuts) qui faciliteraient la lecture de données télémétriques (tensions cellules, courant, SOC) en une seule requête simple. 
Pour obtenir ces données avec le code actuel, l'utilisateur doit connaître l'adresse du registre correspondant 
(disponible dans le chapitre 3 de la documentation) et utiliser `tinybms_read_register`.
