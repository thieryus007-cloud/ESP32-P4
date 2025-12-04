const { SerialPort } = require('serialport');

// CARTE DES REGISTRES (Basée sur TinyBMS Communication Protocols Rev D)
// Cellules 1 à 16 (Reg 0-15)
const cellRegisters = Array.from({ length: 16 }, (_, i) => ({
    id: i, label: `Cell ${i + 1}`, unit: 'V', type: 'UINT16', scale: 0.0001, category: 'Live'
}));

const REGISTER_MAP = [
    // --- LIVE DATA (0-99) ---
    ...cellRegisters,
    { id: 36, label: 'Pack Voltage', unit: 'V', type: 'FLOAT', category: 'Live' },
    { id: 38, label: 'Pack Current', unit: 'A', type: 'FLOAT', category: 'Live' },
    { id: 40, label: 'Min Cell Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Live' },
    { id: 41, label: 'Max Cell Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Live' },
    { id: 42, label: 'Temp Sensor 1', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 43, label: 'Temp Sensor 2', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 45, label: 'State Of Health', unit: '%', type: 'UINT16', scale: 0.002, category: 'Stats' },
    { id: 46, label: 'State Of Charge', unit: '%', type: 'UINT32', scale: 0.000001, category: 'Live' },
    { id: 48, label: 'Internal Temp', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 50, label: 'BMS Status', type: 'UINT16', category: 'Live' },
    { id: 52, label: 'Real Balancing', type: 'UINT16', category: 'Live' },

    // --- STATISTICS (100-199) ---
    { id: 100, label: 'Total Distance', unit: 'km', type: 'UINT32', scale: 0.01, category: 'Stats' },
    { id: 106, label: 'Over-Voltage Count', type: 'UINT16', category: 'Stats' },
    { id: 105, label: 'Under-Voltage Count', type: 'UINT16', category: 'Stats' },
    { id: 111, label: 'Charging Count', type: 'UINT16', category: 'Stats' },
    { id: 112, label: 'Full Charge Count', type: 'UINT16', category: 'Stats' },

    // --- SETTINGS (300-343) - Organisés par Groupes pour l'affichage ---
    
    // Groupe 1: Battery
    { id: 300, label: 'Fully Charged Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings', group: 'battery' },
    { id: 301, label: 'Fully Discharged Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings', group: 'battery' },
    { id: 306, label: 'Battery Capacity', unit: 'Ah', type: 'UINT16', scale: 0.01, category: 'Settings', group: 'battery' },
    { id: 307, label: 'Series Cells Count', unit: '', type: 'UINT16', category: 'Settings', group: 'battery' },
    { id: 322, label: 'Max Cycles Count', unit: '', type: 'UINT16', category: 'Settings', group: 'battery' },
    { id: 328, label: 'Manual SOC Set', unit: '%', type: 'UINT16', scale: 0.002, category: 'Settings', group: 'battery' },

    // Groupe 2: Safety
    { id: 315, label: 'Over-Voltage Cutoff', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings', group: 'safety' },
    { id: 316, label: 'Under-Voltage Cutoff', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings', group: 'safety' },
    { id: 317, label: 'Discharge Over-Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings', group: 'safety' },
    { id: 318, label: 'Charge Over-Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings', group: 'safety' },
    { id: 305, label: 'Peak Discharge Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings', group: 'safety' },
    { id: 319, label: 'Over-Heat Cutoff', unit: '°C', type: 'INT16', scale: 1, category: 'Settings', group: 'safety' },
    { id: 320, label: 'Low Temp Charge Cutoff', unit: '°C', type: 'INT16', scale: 1, category: 'Settings', group: 'safety' },

    // Groupe 3: Balance
    { id: 303, label: 'Early Balancing Threshold', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings', group: 'balance' },
    { id: 304, label: 'Charge Finished Current', unit: 'mA', type: 'UINT16', scale: 1, category: 'Settings', group: 'balance' },
    { id: 308, label: 'Allowed Disbalance', unit: 'mV', type: 'UINT16', scale: 1, category: 'Settings', group: 'balance' },
    { id: 321, label: 'Charge Restart Level', unit: '%', type: 'UINT16', category: 'Settings', group: 'balance' },
    { id: 323, label: 'SOH Setting', unit: '%', type: 'UINT16', scale: 0.002, category: 'Settings', group: 'balance' },
    { id: 332, label: 'Automatic Recovery', unit: 's', type: 'UINT16', category: 'Settings', group: 'balance' },

    // Groupe 4: Hardware
    { id: 310, label: 'Charger Startup Delay', unit: 's', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 311, label: 'Charger Disable Delay', unit: 's', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 312, label: 'Pulses Per Unit', unit: '', type: 'UINT32', category: 'Settings', group: 'hardware' },
    { id: 329, label: 'Configuration Bits', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 330, label: 'Charger Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 331, label: 'Load Switch Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 333, label: 'Charger Switch Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 334, label: 'Ignition Input', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 335, label: 'Charger Detection Input', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 337, label: 'Precharge Pin', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 338, label: 'Precharge Duration', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 339, label: 'Temp Sensor Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 340, label: 'Operation Mode', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' }, // 0=Dual, 1=Single
    { id: 341, label: 'Single Port Switch Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 342, label: 'Broadcast Time', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 343, label: 'Protocol', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },

    // --- VERSION (500+) ---
    { id: 501, label: 'Firmware Version', type: 'UINT16', category: 'Version' }
];

class TinyBMS {
    constructor(path, baudRate = 115200) {
        this.path = path;
        this.baudRate = baudRate;
        this.port = null;
        this.isConnected = false;
    }

    async connect() {
        return new Promise((resolve, reject) => {
            this.port = new SerialPort({ path: this.path, baudRate: this.baudRate, autoOpen: false });
            this.port.open((err) => {
                if (err) return reject(err);
                this.isConnected = true;
                console.log(`Connected to TinyBMS on ${this.path}`);
                resolve();
            });
            this.port.on('error', (err) => {
                console.error('Serial Error:', err);
                this.isConnected = false;
            });
        });
    }

    /**
     * Configure le protocole de communication du TinyBMS
     * @param {number|string} protocolValue - 0/'MODBUS' pour MODBUS, 1/'ASCII' pour ASCII (défaut)
     * @returns {Promise<boolean>} true si la configuration a réussi
     */
    async setProtocol(protocolValue = 1) {
        if (!this.isConnected) {
            throw new Error("Cannot set protocol: not connected");
        }

        // Convertir string en number si nécessaire
        let numericValue = protocolValue;
        if (typeof protocolValue === 'string') {
            numericValue = protocolValue.toUpperCase() === 'ASCII' ? 1 : 0;
        }

        console.log(`Setting TinyBMS protocol to ${numericValue === 1 ? 'ASCII' : 'MODBUS'}...`);

        try {
            const success = await this.writeRegister(343, numericValue);
            if (success) {
                console.log(`Protocol successfully set to ${numericValue === 1 ? 'ASCII' : 'MODBUS'}`);
                // Attendre un peu pour que le BMS applique le changement
                await new Promise(resolve => setTimeout(resolve, 100));
            } else {
                console.warn('Protocol write command sent but no confirmation received');
            }
            return success;
        } catch (error) {
            console.error('Failed to set protocol:', error.message);
            throw error;
        }
    }

    disconnect() {
        return new Promise((resolve) => {
            if (this.port && this.port.isOpen) {
                this.port.close((err) => {
                    if (err) console.error('Error closing port:', err.message);
                    this.isConnected = false;
                    console.log('Disconnected from TinyBMS');
                    resolve();
                });
            } else {
                this.isConnected = false;
                resolve();
            }
        });
    }

    // Calcul CRC Modbus (Poly 0xA001)
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

    // Lecture d'un registre individuel (Fonction 0x09 - Read Individual Register)
    // Format commande: AA 09 02 AddrLSB AddrMSB CRC_LSB CRC_MSB (7 bytes)
    // Format réponse : AA 09 04 AddrEchoLSB AddrEchoMSB DataLSB DataMSB CRC_LSB CRC_MSB (9 bytes)
    // Note: La réponse contient l'adresse (echo) aux bytes 3-4, et les DONNÉES aux bytes 5-6
    // Configuration byte order pour TinyBMS:
    // - ADRESSES: Little Endian (LSB first, MSB second)
    // - DONNÉES: Big Endian (MSB first, LSB second) - confirmé par test_tinybms.py ligne 159
    // - CRC: Little Endian (LSB first, MSB second)
    readIndividualRegister(address) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected) return reject(new Error("Not connected"));

            // Vider le buffer série
            this.port.flush(async (err) => {
                if (err) console.warn('[TinyBMS] Flush error:', err.message);
                await new Promise(r => setTimeout(r, 100));

                // Commande 0x09 : AA 09 02 AddrLSB AddrMSB CRC_LSB CRC_MSB
                const cmd = [0xAA, 0x09, 0x02, address & 0xFF, (address >> 8) & 0xFF];
                const crc = this.calculateCRC(Buffer.from(cmd));
                const finalBuf = Buffer.from([...cmd, crc & 0xFF, (crc >> 8) & 0xFF]);

                let rxBuffer = Buffer.alloc(0);

                const searchForValidFrame = () => {
                    // Chercher trame de 9 bytes: AA 09 04 AddrLSB AddrMSB DataLSB DataMSB CRC_LSB CRC_MSB
                    for (let i = 0; i < rxBuffer.length - 9; i++) {
                        if (rxBuffer[i] === 0xAA && rxBuffer[i + 1] === 0x09 && rxBuffer[i + 2] === 0x04) {
                            const potentialFrame = rxBuffer.slice(i, i + 9);
                            const receivedCrc = (potentialFrame[8] << 8) | potentialFrame[7];
                            const calculatedCrc = this.calculateCRC(potentialFrame.slice(0, 7));

                            if (receivedCrc === calculatedCrc) {
                                console.log(`[TinyBMS] ✅ Valid register response at offset ${i}`);
                                return potentialFrame;
                            }
                        }
                    }
                    return null;
                };

                const onData = (chunk) => {
                    rxBuffer = Buffer.concat([rxBuffer, chunk]);
                    console.log(`[TinyBMS] Read reg ${address}: ${rxBuffer.length} bytes accumulated`);

                    // Chercher une trame valide si buffer > 20 bytes
                    if (rxBuffer.length > 20) {
                        const foundFrame = searchForValidFrame();
                        if (foundFrame) {
                            rxBuffer = foundFrame;
                        } else {
                            console.log(`[TinyBMS] ⏳ Waiting for valid frame...`);
                            return;
                        }
                    }

                    // Attendre au moins 9 bytes
                    if (rxBuffer.length < 9) return;

                    // Vérifier header AA 09 04
                    if (rxBuffer[0] !== 0xAA || rxBuffer[1] !== 0x09 || rxBuffer[2] !== 0x04) {
                        const foundFrame = searchForValidFrame();
                        if (foundFrame) {
                            rxBuffer = foundFrame;
                        } else {
                            console.log(`[TinyBMS] Invalid header, waiting...`);
                            return;
                        }
                    }

                    // Extraire les données (bytes 5 et 6) - Big Endian selon test_tinybms.py ligne 159
                    const valueLSB = rxBuffer[5];
                    const valueMSB = rxBuffer[6];
                    const value = (valueMSB << 8) | valueLSB;

                    this.port.removeListener('data', onData);
                    console.log(`[TinyBMS] ✅ Reg ${address} = ${value} (0x${value.toString(16).padStart(4, '0')}) [bytes: ${valueLSB.toString(16).padStart(2,'0')} ${valueMSB.toString(16).padStart(2,'0')}]`);
                    resolve(value);
                };

                this.port.on('data', onData);
                console.log(`[TinyBMS] Reading register ${address}, cmd: ${finalBuf.toString('hex')}`);
                this.port.write(finalBuf);

                setTimeout(() => {
                    this.port.removeListener('data', onData);
                    reject(new Error(`Timeout reading register ${address}`));
                }, 2000);
            });
        });
    }

    // Lecture des registres de configuration testés (34 registres)
    // Lit uniquement les registres qui ont été validés par le test
    async readConfigurationSettings() {
        if (!this.isConnected) throw new Error("Not connected");

        // Liste des registres de configuration (34 registres)
        const TESTED_REGISTERS = [
            300, 301, 303, 304, 305, 306, 307, 308, 310, 311,
            315, 316, 317, 318, 319, 320, 321, 322, 323, 328,
            329, 330, 331, 332, 333, 334, 335, 337, 338, 339,
            340, 341, 342, 343
        ];

        console.log(`[TinyBMS] Reading ${TESTED_REGISTERS.length} configuration registers...`);

        const result = {};
        let successCount = 0;
        let errorCount = 0;

        for (const regId of TESTED_REGISTERS) {
            const def = REGISTER_MAP.find(r => r.id === regId);
            if (!def) {
                console.warn(`[TinyBMS] Register ${regId} not found in REGISTER_MAP, skipping`);
                continue;
            }

            try {
                const rawValue = await this.readIndividualRegister(regId);

                // Appliquer le scale
                let finalValue = rawValue;
                if (def.scale) finalValue = rawValue * def.scale;

                result[regId] = {
                    id: def.id,
                    label: def.label,
                    value: parseFloat(finalValue.toFixed(4)),
                    unit: def.unit || '',
                    category: def.category,
                    group: def.group
                };

                successCount++;

                // Petit délai entre les lectures (réduit à 50ms pour l'interface)
                await new Promise(r => setTimeout(r, 50));
            } catch (error) {
                console.error(`[TinyBMS] Error reading register ${regId} (${def.label}):`, error.message);
                errorCount++;
                // Continuer avec les autres registres au lieu de tout arrêter
            }
        }

        console.log(`[TinyBMS] ✅ Configuration read complete: ${successCount} success, ${errorCount} errors`);
        return result;
    }

    // Lecture par bloc - lit les registres individuellement avec la commande 0x09
    // Cette approche est plus fiable que la commande 0x07 qui ne fonctionne pas correctement
    async readRegisterBlock(startAddr, count) {
        if (!this.isConnected) throw new Error("Not connected");

        console.log(`[TinyBMS] Reading ${count} registers starting at ${startAddr} using individual reads (0x09)`);

        // Créer un buffer pour stocker les valeurs lues (2 bytes par registre)
        const buffer = Buffer.alloc(count * 2);

        // Lire chaque registre individuellement
        for (let i = 0; i < count; i++) {
            const addr = startAddr + i;
            try {
                const value = await this.readIndividualRegister(addr);
                // Stocker en Little Endian
                buffer.writeUInt16LE(value, i * 2);

                // Petit délai entre les lectures pour ne pas saturer le BMS
                await new Promise(r => setTimeout(r, 50));
            } catch (error) {
                console.error(`[TinyBMS] Error reading register ${addr}:`, error.message);
                throw error;
            }
        }

        console.log(`[TinyBMS] ✅ Successfully read ${count} registers`);
        return this.parseBlock(startAddr, buffer);
    }

    // Ecriture (Fonction 0x0D - Write Individual Registers)
    // Format: AA 0D PL AddrLSB AddrMSB DataLSB DataMSB CRC_LSB CRC_MSB
    // Note: Adresse ET Données en Little Endian (confirmé par test_tinybms.py)
    writeRegister(regId, value) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected) return reject(new Error("Not connected"));

            const def = REGISTER_MAP.find(r => r.id === regId);
            if (!def) return reject(new Error("Unknown register ID"));

            // IMPORTANT: value est la valeur BRUTE à écrire dans le registre
            // L'interface web doit envoyer la valeur brute (ex: 320 pour 3.20 Ah)
            // On n'applique PAS de division par scale ici (contrairement à la lecture où on multiplie)
            let rawValue = Math.round(value);

            // Vider le buffer série AVANT l'écriture (comme test_tinybms.py)
            this.port.flush(async (err) => {
                if (err) {
                    console.warn('[TinyBMS] Flush error before write:', err.message);
                }

                // Attendre un peu après le flush
                await new Promise(r => setTimeout(r, 100));

                // Préparation données (2 octets) - Little Endian (LSB, MSB) comme test_tinybms.py
                const dataBytes = Buffer.alloc(2);
                if (def.type === 'INT16') dataBytes.writeInt16LE(rawValue);
                else dataBytes.writeUInt16LE(rawValue);

                // Header: AA 0D PL AddrLSB AddrMSB (Write Individual - comme test_tinybms.py)
                // PL = 0x04 (4 bytes: 2 pour l'adresse + 2 pour les données)
                const header = [0xAA, 0x0D, 0x04, regId & 0xFF, (regId >> 8) & 0xFF]; // Command 0x0D, Payload Length, Address LE
                const cmdNoCrc = Buffer.concat([Buffer.from(header), dataBytes]);
                const crc = this.calculateCRC(cmdNoCrc);
                const finalBuf = Buffer.concat([cmdNoCrc, Buffer.from([crc & 0xFF, (crc >> 8) & 0xFF])]);

                let rxBuffer = Buffer.alloc(0);

                const onData = (chunk) => {
                    console.log(`[TinyBMS] Write response chunk: ${chunk.length} bytes`);

                    // Accumuler les données reçues
                    rxBuffer = Buffer.concat([rxBuffer, chunk]);

                    // Fonction helper pour chercher une trame ACK/NACK valide
                    const searchForAckNack = () => {
                        for (let i = 0; i < rxBuffer.length - 5; i++) {
                            if (rxBuffer[i] === 0xAA && (rxBuffer[i + 1] === 0x01 || rxBuffer[i + 1] === 0x00)) {
                                const potentialFrame = rxBuffer.slice(i, i + 5);
                                // Vérifier le CRC
                                const receivedCrc = (potentialFrame[4] << 8) | potentialFrame[3];
                                const calculatedCrc = this.calculateCRC(potentialFrame.slice(0, 3));

                                if (receivedCrc === calculatedCrc) {
                                    console.log(`[TinyBMS] ✅ Valid ACK/NACK found at offset ${i}`);
                                    return potentialFrame;
                                }
                            }
                        }
                        return null;
                    };

                    // Si buffer > 20 bytes (ACK est 5 bytes), probablement du debug ASCII mélangé
                    if (rxBuffer.length > 20) {
                        console.log(`[TinyBMS] Large write buffer (${rxBuffer.length} bytes) - searching for ACK/NACK...`);
                        const foundFrame = searchForAckNack();

                        if (foundFrame) {
                            rxBuffer = foundFrame;
                        } else {
                            console.log(`[TinyBMS] ⏳ No complete ACK/NACK yet, waiting...`);
                            return;
                        }
                    }

                    // Attendre au moins 5 bytes (taille minimale réponse ACK/NACK)
                    if (rxBuffer.length < 5) {
                        return;
                    }

                    // Vérification Header AA
                    if (rxBuffer[0] !== 0xAA) {
                        // Header invalide - probablement du debug ASCII
                        console.log(`[TinyBMS] Invalid ACK/NACK header - searching for valid frame...`);
                        const foundFrame = searchForAckNack();
                        if (foundFrame) {
                            rxBuffer = foundFrame;
                            console.log(`[TinyBMS] ACK/NACK recovered from ASCII debug pollution`);
                        } else {
                            console.log(`[TinyBMS] No valid ACK/NACK found yet, waiting...`);
                            return;
                        }
                    }

                    console.log(`[TinyBMS] Write response: ${rxBuffer.toString('hex')}`);

                    // Réponse pour commande 0x0D: AA 01 0D (ACK) ou AA 00 0D ERROR (NACK)
                    if (rxBuffer[0] === 0xAA) {
                        this.port.removeListener('data', onData);

                        if (rxBuffer[1] === 0x01) {
                            // ACK - Vérifier que c'est bien pour la commande 0x0D
                            if (rxBuffer[2] === 0x0D) {
                                console.log(`[TinyBMS] ✅ Write ACK received for register ${regId}`);
                                resolve(true);
                            } else {
                                console.warn(`[TinyBMS] ACK received but for wrong command: 0x${rxBuffer[2].toString(16)}`);
                                resolve(true); // Accepter quand même
                            }
                        } else if (rxBuffer[1] === 0x00) {
                            // NACK - Le code d'erreur est à l'index 3
                            const errorCode = rxBuffer[3] || 0xFF;
                            const errorMessages = {
                                0x00: "Command error",
                                0x01: "CRC error",
                                0x02: "Invalid register address",
                                0x03: "Read-only register",
                                0x04: "Value out of range"
                            };
                            const errorMsg = errorMessages[errorCode] || "Unknown error";
                            console.error(`[TinyBMS] ❌ Write NACK for register ${regId}, error: ${errorMsg} (code: 0x${errorCode.toString(16)})`);
                            reject(new Error(`Write failed: ${errorMsg}`));
                        } else {
                            reject(new Error(`Invalid write response: ${rxBuffer.toString('hex')}`));
                        }
                    } else {
                        this.port.removeListener('data', onData);
                        reject(new Error(`Invalid write response header: ${rxBuffer.toString('hex')}`));
                    }
                };

                this.port.on('data', onData);
                console.log(`[TinyBMS] Sending write command: reg=${regId}, value=${rawValue}, bytes=${finalBuf.toString('hex')}`);
                this.port.write(finalBuf);

                setTimeout(() => {
                    this.port.removeListener('data', onData);
                    resolve(false);
                }, 800); // Augmenté de 800ms à 2000ms
            });
        });
    }

    // Décodage générique selon la map
    parseBlock(startAddr, buffer) {
        const result = {};
        const count = buffer.length / 2; // 2 octets par registre

        for (let i = 0; i < count; i++) {
            const currentRegId = startAddr + i;
            const def = REGISTER_MAP.find(r => r.id === currentRegId);

            if (def) {
                let rawValue = 0;
                let byteOffset = i * 2;

                // DEBUG: Log les octets bruts pour le registre 306 (Battery Capacity)
                if (currentRegId === 306) {
                    const byte0 = buffer[byteOffset];
                    const byte1 = buffer[byteOffset + 1];
                    console.log(`[DEBUG] Reg 306 Battery Capacity - Raw bytes: [${byte0.toString(16).padStart(2, '0')}, ${byte1.toString(16).padStart(2, '0')}]`);
                    console.log(`[DEBUG] Reg 306 - If read as BE: ${buffer.readUInt16BE(byteOffset)} (0x${buffer.readUInt16BE(byteOffset).toString(16)})`);
                    console.log(`[DEBUG] Reg 306 - If read as LE: ${buffer.readUInt16LE(byteOffset)} (0x${buffer.readUInt16LE(byteOffset).toString(16)})`);
                }

                if (def.type === 'FLOAT') {
                    if (byteOffset + 4 <= buffer.length) rawValue = buffer.readFloatLE(byteOffset);
                } else if (def.type === 'UINT32') {
                    if (byteOffset + 4 <= buffer.length) rawValue = buffer.readUInt32LE(byteOffset);
                } else if (def.type === 'INT16') {
                    rawValue = buffer.readInt16LE(byteOffset);
                } else {
                    rawValue = buffer.readUInt16LE(byteOffset);
                }

                let finalValue = rawValue;
                if (def.scale) finalValue = rawValue * def.scale;

                // DEBUG: Log les valeurs pour le registre 306
                if (currentRegId === 306) {
                    console.log(`[DEBUG] Reg 306 - rawValue (LE): ${rawValue}, finalValue (after scale ${def.scale}): ${finalValue}`);
                }

                // On nettoie les flottants (ex: 3.90000001 -> 3.9)
                result[currentRegId] = {
                    id: def.id,
                    label: def.label,
                    value: parseFloat(finalValue.toFixed(4)),
                    unit: def.unit || '',
                    category: def.category,
                    group: def.group // Essentiel pour l'affichage par onglets
                };
            }
        }
        return result;
    }
}

module.exports = TinyBMS;
module.exports.REGISTER_MAP = REGISTER_MAP;
