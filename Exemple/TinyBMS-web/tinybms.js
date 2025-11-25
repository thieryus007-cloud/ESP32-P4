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
    { id: 101, label: 'Total Distance', unit: 'km', type: 'UINT32', scale: 0.01, category: 'Stats' },
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
    { id: 332, label: 'Automatic Recovery', unit: 's', type: 'UINT16', category: 'Settings', group: 'balance' },

    // Groupe 4: Hardware
    { id: 310, label: 'Charger Startup Delay', unit: 's', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 311, label: 'Charger Disable Delay', unit: 's', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 312, label: 'Pulses Per Unit', unit: '', type: 'UINT32', category: 'Settings', group: 'hardware' },
    { id: 330, label: 'Charger Type', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' },
    { id: 340, label: 'Operation Mode', unit: '', type: 'UINT16', category: 'Settings', group: 'hardware' }, // 0=Dual, 1=Single
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

    // Lecture par bloc (Fonction 0x03)
    // Note: Address uses Big Endian (MSB, LSB) per TinyBMS documentation Rev D
    readRegisterBlock(startAddr, count) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected) return reject("Not connected");

            const cmd = [0xAA, 0x03, (startAddr >> 8) & 0xFF, startAddr & 0xFF, 0x00, count & 0xFF]; // Address MSB, LSB (Big Endian)
            const crc = this.calculateCRC(Buffer.from(cmd));
            const finalBuf = Buffer.from([...cmd, crc & 0xFF, (crc >> 8) & 0xFF]);

            const onData = (data) => {
                // Vérification Header AA 03
                if (data[0] !== 0xAA || data[1] !== 0x03) return; 
                const len = data[2];
                if (data.length < 3 + len + 2) return; // Attendre trame complète

                const payload = data.slice(3, 3 + len);
                this.port.removeListener('data', onData);
                resolve(this.parseBlock(startAddr, payload));
            };

            this.port.on('data', onData);
            this.port.write(finalBuf);

            setTimeout(() => {
                this.port.removeListener('data', onData);
                reject("Timeout Read");
            }, 800);
        });
    }

    // Ecriture (Fonction 0x10 - Write Multiple Registers)
    // Utilisé ici pour écrire 1 seul registre à la fois par sécurité
    writeRegister(regId, value) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected) return reject("Not connected");

            const def = REGISTER_MAP.find(r => r.id === regId);
            if (!def) return reject("Unknown register ID");

            let rawValue = value;
            if (def.scale) rawValue = Math.round(value / def.scale);

            // Préparation données (2 octets) - Big Endian for MODBUS
            const dataBytes = Buffer.alloc(2);
            if (def.type === 'INT16') dataBytes.writeInt16BE(rawValue);
            else dataBytes.writeUInt16BE(rawValue);

            // Header: AA 10 AddrMSB AddrLSB 00 01 02 (Address uses Big Endian per TinyBMS documentation Rev D)
            const header = [0xAA, 0x10, (regId >> 8) & 0xFF, regId & 0xFF, 0x00, 0x01, 0x02]; // Address MSB, LSB (Big Endian)
            const cmdNoCrc = Buffer.concat([Buffer.from(header), dataBytes]);
            const crc = this.calculateCRC(cmdNoCrc);
            const finalBuf = Buffer.concat([cmdNoCrc, Buffer.from([crc & 0xFF, (crc >> 8) & 0xFF])]);

            const onData = (data) => {
                // Réponse: AA 10 ...
                if (data[0] === 0xAA && data[1] === 0x10) {
                    this.port.removeListener('data', onData);
                    resolve(true);
                }
            };

            this.port.on('data', onData);
            this.port.write(finalBuf);
            
            setTimeout(() => {
                this.port.removeListener('data', onData);
                resolve(false); 
            }, 800);
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

                if (def.type === 'FLOAT') {
                    if (byteOffset + 4 <= buffer.length) rawValue = buffer.readFloatBE(byteOffset);
                } else if (def.type === 'UINT32') {
                    if (byteOffset + 4 <= buffer.length) rawValue = buffer.readUInt32BE(byteOffset);
                } else if (def.type === 'INT16') {
                    rawValue = buffer.readInt16BE(byteOffset);
                } else {
                    rawValue = buffer.readUInt16BE(byteOffset);
                }

                let finalValue = rawValue;
                if (def.scale) finalValue = rawValue * def.scale;

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
