# Analyse et Améliorations - TinyBMS macOS App

## Résumé Exécutif

Cette analyse compare l'implémentation actuelle de l'application TinyBMS macOS avec la documentation officielle **Enepaq Communication Protocols Rev D (2025-07-04)**. L'application présente une bonne base architecturale mais nécessite plusieurs corrections et améliorations pour être pleinement conforme au protocole.

---

## Table des Matières

1. [Problèmes Critiques](#1-problèmes-critiques)
2. [Problèmes Majeurs](#2-problèmes-majeurs)
3. [Améliorations Recommandées](#3-améliorations-recommandées)
4. [Registres Manquants](#4-registres-manquants)
5. [Fonctionnalités Non Implémentées](#5-fonctionnalités-non-implémentées)
6. [Plan d'Action Priorisé](#6-plan-daction-priorisé)

---

## 1. Problèmes Critiques

### 1.1 Parsing Float32 Incorrect

**Fichier:** `ModbusProtocol.swift`, lignes 103-109

**Problème:** Le parsing des valeurs Float32 est incorrectement implémenté.

```swift
// CODE ACTUEL (INCORRECT)
case .float32:
    if byteOffset + 3 < buffer.count {
        let bytes = buffer.subdata(in: byteOffset..<(byteOffset + 4))
        rawValue = Double(bytes.withUnsafeBytes { $0.load(fromByteOffset: 0, as: Float32.self).bitPattern }.bigEndian.bitPattern)
    }
```

**Correction:**
```swift
case .float32:
    if byteOffset + 3 < buffer.count {
        // Les données MODBUS sont en Big Endian
        let byte0 = buffer[byteOffset]     // MSB
        let byte1 = buffer[byteOffset + 1]
        let byte2 = buffer[byteOffset + 2]
        let byte3 = buffer[byteOffset + 3] // LSB
        
        let bits = UInt32(byte0) << 24 | UInt32(byte1) << 16 | UInt32(byte2) << 8 | UInt32(byte3)
        rawValue = Double(Float(bitPattern: bits))
    }
```

### 1.2 Valeurs de BMS Status Incorrectes

**Fichier:** `BMSData.swift`, lignes 29-38

**Problème:** Les valeurs de statut ne correspondent pas au protocole.

| Code Actuel | Code Protocole | Description |
|-------------|----------------|-------------|
| 0 = Idle    | 0x91 = Charging | Différent |
| 1 = Charging | 0x92 = Fully Charged | Différent |
| 2 = Discharging | 0x93 = Discharging | OK |
| 3 = Regeneration | 0x96 = Regeneration | Différent |
| 4 = Fault | 0x97 = Idle | Différent |
| - | 0x9B = Fault | Manquant |

**Correction:**
```swift
var bmsStatusString: String {
    switch bmsStatus {
    case 0x91: return "Charging"
    case 0x92: return "Fully Charged"
    case 0x93: return "Discharging"
    case 0x96: return "Regeneration"
    case 0x97: return "Idle"
    case 0x9B: return "Fault"
    default: return "Unknown (0x\(String(format: "%02X", bmsStatus)))"
    }
}
```

### 1.3 Table CRC Manquante

**Fichier:** `ModbusProtocol.swift`, lignes 7-23

**Problème:** Le calcul CRC utilise une boucle au lieu de la table de lookup fournie dans la documentation (page 11-12), ce qui est plus lent et moins fiable.

**Amélioration:** Utiliser la table CRC complète du protocole pour optimiser les performances.

---

## 2. Problèmes Majeurs

### 2.1 Gestion du Mode Veille Non Implémentée

**Documentation (Page 12):**
> "If Tiny BMS device is in sleep mode, the first command must be send twice."

**Impact:** L'application ne gère pas le réveil du BMS depuis le mode veille.

**Solution:** Implémenter un mécanisme de double envoi avec détection de timeout.

### 2.2 Réponses d'Erreur Non Gérées

**Documentation (Page 4):**
```
Response from BMS [NACK]
0xAA 0x00 CMD ERROR CRC:LSB CRC:MSB

ERROR codes:
0x00 – CMD ERROR
0x01 – CRC ERROR
```

**Code actuel:** Aucune gestion des réponses NACK.

**Impact:** Erreurs silencieuses, comportement imprévisible.

### 2.3 Registre Total Distance Mal Configuré

**Fichier:** `RegisterMap.swift`, ligne 39

**Problème:** 
```swift
// ACTUEL (INCORRECT)
BMSRegister(id: 101, label: "Total Distance", ...)

// PROTOCOLE (Page 23)
// Reg 100-101: Total Distance [UINT_32] / Resolution 0.01 km
```

Le registre 100 est le LSB du UINT32, pas 101.

### 2.4 Registre 32-33 (Lifetime Counter) Manquant

Ce registre UINT32 important n'est pas implémenté dans le RegisterMap.

### 2.5 Parsing UInt32 Incorrect pour Registres Multi-Word

Le code actuel ne gère pas correctement le fait que les registres UINT32/Float32 occupent 2 registres consécutifs dans la réponse.

---

## 3. Améliorations Recommandées

### 3.1 Ajouter Support des Commandes Propriétaires

Le protocole propose des commandes dédiées plus efficaces que MODBUS générique:

| Commande | Code | Description | Avantage |
|----------|------|-------------|----------|
| Read Pack Voltage | 0x14 | Lecture voltage pack | 1 requête vs bloc |
| Read Pack Current | 0x15 | Lecture courant pack | Réponse Float directe |
| Read Temperatures | 0x1B | 3 températures | 1 seule requête |
| Read Cell Voltages | 0x1C | Toutes les cellules | Optimisé |
| Read SOC | 0x1A | State of Charge | Réponse UINT32 directe |
| Read Status | 0x18 | Statut BMS | Simple et rapide |

### 3.2 Implémenter la Lecture des Events

**Documentation (Pages 7, 24, 26):**

Les events sont stockés aux registres 200-297 et peuvent être lus via:
- Commande 0x11: Lire les events récents
- Commande 0x12: Lire tous les events

**Structure d'un event:**
```
Timestamp [UINT_24] + Event ID [UINT_8]
```

### 3.3 Implémenter Reset et Clear

**Commande 0x02:**
- 0x01: Clear Events
- 0x02: Clear Statistics  
- 0x05: Reset BMS

### 3.4 Ajouter Lecture Version Étendue

**Commande 0x1F:** Retourne informations détaillées:
- Hardware version
- Hardware changes version
- Firmware public version
- Firmware internal version
- Bootloader version
- Register map version

### 3.5 Gérer les Capteurs de Température Déconnectés

**Documentation (Page 9):**
> "value of -32768 if not connected"

Le code doit afficher "N/C" ou "Non connecté" quand la valeur est -32768.

---

## 4. Registres Manquants

### 4.1 Live Data (0-99)

| Reg | Label | Type | Scale | Implémenté |
|-----|-------|------|-------|------------|
| 32-33 | BMS Lifetime Counter | UINT32 | 1s | ❌ |
| 34-35 | Estimated Time Left | UINT32 | 1s | ❌ |
| 44 | Distance Left to Empty | UINT16 | 1km | ❌ |
| 51 | Balancing Decision Bits | UINT16 | - | ❌ |
| 53 | Number of Detected Cells | UINT16 | - | ❌ |
| 54-55 | Speed | FLOAT | km/h | ❌ |

### 4.2 Statistics (100-199)

| Reg | Label | Type | Scale | Implémenté |
|-----|-------|------|-------|------------|
| 100-101 | Total Distance | UINT32 | 0.01km | ❌ (mal configuré) |
| 102 | Max Discharge Current | UINT16 | 100mA | ❌ |
| 103 | Max Charge Current | UINT16 | 100mA | ❌ |
| 104 | Max Cell Voltage Diff | UINT16 | 0.1mV | ❌ |
| 107 | Discharge OC Count | UINT16 | 1 | ❌ |
| 108 | Charge OC Count | UINT16 | 1 | ❌ |
| 109 | Over-Heat Count | UINT16 | 1 | ❌ |
| 113 | Min/Max Pack Temp | INT8+INT8 | 1°C | ❌ |
| 114 | Last Reset/Wakeup | UINT8+UINT8 | - | ❌ |
| 116-117 | Stats Last Cleared | UINT32 | 1s | ❌ |

### 4.3 Settings Manquants

| Reg | Label | Type | Implémenté |
|-----|-------|------|------------|
| 302 | Reserved | - | - |
| 314 | Distance Unit Name | UINT16 | ❌ |
| 323 | State Of Health (write) | UINT16 | ❌ |
| 329 | Config Bits | UINT16 | ❌ |
| 331 | Load Switch Type | UINT8 | ❌ |
| 333 | Charger Switch Type | UINT8 | ❌ |
| 334 | Ignition | UINT8 | ❌ |
| 335 | Charger Detection | UINT8 | ❌ |
| 336 | Speed Sensor Input | UINT8 | ❌ |
| 337 | Precharge Pin | UINT8 | ❌ |
| 338 | Precharge Duration | UINT8 | ❌ |
| 339 | Temperature Sensor Type | UINT8 | ❌ |
| 341 | Single Port Switch Type | UINT8 | ❌ |
| 342 | Broadcast Time | UINT8 | ❌ |

### 4.4 Version (500-509)

| Reg | Label | Implémenté |
|-----|-------|------------|
| 500 | HW Version + Changes | ❌ |
| 501 | FW Public + BPT/BCS | ✅ (partiel) |
| 502 | FW Internal Version | ❌ |
| 503 | Bootloader + Profile | ❌ |
| 504-509 | Serial Number (96 bits) | ❌ |

---

## 5. Fonctionnalités Non Implémentées

### 5.1 Protocole CAN Bus

Le chapitre 2 du protocole décrit entièrement la communication CAN bus:
- Bitrate: 500 kbit/s
- Node ID: 0x01 à 0x3F
- Commandes similaires à UART

### 5.2 Events System

Le BMS maintient un journal de 49 events (registres 200-297) avec:
- Timestamp de l'event
- ID du message (Fault/Warning/Info)

**Types d'events:**
- **Faults (0x01-0x30):** Under-Voltage, Over-Voltage, Over-Temperature, etc.
- **Warnings (0x31-0x60):** Fully Discharged, Low Temp Charging, etc.
- **Info (0x61-0x90):** System Started, Charging Started/Done, etc.

### 5.3 Commandes de Réinitialisation

- Clear Events
- Clear Statistics
- Reset BMS

### 5.4 Vérification ACK/NACK

Toute commande d'écriture doit vérifier la réponse ACK avant de continuer.

---

## 6. Plan d'Action Priorisé

### Phase 1: Corrections Critiques (Priorité Haute)

1. **Corriger le parsing Float32** (1h)
   - Impact: Données de tension/courant incorrectes

2. **Corriger les codes de statut BMS** (30min)
   - Impact: Affichage statut incorrect

3. **Implémenter la table CRC** (1h)
   - Impact: Performance et fiabilité

4. **Corriger le registre Total Distance** (30min)
   - Impact: Statistiques incorrectes

### Phase 2: Fonctionnalités Essentielles (Priorité Moyenne)

5. **Gestion NACK/ACK** (2h)
   - Ajouter parsing des réponses d'erreur
   - Implémenter retry logic

6. **Gestion du mode veille** (2h)
   - Double envoi de commande
   - Détection timeout

7. **Ajouter registres manquants Live Data** (3h)
   - Lifetime counter, Detected cells, Speed, etc.

8. **Gérer capteurs température déconnectés** (30min)
   - Afficher "N/C" si valeur = -32768

### Phase 3: Améliorations (Priorité Normale)

9. **Implémenter commandes propriétaires** (4h)
   - 0x14-0x20 pour lectures optimisées

10. **Ajouter système Events** (6h)
    - Commandes 0x11, 0x12
    - UI pour afficher l'historique
    - Décodage des messages

11. **Compléter les registres Settings** (3h)
    - Tous les registres 300-343

12. **Ajouter informations Version** (2h)
    - Commande 0x1F
    - Affichage numéro de série

### Phase 4: Fonctionnalités Avancées (Optionnel)

13. **Support CAN bus** (8h)
    - Nouveau service CAN
    - Intégration avec adaptateurs CAN-USB

14. **Commandes Reset/Clear** (2h)
    - UI pour maintenance
    - Confirmation utilisateur

---

## Annexe: Code Corrigé

### A.1 ModbusProtocol.swift Amélioré

```swift
import Foundation

class ModbusProtocol {
    
    // Table CRC du protocole Enepaq (page 11-12)
    private static let crcTable: [UInt16] = [
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        // ... (reste de la table)
    ]
    
    static func calculateCRC(_ buffer: Data) -> UInt16 {
        var crc: UInt16 = 0xFFFF
        for byte in buffer {
            let index = Int((UInt8(crc & 0xFF)) ^ byte)
            crc = (crc >> 8) ^ crcTable[index]
        }
        return crc
    }
    
    // Commandes propriétaires pour lecture optimisée
    static func createReadPackVoltageCommand() -> Data {
        var command = Data([0xAA, 0x14])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    static func createReadTemperaturesCommand() -> Data {
        var command = Data([0xAA, 0x1B])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    static func createReadCellVoltagesCommand() -> Data {
        var command = Data([0xAA, 0x1C])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    // Parsing amélioré avec gestion des erreurs
    static func parseResponse(_ data: Data) -> Result<[Int: BMSRegisterValue], BMSError> {
        guard data.count >= 3 else {
            return .failure(.invalidResponse)
        }
        
        // Vérifier NACK
        if data[0] == 0xAA && data[1] == 0x00 {
            let errorCode = data.count > 3 ? data[3] : 0
            return .failure(.nackReceived(cmd: data[2], error: errorCode))
        }
        
        // Vérifier CRC
        let payloadLength = data.count - 2
        let expectedCRC = calculateCRC(data.prefix(payloadLength))
        let receivedCRC = UInt16(data[payloadLength]) | (UInt16(data[payloadLength + 1]) << 8)
        
        guard expectedCRC == receivedCRC else {
            return .failure(.crcMismatch)
        }
        
        // Parser selon le type de commande
        // ...
        return .success([:])
    }
}

enum BMSError: Error {
    case invalidResponse
    case nackReceived(cmd: UInt8, error: UInt8)
    case crcMismatch
    case timeout
    case notConnected
}
```

### A.2 RegisterMap Complet

Voir le fichier `RegisterMap_Complete.swift` pour l'implémentation complète avec tous les registres.

---

## Conclusion

L'application TinyBMS macOS a une architecture solide mais nécessite des corrections importantes pour être conforme au protocole Enepaq. Les problèmes critiques (parsing Float32, codes de statut) doivent être corrigés en priorité car ils affectent la fiabilité des données affichées.

L'ajout des commandes propriétaires et du système d'events améliorerait significativement l'expérience utilisateur et les capacités de diagnostic.

**Estimation totale:** ~35-40 heures de développement pour une implémentation complète.
