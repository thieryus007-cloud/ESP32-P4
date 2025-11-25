# TinyBMS Monitor - Application macOS Standalone

Application native macOS pour monitorer et configurer votre TinyBMS via USB-UART sur Mac Mini.

## 🎯 Objectif

Cette application vous permet de :
- ✅ Connecter votre TinyBMS au Mac Mini via USB-UART
- ✅ Monitorer en temps réel : tensions, courant, SOC, températures
- ✅ Visualiser les 16 cellules individuellement
- ✅ Configurer tous les paramètres du BMS
- ✅ Voir les statistiques et l'historique

## 💻 Prérequis

- **Mac Mini** (ou n'importe quel Mac) avec macOS 13.0 ou supérieur
- **Xcode 15.0+** (gratuit sur l'App Store)
- **Adaptateur USB-UART** (CP2102, FTDI, CH340, etc.)
- **TinyBMS** à connecter

## 🚀 Installation et Compilation

### Étape 1 : Installer Xcode

Si pas déjà installé :
1. Ouvrir l'**App Store**
2. Rechercher "Xcode"
3. Cliquer "Obtenir" / "Installer"
4. Attendre (c'est gros, ~10-15 GB)

### Étape 2 : Créer le Projet Xcode

1. **Lancer Xcode**

2. **Créer un nouveau projet** :
   - File > New > Project (ou Cmd+Shift+N)
   - Sélectionner **"macOS"** en haut
   - Choisir **"App"**
   - Cliquer "Next"

3. **Configurer le projet** :
   - **Product Name** : `TinyBMS Monitor`
   - **Team** : Sélectionner votre compte Apple (ou "None" pour tests locaux)
   - **Organization Identifier** : `com.votrenom` (ou laisser par défaut)
   - **Interface** : **SwiftUI**
   - **Language** : **Swift**
   - **Use Core Data** : NON (décocher)
   - Cliquer "Next"

4. **Sauvegarder** :
   - Naviguer vers le dossier `ESP32-P4/Exemple/TinyBMS-macOS-App/`
   - Cliquer "Create"

### Étape 3 : Ajouter la Dépendance ORSSerialPort

1. Dans Xcode, avec le projet ouvert :
   - **File > Add Package Dependencies**

2. Dans la fenêtre qui s'ouvre :
   - Coller cette URL : `https://github.com/armadsen/ORSSerialPort.git`
   - **Dependency Rule** : "Up to Next Major Version" 2.1.0
   - Cliquer **"Add Package"**

3. Dans la liste des packages :
   - Cocher **"ORSSerialPort"**
   - Target : "TinyBMS Monitor"
   - Cliquer **"Add Package"**

### Étape 4 : Remplacer les Fichiers Sources

1. **Supprimer les fichiers par défaut** dans Xcode :
   - Dans le navigateur de fichiers (à gauche)
   - Clic droit sur `ContentView.swift` → Delete → "Move to Trash"
   - Pareil pour `TinyBMS_MonitorApp.swift` si présent

2. **Ajouter nos fichiers** :
   - Glisser-déposer le dossier **`TinyBMS Monitor`** (qui contient Models, Services, Views)
   - Depuis le Finder vers Xcode
   - Dans la fenêtre qui apparaît :
     - Cocher **"Copy items if needed"**
     - **"Create groups"** (pas folders)
     - Target : "TinyBMS Monitor" (cocher)
     - Cliquer **"Finish"**

### Étape 5 : Configurer les Permissions

1. **Dans Xcode, navigateur de projet** :
   - Cliquer sur **"TinyBMS Monitor"** (icône bleue en haut, le projet)
   - Dans la liste, sélectionner la target **"TinyBMS Monitor"**

2. **Onglet "Signing & Capabilities"** :
   - Si "Team" est "None", vous pouvez le laisser pour tests locaux
   - Cliquer sur **"+"** (en haut à gauche)
   - Rechercher et ajouter **"App Sandbox"**

3. **Configurer App Sandbox** :
   - Sous **"Hardware"**, cocher **"USB"**
   - Sous **"File Access"**, cocher **"User Selected File" (Read/Write)**

4. **Ajouter l'Entitlements** :
   - Toujours dans "Signing & Capabilities"
   - Vérifier que le fichier `TinyBMS_Monitor.entitlements` est bien associé
   - Sinon, dans "Build Settings", chercher "Code Signing Entitlements"
   - Mettre : `TinyBMS Monitor/TinyBMS_Monitor.entitlements`

### Étape 6 : Build & Run

1. **Sélectionner la destination** :
   - En haut de Xcode, à côté du bouton Play
   - Choisir **"My Mac"**

2. **Lancer l'application** :
   - Appuyer sur le bouton **Play** (▶️) ou **Cmd+R**
   - Xcode va compiler (peut prendre 1-2 minutes la première fois)
   - L'application se lance automatiquement !

## 🔌 Connexion au TinyBMS

### Préparation Matérielle

1. **Brancher l'adaptateur USB-UART** :
   - Connecter l'adaptateur au port USB du Mac Mini
   - Attendre que macOS le reconnaisse (quelques secondes)

2. **Câblage vers le TinyBMS** :
   ```
   USB-UART  →  TinyBMS
   ────────────────────
   TX        →  RX
   RX        →  TX
   GND       →  GND
   VCC       →  (optionnel, souvent pas nécessaire)
   ```
   **⚠️ IMPORTANT** : TX et RX sont croisés !

3. **Vérifier le port série** :
   - Ouvrir **Terminal**
   - Taper : `ls /dev/tty.usb*`
   - Vous devriez voir quelque chose comme : `/dev/tty.usbserial-1420`
   - C'est le nom de votre port !

### Utilisation de l'Application

1. **Lancer TinyBMS Monitor**
2. **Cliquer sur "Select Port"**
3. **Choisir votre port** (ex: `/dev/tty.usbserial-1420`)
4. **Cliquer sur "Connect"**
5. **Les données apparaissent** en temps réel ! 🎉

Si vous voyez les données (tensions, courant, SOC, etc.), c'est gagné ! ✅

## 🧪 Mode Test (Sans TinyBMS)

Si vous voulez tester l'application sans matériel :

1. Lancer l'application
2. Cliquer sur "Select Port"
3. Choisir **"Simulation"**
4. Cliquer sur "Connect"
5. L'application affiche des données simulées réalistes

## 📊 Fonctionnalités de l'Application

### Dashboard (Tableau de bord)
- **Jauges** : SOC, Voltage Pack, Courant, Températures
- **Graphique** : Tensions des 16 cellules
- **Informations BMS** : Mode, capacité, seuils de sécurité
- **État** : Balancing actif/inactif

### Cells (Cellules)
- **Vue détaillée** des 16 cellules
- **Indicateurs visuels** : Min (bleu), Max (rouge), Balancing (orange)
- **Statistiques** : Moyenne, Delta, Min/Max
- **Barres de santé** pour chaque cellule

### Settings (Configuration)
- **Battery** : Voltage charge/décharge, capacité, nombre de cellules
- **Safety** : Seuils de protection (over-voltage, under-voltage, courant)
- **Balance** : Paramètres d'équilibrage
- **Hardware** : Configuration matérielle

⚠️ **Attention** : Les modifications dans Settings sont **écrites réellement** dans le BMS !

### Statistics
- Distance totale
- Compteurs d'événements (over/under voltage)
- Cycles de charge
- État de santé (SOH)

## 🛠️ Dépannage

### "Le port série n'apparaît pas"

1. **Vérifier le driver USB-UART** :
   - Pour CP2102 : https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
   - Pour CH340 : https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver
   - Redémarrer le Mac après installation

2. **Vérifier dans Terminal** :
   ```bash
   ls -l /dev/tty.*
   ```
   Si rien n'apparaît, le driver n'est pas installé

3. **Débrancher/Rebrancher** l'adaptateur USB

### "Permission denied" ou "Access denied"

1. Vérifier les permissions dans Xcode :
   - Signing & Capabilities > App Sandbox > USB (coché)

2. Dans macOS :
   - Préférences Système > Confidentialité et Sécurité
   - Autoriser l'application si demandé

### "Timeout" lors de la connexion

1. **Vérifier le câblage** : TX↔RX bien croisés ?
2. **Vérifier le baudrate** : doit être 115200 (c'est le défaut)
3. **Vérifier que le TinyBMS est alimenté**
4. **Essayer un autre câble USB**

### L'application ne compile pas

1. **Nettoyer le build** :
   - Product > Clean Build Folder (Cmd+Shift+K)

2. **Résoudre les packages** :
   - File > Packages > Resolve Package Versions

3. **Vérifier les erreurs** :
   - Lire les erreurs dans la zone "Build" en bas
   - Vérifier que tous les fichiers sont bien importés

## 📦 Créer une Application Distribuable

Si vous voulez installer l'app sur le Mac Mini sans Xcode :

1. Dans Xcode :
   - Product > Archive
   - Attendre la compilation

2. Dans la fenêtre Archives :
   - Cliquer "Distribute App"
   - Choisir "Copy App"
   - Choisir un dossier de destination

3. L'application **TinyBMS Monitor.app** est créée !
   - Vous pouvez la copier dans `/Applications/`
   - Double-cliquer pour lancer

## 🔄 Mise à Jour

Pour mettre à jour l'application :

1. Récupérer les nouveaux fichiers sources
2. Dans Xcode, remplacer les fichiers concernés
3. Product > Build (Cmd+B)
4. Product > Run (Cmd+R)

## 📄 Licence

Basé sur le protocole TinyBMS Communication Protocols Rev D.

## 🆘 Support

Si vous rencontrez des problèmes :
1. Vérifier les étapes ci-dessus
2. Lire la section Dépannage
3. Ouvrir une issue sur GitHub avec :
   - Version de macOS
   - Modèle d'adaptateur USB-UART
   - Capture d'écran de l'erreur

---

**Application créée pour fonctionner sur Mac Mini avec connexion USB-UART réelle au TinyBMS** ✅

**Version** : 1.0.0
**Plateforme** : macOS 13.0+
**Architecture** : Apple Silicon (M1/M2/M3) + Intel
