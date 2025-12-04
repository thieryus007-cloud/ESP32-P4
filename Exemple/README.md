# Exemples et outils de test TinyBMS

Ce répertoire contient des outils pour tester et interagir avec le TinyBMS.

## 📁 Contenu

### 🐍 Script Python de test (`test_tinybms.py`)

Script Python complet pour tester la communication avec le TinyBMS via RS485/UART.

**Fonctionnalités:**
- Lecture/écriture de registres MODBUS
- Mode interactif en ligne de commande
- Gestion automatique du CRC16
- Filtrage des données parasites
- Tests automatiques au démarrage
- Auto-détection du port série

**Documentation complète:** Voir [GUIDE_TEST_TINYBMS.md](GUIDE_TEST_TINYBMS.md)

### 🚀 Script de lancement automatique (`run_test.sh`)

Script Bash qui vérifie tous les prérequis et lance automatiquement le test TinyBMS.

**Utilisation:**

```bash
# Première fois: rendre le script exécutable
chmod +x run_test.sh

# Lancer avec auto-détection du port
./run_test.sh

# Lancer avec un port spécifique
./run_test.sh /dev/ttyUSB0
```

**Le script vérifie automatiquement:**
1. ✅ Python3 installé
2. ✅ Module pyserial installé (et l'installe si nécessaire)
3. ✅ Port série disponible
4. ✅ Permissions d'accès au port série
5. ✅ Connexion au port série fonctionnelle

### 📖 Guide complet (`GUIDE_TEST_TINYBMS.md`)

Documentation détaillée incluant:
- Procédure complète étape par étape
- Résolution de tous les problèmes courants
- Liste des registres TinyBMS utiles
- Checklist de vérification
- Exemples de commandes

## 🎯 Démarrage rapide (3 étapes)

### Option 1: Script automatique (RECOMMANDÉ)

```bash
cd /home/user/ESP32-P4/Exemple
./run_test.sh
```

### Option 2: Manuel

```bash
cd /home/user/ESP32-P4/Exemple

# 1. Vérifier pyserial
python3 -c "import serial; print('OK')"

# 2. Ajouter permissions (si nécessaire, une seule fois)
sudo usermod -a -G dialout $USER
newgrp dialout

# 3. Lancer le script
python3 test_tinybms.py
```

## 🔧 Résolution de problèmes

### Erreur: "Permission denied"

```bash
# Solution rapide (temporaire)
sudo chmod 666 /dev/ttyUSB0

# Solution permanente
sudo usermod -a -G dialout $USER
newgrp dialout  # Ou se déconnecter/reconnecter
```

### Erreur: "No module named 'serial'"

```bash
pip3 install pyserial
```

### Aucun port série détecté

```bash
# Lister les ports disponibles
ls -la /dev/ttyUSB* /dev/ttyACM*

# Vérifier après branchement USB
dmesg | grep -i tty | tail -5
```

## 📚 Documentation complète

Pour la documentation complète avec toutes les procédures détaillées:

👉 **[GUIDE_TEST_TINYBMS.md](GUIDE_TEST_TINYBMS.md)**

## 🗂️ Autres exemples

- **`Gemini/`** - Exemples d'intégration avec Gemini
- **`TinyBMS-web/`** - Interface web pour TinyBMS
- **`mac-local/`** - Serveur de test local Node.js pour macOS

## 🆘 Support

Si vous rencontrez des problèmes:

1. Consultez d'abord le [GUIDE_TEST_TINYBMS.md](GUIDE_TEST_TINYBMS.md)
2. Vérifiez la checklist dans le guide
3. Utilisez le script automatique `run_test.sh` qui diagnostique les problèmes

## ✅ Checklist avant de commencer

- [ ] Python3 installé (`python3 --version`)
- [ ] Module pyserial installé (`python3 -c "import serial"`)
- [ ] Câble USB branché
- [ ] TinyBMS alimenté
- [ ] Port série accessible (`ls /dev/ttyUSB*`)
- [ ] Permissions OK (membre du groupe dialout)

---

**Note**: Pour toute utilisation en production, veuillez vous référer à la documentation officielle du TinyBMS.
