# Guide de Démarrage Rapide

## Option 1 : Utiliser comme Swift Package (Recommandé)

### Pour développement/tests

1. Ouvrir le package dans Xcode :
```bash
cd Exemple/TinyBMS-iOS-macOS
open Package.swift
```

2. Xcode va automatiquement :
   - Résoudre les dépendances (ORSSerialPort)
   - Configurer le projet
   - Permettre de build et run

3. Dans Xcode :
   - Sélectionner la cible (macOS ou iOS Simulator)
   - Appuyer sur `Cmd+R` pour build et run

**Note** : Cette méthode est idéale pour développer le package lui-même, mais ne crée pas d'application standalone.

## Option 2 : Créer une Application Xcode (Pour Distribution)

### macOS App

1. Créer un nouveau projet :
   - Ouvrir Xcode
   - File > New > Project
   - Choisir "macOS" > "App"
   - Nom : "TinyBMS Monitor"
   - Interface : SwiftUI
   - Language : Swift

2. Ajouter le package :
   - File > Add Package Dependencies
   - Cliquer sur "Add Local..."
   - Sélectionner le dossier `TinyBMS-iOS-macOS`
   - Ajouter la library "TinyBMS" à la target

3. Remplacer le contenu de l'App :
   - Dans `TinyBMSMonitorApp.swift` :
   ```swift
   import SwiftUI
   import TinyBMS

   @main
   struct TinyBMSMonitorApp: App {
       @StateObject private var dataStore = BMSDataStore()
       @StateObject private var bmsService: TinyBMSService

       init() {
           let dataStore = BMSDataStore()
           _dataStore = StateObject(wrappedValue: dataStore)
           _bmsService = StateObject(wrappedValue: TinyBMSService(dataStore: dataStore))
       }

       var body: some Scene {
           WindowGroup {
               ContentView()
                   .environmentObject(dataStore)
                   .environmentObject(bmsService)
           }
       }
   }
   ```

4. Build et run (`Cmd+R`)

### iOS App

Même processus, mais :
- Choisir "iOS" > "App" lors de la création
- L'app fonctionnera en mode simulation uniquement (pas de port série USB sur iOS)

## Option 3 : Copier les Fichiers Directement

Si vous préférez ne pas utiliser Swift Package Manager :

1. Créer un nouveau projet Xcode (iOS ou macOS App)

2. Copier tous les fichiers :
   - Créer les groupes : Models, Services, Views, App
   - Glisser-déposer les fichiers .swift correspondants

3. Ajouter ORSSerialPort manuellement :
   - Pour macOS seulement
   - File > Add Package Dependencies
   - URL : `https://github.com/armadsen/ORSSerialPort.git`

4. Conditionner l'import pour macOS :
   - Dans les fichiers utilisant ORSSerialPort, garder `#if os(macOS)`

## Test en Mode Simulation

Pour tester sans matériel :

1. Lancer l'app
2. Cliquer sur "Select Port"
3. Choisir "Simulation"
4. Cliquer sur "Connect"
5. Observer les données simulées qui varient dynamiquement

## Connexion Réelle (macOS uniquement)

1. Brancher l'adaptateur USB-UART
2. Vérifier le port dans Terminal :
   ```bash
   ls /dev/tty.usb*
   ```
3. Dans l'app :
   - Cliquer sur "Select Port"
   - Choisir le port `/dev/tty.usbserial-XXXX`
   - Cliquer sur "Connect"

## Problèmes Courants

### Le package ne se résout pas

```bash
# Nettoyer et réessayer
rm -rf .build
xcodebuild -resolvePackageDependencies
```

### Erreur de compilation sur ORSSerialPort

Vérifier que vous ciblez macOS pour les fichiers qui utilisent ORSSerialPort. Les `#if os(macOS)` doivent être présents.

### Impossible de se connecter au port série

1. Vérifier les permissions :
   - Dans Xcode : Target > Signing & Capabilities
   - Activer "App Sandbox" si nécessaire
   - Ajouter "USB" sous Hardware

2. Vérifier le câblage :
   - TX (Mac) → RX (BMS)
   - RX (Mac) → TX (BMS)
   - GND → GND

## Prochaines Étapes

1. Tester en mode simulation
2. Connecter au BMS réel (macOS)
3. Explorer le Dashboard
4. Visualiser les cellules
5. Modifier les settings (attention : écrit réellement dans le BMS!)
6. Consulter les statistiques

## Fichiers Importants

- `Package.swift` : Configuration du package
- `Sources/App/TinyBMSApp.swift` : Point d'entrée
- `Sources/Services/TinyBMSService.swift` : Service principal
- `Sources/Services/ModbusProtocol.swift` : Protocole Modbus
- `Sources/Views/ContentView.swift` : Interface principale

## Personnalisation

### Changer le polling interval

Dans `TinyBMSService.swift`, ligne ~152 :
```swift
try? await Task.sleep(nanoseconds: 1_000_000_000) // 1 seconde
```

### Ajouter des registres

Voir `RegisterMap.swift` et ajouter vos définitions.

### Modifier l'interface

Toutes les vues sont dans `Sources/Views/` et utilisent SwiftUI.

---

**Bon développement !** 🚀
