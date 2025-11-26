# Guide de Référence Complet - Communication UART TinyBMS

> **Document de référence officiel basé sur le protocole Enepaq TinyBMS Rev D (2025-07-04)**
>
> Ce document constitue la référence exhaustive pour la communication UART avec le TinyBMS. Il couvre l'intégralité des commandes, registres, et mécanismes de communication.

---

## Table des matières

1. [Configuration UART](#1-configuration-uart)
2. [Structure des Trames](#2-structure-des-trames)
3. [Calcul du CRC](#3-calcul-du-crc)
4. [Commandes Propriétaires](#4-commandes-propriétaires)
5. [Commandes MODBUS](#5-commandes-modbus)
6. [Cartographie des Registres](#6-cartographie-des-registres)
7. [Événements (Events)](#7-événements-events)
8. [Exemples Pratiques](#8-exemples-pratiques)

---

## 1. Configuration UART

### Paramètres de communication

| Paramètre | Valeur | Modifiable |
|-----------|--------|------------|
| Baudrate | 115200 bit/s | ❌ Non |
| Data bits | 8 | ❌ Non |
| Stop bit | 1 | ❌ Non |
| Parity | None | ❌ Non |
| Flow control | None | ❌ Non |

⚠️ **Important** : Ces paramètres ne peuvent pas être modifiés par l'utilisateur.

### Note sur le mode veille

Si le TinyBMS est en mode veille, **la première commande doit être envoyée deux fois** :
1. La première commande réveille le BMS
2. La seconde commande obtient la réponse

Le BMS ne se rendort pas tant que la communication est active.

---

## 2. Structure des Trames

### Format général

Toutes les trames (requêtes et réponses) suivent ce format :

```
[Header] [Command] [Payload] [CRC_LSB] [CRC_MSB]
```

- **Header** : `0xAA` (constant)
- **Command** : Code de la commande (1 octet)
- **Payload** : Données variables selon la commande
- **CRC** : Checksum 16 bits (LSB first)

### Types de réponses

Le BMS peut répondre de trois façons :

#### Réponse ACK (Acquittement)
```
0xAA 0x01 CMD CRC_LSB CRC_MSB
```

#### Réponse NACK (Erreur)
```
0xAA 0x00 CMD ERROR CRC_LSB CRC_MSB
```

Codes d'erreur :
- `0x00` : Erreur de commande
- `0x01` : Erreur de CRC

#### Réponse avec données
```
0xAA CMD PL [DATA...] CRC_LSB CRC_MSB
```

### Payload Length (PL)

Pour les commandes retournant des données variables, l'octet PL indique la longueur :

| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|-------|-------|-------|-------|-------|-------|-------|-------|
| **0** | Reserved | Taille payload en octets (dernier paquet) |
| **1** | Reserved | ID du paquet actuel |

---

## 3. Calcul du CRC

### Algorithme

Le TinyBMS utilise un CRC-16 basé sur le polynôme MODBUS : **x¹⁶ + x¹⁵ + x² + 1** (0x8005)

### Implémentation C

```c
const static uint16_t crcTable[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t CRC16(const uint8_t* data, uint16_t length) {
    uint8_t tmp;
    uint16_t crcWord = 0xFFFF;

    while (length--) {
        tmp = *data++ ^ crcWord;
        crcWord >>= 8;
        crcWord ^= crcTable[tmp];
    }

    return crcWord;
}
```

### Utilisation

Le CRC est calculé sur **tous les octets** de la trame avant le CRC lui-même, puis ajouté en **LSB first** :

```
Trame : 0xAA 0x14
CRC = CRC16([0xAA, 0x14], 2) = 0x1B3F
Trame complète : 0xAA 0x14 0x3F 0x1B
```

---

## 4. Commandes Propriétaires

### Vue d'ensemble

| Code | Nom | Description | Type |
|------|-----|-------------|------|
| 0x02 | Reset/Clear | Reset BMS, effacer Events/Stats | Contrôle |
| 0x07 | Read Block | Lire un bloc de registres | Lecture |
| 0x09 | Read Individual | Lire des registres individuels | Lecture |
| 0x0B | Write Block | Écrire un bloc de registres | Écriture |
| 0x0D | Write Individual | Écrire des registres individuels | Écriture |
| 0x11 | Read Newest Events | Lire les derniers événements | Lecture |
| 0x12 | Read All Events | Lire tous les événements | Lecture |
| 0x14 | Read Voltage | Lire tension du pack | Lecture |
| 0x15 | Read Current | Lire courant du pack | Lecture |
| 0x16 | Read Max Cell V | Lire tension cellule max | Lecture |
| 0x17 | Read Min Cell V | Lire tension cellule min | Lecture |
| 0x18 | Read Status | Lire statut en ligne | Lecture |
| 0x19 | Read Lifetime | Lire compteur de vie | Lecture |
| 0x1A | Read SOC | Lire état de charge | Lecture |
| 0x1B | Read Temperatures | Lire températures | Lecture |
| 0x1C | Read Cell Voltages | Lire toutes les tensions | Lecture |
| 0x1D | Read Settings | Lire paramètres (min/max/def/cur) | Lecture |
| 0x1E | Read Version | Lire version | Lecture |
| 0x1F | Read Extended Version | Lire version étendue | Lecture |
| 0x20 | Read Speed/Distance | Lire vitesse/distance/temps | Lecture |

---

### 0x02 - Reset BMS / Clear Events / Clear Statistics

#### Requête
```
0xAA 0x02 OPTION CRC_LSB CRC_MSB
```

Options :
- `0x01` : Effacer les événements
- `0x02` : Effacer les statistiques
- `0x05` : Reset du BMS

#### Réponse (OK)
```
0xAA 0x01 0x02 CRC_LSB CRC_MSB
```

#### Réponse (Erreur)
```
0xAA 0x00 0x02 ERROR CRC_LSB CRC_MSB
```

---

### 0x07 - Read Tiny BMS Registers Block

Lecture d'un bloc consécutif de registres.

#### Requête
```
0xAA 0x07 RL ADDR_LSB ADDR_MSB CRC_LSB CRC_MSB
```

- **RL** : Nombre de registres à lire
- **ADDR** : Adresse du premier registre (UINT16)

#### Réponse (OK)
```
0xAA 0x07 PL DATA1_LSB DATA1_MSB ... DATAn_LSB DATAn_MSB CRC_LSB CRC_MSB
```

#### Exemple
Lire 5 registres à partir de l'adresse 0x0005 (cellules 5 à 9) :

**Requête** :
```
0xAA 0x07 0x05 0x05 0x00 [CRC_LSB] [CRC_MSB]
```

**Réponse** :
```
0xAA 0x07 0x0A 0x40 0x97 0x40 0x97 0x2C 0x97 0x2C 0x97 0x2C 0x97 [CRC]
```
- PL = 0x0A (10 octets = 5 registres × 2 octets)
- Cell 5 = 0x9740 = 38720 = 3.872V
- Cell 6 = 0x9740 = 38720 = 3.872V
- Cell 7 = 0x972C = 38700 = 3.870V
- Cell 8 = 0x972C = 38700 = 3.870V
- Cell 9 = 0x972C = 38700 = 3.870V

---

### 0x09 - Read Individual Registers

Lecture de registres non consécutifs.

#### Requête
```
0xAA 0x09 PL ADDR1_LSB ADDR1_MSB ... ADDRn_LSB ADDRn_MSB CRC_LSB CRC_MSB
```

- **PL** : Longueur du payload en octets

#### Réponse (OK)
```
0xAA 0x09 PL ADDR1_LSB ADDR1_MSB DATA1_LSB DATA1_MSB ... CRC_LSB CRC_MSB
```

Chaque réponse contient 4 octets par registre : adresse (2) + donnée (2).

---

### 0x0B - Write Registers Block

Écriture d'un bloc consécutif de registres.

#### Requête
```
0xAA 0x0B PL ADDR_LSB ADDR_MSB DATA1_LSB DATA1_MSB ... DATAn_LSB DATAn_MSB CRC_LSB CRC_MSB
```

- **Adresses valides** : `0x012C` à `0x018F` (registres de configuration)
- **ADDR** : Adresse du premier registre
- **PL** : Longueur du payload

#### Réponse (OK)
```
0xAA 0x01 0x0B CRC_LSB CRC_MSB
```

---

### 0x0D - Write Individual Registers

Écriture de registres non consécutifs.

#### Requête
```
0xAA 0x0D PL ADDR1_LSB ADDR1_MSB DATA1_LSB DATA1_MSB ... CRC_LSB CRC_MSB
```

- **Adresses valides** : `0x012C` à `0x018F`

#### Réponse (OK)
```
0xAA 0x01 0x0D CRC_LSB CRC_MSB
```

---

### 0x11 - Read Newest Events

Lire les événements les plus récents.

#### Requête
```
0xAA 0x11 CRC_LSB CRC_MSB
```

#### Réponse (OK) - Message 1
```
0xAA 0x11 PL BTSP_LSB BTSP BTSP BTSP_MSB TSP1_LSB TSP1 TSP1_MSB EVENT1 ... CRC
```

- **BTSP** : Timestamp du BMS (UINT32, en secondes)
- **TSP** : Timestamp de l'événement (UINT24, en secondes)
- **EVENT** : ID de l'événement (UINT8)

#### Réponse (OK) - Messages suivants
```
0xAA 0x11 PL TSPn_LSB TSPn TSPn_MSB EVENTn CRC_LSB CRC_MSB
```

---

### 0x12 - Read All Events

Identique à 0x11 mais retourne TOUS les événements enregistrés (jusqu'à 49 événements).

---

### 0x14 - Read Battery Pack Voltage (Reg:36)

#### Requête
```
0xAA 0x14 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x14 DATA_LSB DATA DATA DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : Tension du pack (FLOAT, 4 octets, en Volts)

---

### 0x15 - Read Battery Pack Current (Reg:38)

#### Requête
```
0xAA 0x15 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x15 DATA_LSB DATA DATA DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : Courant du pack (FLOAT, 4 octets, en Ampères)

---

### 0x16 - Read Max Cell Voltage (Reg:41)

#### Requête
```
0xAA 0x16 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x16 DATA_LSB DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : Tension cellule max (UINT16, en mV)

---

### 0x17 - Read Min Cell Voltage (Reg:40)

#### Requête
```
0xAA 0x17 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x17 DATA_LSB DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : Tension cellule min (UINT16, en mV)

---

### 0x18 - Read Online Status (Reg:50)

#### Requête
```
0xAA 0x18 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x18 DATA_LSB DATA_MSB CRC_LSB CRC_MSB
```

**Codes de statut** :
- `0x91` : Charging (En charge) [INFO]
- `0x92` : Fully Charged (Complètement chargé) [INFO]
- `0x93` : Discharging (Décharge) [INFO]
- `0x96` : Regeneration (Régénération) [INFO]
- `0x97` : Idle (Repos) [INFO]
- `0x9B` : Fault (Défaut) [ERROR]

---

### 0x19 - Read Lifetime Counter (Reg:32)

#### Requête
```
0xAA 0x19 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x19 DATA_LSB DATA DATA DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : Compteur de vie (UINT32, en secondes)

---

### 0x1A - Read SOC (Reg:46)

#### Requête
```
0xAA 0x1A CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x1A DATA_LSB DATA DATA DATA_MSB CRC_LSB CRC_MSB
```

- **DATA** : État de charge (UINT32, résolution 0.000001%)

---

### 0x1B - Read Temperatures (Reg:48, 42, 43)

#### Requête
```
0xAA 0x1B CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x1B PL DATA1_LSB DATA1_MSB DATA2_LSB DATA2_MSB DATA3_LSB DATA3_MSB CRC_LSB CRC_MSB
```

- **DATA1** : Température interne du BMS (INT16, résolution 0.1°C)
- **DATA2** : Sonde externe #1 (INT16, résolution 0.1°C, -32768 si déconnectée)
- **DATA3** : Sonde externe #2 (INT16, résolution 0.1°C, -32768 si déconnectée)

#### Exemple
Commande : `0xAA 0x1B 0x3F 0x1B`

Réponse : `0xAA 0x1B 0x06 0x16 0x01 0x14 0x01 0x16 0x01 0x0E 0x4E`
- PL = 6 octets
- Temp interne = 0x0116 = 278 → 27.8°C
- Temp externe #1 = 0x0114 = 276 → 27.6°C
- Temp externe #2 = 0x0116 = 278 → 27.8°C

---

### 0x1C - Read Cell Voltages

#### Requête
```
0xAA 0x1C CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x1C PL DATA1_LSB DATA1_MSB ... DATAn_LSB DATAn_MSB CRC_LSB CRC_MSB
```

- **DATAx** : Tension de la cellule x (UINT16, résolution 0.1 mV)
- Le nombre de cellules dépend de la configuration du BMS (4 à 16 cellules)

---

### 0x1D - Read Settings Values

Permet de lire les valeurs min, max, par défaut ou actuelles des paramètres.

#### Requête
```
0xAA 0x1D OPTION 0x00 RL CRC_LSB CRC_MSB
```

**Options** :
- `0x01` : Valeurs minimales
- `0x02` : Valeurs maximales
- `0x03` : Valeurs par défaut
- `0x04` : Valeurs actuelles

- **RL** : Nombre de registres à lire (max 100)

#### Réponse (OK)
```
0xAA 0x1D PL DATA1_LSB DATA1_MSB ... DATAn_LSB DATAn_MSB CRC_LSB CRC_MSB
```

---

### 0x1E - Read Version

#### Requête
```
0xAA 0x1E CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x1E PL DATA1 DATA2 DATA3 DATA4_LSB DATA4_MSB CRC_LSB CRC_MSB
```

- **DATA1** : Version matérielle (UINT8)
- **DATA2** : Version des changements matériels (UINT8)
- **DATA3** : Version publique du firmware (UINT8)
- **DATA4** : Version interne du firmware (UINT16)

---

### 0x1F - Read Extended Version

#### Requête
```
0xAA 0x1F CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x1F PL DATA1 DATA2 DATA3 DATA4_LSB DATA4_MSB DATA5 DATA6 CRC_LSB CRC_MSB
```

- **DATA1** : Version matérielle
- **DATA2** : Version changements matériel
- **DATA3** : Version publique firmware
- **DATA4** : Version interne firmware
- **DATA5** : Version bootloader
- **DATA6** : Version de la cartographie des registres

---

### 0x20 - Read Speed, Distance, Time

#### Requête
```
0xAA 0x20 CRC_LSB CRC_MSB
```

#### Réponse (OK)
```
0xAA 0x20 DATA1_LSB ... DATA1_MSB DATA2_LSB ... DATA2_MSB DATA3_LSB ... DATA3_MSB CRC_LSB CRC_MSB
```

- **DATA1** : Vitesse (FLOAT, 4 octets, en km/h)
- **DATA2** : Distance restante jusqu'à batterie vide (UINT32, en km)
- **DATA3** : Temps estimé restant (UINT32, en secondes)

---

## 5. Commandes MODBUS

Le TinyBMS supporte les commandes MODBUS standard 03 (Read Holding Registers) et 16 (Write Multiple Registers).

### 0x03 - Read Holding Registers (MODBUS)

#### Requête
```
0xAA 0x03 ADDR_MSB ADDR_LSB 0x00 RL CRC_LSB CRC_MSB
```

- **ADDR** : Adresse du premier registre (**MSB first**, différent des commandes propriétaires !)
- **RL** : Nombre de registres à lire (max 127)

#### Réponse (OK)
```
0xAA 0x03 PL DATA1_MSB DATA1_LSB ... DATAn_MSB DATAn_LSB CRC_LSB CRC_MSB
```

⚠️ **Important** : Les données sont retournées en **MSB first** (big-endian).

#### Exemple - Lecture des cellules 5 à 9

**Requête** :
```
0xAA 0x03 0x00 0x05 0x00 0x05 0x8C 0x13
```
- Adresse : 0x0005 (MSB first)
- Nombre : 5 registres

**Réponse** :
```
0xAA 0x03 0x0A 0x97 0x40 0x97 0x40 0x97 0x2C 0x97 0x2C 0x97 0x2C 0x3E 0xC7
```
- PL = 10 octets
- Cell 5 = 0x9740 (MSB first) = 38720 = 3.872V
- Cell 6 = 0x9740 = 3.872V
- Cell 7 = 0x972C = 3.870V
- Cell 8 = 0x972C = 3.870V
- Cell 9 = 0x972C = 3.870V

---

### 0x10 - Write Multiple Registers (MODBUS)

#### Requête
```
0xAA 0x10 ADDR_MSB ADDR_LSB 0x00 RL PL DATA1_MSB DATA1_LSB ... CRC_LSB CRC_MSB
```

- **ADDR** : Adresse du premier registre (**MSB first**)
- **RL** : Nombre de registres à écrire (max 100)
- **PL** : Longueur du payload en octets

#### Réponse (OK)
```
0xAA 0x10 ADDR_MSB ADDR_LSB 0x00 RL CRC_LSB CRC_MSB
```

#### Exemple - Configuration seuils Over/Under Voltage

Configurer :
- Over-Voltage Cutoff (Reg 315 = 0x013B) → 4.2V = 4200mV = 0x1068
- Under-Voltage Cutoff (Reg 316 = 0x013C) → 2.5V = 2500mV = 0x09C4

**Requête** :
```
0xAA 0x10 0x01 0x3B 0x00 0x02 0x04 0x10 0x68 0x09 0xC4 0x19 0x61
```
- ADDR = 0x013B (MSB first)
- RL = 0x02 (2 registres)
- PL = 0x04 (4 octets de données)
- DATA1 = 0x1068 (MSB first)
- DATA2 = 0x09C4 (MSB first)

**Réponse** :
```
0xAA 0x10 0x01 0x3B 0x00 0x02 0x28 0x22
```
- Confirmation : 2 registres écrits à partir de 0x013B

---

## 6. Cartographie des Registres

### 6.1 Live Data (Registres 0-99)

| Registre | Nom | Type | Résolution | Accès |
|----------|-----|------|------------|-------|
| 0 | Cell 1 Voltage | UINT16 | 0.1 mV | R |
| 1 | Cell 2 Voltage | UINT16 | 0.1 mV | R |
| 2 | Cell 3 Voltage | UINT16 | 0.1 mV | R |
| 3 | Cell 4 Voltage | UINT16 | 0.1 mV | R |
| 4 | Cell 5 Voltage | UINT16 | 0.1 mV | R |
| 5 | Cell 6 Voltage | UINT16 | 0.1 mV | R |
| 6 | Cell 7 Voltage | UINT16 | 0.1 mV | R |
| 7 | Cell 8 Voltage | UINT16 | 0.1 mV | R |
| 8 | Cell 9 Voltage | UINT16 | 0.1 mV | R |
| 9 | Cell 10 Voltage | UINT16 | 0.1 mV | R |
| 10 | Cell 11 Voltage | UINT16 | 0.1 mV | R |
| 11 | Cell 12 Voltage | UINT16 | 0.1 mV | R |
| 12 | Cell 13 Voltage | UINT16 | 0.1 mV | R |
| 13 | Cell 14 Voltage | UINT16 | 0.1 mV | R |
| 14 | Cell 15 Voltage | UINT16 | 0.1 mV | R |
| 15 | Cell 16 Voltage | UINT16 | 0.1 mV | R |
| 16-31 | **Reserved** | - | - | R |
| 32-33 | BMS Lifetime Counter | UINT32 | 1 s | R |
| 34-35 | Estimated Time Left | UINT32 | 1 s | R |
| 36-37 | Battery Pack Voltage | FLOAT | 1 V | R |
| 38-39 | Battery Pack Current | FLOAT | 1 A | R |
| 40 | Minimal Cell Voltage | UINT16 | 1 mV | R |
| 41 | Maximal Cell Voltage | UINT16 | 1 mV | R |
| 42 | External Temp Sensor #1 | INT16 | 0.1 °C | R |
| 43 | External Temp Sensor #2 | INT16 | 0.1 °C | R |
| 44 | Distance Left | UINT16 | 1 km | R |
| 45 | State Of Health | UINT16 | 0.002 % | R |
| 46-47 | State Of Charge | UINT32 | 0.000001 % | R |
| 48 | BMS Internal Temperature | INT16 | 0.1 °C | R |
| 49 | **Reserved** | - | - | R |
| 50 | BMS Online Status | UINT16 | - | R |
| 51 | Balancing Decision Bits | UINT16 | Bitmap | R |
| 52 | Real Balancing Bits | UINT16 | Bitmap | R |
| 53 | Number Of Detected Cells | UINT16 | - | R |
| 54-55 | Speed | FLOAT | km/h | R |
| 56-99 | **Reserved** | - | - | R |

#### Notes sur les Live Data

**Températures** :
- Les sondes externes retournent `-32768` (0x8000) si déconnectées

**Status (Reg 50)** :
- `0x91` : Charging
- `0x92` : Fully Charged
- `0x93` : Discharging
- `0x96` : Regeneration
- `0x97` : Idle
- `0x9B` : Fault

**Balancing Bits (Reg 51-52)** :
- Bit 0 (LSB) = Cellule 1
- Bit 1 = Cellule 2
- ...
- 1 = Équilibrage nécessaire/actif
- 0 = Pas d'équilibrage

---

### 6.2 Statistics Data (Registres 100-199)

| Registre | Nom | Type | Résolution | Accès |
|----------|-----|------|------------|-------|
| 100-101 | Total Distance | UINT32 | 0.01 km | R |
| 102 | Maximal Discharge Current | UINT16 | 100 mA | R |
| 103 | Maximal Charge Current | UINT16 | 100 mA | R |
| 104 | Maximal Cell Voltage Difference | UINT16 | 0.1 mV | R |
| 105 | Under-Voltage Protection Count | UINT16 | 1 count | R |
| 106 | Over-Voltage Protection Count | UINT16 | 1 count | R |
| 107 | Discharge Over-Current Protection Count | UINT16 | 1 count | R |
| 108 | Charge Over-Current Protection Count | UINT16 | 1 count | R |
| 109 | Over-Heat Protection Count | UINT16 | 1 count | R |
| 110 | **Reserved** | - | - | R |
| 111 | Charging Count | UINT16 | 1 count | R |
| 112 | Full Charge Count | UINT16 | 1 count | R |
| 113 | Min/Max Pack Temperature | INT8 + INT8 | 1 °C | R |
| 114 | Last Reset Event / Last Wakeup Event | UINT8 + UINT8 | - | R |
| 115 | **Reserved** | - | - | R |
| 116-117 | Statistics Last Cleared Timestamp | UINT32 | 1 s | R |
| 118-199 | **Reserved** | - | - | R |

#### Reg 113 - Min/Max Pack Temperature

Ce registre contient deux valeurs INT8 :
- **LSB** : Température minimale du pack
- **MSB** : Température maximale du pack

#### Reg 114 - Last Events

- **LSB** : Dernier événement de reset
  - `0x00` : Unknown
  - `0x01` : Low power reset
  - `0x02` : Window watchdog reset
  - `0x03` : Independent watchdog reset
  - `0x04` : Software reset
  - `0x05` : POR/PDR reset
  - `0x06` : PIN reset
  - `0x07` : Options bytes loading reset

- **MSB** : Dernier événement de réveil
  - `0x00` : Charger connected
  - `0x01` : Ignition
  - `0x02` : Discharging detected
  - `0x03` : UART communication detected

---

### 6.3 Events Data (Registres 200-299)

Les événements sont stockés de manière circulaire (49 événements max).

| Registre | Structure |
|----------|-----------|
| 200-201 | Event 0: Timestamp (UINT24 LSB) + Event ID (UINT8 MSB) |
| 202-203 | Event 1: Timestamp (UINT24 LSB) + Event ID (UINT8 MSB) |
| ... | ... |
| 296-297 | Event 48: Timestamp (UINT24 LSB) + Event ID (UINT8 MSB) |
| 298-299 | **Reserved** |

#### Structure d'un événement (2 registres)

**Registre N (LSB)** :
```
[Timestamp_Byte0] [Timestamp_Byte1]
```

**Registre N+1 (MSB)** :
```
[Timestamp_Byte2] [Event_ID]
```

Le timestamp est en secondes (UINT24), l'Event ID est défini dans la section 7.

---

### 6.4 Settings (Registres 300-399)

⚠️ **Plage d'adresses en écriture** : `0x012C` (300) à `0x018F` (399)

| Registre | Nom | Type | Plage/Valeurs | Accès |
|----------|-----|------|---------------|-------|
| 300 | Fully Charged Voltage | UINT16 | 1200-4500 mV | R/W |
| 301 | Fully Discharged Voltage | UINT16 | 1000-3500 mV | R/W |
| 302 | **Reserved** | - | - | R/W |
| 303 | Early Balancing Threshold | UINT16 | 1000-4500 mV | R/W |
| 304 | Charge Finished Current | UINT16 | 100-5000 mA* | R/W |
| 305 | Peak Discharge Current Cutoff | UINT16 | 1-750 A* | R/W |
| 306 | Battery Capacity | UINT16 | 10-65500 (×0.01 Ah) | R/W |
| 307 | Number Of Series Cells | UINT16 | 4-16 | R/W |
| 308 | Allowed Disbalance | UINT16 | 15-100 mV | R/W |
| 309 | **Reserved** | - | - | R/W |
| 310 | Charger Startup Delay | UINT16 | 5-60 s | R/W |
| 311 | Charger Disable Delay | UINT16 | 0-60 s | R/W |
| 312-313 | Pulses Per Unit | UINT32 | 1-100000 | R/W |
| 314 | Distance Unit Name | UINT16 | 0x01-0x05 | R/W |
| 315 | Over-Voltage Cutoff | UINT16 | 1200-4500 mV | R/W |
| 316 | Under-Voltage Cutoff | UINT16 | 800-3500 mV | R/W |
| 317 | Discharge Over-Current Cutoff | UINT16 | 1-750 A* | R/W |
| 318 | Charge Over-Current Cutoff | UINT16 | 1-750 A* | R/W |
| 319 | Over-Heat Cutoff | INT16 | +20 to +90 °C | R/W |
| 320 | Low Temperature Charger Cutoff | INT16 | -40 to +10 °C | R/W |
| 321 | Charge Restart Level | UINT16 | 60-95 % | R/W |
| 322 | Battery Maximum Cycles Count | UINT16 | 10-65000 | R/W |
| 323-324 | State Of Health | UINT16 | 0-50000 (×0.002%) | R/W |
| 325-327 | **Reserved** | - | - | R/W |
| 328-329 | State Of Charge | UINT16 | 0-50000 (×0.002%) | R/W |
| 329 | Configuration Flags | Bitmap | Voir détails | R/W |
| 330 | Charger Type + DOC Timeout | UINT8+UINT8 | Voir détails | R/W |
| 331 | Load Switch Type | UINT8 | 0x00-0x08 | R/W |
| 332 | Automatic Recovery | UINT8 | 1-30 s | R/W |
| 333 | Charger Switch Type | UINT8 | 0x01-0x09 | R/W |
| 334 | Ignition Input | UINT8 | 0x00-0x06 | R/W |
| 335 | Charger Detection | UINT8 | 0x01-0x07 | R/W |
| 336 | Speed Sensor Input | UINT8 | 0x00-0x02 | R/W |
| 337 | Precharge Pin | UINT8 | 0x00-0x10 | R/W |
| 338 | Precharge Duration | UINT8 | 0x00-0x07 | R/W |
| 339 | Temperature Sensor Type | UINT8 | 0x00-0x01 | R/W |
| 340 | BMS Operation Mode | UINT8 | 0x00-0x01 | R/W |
| 341 | Single Port Switch Type | UINT8 | 0x00-0x08 | R/W |
| 342 | Broadcast Time | UINT8 | 0x00-0x07 | R/W |
| 343 | Protocol | UINT8 | 0x00-0x02 | R/W |
| 344-399 | **Reserved** | - | - | R/W |

\* Les plages min/max peuvent être ajustées automatiquement par le BMS selon le capteur de courant utilisé.

#### Reg 314 - Distance Unit Name

| Valeur | Unité |
|--------|-------|
| 0x01 | Meter |
| 0x02 | Kilometer |
| 0x03 | Feet |
| 0x04 | Mile |
| 0x05 | Yard |

#### Reg 329 - Configuration Flags

| Bit | Nom | Valeurs |
|-----|-----|---------|
| 0 | Invert External Current Sensor Direction | 0 = Normal, 1 = Inversé |
| 1 | Disable Load/Charger Switch Diagnostics | 0 = Enabled, 1 = Disabled |
| 2 | Enable Charger Restart Level | 0 = Disabled, 1 = Enabled |
| 3-15 | Reserved | - |

#### Reg 330 - Charger Type + Discharge Over-Current Cutoff Timeout

- **LSB (8 bits)** : Charger Type
  - `0x00` : Variable (Reserved)
  - `0x01` : CC/CV
  - `0x02` : CAN (Reserved)

- **MSB (8 bits)** : Discharge Over-Current Cutoff Timeout (0-30 secondes)

#### Reg 331 - Load Switch Type

| Valeur | Type |
|--------|------|
| 0x00 | FET |
| 0x01 | AIDO1 |
| 0x02 | AIDO2 |
| 0x03 | DIDO1 |
| 0x04 | DIDO2 |
| 0x05 | AIHO1 Active Low |
| 0x06 | AIHO1 Active High |
| 0x07 | AIHO2 Active Low |
| 0x08 | AIHO2 Active High |

#### Reg 333 - Charger Switch Type

| Valeur | Type |
|--------|------|
| 0x01 | Charge FET |
| 0x02 | AIDO1 |
| 0x03 | AIDO2 |
| 0x04 | DIDO1 |
| 0x05 | DIDO2 |
| 0x06 | AIHO1 Active Low |
| 0x07 | AIHO1 Active High |
| 0x08 | AIHO2 Active Low |
| 0x09 | AIHO2 Active High |

#### Reg 334 - Ignition

| Valeur | Source |
|--------|--------|
| 0x00 | Disabled |
| 0x01 | AIDO1 |
| 0x02 | AIDO2 |
| 0x03 | DIDO1 |
| 0x04 | DIDO2 |
| 0x05 | AIHO1 |
| 0x06 | AIHO2 |

#### Reg 335 - Charger Detection

| Valeur | Source |
|--------|--------|
| 0x01 | Internal |
| 0x02 | AIDO1 |
| 0x03 | AIDO2 |
| 0x04 | DIDO1 |
| 0x05 | DIDO2 |
| 0x06 | AIHO1 |
| 0x07 | AIHO2 |

#### Reg 336 - Speed Sensor Input

| Valeur | Source |
|--------|--------|
| 0x00 | Disabled |
| 0x01 | DIDO1 |
| 0x02 | DIDO2 |

#### Reg 337 - Precharge Pin

| Valeur | Type |
|--------|------|
| 0x00 | Disabled |
| 0x02 | Discharge FET |
| 0x03 | AIDO1 |
| 0x04 | AIDO2 |
| 0x05 | DIDO1 |
| 0x06 | DIDO2 |
| 0x07 | AIHO1 Active Low |
| 0x08 | AIHO1 Active High |
| 0x09 | AIHO2 Active Low |
| 0x10 | AIHO2 Active High |

#### Reg 338 - Precharge Duration

| Valeur | Durée |
|--------|-------|
| 0x00 | 0.1 s |
| 0x01 | 0.2 s |
| 0x02 | 0.5 s |
| 0x03 | 1 s |
| 0x04 | 2 s |
| 0x05 | 3 s |
| 0x06 | 4 s |
| 0x07 | 5 s |

#### Reg 339 - Temperature Sensor Type

| Valeur | Type |
|--------|------|
| 0x00 | Dual 10K NTC |
| 0x01 | Multipoint Active Sensor |

#### Reg 340 - BMS Operation Mode

| Valeur | Mode |
|--------|------|
| 0x00 | Dual Port Operation |
| 0x01 | Single Port Operation |

#### Reg 342 - Broadcast Time

| Valeur | Intervalle |
|--------|------------|
| 0x00 | Disabled |
| 0x01 | 0.1 s |
| 0x02 | 0.2 s |
| 0x03 | 0.5 s |
| 0x04 | 1 s |
| 0x05 | 2 s |
| 0x06 | 5 s |
| 0x07 | 10 s |

#### Reg 343 - Protocol

| Valeur | Protocole |
|--------|-----------|
| 0x00 | CA V3 |
| 0x01 | ASCII |
| 0x02 | SOC BAR |

---

### 6.5 Version Data (Registres 500-599)

| Registre | Nom | Type | Description | Accès |
|----------|-----|------|-------------|-------|
| 500 | Hardware Version + HW Changes | UINT8 + UINT8 | Version matérielle | R |
| 501 | Firmware Public + Flags | UINT8 + Bits | Version firmware publique + BPT/BCS | R |
| 502 | Internal Firmware Version | UINT16 | Version interne firmware | R |
| 503 | Bootloader + Profile Version | UINT8 + UINT8 | Versions bootloader et profil | R |
| 504-509 | Product Serial Number | 96 bits | Numéro de série du produit | R |
| 510-599 | **Reserved** | - | - | R |

#### Reg 501 - Flags

- **Bit 0 (BPT)** : BMS Power Type
  - `0` : Low Power
  - `1` : High Power

- **Bits 1-2 (BCS)** : BMS Current Sensor
  - `00` : Internal Resistor
  - `01` : Internal HALL
  - `10` : External

---

## 7. Événements (Events)

### 7.1 Fault Messages (ID 0x01-0x30)

| ID | Message |
|----|---------|
| 0x02 | Under-Voltage Cutoff Occurred |
| 0x03 | Over-Voltage Cutoff Occurred |
| 0x04 | Over-Temperature Cutoff Occurred |
| 0x05 | Discharging Over-Current Cutoff Occurred |
| 0x06 | Charging Over-Current Cutoff Occurred |
| 0x07 | Regeneration Over-Current Cutoff Occurred |
| 0x0A | Low Temperature Cutoff Occurred |
| 0x0B | Charger Switch Error Detected |
| 0x0C | Load Switch Error Detected |
| 0x0D | Single Port Switch Error Detected |
| 0x0E | External Current Sensor Disconnected (BMS restart required) |
| 0x0F | External Current Sensor Connected (BMS restart required) |

### 7.2 Warning Messages (ID 0x31-0x60)

| ID | Message |
|----|---------|
| 0x31 | Fully Discharged Cutoff Occurred |
| 0x37 | Low Temperature Charging Cutoff Occurred |
| 0x38 | Charging Done (Charger voltage too high) |
| 0x39 | Charging Done (Charger voltage too low) |

### 7.3 Information Messages (ID 0x61-0x90)

| ID | Message |
|----|---------|
| 0x61 | System Started |
| 0x62 | Charging Started |
| 0x63 | Charging Done |
| 0x64 | Charger Connected |
| 0x65 | Charger Disconnected |
| 0x66 | Dual Port Operation Mode Activated |
| 0x67 | Single Port Operation Mode Activated |
| 0x73 | Recovered From Over-Temperature Fault Condition |
| 0x74 | Recovered From Low Temperature Warning Condition |
| 0x75 | Recovered From Low Temperature Fault Condition |
| 0x76 | Recovered From Charging Over-Current Fault Condition |
| 0x77 | Recovered From Discharging Over-Current Fault Condition |
| 0x78 | Recovered From Regeneration Over-Current Fault Condition |
| 0x79 | Recovered From Over-Voltage Fault Condition |
| 0x7A | Recovered From Fully Discharged Voltage Warning Condition |
| 0x7B | Recovered From Under-Voltage Fault Condition |
| 0x7C | External Current Sensor Connected |
| 0x7D | External Current Sensor Disconnected |

---

## 8. Exemples Pratiques

### Exemple 1 : Lecture des températures

**Objectif** : Lire la température interne du BMS et des deux sondes externes.

#### Requête
```
0xAA 0x1B 0x3F 0x1B
```

Calcul CRC :
```c
CRC16([0xAA, 0x1B], 2) = 0x1B3F
```

#### Réponse
```
0xAA 0x1B 0x06 0x16 0x01 0x14 0x01 0x16 0x01 0x0E 0x4E
```

Décodage :
- PL = 0x06 (6 octets de données)
- Temp interne = `0x0116` = 278 → **27.8°C**
- Temp externe #1 = `0x0114` = 276 → **27.6°C**
- Temp externe #2 = `0x0116` = 278 → **27.8°C**
- CRC = 0x4E0E

---

### Exemple 2 : Lecture de 5 cellules (MODBUS)

**Objectif** : Lire les tensions des cellules 5 à 9 avec la commande MODBUS 0x03.

#### Requête
```
0xAA 0x03 0x00 0x05 0x00 0x05 0x8C 0x13
```

Détails :
- Commande : 0x03 (MODBUS Read Holding Registers)
- Adresse : 0x0005 (MSB first)
- Nombre : 0x0005 (5 registres)
- CRC : 0x138C

#### Réponse
```
0xAA 0x03 0x0A 0x97 0x40 0x97 0x40 0x97 0x2C 0x97 0x2C 0x97 0x2C 0x3E 0xC7
```

Décodage :
- PL = 0x0A (10 octets)
- Cell 5 = 0x9740 (MSB first) = 38720 → **3.872V**
- Cell 6 = 0x9740 = **3.872V**
- Cell 7 = 0x972C = 38700 → **3.870V**
- Cell 8 = 0x972C = **3.870V**
- Cell 9 = 0x972C = **3.870V**
- CRC = 0xC73E

---

### Exemple 3 : Configuration Over/Under-Voltage (MODBUS)

**Objectif** : Configurer les seuils de coupure :
- Over-Voltage Cutoff (Reg 315) = **4.2V** (4200 mV)
- Under-Voltage Cutoff (Reg 316) = **2.5V** (2500 mV)

#### Requête
```
0xAA 0x10 0x01 0x3B 0x00 0x02 0x04 0x10 0x68 0x09 0xC4 0x19 0x61
```

Détails :
- Commande : 0x10 (MODBUS Write Multiple Registers)
- Adresse : 0x013B = 315 (MSB first)
- Nombre : 0x0002 (2 registres)
- PL : 0x04 (4 octets)
- DATA1 : 0x1068 = 4200 mV (MSB first)
- DATA2 : 0x09C4 = 2500 mV (MSB first)
- CRC : 0x6119

#### Réponse
```
0xAA 0x10 0x01 0x3B 0x00 0x02 0x28 0x22
```

Décodage :
- Confirmation d'écriture de 2 registres à partir de l'adresse 0x013B
- CRC : 0x2228

---

### Exemple 4 : Lecture du statut en ligne

**Objectif** : Vérifier l'état actuel du BMS (charge, décharge, idle, etc.).

#### Requête
```
0xAA 0x18 [CRC_LSB] [CRC_MSB]
```

#### Réponses possibles

**Cas 1 : En charge**
```
0xAA 0x18 0x91 0x00 [CRC_LSB] [CRC_MSB]
```
→ Statut = 0x0091 = **Charging**

**Cas 2 : Décharge**
```
0xAA 0x18 0x93 0x00 [CRC_LSB] [CRC_MSB]
```
→ Statut = 0x0093 = **Discharging**

**Cas 3 : Défaut**
```
0xAA 0x18 0x9B 0x00 [CRC_LSB] [CRC_MSB]
```
→ Statut = 0x009B = **Fault**

---

### Exemple 5 : Lecture d'événements

**Objectif** : Récupérer les 3 derniers événements enregistrés.

#### Requête
```
0xAA 0x11 [CRC_LSB] [CRC_MSB]
```

#### Réponse Message 1 (Timestamp du BMS)
```
0xAA 0x11 0x04 0x00 0x2A 0x3B 0x00 ... [CRC]
```
- BTSP = 0x003B2A00 = 3877376 secondes

#### Réponse Message 2 (Event 1)
```
0xAA 0x11 0x04 0x10 0x28 0x3B 0x62 ... [CRC]
```
- TSP = 0x3B2810 = 3876880 secondes
- Event ID = 0x62 = **Charging Started**

#### Réponse Message 3 (Event 2)
```
0xAA 0x11 0x04 0x50 0x29 0x3B 0x63 ... [CRC]
```
- TSP = 0x3B2950 = 3877200 secondes
- Event ID = 0x63 = **Charging Done**

---

### Exemple 6 : Lecture du SOC

**Objectif** : Lire l'état de charge (State Of Charge).

#### Requête
```
0xAA 0x1A [CRC_LSB] [CRC_MSB]
```

#### Réponse
```
0xAA 0x1A 0x00 0x24 0xF4 0x01 [CRC_LSB] [CRC_MSB]
```

Décodage :
- SOC = 0x01F42400 = 32800000
- Résolution = 0.000001%
- **SOC = 32.8%**

---

### Exemple 7 : Effacer les statistiques

**Objectif** : Réinitialiser les compteurs de statistiques.

#### Requête
```
0xAA 0x02 0x02 [CRC_LSB] [CRC_MSB]
```

Détails :
- Commande : 0x02 (Reset/Clear)
- Option : 0x02 (Clear Statistics)

#### Réponse (OK)
```
0xAA 0x01 0x02 [CRC_LSB] [CRC_MSB]
```

---

### Exemple 8 : Lecture de la version complète

**Objectif** : Obtenir toutes les informations de version (matériel, firmware, bootloader).

#### Requête
```
0xAA 0x1F [CRC_LSB] [CRC_MSB]
```

#### Réponse
```
0xAA 0x1F 0x08 0x01 0x00 0x14 0x3A 0x00 0x02 0x05 [CRC_LSB] [CRC_MSB]
```

Décodage :
- PL = 8 octets
- DATA1 = 0x01 → **HW Version = 1**
- DATA2 = 0x00 → **HW Changes = 0**
- DATA3 = 0x14 → **FW Public = 20**
- DATA4 = 0x003A → **FW Internal = 58**
- DATA5 = 0x02 → **Bootloader = 2**
- DATA6 = 0x05 → **Register Map = 5**

---

## 9. Résumé des Différences MODBUS vs Propriétaire

| Aspect | Commandes Propriétaires | Commandes MODBUS |
|--------|------------------------|------------------|
| **Ordre des octets** | LSB First (little-endian) | MSB First (big-endian) |
| **Adresses** | LSB first dans la trame | MSB first dans la trame |
| **Données** | LSB first | MSB first |
| **Commandes lecture** | 0x07, 0x09 | 0x03 |
| **Commandes écriture** | 0x0B, 0x0D | 0x10 |
| **Flexibilité** | Lecture bloc + individuel | Lecture bloc uniquement |
| **Limite lecture** | Variable selon commande | 127 registres max |
| **Limite écriture** | Variable selon commande | 100 registres max |

---

## 10. Codes d'Erreur

### Erreurs de communication

| Code | Signification |
|------|---------------|
| 0x00 | CMD ERROR - Commande invalide |
| 0x01 | CRC ERROR - Erreur de checksum |

### Autres erreurs possibles

- Pas de réponse : BMS en veille (envoyer la commande 2 fois)
- Timeout : Vérifier la configuration UART et les connexions
- Données incohérentes : Vérifier le calcul du CRC

---

## 11. Bonnes Pratiques

### Communication

1. **Toujours vérifier le CRC** avant de traiter une réponse
2. **Gérer le mode veille** : envoyer la première commande deux fois si nécessaire
3. **Respecter les timeouts** : attendre la réponse complète avant d'envoyer une nouvelle commande
4. **Valider les plages** : vérifier que les valeurs lues sont dans les plages attendues

### Écriture de configuration

1. **Lire avant d'écrire** : toujours vérifier les valeurs actuelles
2. **Écrire par blocs cohérents** : grouper les paramètres liés
3. **Vérifier après écriture** : relire pour confirmer
4. **Respecter les plages** : ne pas dépasser les min/max

### Performance

1. **Utiliser les commandes optimales** :
   - Bloc continu → 0x07 ou 0x03
   - Registres épars → 0x09

2. **Minimiser les transactions** :
   - Lire plusieurs registres en une fois plutôt que un par un

3. **Choisir le bon protocole** :
   - MODBUS si intégration industrielle standard
   - Propriétaire pour plus de flexibilité

---

## 12. Tableaux de Référence Rapide

### Commandes les plus utilisées

| Besoin | Commande | Code |
|--------|----------|------|
| Lire toutes les cellules | Read Cell Voltages | 0x1C |
| Lire tension pack | Read Voltage | 0x14 |
| Lire courant pack | Read Current | 0x15 |
| Lire SOC | Read SOC | 0x1A |
| Lire températures | Read Temperatures | 0x1B |
| Lire statut | Read Status | 0x18 |
| Lire événements récents | Read Newest Events | 0x11 |
| Configurer seuils | Write Block (MODBUS) | 0x10 |

### Registres les plus utilisés

| Donnée | Adresse | Type |
|--------|---------|------|
| Cellule 1 | 0 | UINT16 (0.1 mV) |
| Tension pack | 36-37 | FLOAT (V) |
| Courant pack | 38-39 | FLOAT (A) |
| Cellule min | 40 | UINT16 (mV) |
| Cellule max | 41 | UINT16 (mV) |
| SOC | 46-47 | UINT32 (0.000001%) |
| Temp interne | 48 | INT16 (0.1°C) |
| Statut | 50 | UINT16 |
| Over-Voltage Cutoff | 315 | UINT16 (mV) |
| Under-Voltage Cutoff | 316 | UINT16 (mV) |

---

## Annexe A : Glossaire

| Terme | Définition |
|-------|------------|
| **BMS** | Battery Management System - Système de gestion de batterie |
| **SOC** | State Of Charge - État de charge (%) |
| **SOH** | State Of Health - État de santé de la batterie |
| **CRC** | Cyclic Redundancy Check - Contrôle de redondance cyclique |
| **LSB** | Least Significant Byte - Octet de poids faible |
| **MSB** | Most Significant Byte - Octet de poids fort |
| **UART** | Universal Asynchronous Receiver-Transmitter |
| **MODBUS** | Protocole de communication industriel standard |
| **OV** | Over-Voltage - Surtension |
| **UV** | Under-Voltage - Sous-tension |
| **OC** | Over-Current - Surintensité |
| **OT** | Over-Temperature - Surchauffe |

---

## Annexe B : Checklist de Démarrage

### Configuration initiale

- [ ] Vérifier la configuration UART (115200 8N1)
- [ ] Implémenter la fonction de calcul CRC
- [ ] Tester la lecture de version (0x1E)
- [ ] Tester la lecture du statut (0x18)
- [ ] Tester la lecture des cellules (0x1C)

### Monitoring de base

- [ ] Lire toutes les tensions de cellules
- [ ] Lire la tension et le courant du pack
- [ ] Lire les températures
- [ ] Lire le SOC
- [ ] Lire le statut en ligne

### Configuration avancée

- [ ] Lire les paramètres actuels (0x1D option 0x04)
- [ ] Modifier les seuils de protection si nécessaire
- [ ] Vérifier l'écriture en relisant
- [ ] Tester les événements (0x11)

---

**Fin du document de référence**

*Ce document couvre l'intégralité du protocole UART du TinyBMS basé sur la spécification Enepaq Rev D (2025-07-04). Pour toute question ou clarification, référez-vous au document source officiel.*
