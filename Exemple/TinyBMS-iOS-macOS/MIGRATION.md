# Migration Web → iOS/macOS

Ce document décrit comment la version web Node.js a été transformée en application native iOS/macOS.

## Vue d'Ensemble

| Aspect | Version Web | Version iOS/macOS |
|--------|-------------|-------------------|
| **Langage** | JavaScript (Node.js) | Swift |
| **Framework UI** | HTML/CSS/JavaScript | SwiftUI |
| **Communication** | Socket.IO + SerialPort | ORSSerialPort (macOS) / Simulation (iOS) |
| **Architecture** | Express Server + Client | Application Native |
| **Plateformes** | Navigateur Web | macOS 13+, iOS 16+ |

## Correspondance des Fichiers

### Backend → Services

| Web | iOS/macOS | Description |
|-----|-----------|-------------|
| `tinybms.js` | `ModbusProtocol.swift` + `TinyBMSService.swift` | Protocole Modbus et communication |
| `server.js` | `TinyBMSService.swift` | Gestion connexion et polling |
| `simulator.js` | `BMSSimulator.swift` | Simulateur de données |
| `serialport` (npm) | `ORSSerialPort` (SPM) | Communication série |

### Frontend → Views

| Web | iOS/macOS | Description |
|-----|-----------|-------------|
| `public/index.html` | `ContentView.swift` | Structure principale |
| Dashboard (HTML) | `DashboardView.swift` | Vue dashboard |
| Cells Section | `CellsView.swift` | Vue des cellules |
| Settings Section | `SettingsView.swift` | Configuration |
| Stats Section | `StatsView.swift` | Statistiques |
| `public/styles.css` | SwiftUI Styles | Style natif SwiftUI |
| `public/app.js` | `BMSData.swift` + Views | Logique et données |

### Modèles de Données

| Web | iOS/macOS | Description |
|-----|-----------|-------------|
| `REGISTER_MAP` (JS array) | `RegisterMap.swift` | Définition des registres |
| Objects JavaScript | `BMSRegister.swift` | Modèles typés |
| Socket events | `@Published` properties | Réactivité |

## Architecture Technique

### Gestion d'État

**Web (Socket.IO + Events)**
```javascript
io.emit('bms-live', liveData);
socket.on('bms-live', (data) => {
  updateUI(data);
});
```

**iOS/macOS (Combine + @Published)**
```swift
@Published var liveData = BMSLiveData()

// Automatiquement met à jour l'UI
dataStore.liveData.packVoltage = newValue
```

### Communication Série

**Web (serialport)**
```javascript
const port = new SerialPort({ path, baudRate: 115200 });
port.on('data', (data) => { ... });
```

**macOS (ORSSerialPort)**
```swift
let port = ORSSerialPort(path: path)
port.baudRate = 115200
port.delegate = self
func serialPort(_ port: ORSSerialPort, didReceive data: Data) { ... }
```

**iOS (Simulation)**
```swift
// iOS n'a pas de port série USB natif
class BMSSimulator: TinyBMSServiceProtocol {
  func readRegisters(...) async throws -> [Int: BMSRegisterValue] {
    // Retourne des données simulées
  }
}
```

### Protocole Modbus

**Identique dans les deux versions**
- CRC16 (polynôme 0xA001)
- Big Endian pour les adresses
- Fonction 0x03 (Read) et 0x10 (Write)

**Web**
```javascript
calculateCRC(buffer) {
  let crc = 0xFFFF;
  // ... algorithme
  return crc;
}
```

**Swift**
```swift
static func calculateCRC(_ buffer: Data) -> UInt16 {
  var crc: UInt16 = 0xFFFF
  // ... même algorithme
  return crc
}
```

## Différences Clés

### 1. Gestion Asynchrone

**Web (Callbacks/Promises)**
```javascript
port.on('data', onData);
setTimeout(() => reject("Timeout"), 800);
```

**Swift (async/await)**
```swift
let response = try await withCheckedThrowingContinuation { continuation in
  // Setup
}
```

### 2. Types

**Web (Dynamic)**
```javascript
let value = buffer.readUInt16BE(offset);
// Type inféré à runtime
```

**Swift (Static)**
```swift
let value = buffer.withUnsafeBytes {
  $0.load(fromByteOffset: offset, as: UInt16.self).bigEndian
}
// Type vérifié à la compilation
```

### 3. UI Update

**Web (Imperative)**
```javascript
document.getElementById('soc').textContent = soc + '%';
```

**Swift (Declarative)**
```swift
Text("\(String(format: "%.1f%%", dataStore.liveData.stateOfCharge))")
// Met à jour automatiquement quand dataStore change
```

### 4. Platformes

**Web**
- ✅ Fonctionne dans tout navigateur
- ✅ Cross-platform (Windows, Mac, Linux)
- ❌ Nécessite serveur Node.js
- ❌ Pas d'app standalone

**iOS/macOS**
- ✅ Application native standalone
- ✅ Performance optimale
- ✅ Look & feel natif
- ❌ macOS uniquement pour port série
- ❌ iOS en simulation seulement

## Nouvelles Fonctionnalités

### 1. Mode Simulation Intégré
```swift
class BMSSimulator: TinyBMSServiceProtocol {
  private func tick() {
    // Simule variations réalistes
    let currentPhase = sin(simulationTime * 0.1) * 15.0
    simulatedData[38] = currentPhase
  }
}
```

### 2. SwiftUI Moderne
- Responsive automatique
- Dark/Light mode natif
- Animations fluides
- Accessibilité intégrée

### 3. Type Safety
```swift
enum RegisterType {
  case uint16, int16, uint32, float32
}
// Impossible de confondre les types
```

## Fonctionnalités Conservées

✅ Toutes les fonctionnalités principales :
- Lecture de tous les registres (Live, Stats, Settings)
- Écriture des paramètres
- Polling automatique
- Visualisation des cellules
- Graphiques et jauges
- Mode simulation

## Ce qui Manque (vs Web)

❌ **Support iOS pour port série USB**
- iOS ne supporte pas les ports série USB nativement
- Solution : Simulation ou pont Bluetooth

❌ **Accès web browser**
- Application native seulement
- Solution : Garder aussi la version web

❌ **Multi-utilisateurs simultanés**
- Web permettait plusieurs navigateurs connectés
- Native est mono-utilisateur

## Roadmap Futures Améliorations

### Court Terme
1. ✅ Application de base fonctionnelle
2. ✅ Mode simulation
3. ✅ Interface complète
4. 🔲 Tests unitaires
5. 🔲 Tests d'intégration

### Moyen Terme
1. 🔲 Support Bluetooth pour iOS (via ESP32 BLE)
2. 🔲 Graphiques historiques
3. 🔲 Export de données (CSV, JSON)
4. 🔲 Notifications push
5. 🔲 Widget iOS/macOS

### Long Terme
1. 🔲 watchOS companion app
2. 🔲 Profils de configuration
3. 🔲 Mode multi-BMS
4. 🔲 Cloud sync (optionnel)

## Guide de Portage

Si vous voulez porter d'autres fonctionnalités de la version web :

### 1. Identifier la Fonctionnalité
- Backend (server.js) → Service Swift
- Frontend (HTML/JS) → SwiftUI View
- Données (objects JS) → Swift struct/class

### 2. Porter la Logique
```javascript
// Web
function calculatePower(voltage, current) {
  return voltage * current;
}
```

```swift
// Swift
func calculatePower(voltage: Double, current: Double) -> Double {
  return voltage * current
}
```

### 3. Créer l'UI
```javascript
// Web
<div class="power">
  <span id="power-value">0</span> W
</div>
```

```swift
// Swift
HStack {
  Text(String(format: "%.1f W", power))
}
```

### 4. Connecter avec @Published
```swift
@Published var power: Double = 0.0

// Dans la vue
Text(String(format: "%.1f W", dataStore.power))
```

## Conclusion

La migration vers iOS/macOS apporte :

**Avantages**
- ✅ Performance native
- ✅ Intégration système
- ✅ Pas de serveur nécessaire
- ✅ Type safety
- ✅ Modern Swift/SwiftUI

**Compromis**
- ⚠️ iOS limité (simulation)
- ⚠️ Développement Apple uniquement
- ⚠️ Pas d'accès web

**Recommandation** : Garder les deux versions !
- Web pour accès universel et multi-utilisateurs
- Native pour meilleure expérience utilisateur sur Mac

---

**Version iOS/macOS** : 1.0.0
**Basé sur Version Web** : TinyBMS-web originale
**Date** : 2025-11-25
