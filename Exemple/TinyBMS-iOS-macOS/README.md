# TinyBMS iOS/macOS App

Application native iOS et macOS pour le monitoring et la configuration du TinyBMS.

## 📱 Fonctionnalités

### Dashboard
- Visualisation en temps réel des données du BMS
- Jauges pour SOC, tension, courant et températures
- Graphique des tensions de cellules avec min/max/delta
- État de la batterie et statut de balancing

### Cellules
- Vue détaillée des 16 cellules
- Indicateurs visuels pour min/max/balancing
- Statistiques (moyenne, min, max, delta)
- Barre de santé pour chaque cellule

### Configuration
- Lecture et écriture des paramètres du BMS
- Organisé en 4 groupes :
  - **Battery** : Configuration de base de la batterie
  - **Safety** : Seuils de sécurité (tension, courant, température)
  - **Balance** : Paramètres d'équilibrage
  - **Hardware** : Configuration matérielle

### Statistiques
- Distance totale parcourue
- Compteurs d'événements (over-voltage, under-voltage)
- Cycles de charge
- Historique des défauts

## 🛠️ Architecture

### Structure du Projet

```
TinyBMS-iOS-macOS/
├── Package.swift                    # Configuration Swift Package
├── Sources/
│   ├── Models/                      # Modèles de données
│   │   ├── BMSRegister.swift       # Définition des registres
│   │   ├── RegisterMap.swift       # Carte complète des registres
│   │   └── BMSData.swift           # Modèles de données BMS
│   ├── Services/                    # Services de communication
│   │   ├── ModbusProtocol.swift    # Protocole Modbus/CRC
│   │   ├── TinyBMSService.swift    # Service principal
│   │   └── BMSSimulator.swift      # Simulateur pour tests
│   ├── Views/                       # Interfaces SwiftUI
│   │   ├── ContentView.swift       # Vue principale
│   │   ├── DashboardView.swift     # Dashboard
│   │   ├── CellsView.swift         # Vue des cellules
│   │   ├── SettingsView.swift      # Configuration
│   │   └── StatsView.swift         # Statistiques
│   └── App/
│       └── TinyBMSApp.swift        # Point d'entrée de l'app
```

### Technologies

- **SwiftUI** : Interface utilisateur déclarative
- **Combine** : Réactivité avec `@Published` et `ObservableObject`
- **ORSSerialPort** : Communication série sur macOS
- **Modbus Protocol** : Protocole de communication TinyBMS
- **Async/Await** : Communication asynchrone moderne

## 🚀 Installation

### Prérequis

- macOS 13.0+ ou iOS 16.0+
- Xcode 15.0+
- Swift 5.9+

### Via Swift Package Manager

1. Cloner le dépôt :
```bash
git clone <repository-url>
cd TinyBMS-iOS-macOS
```

2. Ouvrir le package dans Xcode :
```bash
open Package.swift
```

3. Build et run :
   - Pour macOS : Sélectionner "My Mac" comme cible
   - Pour iOS : Sélectionner un simulateur ou appareil iOS

### Création d'une Application Xcode

Pour créer une application standalone :

1. Créer un nouveau projet Xcode (iOS ou macOS App)
2. Ajouter le package local dans "Package Dependencies"
3. Ou copier tous les fichiers Sources/ dans le projet

## 🔌 Connexion au BMS

### macOS

1. Brancher l'adaptateur USB-UART au Mac
2. Connecter TX/RX/GND au TinyBMS (attention au croisement TX↔RX)
3. Lancer l'application
4. Cliquer sur "Select Port"
5. Choisir le port `/dev/tty.usbserial-XXXX`
6. Cliquer sur "Connect"

### iOS

iOS ne supporte pas nativement les ports série USB. L'application fonctionne en **mode simulation** :

1. Lancer l'application sur iOS
2. Elle se connecte automatiquement au simulateur
3. Les données sont générées dynamiquement pour tester l'interface

### Mode Simulation (disponible sur macOS et iOS)

Pour tester sans matériel :
1. Sélectionner "Simulation" dans la liste des ports
2. Cliquer sur "Connect"
3. Le simulateur génère des données réalistes avec variations dynamiques

## 📡 Protocole de Communication

### Modbus RTU

Le TinyBMS utilise le protocole Modbus RTU via UART :
- **Baudrate** : 115200
- **Data bits** : 8
- **Parity** : None
- **Stop bits** : 1
- **Header** : 0xAA
- **CRC** : Modbus CRC16 (polynôme 0xA001)

### Fonctions Modbus

- **0x03** : Read Multiple Registers
- **0x10** : Write Multiple Registers

### Exemple de Commande

Lecture des cellules 1-16 (registres 0-15) :
```
AA 03 00 00 00 10 [CRC_L] [CRC_H]
```

Écriture du registre 300 (Fully Charged Voltage = 4.2V = 4200) :
```
AA 10 01 2C 00 01 02 10 68 [CRC_L] [CRC_H]
```

## 🎨 Interface Utilisateur

### Dark Mode

L'application utilise automatiquement le mode sombre/clair du système.

### Responsive

- **macOS** : Layout avec sidebar et zone de contenu principale
- **iOS** : TabView avec navigation en bas d'écran

### Couleurs

- **Vert** : Valeurs normales
- **Bleu** : Minimum / Info
- **Rouge** : Maximum / Danger
- **Orange** : Balancing / Warning

## 🔧 Développement

### Ajouter un Registre

1. Ajouter la définition dans `RegisterMap.swift` :
```swift
BMSRegister(id: 999, label: "New Register", unit: "V",
            type: .uint16, scale: 0.001,
            category: .settings, group: .battery)
```

2. Ajouter le mapping dans `BMSData.swift` :
```swift
case 999: settings.newRegister = value
```

3. Mettre à jour l'interface dans `SettingsView.swift`

### Ajouter une Vue

1. Créer un nouveau fichier dans `Sources/Views/`
2. Importer SwiftUI et utiliser `@EnvironmentObject`
3. Ajouter la vue dans `ContentView.swift`

### Simulateur

Le simulateur génère des données réalistes :
- Courant variable (sinusoïdal)
- SOC qui varie selon le courant
- Tensions de cellules avec bruit aléatoire
- Balancing aléatoire occasionnel
- Températures avec variations

## 📝 Registres Supportés

### Live Data (0-99)
- 0-15 : Cell Voltages
- 36 : Pack Voltage
- 38 : Pack Current
- 40-41 : Min/Max Cell Voltage
- 42-43 : Temperature Sensors
- 46 : State of Charge
- 48 : Internal Temperature
- 50 : BMS Status
- 52 : Real Balancing

### Statistics (100-199)
- 101 : Total Distance
- 105-106 : Under/Over Voltage Counts
- 111-112 : Charging Counts

### Settings (300-343)
- **Battery** : 300, 301, 306, 307, 322, 328
- **Safety** : 315, 316, 317, 318, 305, 319, 320
- **Balance** : 303, 304, 308, 321, 332
- **Hardware** : 310, 311, 312, 330, 340, 343

### Version (500+)
- 501 : Firmware Version

## 🐛 Dépannage

### Port série non détecté (macOS)

1. Vérifier que le driver USB-UART est installé
2. Vérifier avec `ls /dev/tty.*` dans Terminal
3. Débrancher/rebrancher l'adaptateur USB

### Timeout de lecture

1. Vérifier le câblage (TX↔RX croisés)
2. Vérifier le baudrate (115200)
3. Vérifier l'alimentation du TinyBMS

### Application ne compile pas

1. Nettoyer le build : `cmd+shift+K`
2. Nettoyer les packages : Supprimer `.build/` et DerivedData
3. Résoudre les dépendances : `File > Packages > Resolve Package Versions`

## 📄 Licence

Ce projet est basé sur le protocole TinyBMS Communication Protocols Rev D.

## 🤝 Contribution

Les contributions sont les bienvenues ! Pour contribuer :

1. Fork le projet
2. Créer une branche feature (`git checkout -b feature/amazing-feature`)
3. Commit les changements (`git commit -m 'Add amazing feature'`)
4. Push vers la branche (`git push origin feature/amazing-feature`)
5. Ouvrir une Pull Request

## 📞 Support

Pour les questions et le support :
- Ouvrir une issue sur GitHub
- Consulter la documentation du TinyBMS
- Vérifier le wiki du projet

## 🔮 Roadmap

- [ ] Support Bluetooth pour iOS (via ESP32 ou adaptateur BLE)
- [ ] Graphiques historiques avec persistance
- [ ] Notifications push pour les alertes
- [ ] Export des données (CSV, JSON)
- [ ] Widget iOS/macOS
- [ ] watchOS companion app
- [ ] Profils de configuration sauvegardés
- [ ] Mode multi-BMS (plusieurs batteries)

## ✨ Différences avec la Version Web

### Avantages

- ✅ Interface native (performance, look & feel)
- ✅ Pas besoin de serveur Node.js
- ✅ Application standalone
- ✅ Support macOS et iOS
- ✅ SwiftUI moderne et réactive
- ✅ Mode simulation intégré

### Limitations

- ⚠️ iOS ne supporte pas les ports série USB (simulation uniquement)
- ⚠️ macOS seulement pour connexion série réelle
- ⚠️ Pas de support web browser

Pour une utilisation sur iOS avec un BMS réel, envisager :
- Pont Bluetooth (ESP32 avec BLE)
- Serveur intermédiaire (WiFi)
- Application macOS avec partage réseau

---

**Version** : 1.0.0
**Dernière mise à jour** : 2025-11-25
