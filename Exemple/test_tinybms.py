#!/usr/bin/env python3
"""
Test de communication TinyBMS - Lecture et écriture de registres
Usage: python3 test_tinybms.py [port]
Example: python3 test_tinybms.py /dev/ttyUSB0
"""

import serial
import time
import sys

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

def build_write_frame(address, value):
    """Construit une trame d'écriture TinyBMS (Cmd 0x0D)"""
    frame = bytearray([
        0xAA,                    # Preamble
        0x0D,                    # Command: Write Individual
        0x04,                    # Payload Length
        address & 0xFF,          # Address LSB
        (address >> 8) & 0xFF,   # Address MSB
        value & 0xFF,            # Data LSB
        (value >> 8) & 0xFF,     # Data MSB
    ])

    # Calcul et ajout du CRC
    crc = crc16_modbus(frame)
    frame.append(crc & 0xFF)         # CRC LSB
    frame.append((crc >> 8) & 0xFF)  # CRC MSB

    return frame

def read_register(ser, address, reg_name=""):
    """Lit un registre TinyBMS"""
    # Construction de la trame
    frame = build_read_frame(address)

    print(f"\n📤 Envoi de la trame de lecture:")
    if reg_name:
        print(f"   Registre: {reg_name}")
    print(f"   Adresse: 0x{address:04X} ({address})")
    print(f"   Trame: {' '.join(f'{b:02X}' for b in frame)}")

    # Vider le buffer
    ser.reset_input_buffer()

    # Envoi
    ser.write(frame)

    # Attente de la réponse
    time.sleep(0.15)

    # Lecture de la réponse
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"\n📥 Réponse reçue ({len(response)} octets):")
        print(f"   Hex: {' '.join(f'{b:02X}' for b in response)}")

        # Vérification de la réponse
        if len(response) >= 9 and response[0] == 0xAA and response[1] == 0x09:
            # Vérification du CRC
            received_crc = (response[-1] << 8) | response[-2]
            calculated_crc = crc16_modbus(response[:-2])

            if received_crc != calculated_crc:
                print(f"⚠️  CRC invalide! (reçu: 0x{received_crc:04X}, calculé: 0x{calculated_crc:04X})")
                return None

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
            if len(response) >= 3 and response[0] == 0xAA and response[1] == 0x00:
                error_code = response[2] if len(response) > 2 else 0xFF
                print(f"   NACK reçu - Code erreur: 0x{error_code:02X}")
            return None
    else:
        print("❌ Aucune réponse reçue (timeout)")
        return None

def write_register(ser, address, value, reg_name=""):
    """Écrit un registre TinyBMS"""
    # Construction de la trame
    frame = build_write_frame(address, value)

    print(f"\n📤 Envoi de la trame d'écriture:")
    if reg_name:
        print(f"   Registre: {reg_name}")
    print(f"   Adresse: 0x{address:04X} ({address})")
    print(f"   Valeur: {value} (0x{value:04X})")
    print(f"   Trame: {' '.join(f'{b:02X}' for b in frame)}")

    # Vider le buffer
    ser.reset_input_buffer()

    # Envoi
    ser.write(frame)

    # Attente de la réponse
    time.sleep(0.15)

    # Lecture de la réponse (ACK/NACK)
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"\n📥 Réponse reçue ({len(response)} octets):")
        print(f"   Hex: {' '.join(f'{b:02X}' for b in response)}")

        # Vérification ACK/NACK
        if len(response) >= 3 and response[0] == 0xAA:
            if response[1] == 0x01:  # ACK
                print("✅ ACK reçu - Écriture réussie")
                return True
            elif response[1] == 0x00:  # NACK
                error_code = response[3] if len(response) > 3 else 0xFF
                print(f"❌ NACK reçu - Code erreur: 0x{error_code:02X}")
                error_messages = {
                    0x01: "Invalid register address",
                    0x02: "Read-only register",
                    0x03: "Value out of range",
                    0x04: "CRC error",
                    0xFF: "Unknown error"
                }
                print(f"   Erreur: {error_messages.get(error_code, 'Unknown')}")
                return False
        else:
            print("❌ Réponse invalide")
            return False
    else:
        print("❌ Aucune réponse reçue (timeout)")
        return False

def interactive_mode(ser):
    """Mode interactif pour lire/écrire des registres"""
    print("\n" + "=" * 60)
    print("  MODE INTERACTIF")
    print("=" * 60)
    print("\nCommandes disponibles:")
    print("  r <addr>       - Lire un registre (ex: r 0x0157)")
    print("  w <addr> <val> - Écrire un registre (ex: w 0x012C 4200)")
    print("  q              - Quitter")
    print("\nExemples de registres:")
    print("  0x012C - Fully Charged Voltage (mV)")
    print("  0x0157 - Current Offset (mA)")
    print("  0x0064 - Cell 1 Voltage (Read Only)")

    while True:
        try:
            cmd = input("\n> ").strip().lower()

            if cmd == 'q':
                break

            parts = cmd.split()
            if len(parts) < 2:
                print("Commande invalide. Utilisez 'r <addr>' ou 'w <addr> <val>'")
                continue

            # Parse l'adresse
            try:
                addr = int(parts[1], 0)  # Auto-detect hex/dec
            except ValueError:
                print("Adresse invalide")
                continue

            if parts[0] == 'r':
                read_register(ser, addr)
            elif parts[0] == 'w':
                if len(parts) < 3:
                    print("Valeur manquante. Utilisez 'w <addr> <val>'")
                    continue
                try:
                    val = int(parts[2], 0)  # Auto-detect hex/dec
                except ValueError:
                    print("Valeur invalide")
                    continue
                if write_register(ser, addr, val):
                    # Vérification après écriture
                    print("\nVérification de l'écriture...")
                    time.sleep(0.2)
                    read_register(ser, addr)
            else:
                print("Commande inconnue")

        except KeyboardInterrupt:
            print("\n")
            break

def main():
    """Programme principal"""
    # Configuration du port série
    if len(sys.argv) > 1:
        PORT = sys.argv[1]
    else:
        # Auto-détection
        import glob
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') + glob.glob('/dev/cu.usb*')
        if ports:
            PORT = ports[0]
            print(f"ℹ️  Port auto-détecté: {PORT}")
        else:
            PORT = '/dev/ttyUSB0'
            print(f"⚠️  Aucun port détecté, utilisation de: {PORT}")

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

            # Tests automatiques
            print("\n" + "=" * 60)
            print("  TESTS AUTOMATIQUES")
            print("=" * 60)

            # Registre 343 (0x0157)
            print("\n--- Test 1: Registre 0x0157 (343 décimal) ---")
            read_register(ser, 0x0157, "Current Offset")

            time.sleep(0.3)

            # Registre 0x012C (Fully Charged Voltage)
            print("\n--- Test 2: Registre 0x012C (Fully Charged Voltage) ---")
            read_register(ser, 0x012C, "Fully Charged Voltage")

            time.sleep(0.3)

            # Registre 0x0064 (Cell 1 Voltage - Read Only)
            print("\n--- Test 3: Registre 0x0064 (Cell 1 Voltage) ---")
            read_register(ser, 0x0064, "Cell 1 Voltage")

            # Mode interactif
            try:
                interactive_mode(ser)
            except KeyboardInterrupt:
                print("\n")

    except serial.SerialException as e:
        print(f"\n❌ Erreur d'ouverture du port série: {e}")
        print(f"\nConseils:")
        print(f"  - Vérifiez que le port {PORT} existe")
        print(f"  - Vérifiez que vous avez les permissions (ajoutez-vous au groupe dialout)")
        print(f"  - Essayez: sudo usermod -a -G dialout $USER")
        return 1
    except KeyboardInterrupt:
        print("\n\n⚠️  Interruption par l'utilisateur")
    finally:
        print("\n" + "=" * 60)
        print("  Fin du programme")
        print("=" * 60)

    return 0

if __name__ == "__main__":
    sys.exit(main())
