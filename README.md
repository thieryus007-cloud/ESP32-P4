# ESP32-P4 BMS HMI Interface

Interface Homme-Machine (HMI) pour système de gestion de batterie (BMS) basée sur ESP32-P4 avec écran tactile 7 pouces.

## 📚 Sommaire rapide

- [Présentation du projet](#-présentation-du-projet)
- [Matériel requis](#-matériel-requis)
- [Fonctionnalités](#-fonctionnalités)
- [Architecture logicielle](#-architecture-logicielle)
- [Structure du projet](#-structure-du-projet)
- [Démarrage rapide](#-démarrage-rapide)
- [Flux de données](#-flux-de-données)
- [Événements système](#-événements-système)
- [Serveur de test](#-serveur-de-test)
- [État du développement](#-état-du-développement)
- [Interfaces matérielles](#-interfaces-matérielles)

## 🎯 Présentation du projet

Ce projet est une interface graphique avancée développée pour améliorer le projet **BMS (Battery Management System)** existant. Il fournit une interface tactile complète et intuitive pour visualiser et contrôler un système de gestion de batterie en temps réel.

### Contexte

Le projet s'appuie sur un système BMS existant fonctionnant sur ESP32-S3 et offre :
- **Une interface tactile 7 pouces** pour remplacer/compléter l'interface web
- **Affichage en temps réel** de toutes les données de télémétrie
- **Contrôle complet** du système de batterie
- **Architecture événementielle** robuste et modulaire

## 🔧 Matériel requis

- **Plateforme** : [ESP32-P4-WIFI6-Touch-LCD-7B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7b.htm) (Waveshare)
  - Processeur ESP32-P4 avec support WiFi 6
  - Écran tactile capacitif 7 pouces (800x480)
  - Interfaces RS485 et CAN intégrées
  - Support Ethernet

### Modes de fonctionnement

- **Connecté S3** : l'ESP32-P4 agit comme afficheur réseau pour un BMS ESP32-S3 (WebSocket + HTTP REST).
- **Autonome TinyBMS** : l'ESP32-P4 dialogue directement avec un TinyBMS via RS485/UART et peut publier en MQTT/HTTP.
- **CAN Victron (en cours d'intégration)** : le couple `can_victron` + `can_publisher` diffuse les trames Victron Energy pour compatibilité onduleurs/chargeurs.

## ✨ Fonctionnalités

### Interface graphique (7 écrans)

#### Écrans BMS S3 (via WiFi/WebSocket)

1. **🏠 Écran d'accueil (Home)**
   - Affichage grand format du SOC (State of Charge)
   - Tension, courant, puissance et température
   - Indicateurs de statut : BMS, CAN, MQTT, WiFi, Équilibrage, Alarmes
   - Codes couleur pour l'état du système (vert/jaune/rouge/gris)

2. **🔋 Écran Batterie (Battery/Pack)**
   - Résumé du pack : SOC, tension, courant, puissance
   - Statistiques des cellules : min, max, delta, moyenne
   - Indicateur d'équilibrage
   - Tableau des tensions de cellules

3. **📊 Écran Cellules (Cells)**
   - En-tête avec statistiques (min/max/delta/moyenne)
   - Indicateurs de seuils d'équilibrage
   - Graphique à barres défilant pour jusqu'à 32 cellules
   - Indicateurs d'équilibrage par cellule

4. **⚡ Écran Flux d'énergie (Power Flow)**
   - Visualisation du flux de puissance
   - Affichage PV (panneau solaire - prévu)
   - État de la batterie avec indicateur directionnel
   - Indicateur de charge/décharge avec codes couleur

5. **⚙️ Écran Configuration (Config)**
   - Interface de configuration (en développement)
   - Intégration prévue avec les endpoints REST API

#### Écrans TinyBMS (via UART/RS485 direct)

6. **🔌 Écran TinyBMS Status**
   - État de connexion UART en temps réel
   - Statistiques de communication (Reads, Writes, Errors)
   - Bouton "Read All" pour scanner les 34 registres
   - Bouton "Restart BMS" pour redémarrer TinyBMS

7. **⚙️ Écran TinyBMS Config**
   - Configuration complète des 34 registres TinyBMS
   - Sections : Battery (9), Charger (2), Safety (6), Advanced (5), System (13)
   - Affichage avec unités (mV, A, Ah, %, °C)
   - Mise à jour automatique après lecture

### Communication

#### Communication réseau (BMS S3)
- **WiFi** : Connexion au système BMS S3
- **WebSocket** :
  - `/ws/telemetry` - Flux de données de batterie
  - `/ws/events` - Flux d'événements système
- **HTTP REST API** : Envoi de commandes et configuration
- **Mode Connecté/Autonome** : un paramètre `menuconfig` (persisté en NVS) ou un toggle GUI (événement `EVENT_USER_INPUT_CHANGE_MODE`) permet de basculer entre « Connecté S3 » et « Autonome TinyBMS » pour activer ou non la télémétrie réseau.
- **Publisher périodique MQTT/HTTP** : un module `network_publisher` expédie un sous-ensemble des mesures TinyBMS (voltage, courant, puissance, SOC, SOH, température, min/max/delta cellules) avec un tampon « offline » rejoué à la reconnexion.

  **Schéma des topics/payloads**

  - **MQTT** : topic `tinybms/telemetry` (configurable), payload texte clef=valeur compact
    ```
    ts_ms=1700001234 soc=72.35 voltage_v=51.234 current_a=-12.401 power_w=-635.45 temp_c=28.1
    ```
  - **HTTP** : POST `/api/telemetry/local` (configurable) avec JSON compact
    ```json
    {
      "ts_ms": 1700001234,
      "soc": 72.35,
      "soh": 99.10,
      "voltage_v": 51.234,
      "current_a": -12.401,
      "power_w": -635.45,
      "temperature_c": 28.1,
      "cell_min_mv": 3201.5,
      "cell_max_mv": 3230.1,
      "cell_delta_mv": 28.6
    }
    ```

  **Configuration menuconfig**

  - `NETWORK_TELEMETRY_PUBLISHER_ENABLED` : toggle d'activation.
  - `NETWORK_TELEMETRY_PERIOD_MS` : cadence d'envoi en millisecondes.
  - `NETWORK_TELEMETRY_HTTP_PATH` / `NETWORK_TELEMETRY_MQTT_TOPIC` : personnalisation des cibles.
  - `NETWORK_TELEMETRY_OFFLINE_BUFFER` + `NETWORK_TELEMETRY_BUFFER_DEPTH` : activer et dimensionner le tampon rejoué après reconnexion.

#### Communication directe (TinyBMS)
- **RS485/UART** : ✅ **Implémenté** - Communication directe avec TinyBMS
  - UART1 sur GPIO27 (RXD) / GPIO26 (TXD)
  - 115200 baud, 8N1
  - Protocole binaire avec CRC16
  - 34 registres configurables
  - Lecture/écriture avec retry et vérification

- **CAN Bus** : Communication avec le pack batterie (prévu)
  - RXD: GPIO21, TXD: GPIO22

## 🏗️ Architecture logicielle

Le projet suit une **architecture événementielle en 5 couches** :

```
┌──────────────────────────────────────────────────┐
│   Couche 5 : Présentation (LVGL GUI)            │
│   • 7 écrans tactiles interactifs (5 S3 + 2 TBMS)│
└──────────────────────────────────────────────────┘
                    ↓ Events
┌──────────────────────────────────────────────────┐
│   Couche 4 : Application/Modèle                  │
│   • telemetry_model      • tinybms_model         │
│   • system_events_model  • tinybms_client        │
│   • config_model                                 │
└──────────────────────────────────────────────────┘
                    ↓ Events
┌──────────────────────────────────────────────────┐
│   Couche 3 : Communication                       │
│   • net_client (WiFi + WebSocket)                │
│   • remote_event_adapter (JSON ↔ Events)         │
│   • tinybms_client (UART/RS485 ↔ Events)         │
└──────────────────────────────────────────────────┘
                    ↓ Events
┌──────────────────────────────────────────────────┐
│   Couche 2 : Noyau Système                       │
│   • EventBus (Publish/Subscribe)                 │
│   • FreeRTOS Tasks                               │
└──────────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────────┐
│   Couche 1 : HAL & BSP                           │
│   • Drivers LCD, tactile, WiFi, UART             │
└──────────────────────────────────────────────────┘
```

### Composants principaux

#### EventBus (`components/event_bus/`)
- **Système publish-subscribe** pour communication décuplée entre modules
- Support de 14 types d'événements
- Thread-safe avec callbacks synchrones
- Maximum 32 abonnés simultanés

#### Types d'événements (`components/event_types/`)
Structures de données principales :
- `battery_status_t` : État global de la batterie (SOC, SOH, tension, courant, etc.)
- `pack_stats_t` : Statistiques au niveau cellule (jusqu'à 32 cellules)
- `system_status_t` : Indicateurs de santé système (état WiFi/bridge, mode courant,
  et statut réseau tri-états : non configuré, en échec, actif)

#### Client réseau (`components/net_client/`)
- Gestion de la connexion WiFi en mode Station
- Client WebSocket pour flux de données temps réel
- Client HTTP REST pour commandes
- Reconnexion automatique (jusqu'à 5 tentatives)
- Configuration via menuconfig (SSID, mot de passe, hôte du bridge)
- Bascule optionnelle vers le mode autonome si le WiFi échoue plusieurs fois :
  activable via `CONFIG_HMI_WIFI_FAILOVER_ENABLED` et seuil configurable avec
  `CONFIG_HMI_WIFI_FAILOVER_THRESHOLD`; un événement dédié alerte la GUI.

#### Adaptateur d'événements (`components/remote_event_adapter/`)
- Convertit les messages JSON ↔ événements internes
- Parse la télémétrie depuis `/ws/telemetry`
- Extrait : tension/courant du pack, SOC, SOH, température
- Traite les tableaux de tensions et états d'équilibrage des cellules

#### Interface graphique (`components/gui_lvgl/`)
- Basée sur **LVGL** (Light and Versatile Graphics Library)
- 7 écrans dans une interface à onglets
- Thread-safe avec `lv_async_call()`
- 1,728 lignes de code GUI (5 écrans S3 + 2 écrans TinyBMS)
- Onglet Config : bouton dédié pour redemander le mode connecté après une bascule autonome.

#### Client TinyBMS (`components/tinybms_client/`)
- **Protocole binaire UART** avec CRC16 (Modbus-like)
- Communication sur UART1 (GPIO27/26) à 115200 baud
- Fonctions de lecture/écriture thread-safe avec mutex
- Retry automatique (3 tentatives)
- Vérification après écriture
- Statistiques détaillées (reads/writes OK/failed, CRC errors, timeouts)
- Commande de redémarrage TinyBMS

#### Modèle TinyBMS (`components/tinybms_model/`)
- **Catalogue complet** de 34 registres répartis en 5 groupes :
  - Battery (9 registres) : tensions, capacité, cellules
  - Charger (2 registres) : délais de démarrage/arrêt
  - Safety (6 registres) : seuils de protection
  - Advanced (5 registres) : SOC/SOH, cycles
  - System (13 registres) : modes de fonctionnement
- Conversion raw ↔ user value avec scaling et précision
- Validation des valeurs (min/max/step)
- Support des enums (13 registres de type enum)
- Cache local avec timestamps
- API : read_all(), read_register(), write_register(), get_config()

## 📁 Structure du projet

```
ESP32-P4/
├── README.md
├── main/
│   ├── app_main.c.c         # Point d'entrée (36 lignes)
│   ├── hmi_main.c           # Orchestrateur système (73 lignes)
│   └── hmi_main.h
├── components/
│   ├── event_bus/           # Système d'événements pub/sub
│   ├── event_types/         # Définitions de types et structures
│   ├── gui_lvgl/            # Interface graphique LVGL (1,728 lignes)
│   │   ├── gui_init.c/h
│   │   ├── screen_home.c/h            (251 lignes)
│   │   ├── screen_battery.c/h         (260 lignes)
│   │   ├── screen_cells.c/h           (226 lignes)
│   │   ├── screen_power.c/h           (117 lignes)
│   │   ├── screen_config.c/h          (22 lignes)
│   │   ├── screen_tinybms_status.c/h  (209 lignes)
│   │   └── screen_tinybms_config.c/h  (255 lignes)
│   ├── net_client/          # Client WiFi + WebSocket
│   ├── remote_event_adapter/# Convertisseur JSON ↔ EventBus
│   ├── tinybms_client/      # Client UART TinyBMS (911 lignes)
│   │   ├── tinybms_client.c/h
│   │   └── tinybms_protocol.c/h
│   └── tinybms_model/       # Modèle registres TinyBMS (1,018 lignes)
│       ├── tinybms_model.c/h
│       └── tinybms_registers.c/h
└── Exemple/
    └── mac-local/           # Serveur de test Node.js pour TinyBMS
```

**Statistiques du projet :**
- 37 fichiers source
- 5,818 lignes de code
- Architecture modulaire avec composants indépendants
- 3 nouveaux composants TinyBMS (2,575 lignes)

## 🚀 Démarrage rapide

### Prérequis

- **ESP-IDF** v5.0 ou supérieur
- **Outils de développement ESP-IDF** configurés
- **Carte ESP32-P4-WIFI6-Touch-LCD-7B**

### Checklist environnement

1. Installer l'ESP-IDF (script `install.sh`) et sourcer `export.sh` dans votre shell.
2. Vérifier la version avec `idf.py --version` (>= 5.0 recommandé).
3. Installer les dépendances Python de LVGL via `pip install -r $IDF_PATH/requirements.txt` si nécessaire.
4. Connecter la carte en USB et repérer le port série (`ls /dev/ttyUSB*`).

### Dépendances

- ESP-IDF framework
- LVGL (Light and Versatile Graphics Library)
- esp_lvgl_port (intégration LVGL pour ESP)
- BSP Waveshare pour ESP32-P4
- cJSON pour le parsing JSON
- FreeRTOS (inclus dans ESP-IDF)

### Compilation et flash

```bash
# Initialiser l'environnement ESP-IDF si ce n'est pas fait
. $IDF_PATH/export.sh

# Cloner le projet
git clone <repository-url>
cd ESP32-P4

# Configurer le projet
idf.py menuconfig
# Configurer :
# - WiFi SSID et mot de passe
# - Hôte et port du bridge BMS S3

# Compiler
idf.py build

# Flasher sur l'ESP32-P4
idf.py -p /dev/ttyUSB0 flash monitor
```

### Configuration rapide par mode

- **Mode connecté S3** :
  - Renseigner SSID/mot de passe WiFi et l'hôte du bridge (`CONFIG_HMI_WIFI_*`, `CONFIG_HMI_BRIDGE_*`).
  - S'assurer que le bridge expose `/ws/telemetry` et `/ws/events`.
- **Mode TinyBMS autonome** :
  - Activer le port RS485/UART1 (GPIO27/26) et vérifier le câblage A/B.
  - Régler l'intervalle MQTT/HTTP dans `NETWORK_TELEMETRY_*` si la publication distante est souhaitée.
- **Intégration CAN Victron** :
  - Utiliser les GPIO 22 (TX) / 21 (RX) et la vitesse 500 kbps.
  - Lancer `can_victron_init()` puis `can_publisher_init()` après l'EventBus pour diffuser les 19 trames Victron.

### Configuration WiFi

Dans `menuconfig`, configurer :
- `CONFIG_HMI_WIFI_SSID` : SSID du réseau WiFi
- `CONFIG_HMI_WIFI_PASSWORD` : Mot de passe WiFi
- `CONFIG_HMI_BRIDGE_HOST` : Adresse IP du BMS S3
- `CONFIG_HMI_BRIDGE_PORT` : Port du serveur BMS

## 🔄 Flux de données

### Télémétrie (S3 → HMI)
```
1. S3 envoie JSON via WebSocket /ws/telemetry
2. net_client reçoit et transmet à remote_event_adapter
3. Adapter parse JSON → structures C
4. Publie EVENT_BATTERY_STATUS_UPDATED et EVENT_PACK_STATS_UPDATED
5. Composants GUI s'abonnent et mettent à jour via lv_async_call()
6. LVGL rend les mises à jour sur l'écran 800x480
```

### Commandes (HMI → S3) - En développement
```
1. L'utilisateur interagit avec les widgets LVGL
2. GUI publie des événements EVENT_USER_INPUT_*
3. remote_event_adapter convertit en JSON
4. net_client envoie via WebSocket ou HTTP POST
5. S3 traite et retourne le résultat
```

## 📊 Événements système

### Catégories d'événements

**Données du S3 (lecture seule) :**
- `EVENT_REMOTE_TELEMETRY_UPDATE` : Télémétrie brute de la batterie
- `EVENT_REMOTE_SYSTEM_EVENT` : Événements système (WiFi, storage, alertes)
- `EVENT_REMOTE_CONFIG_SNAPSHOT` : Configuration globale

**Modèle interne (pour GUI) :**
- `EVENT_BATTERY_STATUS_UPDATED` : État batterie (SOC, U, I, P, T°)
- `EVENT_PACK_STATS_UPDATED` : Statistiques cellules
- `EVENT_SYSTEM_STATUS_UPDATED` : État des connexions

**Actions utilisateur (prévues) :**
- `EVENT_USER_INPUT_SET_TARGET_SOC` : Définir SOC cible
- `EVENT_USER_INPUT_CHANGE_MODE` : Changer mode (normal/eco/debug)
- `EVENT_USER_INPUT_ACK_ALARM` : Acquitter alarme
- `EVENT_USER_INPUT_WRITE_CONFIG` : Écrire configuration

## 🛠️ Serveur de test

Le répertoire `Exemple/mac-local/` contient un serveur de test Node.js :

**Fonctionnalités :**
- Interface web locale pour Mac mini
- Communication USB-UART avec TinyBMS
- Lecture/écriture de registres
- API REST : `/api/registers`, `/api/system/restart`
- Auto-détection du port série
- Filtrage par groupe de registres
- Édition inline des valeurs

**Démarrage :**
```bash
cd Exemple/mac-local
npm install
npm start
# Ouvrir http://localhost:5173
```

## 📈 État du développement

### ✅ Implémenté
- Architecture EventBus centrale avec 19 types d'événements
- Définitions de types d'événements étendues
- Client réseau (WiFi + WebSocket)
- Adaptateur JSON vers événements
- Interface graphique complète 7 écrans LVGL (5 S3 + 2 TinyBMS)
- Orchestration système de base
- **Communication UART/RS485 TinyBMS complète**
  - Protocole binaire avec CRC16
  - Client thread-safe avec retry
  - Catalogue complet de 34 registres
  - Modèle avec cache et validation
  - GUI de statut et configuration
- Intégration complète dans hmi_main

### 🚧 En cours / Prévu
- Composants modèle S3 (telemetry_model, system_events_model, config_model)
- Composant logger
- Système de configuration (CMakeLists.txt, sdkconfig)
- Gestion des entrées utilisateur (commandes vers S3)
- Interface de configuration S3 complète
- Édition interactive des registres TinyBMS dans GUI
- Module de communication CAN
- Support mise à jour OTA

## 🛠️ Dépannage et vérifications rapides

- **Connexion UART TinyBMS** : vérifier que `tinybms_client_get_stats()` retourne des compteurs de lectures > 0 et aucune erreur CRC.
- **Flux WebSocket S3** : surveiller les logs `EVENT_REMOTE_TELEMETRY_UPDATE`; en l'absence d'événements, valider SSID/mot de passe et l'URL du bridge.
- **Affichage LVGL** : si l'écran reste noir, vérifier l'appel à `gui_init()` et la présence du task LVGL dans `hmi_main`.
- **CAN Victron** : confirmer que la tâche `can_victron_task` est démarrée et que les événements `EVENT_CAN_MESSAGE_RX` arrivent après le handshake 0x307.
- **Diffusion MQTT/HTTP** : en mode autonome, activer le tampon offline (`NETWORK_TELEMETRY_OFFLINE_BUFFER`) pour éviter la perte de mesures pendant les coupures réseau.

## 🔌 Interfaces matérielles

### UART/RS485
- **RXD** : GPIO27
- **TXD** : GPIO26
- Communication directe avec TinyBMS
- Référence : [Waveshare exemple 13_RS485_Test](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/tree/main/examples/ESP-IDF/13_RS485_Test)

### CAN Bus
- **RXD** : GPIO21
- **TXD** : GPIO22
- Communication avec le pack batterie
- Référence : [Waveshare exemple 14_TWAItransmit](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/tree/main/examples/ESP-IDF/14_TWAItransmit)

## 🎨 Principes de conception

1. **Publish-Subscribe** : Toute communication inter-module via EventBus
2. **Architecture en couches** : Séparation claire des responsabilités
3. **Thread-safe GUI** : Callbacks asynchrones pour mises à jour LVGL
4. **Data-Driven** : Schémas JSON mappés directement vers structures C
5. **Composants modulaires** : Chaque composant est indépendant et testable
6. **Main minimal** : `app_main.c` orchestre uniquement, pas de logique métier

## 📝 Roadmap

1. ✅ Squelette projet & EventBus
2. ✅ `app_main.c` + `hmi_main.c`
3. ✅ Intégration LVGL + écran
4. ✅ Module net_client (connexion S3 + WS/HTTP)
5. ✅ Module remote_event_adapter
6. ✅ Module modèle `telemetry_model` (statuts batterie/pack)
7. 🚧 Module modèle `system_events_model` (état système global)
8. ✅ GUI LVGL v1 (lecture seule)
9. ✅ GUI LVGL v2 (actions utilisateur)
10. ✅ Extensions (config, historique, debug UART/CAN)

## 🤝 Contribution

Ce projet fait partie d'une suite d'outils BMS. Pour contribuer :
1. Fork le projet
2. Créer une branche de fonctionnalité
3. Commiter les changements
4. Pousser vers la branche
5. Ouvrir une Pull Request

## 📄 Licence

[À définir]

## 🔗 Projets liés

- **Projet BMS** : Système de gestion de batterie sur ESP32-S3 (GitHub)
- **TinyBMS** : Système BMS compact
- **Interface Web** : Interface web du BMS

## 📞 Contact

[À compléter]

---

**Note** : Ce projet est en développement actif. Certaines fonctionnalités sont encore en cours d'implémentation.
