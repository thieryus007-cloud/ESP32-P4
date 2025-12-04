# Guide de Test Modbus TinyBMS - Outils et Exemples

## 🎯 Objectif
Ce guide vous permet de tester la communication série avec TinyBMS sans utiliser l'interface web, en utilisant des outils logiciels existants.

---

## 🛠️ Logiciels Recommandés

### 1. **QModMaster** (⭐ Recommandé - Gratuit, Multiplateforme)
- **Téléchargement:** https://github.com/ed-chemnitz/qmodmaster/releases
- **Plateformes:** Windows, macOS, Linux
- **Avantages:**
  - Interface graphique intuitive
  - Supporte Modbus RTU et ASCII
  - Monitoring en temps réel
  - Affichage hexadécimal des trames
  - Calcul automatique du CRC

**Configuration pour TinyBMS:**
```
Port: /dev/ttyUSB0 (ou COM3 sur Windows)
Baud Rate: 115200
Data Bits: 8
Parity: None
Stop Bits: 1
Flow Control: None
```

### 2. **ModbusMechanic** (Windows uniquement)
- **Site:** http://www.modbusdriver.com/modbustester.html
- **Avantages:**
  - Très simple d'utilisation
  - Affichage brut des trames hexadécimales
  - Parfait pour déboguer

### 3. **CoolTerm** (Gratuit, Multiplateforme)
- **Site:** https://freeware.the-meiers.org/
- **Avantages:**
  - Terminal série universel
  - Affichage hexadécimal
  - Capture de trames
  - Permet d'envoyer des trames manuellement

### 4. **mbpoll** (Ligne de commande)
```bash
# Installation
sudo apt-get install mbpoll   # Linux/Debian
brew install mbpoll            # macOS

# Exemple - Lire le registre 343
mbpoll -a 1 -b 115200 -t 3 -r 343 -1 /dev/ttyUSB0
```

### 5. **pymodbus** (Python - Script personnalisé)
```bash
pip install pymodbus pyserial
```

---

## 📡 Exemples de Commandes - Registre 343 (0x0157)

### 🔹 Format TinyBMS (Commande 0x09 - Read Individual)

**Trame complète à envoyer (en hexadécimal):**
```
AA 09 02 57 01 3F 8C
```

**Détail octet par octet:**
```
┌──────┬──────────────────────────────────┐
│ Octet│ Valeur │ Description           │
├──────┼────────┼───────────────────────┤
│  0   │  AA    │ Préambule TinyBMS     │
│  1   │  09    │ Cmd: Read Individual  │
│  2   │  02    │ Payload Length: 2     │
│  3   │  57    │ Adresse LSB (0x0157)  │
│  4   │  01    │ Adresse MSB (0x0157)  │
│  5   │  3F    │ CRC-16 LSB            │
│  6   │  8C    │ CRC-16 MSB            │
└──────┴────────┴───────────────────────┘
```

**Calcul du CRC-16:**
- **Données pour CRC:** `AA 09 02 57 01` (5 premiers octets)
- **Algorithme:** CRC-16 Modbus (polynôme 0xA001)
- **Valeur initiale:** 0xFFFF
- **Résultat:** 0x8C3F
  - LSB = 0x3F
  - MSB = 0x8C

**Réponse attendue (9 octets):**
```
AA 09 04 57 01 [DATA_LSB] [DATA_MSB] [CRC_LSB] [CRC_MSB]
```

**Exemple de réponse réelle:**
```
AA 09 04 57 01 2C 01 XX XX
```
- DATA = 0x012C = 300 (exemple de valeur)

---

### 🔹 Format Modbus RTU Pur (Commande 0x03)

**⚠️ Note:** Cette commande n'est pas encore implémentée dans le firmware actuel, mais elle est définie dans le protocole TinyBMS Rev D.

**Trame Modbus standard (si implémenté):**
```
AA 03 04 57 01 00 01 [CRC_LSB] [CRC_MSB]
```

**Détail:**
```
┌──────┬────────┬───────────────────────┐
│ Octet│ Valeur │ Description           │
├──────┼────────┼───────────────────────┤
│  0   │  AA    │ Préambule TinyBMS     │
│  1   │  03    │ Cmd: Modbus Read (0x03)│
│  2   │  04    │ Payload Length: 4     │
│  3   │  57    │ Start Addr LSB        │
│  4   │  01    │ Start Addr MSB        │
│  5   │  00    │ Quantity LSB (1 reg)  │
│  6   │  01    │ Quantity MSB          │
│  7   │  ??    │ CRC LSB               │
│  8   │  ??    │ CRC MSB               │
└──────┴────────┴───────────────────────┘
```

---

## 🐍 Script Python de Test

Voici un script Python simple pour tester la communication:

```python
#!/usr/bin/env python3
"""
Test de communication TinyBMS - Lecture du registre 343 (0x0157)
"""

import serial
import time

def crc16_modbus(data):
    """Calcule le CRC-16 Modbus"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def build_read_frame(address):
    """Construit une trame de lecture TinyBMS (Cmd 0x09)"""
    frame = bytearray([
        0xAA,                    # Preamble
        0x09,                    # Command: Read Individual
        0x02,                    # Payload Length
        address & 0xFF,          # Address LSB
        (address >> 8) & 0xFF,   # Address MSB
    ])

    # Calcul et ajout du CRC
    crc = crc16_modbus(frame)
    frame.append(crc & 0xFF)         # CRC LSB
    frame.append((crc >> 8) & 0xFF)  # CRC MSB

    return frame

def read_register(ser, address):
    """Lit un registre TinyBMS"""
    # Construction de la trame
    frame = build_read_frame(address)

    print(f"\n📤 Envoi de la trame:")
    print(f"   Hex: {' '.join(f'{b:02X}' for b in frame)}")
    print(f"   Adresse: 0x{address:04X} ({address})")

    # Envoi
    ser.write(frame)

    # Attente de la réponse
    time.sleep(0.1)

    # Lecture de la réponse
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"\n📥 Réponse reçue ({len(response)} octets):")
        print(f"   Hex: {' '.join(f'{b:02X}' for b in response)}")

        # Vérification de la réponse
        if len(response) >= 9 and response[0] == 0xAA and response[1] == 0x09:
            # Extraction de la valeur (octets 5 et 6)
            value_lsb = response[5]
            value_msb = response[6]
            value = (value_msb << 8) | value_lsb

            print(f"\n✅ Valeur lue:")
            print(f"   Décimal: {value}")
            print(f"   Hexadécimal: 0x{value:04X}")
            print(f"   LSB: 0x{value_lsb:02X}, MSB: 0x{value_msb:02X}")

            return value
        else:
            print("❌ Réponse invalide ou incomplète")
            return None
    else:
        print("❌ Aucune réponse reçue (timeout)")
        return None

def main():
    """Programme principal"""
    # Configuration du port série
    PORT = '/dev/ttyUSB0'  # Changez selon votre système
    BAUDRATE = 115200

    print("=" * 60)
    print("  Test de Communication TinyBMS")
    print("=" * 60)
    print(f"Port: {PORT}")
    print(f"Baud Rate: {BAUDRATE}")

    try:
        # Ouverture du port série
        with serial.Serial(PORT, BAUDRATE, timeout=1) as ser:
            print("\n✅ Port série ouvert")

            # Attente de stabilisation
            time.sleep(0.5)

            # Vider le buffer de réception
            ser.reset_input_buffer()

            # Lecture du registre 343 (0x0157)
            print("\n" + "=" * 60)
            print("  Lecture du registre 343 (0x0157)")
            print("=" * 60)

            value = read_register(ser, 0x0157)

            # Autres exemples de registres
            print("\n" + "=" * 60)
            print("  Autres exemples de registres")
            print("=" * 60)

            # Registre 0x012C (Fully Charged Voltage)
            print("\n--- Registre 0x012C (Fully Charged Voltage) ---")
            read_register(ser, 0x012C)

            # Registre 0x0064 (Cell 1 Voltage - Read Only)
            print("\n--- Registre 0x0064 (Cell 1 Voltage) ---")
            read_register(ser, 0x0064)

    except serial.SerialException as e:
        print(f"\n❌ Erreur d'ouverture du port série: {e}")
    except KeyboardInterrupt:
        print("\n\n⚠️  Interruption par l'utilisateur")
    finally:
        print("\n" + "=" * 60)
        print("  Fin du programme")
        print("=" * 60)

if __name__ == "__main__":
    main()
```

**Utilisation du script:**
```bash
# Rendre le script exécutable
chmod +x test_tinybms.py

# Exécuter
python3 test_tinybms.py
```

---

## 🔧 Test avec CoolTerm (Manuel)

1. **Ouvrir CoolTerm**
2. **Options → Serial Port:**
   - Port: `/dev/ttyUSB0` (ou votre port)
   - Baudrate: 115200
   - Data Bits: 8
   - Parity: None
   - Stop Bits: 1

3. **Options → Terminal:**
   - Cocher "Hex Display"
   - Line Mode: Off

4. **Connection → Send String:**
   - Taper: `AA 09 02 57 01 3F 8C`
   - Format: Hex
   - Cliquer "Send"

5. **Observer la réponse** dans le terminal hexadécimal

---

## 📋 Registres TinyBMS Courants

Voici quelques registres utiles pour tester:

| Adresse | Nom                    | Type      | Unité | Exemple Trame       |
|---------|------------------------|-----------|-------|---------------------|
| 0x0064  | Cell 1 Voltage         | Read Only | mV    | `AA 09 02 64 00 ...`|
| 0x012C  | Fully Charged Voltage  | RW        | mV    | `AA 09 02 2C 01 ...`|
| 0x012D  | Fully Charged Delay    | RW        | sec   | `AA 09 02 2D 01 ...`|
| 0x0157  | Current Offset         | RW        | mA    | `AA 09 02 57 01 ...`|
| 0x015A  | Protection Config      | RW        | bits  | `AA 09 02 5A 01 ...`|

---

## 🧮 Calculateur CRC en ligne

Si vous voulez calculer le CRC manuellement:
- https://www.lammertbies.nl/comm/info/crc-calculation
- Sélectionner "CRC-16 (Modbus)"
- Polynomial: 0xA001
- Initial value: 0xFFFF

**Exemple:**
```
Input: AA 09 02 57 01
CRC: 0x8C3F
  → LSB: 0x3F
  → MSB: 0x8C
```

---

## 🐛 Dépannage

### Pas de réponse
- ✅ Vérifier le câblage RS485 (A/B)
- ✅ Vérifier le port série (bon port?)
- ✅ Vérifier la baudrate (115200)
- ✅ Vérifier que TinyBMS est alimenté

### Réponse invalide
- ✅ Vérifier le CRC de la trame envoyée
- ✅ Vérifier l'ordre des octets (LSB/MSB)
- ✅ Vérifier que l'adresse existe

### CRC Error
- ✅ Recalculer le CRC avec l'outil en ligne
- ✅ Vérifier l'ordre LSB/MSB du CRC
- ✅ Vérifier que tous les octets sont inclus dans le calcul

---

## 📚 Références

- **Documentation TinyBMS:** `docs/tinybms_commands_reference.md`
- **Protocole détaillé:** `docs/rapport_UART_protocol.md`
- **Implémentation C:** `components/tinybms_client/tinybms_protocol.c`
- **Implémentation JS:** `Exemple/mac-local/src/serial.js`

---

## 🎯 Résumé Rapide

**Pour lire le registre 343 (0x0157):**

1. **Trame à envoyer:**
   ```
   AA 09 02 57 01 3F 8C
   ```

2. **Réponse attendue:**
   ```
   AA 09 04 57 01 [DATA_LSB] [DATA_MSB] [CRC_LSB] [CRC_MSB]
   ```

3. **Outils recommandés:**
   - QModMaster (GUI)
   - Script Python ci-dessus
   - CoolTerm (manuel)

**Bonne chance avec vos tests!** 🚀
