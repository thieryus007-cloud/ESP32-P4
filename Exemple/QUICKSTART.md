# 🚀 Démarrage Rapide - Test TinyBMS

## Pour macOS (Homebrew Python)

### Solution 1 : Script Automatique (RECOMMANDÉ)

```bash
cd Exemple
./run_test.sh
```

**C'est tout !** Le script va :
- ✅ Créer automatiquement un environnement virtuel Python
- ✅ Installer pyserial
- ✅ Détecter votre port USB (`/dev/tty.usbserial-0001`)
- ✅ Lancer les tests

### Solution 2 : Spécifier un port différent

```bash
./run_test.sh /dev/tty.usbserial-XXXX
```

### Solution 3 : Lancer directement le script Python

```bash
# Après le premier lancement du run_test.sh
source .venv/bin/activate
python3 test_tinybms.py
deactivate
```

## Pour Linux

```bash
cd Exemple
./run_test.sh /dev/ttyUSB0
```

## Dépannage

### Le port n'est pas trouvé

Lister les ports disponibles :
```bash
ls /dev/tty.* | grep -i usb
```

### Erreur de permission

Sur Linux :
```bash
sudo chmod 666 /dev/ttyUSB0
```

Sur macOS, pas de problème de permission normalement.

### Python externally-managed-environment

Le script `run_test.sh` gère automatiquement ce problème avec un environnement virtuel.

## Configuration

Le port par défaut est `/dev/tty.usbserial-0001` (configurable dans `test_tinybms.py`).

Le script utilise automatiquement :
- **Baudrate**: 115200
- **Timeout**: 1 seconde
- **Port**: `/dev/tty.usbserial-0001`
