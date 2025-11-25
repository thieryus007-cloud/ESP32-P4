# 🚀 Guide Rapide - Mac Mini

## En 5 minutes : De zéro à l'application qui tourne

### ✅ Ce dont vous avez besoin

- Mac Mini (ou n'importe quel Mac)
- Adaptateur USB-UART
- Xcode (gratuit sur App Store)
- 10 minutes

---

## Étape 1 : Installer Xcode (5 min)

**Si Xcode n'est pas déjà installé :**

```
App Store → Rechercher "Xcode" → Installer
```

(C'est gros, prenez un café ☕)

---

## Étape 2 : Créer le Projet (2 min)

### 2.1 Lancer Xcode

```
Spotlight (Cmd+Space) → Taper "Xcode" → Entrée
```

### 2.2 Nouveau Projet

```
File > New > Project (Cmd+Shift+N)
```

### 2.3 Configuration

| Champ | Valeur |
|-------|--------|
| Platform | **macOS** |
| Template | **App** |
| Product Name | **TinyBMS Monitor** |
| Interface | **SwiftUI** |
| Language | **Swift** |

**Cliquer "Next" puis "Create"**

**Sauvegarder dans** : `ESP32-P4/Exemple/TinyBMS-macOS-App/`

---

## Étape 3 : Ajouter ORSSerialPort (1 min)

```
File > Add Package Dependencies
```

**Coller cette URL** :
```
https://github.com/armadsen/ORSSerialPort.git
```

**Version** : Up to Next Major 2.1.0

**Cliquer "Add Package"** → Cocher "ORSSerialPort" → "Add Package"

---

## Étape 4 : Remplacer les Fichiers (30 sec)

### 4.1 Supprimer les fichiers par défaut

Dans le navigateur Xcode (à gauche) :
- `ContentView.swift` → Delete → Move to Trash
- `TinyBMS_MonitorApp.swift` → Delete → Move to Trash

### 4.2 Ajouter nos fichiers

**Glisser-déposer** le dossier `TinyBMS Monitor` depuis le Finder vers Xcode

Options :
- ✅ Copy items if needed
- ✅ Create groups
- ✅ Target: TinyBMS Monitor

**Cliquer "Finish"**

---

## Étape 5 : Permissions USB (30 sec)

### 5.1 Ouvrir les Capabilities

```
Projet (icône bleue) > Target "TinyBMS Monitor" > Signing & Capabilities
```

### 5.2 Ajouter App Sandbox

```
Cliquer "+" → Rechercher "App Sandbox" → Double-cliquer
```

### 5.3 Activer USB

Sous **"Hardware"**, cocher :
- ✅ **USB**

---

## Étape 6 : Lancer ! (10 sec)

### 6.1 Sélectionner la destination

En haut : **"My Mac"**

### 6.2 Build & Run

**Appuyer sur le bouton Play (▶️)** ou **Cmd+R**

**L'application se lance !** 🎉

---

## Connexion au TinyBMS

### Matériel

1. **Brancher USB-UART** au Mac Mini
2. **Câbler au TinyBMS** :
   ```
   TX (UART) → RX (BMS)
   RX (UART) → TX (BMS)
   GND       → GND
   ```

### Dans l'Application

1. **"Select Port"**
2. Choisir `/dev/tty.usbserial-XXXX`
3. **"Connect"**
4. **Voir les données ! ✅**

---

## Test Sans Matériel

1. **"Select Port"**
2. Choisir **"Simulation"**
3. **"Connect"**
4. Données simulées !

---

## Problèmes ?

### Le port n'apparaît pas

**Terminal** :
```bash
ls /dev/tty.*
```

Si rien → Installer le driver USB :
- CP2102 : [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- CH340 : [Driver CH340](https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver)

### Permission denied

**Vérifier** :
```
Signing & Capabilities > App Sandbox > USB ✅
```

### Timeout

- Vérifier TX↔RX croisés
- Vérifier que le BMS est alimenté
- Essayer un autre câble

---

## C'est Tout ! 🎉

Votre application fonctionne maintenant sur le Mac Mini avec connexion USB-UART réelle au TinyBMS.

**Profitez du monitoring en temps réel !** 📊⚡

---

## Raccourcis Xcode Utiles

| Action | Raccourci |
|--------|-----------|
| Build | **Cmd+B** |
| Run | **Cmd+R** |
| Stop | **Cmd+.** |
| Clean | **Cmd+Shift+K** |
| Navigate | **Cmd+Shift+O** |

---

**Temps total : ~10 minutes** ⏱️
**Difficulté : Facile** 😊
