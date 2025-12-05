# Comparaison Protocole TinyBMS: ESP32-P4 vs Interface Web

Date: 2025-12-05
Auteur: Analyse technique du protocole de communication TinyBMS

## Résumé Exécutif

✅ **Le byte order et le parsing sont CORRECTS dans l'ESP32-P4**
⚠️ **Une amélioration recommandée: flush du buffer UART avant chaque lecture**

---

## 1. Byte Order (Ordre des octets)

### Commande 0x09 - Read Individual Register

**Format de réponse**: `AA 09 04 AddrLSB AddrMSB DataLSB DataMSB CRC_LSB CRC_MSB`

### Interface Web (tinybms.js ligne 344-347)
```javascript
// Extraire les données (bytes 5 et 6)
const valueLSB = rxBuffer[5];  // Byte 5: LSB
const valueMSB = rxBuffer[6];  // Byte 6: MSB
const value = (valueMSB << 8) | valueLSB;  // Reconstruction Little Endian
```

### ESP32-P4 (tinybms_protocol.cpp ligne 364)
```cpp
// Extract value (little-endian) from Byte6-7
*value = frame[5] | (frame[6] << 8);  // LSB | (MSB << 8)
```

**Verdict**: ✅ **IDENTIQUE** - Les deux implémentations lisent en Little Endian

---

## 2. Construction des trames

### Adresse en Little Endian

**Interface Web** (tinybms.js ligne 292):
```javascript
const cmd = [0xAA, 0x09, 0x02, address & 0xFF, (address >> 8) & 0xFF];
```

**ESP32-P4** (tinybms_protocol.cpp ligne 54-55):
```cpp
frame[3] = address & 0xFF;           // Address low byte
frame[4] = (address >> 8) & 0xFF;    // Address high byte
```

**Verdict**: ✅ **IDENTIQUE** - Adresse encodée en Little Endian

### CRC en Little Endian

**Interface Web** (tinybms.js ligne 293-294):
```javascript
const crc = this.calculateCRC(Buffer.from(cmd));
const finalBuf = Buffer.from([...cmd, crc & 0xFF, (crc >> 8) & 0xFF]);
```

**ESP32-P4** (tinybms_protocol.cpp ligne 58-60):
```cpp
uint16_t crc = tinybms_crc16(frame, 5);
frame[5] = crc & 0xFF;           // CRC low byte
frame[6] = (crc >> 8) & 0xFF;    // CRC high byte
```

**Verdict**: ✅ **IDENTIQUE** - CRC encodé en Little Endian

---

## 3. Recherche de trame valide

### Interface Web (tinybms.js ligne 298-313)
```javascript
const searchForValidFrame = () => {
    // Chercher trame de 9 bytes: AA 09 04 ...
    for (let i = 0; i < rxBuffer.length - 9; i++) {
        if (rxBuffer[i] === 0xAA && rxBuffer[i + 1] === 0x09 && rxBuffer[i + 2] === 0x04) {
            const potentialFrame = rxBuffer.slice(i, i + 9);
            const receivedCrc = (potentialFrame[8] << 8) | potentialFrame[7];
            const calculatedCrc = this.calculateCRC(potentialFrame.slice(0, 7));
            if (receivedCrc === calculatedCrc) {
                return potentialFrame;  // Trame valide trouvée
            }
        }
    }
    return null;
};
```

### ESP32-P4 (tinybms_protocol.cpp ligne 276-318)
```cpp
esp_err_t tinybms_extract_frame(const uint8_t *buffer, size_t buffer_len, ...) {
    // Search for preamble
    const uint8_t *preamble_pos = NULL;
    for (size_t i = 0; i < buffer_len; i++) {
        if (buffer[i] == TINYBMS_PREAMBLE) {
            preamble_pos = &buffer[i];
            break;
        }
    }

    // ... validation de longueur ...

    // Verify CRC
    uint16_t expected_crc = preamble_pos[total_frame_len - 2] |
                           (preamble_pos[total_frame_len - 1] << 8);
    uint16_t computed_crc = tinybms_crc16(preamble_pos, total_frame_len - 2);

    if (expected_crc != computed_crc) {
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}
```

**Verdict**: ✅ **ÉQUIVALENT** - Les deux cherchent le preamble et valident le CRC

---

## 4. ⚠️ Différence importante: Flush du buffer UART

### Interface Web (tinybms.js ligne 287-289)
```javascript
// Vider le buffer série AVANT chaque lecture
this.port.flush(async (err) => {
    if (err) console.warn('[TinyBMS] Flush error:', err.message);
    await new Promise(r => setTimeout(r, 100));  // Attente 100ms après flush

    // ... puis envoi de la commande ...
});
```

### ESP32-P4 (tinybms_client.cpp ligne 359-380)
```cpp
static esp_err_t read_register_internal(uint16_t address, uint16_t *value) {
    // PAS de flush avant envoi ❌

    // Build request frame
    esp_err_t ret = tinybms_build_read_frame(tx_frame, address);

    // Send request (sans flush préalable)
    int written = uart_write_bytes(TINYBMS_UART_NUM, tx_frame, TINYBMS_READ_FRAME_LEN);

    // Flush seulement en cas d'erreur (lignes 409-410, 442-443)
}
```

**Verdict**: ⚠️ **DIFFÉRENT** - L'ESP32-P4 ne flush pas le buffer avant chaque lecture

---

## 5. Calcul CRC

### Interface Web (tinybms.js ligne 257-272)
```javascript
calculateCRC(buffer) {
    let crc = 0xFFFF;
    for (let pos = 0; pos < buffer.length; pos++) {
        crc ^= buffer[pos];
        for (let i = 8; i !== 0; i--) {
            if ((crc & 0x0001) !== 0) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
```

### ESP32-P4 (tinybms_protocol.cpp ligne 17-34)
```cpp
uint16_t tinybms_crc16(const uint8_t *buffer, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= buffer[i];

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = ((crc >> 1) ^ 0xA001) & 0xFFFF;
            } else {
                crc = (crc >> 1) & 0xFFFF;
            }
        }
    }

    return crc & 0xFFFF;
}
```

**Verdict**: ✅ **IDENTIQUE** - Algorithme CRC16 Modbus (polynôme 0xA001)

---

## 6. Recommandations

### 🟢 Points forts de l'implémentation ESP32-P4

1. ✅ **Byte order correct** pour adresses, données et CRC
2. ✅ **Recherche robuste** de trames valides dans le buffer
3. ✅ **Validation CRC** systématique
4. ✅ **Gestion des erreurs UART** (overflow, buffer full)
5. ✅ **Timeout configurables** et retry logic

### 🟡 Amélioration recommandée

**Ajouter un flush du buffer UART avant chaque lecture** pour éviter la pollution du buffer par des données résiduelles:

```cpp
static esp_err_t read_register_internal(uint16_t address, uint16_t *value) {
    // ⚠️ AMÉLIORATION: Flush avant lecture
    uart_flush_input(TINYBMS_UART_NUM);
    vTaskDelay(pdMS_TO_TICKS(50));  // Délai court après flush

    // Build and send request
    esp_err_t ret = tinybms_build_read_frame(tx_frame, address);
    // ... reste du code ...
}
```

**Justification**:
- L'interface web fait ce flush et fonctionne de manière fiable
- Évite les problèmes de trames mélangées ou de debug ASCII pollué
- Coût minimal: ~50ms par lecture (déjà limité par le délai inter-registres de 50ms du poller)

### Impact sur les performances

Avec le poller qui lit 29 registres:
- **Sans flush**: ~1.5s par cycle (29 × 50ms)
- **Avec flush**: ~2.9s par cycle (29 × 100ms)
- **Impact**: +1.4s par cycle de 2s → acceptable

---

## 7. Conclusion

### ✅ Protocole correct

Le protocole de communication ESP32-P4 est **fondamentalement correct** et conforme à la spécification TinyBMS Rev D. Les corrections de byte order de l'interface web sont **déjà présentes** dans l'ESP32-P4.

### ⚠️ Amélioration mineure recommandée

L'ajout d'un flush avant chaque lecture améliorerait la robustesse, particulièrement dans des environnements avec du debug ASCII activé sur le TinyBMS.

### 🎯 Priorités

1. **Priorité HAUTE**: Tester le système actuel (très probable qu'il fonctionne)
2. **Priorité MOYENNE**: Si des problèmes de lecture apparaissent, ajouter le flush
3. **Priorité BASSE**: Optimisation fine des délais et timeouts

---

## Références

- TinyBMS Communication Protocols Revision D (2025-07-04)
- Interface Web: `Exemple/TinyBMS-web/tinybms.js`
- ESP32-P4: `components/tinybms_client/tinybms_protocol.cpp`
- ESP32-P4: `components/tinybms_client/tinybms_client.cpp`
