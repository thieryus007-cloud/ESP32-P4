# Guide d'utilisation du script de test TinyBMS

## 📋 Prérequis

Le script `test_tinybms.py` nécessite:
- Python 3.6 ou supérieur ✅
- Module `pyserial` pour la communication série ✅
- Permissions d'accès au port série

## 🚀 Procédure complète (à utiliser à chaque fois)

### 1. Vérifier que Python3 et pyserial sont installés

```bash
# Vérifier Python3
python3 --version

# Vérifier pyserial
python3 -c "import serial; print('pyserial version:', serial.__version__)"
```

Si pyserial n'est pas installé:
```bash
pip3 install pyserial
# ou
python3 -m pip install pyserial
```

### 2. Identifier le port série

```bash
# Lister tous les ports série disponibles
ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "Aucun port USB détecté"

# Alternative avec dmesg (après branchement du câble)
dmesg | grep -i tty | tail -5
```

Le port sera généralement:
- `/dev/ttyUSB0` ou `/dev/ttyUSB1` sur Linux
- `/dev/ttyACM0` ou `/dev/ttyACM1` sur Linux
- `/dev/cu.usbserial-*` sur macOS

### 3. Vérifier les permissions du port série

```bash
# Vérifier les permissions du port (exemple avec ttyUSB0)
ls -l /dev/ttyUSB0

# Vérifier à quel groupe appartient le port
ls -l /dev/ttyUSB0 | awk '{print $4}'
```

Si vous n'avez pas les permissions:

**Option A: Ajouter votre utilisateur au groupe dialout (RECOMMANDÉ - permanent)**
```bash
# Ajouter l'utilisateur au groupe dialout
sudo usermod -a -G dialout $USER

# Ajouter au groupe uucp si nécessaire (certaines distributions)
sudo usermod -a -G uucp $USER

# IMPORTANT: Vous devez vous déconnecter et reconnecter pour que les changements prennent effet
# Ou exécuter:
newgrp dialout

# Vérifier que vous êtes dans le groupe
groups | grep dialout
```

**Option B: Permissions temporaires (jusqu'au prochain redémarrage)**
```bash
sudo chmod 666 /dev/ttyUSB0
```

### 4. Tester la communication avec le port série

```bash
# Test rapide d'ouverture du port
python3 -c "import serial; s = serial.Serial('/dev/ttyUSB0', 115200, timeout=1); print('Port ouvert avec succès!'); s.close()"
```

### 5. Lancer le script de test TinyBMS

```bash
cd /home/user/ESP32-P4/Exemple

# Avec auto-détection du port
python3 test_tinybms.py

# Ou spécifier le port manuellement
python3 test_tinybms.py /dev/ttyUSB0

# Avec sortie dans un fichier log
python3 test_tinybms.py /dev/ttyUSB0 2>&1 | tee test_$(date +%Y%m%d_%H%M%S).log
```

## 🔧 Script de lancement automatique

Pour simplifier, utilisez le script `run_test.sh` (voir ci-dessous).

### Mode d'emploi du script

```bash
# Rendre le script exécutable (une seule fois)
chmod +x run_test.sh

# Lancer le test
./run_test.sh

# Avec un port spécifique
./run_test.sh /dev/ttyUSB0
```

## ⚠️ Résolution de problèmes courants

### Erreur: "Permission denied" sur /dev/ttyUSB0

**Cause**: Votre utilisateur n'a pas les permissions pour accéder au port série.

**Solution**:
```bash
# Solution rapide (temporaire)
sudo chmod 666 /dev/ttyUSB0

# Solution permanente
sudo usermod -a -G dialout $USER
# Puis se déconnecter/reconnecter ou:
newgrp dialout
```

### Erreur: "No such file or directory: '/dev/ttyUSB0'"

**Cause**: Le port série n'existe pas ou le câble USB n'est pas branché.

**Solution**:
1. Vérifier que le câble USB est bien branché
2. Lister les ports disponibles: `ls /dev/ttyUSB* /dev/ttyACM*`
3. Débrancher/rebrancher le câble et vérifier: `dmesg | tail -20`

### Erreur: "ModuleNotFoundError: No module named 'serial'"

**Cause**: Le module pyserial n'est pas installé.

**Solution**:
```bash
pip3 install pyserial
# ou
python3 -m pip install pyserial
```

### Le script ne répond pas / timeout

**Cause**:
- Mauvais port série
- TinyBMS non alimenté
- Câblage RS485 incorrect
- Vitesse de communication incorrecte

**Solution**:
1. Vérifier que le TinyBMS est alimenté
2. Vérifier le câblage RS485 (A, B, GND)
3. Vérifier que la vitesse est bien 115200 bauds
4. Essayer un autre port USB

### Données parasites / réponses trop grandes

**Cause**: Debug messages du TinyBMS mélangés avec les trames MODBUS.

**Solution**: Le script gère automatiquement ce cas et filtre les données parasites.

## 📊 Utilisation du mode interactif

Une fois le script lancé, vous pouvez utiliser les commandes:

```
> r 0x0157          # Lire le registre 0x0157 (Current Offset)
> w 0x012C 4200     # Écrire 4200 dans le registre 0x012C (Fully Charged Voltage)
> r 343             # Lire le registre 343 en décimal
> q                 # Quitter
```

## 🔗 Registres TinyBMS utiles

| Adresse | Nom | Type | Unité |
|---------|-----|------|-------|
| 0x0064 | Cell 1 Voltage | RO | mV |
| 0x012C | Fully Charged Voltage | RW | mV |
| 0x0157 | Current Offset | RW | mA |
| 0x0158 | Shunt Resistance | RW | µΩ |

Pour la liste complète, voir la documentation MODBUS TinyBMS.

## 📝 Notes importantes

1. **Toujours vérifier les permissions** du port série avant de lancer le script
2. **Se déconnecter/reconnecter** après avoir ajouté votre utilisateur au groupe dialout
3. **Le TinyBMS doit être alimenté** pour répondre aux commandes
4. **Le câblage RS485** doit être correct (A, B, et GND si nécessaire)
5. **Une seule connexion** à la fois: fermer tous les autres programmes utilisant le port série

## ✅ Checklist rapide

- [ ] Python3 installé
- [ ] Module pyserial installé
- [ ] Port série identifié (ex: /dev/ttyUSB0)
- [ ] Permissions OK (membre du groupe dialout OU chmod 666)
- [ ] TinyBMS alimenté
- [ ] Câble USB branché
- [ ] Aucun autre programme n'utilise le port série

Si tous les points sont cochés, la commande devrait fonctionner!
