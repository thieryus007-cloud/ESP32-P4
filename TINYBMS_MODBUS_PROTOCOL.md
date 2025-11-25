# TinyBMS MODBUS Protocol Documentation

Cette documentation reflète le protocole officiel TinyBMS Communication Protocols Rev D (2025-07-04).

## 📋 Vue d'ensemble

Le TinyBMS utilise un protocole MODBUS personnalisé sur UART avec les caractéristiques suivantes :
- **Baud rate**: 9600 bps
- **Format**: 8 bits de données, 1 bit de stop, pas de parité
- **CRC**: MODBUS CRC-16 (Poly 0x8005, Reversed 0xA001)
- **Start byte**: `0xAA`

## 🔢 Ordre des octets (Byte Order)

### ⚠️ IMPORTANT : Convention d'ordre des octets

Le protocole TinyBMS utilise différentes conventions selon le type de données :

| Type de données | Convention | Ordre | Exemple (0x1234) |
|----------------|------------|-------|------------------|
| **Adresse (ADDR)** | Big Endian | MSB, LSB | `0x12, 0x34` |
| **Données (DATA)** | Big Endian | MSB, LSB | `0x12, 0x34` |
| **CRC** | Little Endian | LSB, MSB | `0x34, 0x12` |

### Explication

- **Big Endian** : Le byte le plus significatif (MSB) est envoyé en premier
- **Little Endian** : Le byte le moins significatif (LSB) est envoyé en premier

## 📖 Section 1.1.6 : Read Tiny BMS registers block

### Requête vers le BMS (Read Request)

```
┌─────────┬─────────┬──────────┬──────────┬─────────┬─────────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3  │  Byte 4  │  Byte 5 │  Byte 6 │  Byte 7 │  Byte 8 │
├─────────┼─────────┼──────────┼──────────┼─────────┼─────────┼─────────┼─────────┤
│   0xAA  │   0x03  │ ADDR:MSB │ ADDR:LSB │  0x00   │   RL    │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴──────────┴──────────┴─────────┴─────────┴─────────┴─────────┘
```

**Paramètres :**
- `ADDR` : Adresse du premier registre (Big Endian - MSB en premier)
- `RL` : Nombre de registres à lire (Max: 127 registres = 0x7F)
- `CRC` : CRC-16 MODBUS (Little Endian - LSB en premier)

### Réponse du BMS [OK]

```
┌─────────┬─────────┬─────────┬───────────┬───────────┬─────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3 │  Byte 4   │  Byte 5   │ ... │ Byte n  │ Byte n+1│
├─────────┼─────────┼─────────┼───────────┼───────────┼─────┼─────────┼─────────┤
│   0xAA  │   0x03  │   PL    │ DATA1:MSB │ DATA1:LSB │ ... │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴─────────┴───────────┴───────────┴─────┴─────────┴─────────┘
```

**Paramètres :**
- `PL` : Longueur du payload en bytes
- `DATAn` : Valeur du registre n (Big Endian - MSB en premier)
- `CRC` : CRC-16 MODBUS (Little Endian - LSB en premier)

### Réponse du BMS [ERROR]

```
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3 │  Byte 4 │  Byte 5 │  Byte 6 │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
│   0xAA  │   0x00  │  0x03   │  ERROR  │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
```

## 📝 Section 1.1.7 : Write Tiny BMS registers block

### Requête vers le BMS (Write Request)

```
┌─────────┬─────────┬──────────┬──────────┬─────────┬─────────┬─────────┬───────────┬───────────┬─────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3  │  Byte 4  │  Byte 5 │  Byte 6 │  Byte 7 │  Byte 8   │  Byte 9   │ ... │ Byte n  │ Byte n+1│
├─────────┼─────────┼──────────┼──────────┼─────────┼─────────┼─────────┼───────────┼───────────┼─────┼─────────┼─────────┤
│   0xAA  │   0x10  │ ADDR:MSB │ ADDR:LSB │  0x00   │   RL    │   PL    │ DATA1:MSB │ DATA1:LSB │ ... │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴──────────┴──────────┴─────────┴─────────┴─────────┴───────────┴───────────┴─────┴─────────┴─────────┘
```

**Paramètres :**
- `ADDR` : Adresse du premier registre (Big Endian - MSB en premier)
- `RL` : Nombre de registres à écrire (Max: 100 registres = 0x64)
- `PL` : Longueur du payload en bytes
- `DATAn` : Valeur du registre n à écrire (Big Endian - MSB en premier)
- `CRC` : CRC-16 MODBUS (Little Endian - LSB en premier)

### Réponse du BMS [OK]

```
┌─────────┬─────────┬──────────┬──────────┬─────────┬─────────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3  │  Byte 4  │  Byte 5 │  Byte 6 │  Byte 7 │  Byte 8 │
├─────────┼─────────┼──────────┼──────────┼─────────┼─────────┼─────────┼─────────┤
│   0xAA  │   0x10  │ ADDR:MSB │ ADDR:LSB │  0x00   │   RL    │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴──────────┴──────────┴─────────┴─────────┴─────────┴─────────┘
```

### Réponse du BMS [ERROR]

```
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│  Byte 1 │  Byte 2 │  Byte 3 │  Byte 4 │  Byte 5 │  Byte 6 │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
│   0xAA  │   0x00  │  0x10   │  ERROR  │ CRC:LSB │ CRC:MSB │
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
```

## 💻 Implémentation TypeScript/JavaScript

### Construction d'une commande Read

```typescript
function buildReadRegisterCommand(startAddr: number, count: number): Uint8Array {
    const buf = [
        0xAA,                    // Start byte
        0x03,                    // Command: Read
        (startAddr >> 8) & 0xFF, // Address MSB (Big Endian)
        startAddr & 0xFF,        // Address LSB
        0x00,                    // Reserved
        count & 0xFF             // Register count
    ];
    const crc = calculateCRC(buf);
    buf.push(crc & 0xFF);        // CRC LSB (Little Endian)
    buf.push((crc >> 8) & 0xFF); // CRC MSB
    return new Uint8Array(buf);
}
```

### Construction d'une commande Write

```typescript
function buildWriteRegisterCommand(addr: number, value: number): Uint8Array {
    const buf = [
        0xAA,                    // Start byte
        0x10,                    // Command: Write
        (addr >> 8) & 0xFF,      // Address MSB (Big Endian)
        addr & 0xFF,             // Address LSB
        0x00,                    // Reserved
        0x01,                    // Register count (1)
        0x02,                    // Payload length (2 bytes)
        (value >> 8) & 0xFF,     // Data MSB (Big Endian)
        value & 0xFF             // Data LSB
    ];
    const crc = calculateCRC(buf);
    buf.push(crc & 0xFF);        // CRC LSB (Little Endian)
    buf.push((crc >> 8) & 0xFF); // CRC MSB
    return new Uint8Array(buf);
}
```

### Calcul du CRC-16 MODBUS

```typescript
const CRC_TABLE = new Uint16Array([
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    // ... (voir implémentation complète dans les fichiers source)
]);

function calculateCRC(data: Uint8Array | number[]): number {
    let crc = 0xFFFF;
    for (let i = 0; i < data.length; i++) {
        const tmp = (data[i] ^ crc) & 0xFF;
        crc = (crc >> 8) ^ CRC_TABLE[tmp];
    }
    return crc;
}
```

### Parsing d'une réponse Read

```typescript
function parseReadResponse(buffer: Uint8Array): { data: number[], valid: boolean } {
    if (buffer[0] !== 0xAA || buffer[1] !== 0x03) {
        return { data: [], valid: false };
    }

    const payloadLen = buffer[2];
    const totalLen = 3 + payloadLen + 2; // Header + Payload + CRC

    // Vérifier le CRC (Little Endian)
    const receivedCrc = buffer[totalLen - 2] | (buffer[totalLen - 1] << 8);
    const frameData = buffer.slice(0, totalLen - 2);
    const calcCrc = calculateCRC(frameData);

    if (receivedCrc !== calcCrc) {
        return { data: [], valid: false };
    }

    // Extraire les données (Big Endian)
    const data: number[] = [];
    for (let i = 0; i < payloadLen; i += 2) {
        const msb = buffer[3 + i];
        const lsb = buffer[3 + i + 1];
        data.push((msb << 8) | lsb);
    }

    return { data, valid: true };
}
```

## 🔍 Points clés à retenir

1. ✅ **Adresses** : Toujours Big Endian (MSB, LSB)
2. ✅ **Données** : Toujours Big Endian (MSB, LSB)
3. ✅ **CRC** : Toujours Little Endian (LSB, MSB)
4. ⚠️ Le CRC est calculé sur **tous les bytes avant le CRC** (du start byte 0xAA jusqu'au dernier byte de données)
5. 📝 Le CRC utilise le polynôme MODBUS standard (0x8005 / 0xA001 reversed)

## 📚 Référence

**Document source** : TinyBMS Communication Protocols Rev D
**Date** : 2025-07-04
**Sections** : 1.1.6 (Read) et 1.1.7 (Write)

## 🔗 Fichiers d'implémentation

- `/Exemple/TinyBMS-web/docs/TinyBMS_service.ts` - Implémentation TypeScript
- `/Exemple/Gemini/TinyBMS_service.ts` - Implémentation Gemini
- `/Exemple/TinyBMS-web/tinybms.js` - Implémentation JavaScript

---

**Note** : Cette documentation a été créée pour clarifier l'ordre des octets dans le protocole MODBUS TinyBMS et éviter toute confusion entre Big Endian et Little Endian.
