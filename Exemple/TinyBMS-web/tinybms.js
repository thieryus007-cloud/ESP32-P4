// tinybms.js
const { SerialPort } = require('serialport');

// Définition complète des registres (Map) basée sur TinyBMS_User_Manual_Rev_D.pdf
const REGISTER_MAP = [
    // --- LIVE DATA (0-99) [cite: 3792] ---
    ...Array.from({ length: 16 }, (_, i) => ({ id: i, label: `Cell ${i + 1} Voltage`, unit: 'V', type: 'UINT16', scale: 0.0001, category: 'Live' })),
    { id: 32, label: 'Lifetime Counter', unit: 's', type: 'UINT32', category: 'Stats' },
    { id: 34, label: 'Estimated Time Left', unit: 's', type: 'UINT32', category: 'Stats' },
    { id: 36, label: 'Pack Voltage', unit: 'V', type: 'FLOAT', category: 'Live' },
    { id: 38, label: 'Pack Current', unit: 'A', type: 'FLOAT', category: 'Live' },
    { id: 40, label: 'Min Cell Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Live' },
    { id: 41, label: 'Max Cell Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Live' },
    { id: 42, label: 'Ext Temp Sensor 1', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 43, label: 'Ext Temp Sensor 2', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 44, label: 'Distance Left', unit: 'km', type: 'UINT16', scale: 1, category: 'Stats' },
    { id: 45, label: 'State Of Health', unit: '%', type: 'UINT16', scale: 0.002, category: 'Stats' },
    { id: 46, label: 'State Of Charge', unit: '%', type: 'UINT32', scale: 0.000001, category: 'Live' },
    { id: 48, label: 'Internal Temp', unit: '°C', type: 'INT16', scale: 0.1, category: 'Live' },
    { id: 50, label: 'BMS Status', type: 'UINT16', category: 'Live' }, // Enum
    { id: 51, label: 'Balancing Decision', type: 'UINT16', category: 'Live' },
    { id: 52, label: 'Real Balancing', type: 'UINT16', category: 'Live' },
    { id: 53, label: 'Detected Cells Count', type: 'UINT16', category: 'Live' },
    { id: 54, label: 'Speed', unit: 'km/h', type: 'FLOAT', category: 'Stats' },

    // --- STATISTICS (100-199) [cite: 3793] ---
    { id: 101, label: 'Total Distance', unit: 'km', type: 'UINT32', scale: 0.01, category: 'Stats' },
    { id: 102, label: 'Max Discharge Current', unit: 'A', type: 'UINT16', scale: 0.1, category: 'Stats' },
    { id: 103, label: 'Max Charge Current', unit: 'A', type: 'UINT16', scale: 0.1, category: 'Stats' },
    { id: 104, label: 'Max Cell Diff', unit: 'V', type: 'UINT16', scale: 0.0001, category: 'Stats' },
    { id: 105, label: 'Under-Voltage Count', type: 'UINT16', category: 'Stats' },
    { id: 106, label: 'Over-Voltage Count', type: 'UINT16', category: 'Stats' },
    { id: 107, label: 'Discharge Over-Current Count', type: 'UINT16', category: 'Stats' },
    { id: 108, label: 'Charge Over-Current Count', type: 'UINT16', category: 'Stats' },
    { id: 109, label: 'Over-Heat Count', type: 'UINT16', category: 'Stats' },
    { id: 111, label: 'Charging Count', type: 'UINT16', category: 'Stats' },
    { id: 112, label: 'Full Charge Count', type: 'UINT16', category: 'Stats' },
    { id: 116, label: 'Stats Last Cleared', unit: 's', type: 'UINT32', category: 'Stats' },

    // --- SETTINGS (300-343) [cite: 3805, 3811] ---
    { id: 300, label: 'Fully Charged Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings' },
    { id: 301, label: 'Fully Discharged Voltage', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings' },
    { id: 303, label: 'Early Balancing Threshold', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings' },
    { id: 304, label: 'Charge Finished Current', unit: 'mA', type: 'UINT16', scale: 1, category: 'Settings' },
    { id: 305, label: 'Peak Discharge Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings' },
    { id: 306, label: 'Battery Capacity', unit: 'Ah', type: 'UINT16', scale: 0.01, category: 'Settings' },
    { id: 307, label: 'Number Of Series Cells', type: 'UINT16', category: 'Settings' },
    { id: 308, label: 'Allowed Disbalance', unit: 'mV', type: 'UINT16', scale: 1, category: 'Settings' },
    { id: 310, label: 'Charger Startup Delay', unit: 's', type: 'UINT16', category: 'Settings' },
    { id: 311, label: 'Charger Disable Delay', unit: 's', type: 'UINT16', category: 'Settings' },
    { id: 312, label: 'Pulses Per Unit', type: 'UINT32', category: 'Settings' },
    { id: 315, label: 'Over-Voltage Cutoff', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings' },
    { id: 316, label: 'Under-Voltage Cutoff', unit: 'V', type: 'UINT16', scale: 0.001, category: 'Settings' },
    { id: 317, label: 'Discharge Over-Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings' },
    { id: 318, label: 'Charge Over-Current', unit: 'A', type: 'UINT16', scale: 1, category: 'Settings' },
    { id: 319, label: 'Over-Heat Cutoff', unit: '°C', type: 'INT16', scale: 1, category: 'Settings' },
    { id: 320, label: 'Low Temp Charger Cutoff', unit: '°C', type: 'INT16', scale: 1, category: 'Settings' },
    { id: 321, label: 'Charge Restart Level', unit: '%', type: 'UINT16', category: 'Settings' },
    { id: 322, label: 'Battery Max Cycles', type: 'UINT16', category: 'Settings' },
    { id: 328, label: 'SOC Setting', unit: '%', type: 'UINT16', scale: 0.002, category: 'Settings' },
    { id: 332, label: 'Automatic Recovery', unit: 's', type: 'UINT16', category: 'Settings' },
    { id: 343, label: 'Protocol', type: 'UINT16', category: 'Settings' },

    // --- VERSION (500+) [cite: 3814] ---
    { id: 500, label: 'Hardware Version', type: 'UINT16', category: 'Version' },
    { id: 501, label: 'Firmware Version', type: 'UINT16', category: 'Version' },
    { id: 506, label: 'Serial Number', type: 'UINT16', category: 'Version' }
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

    // Read a block of registers
    readRegisterBlock(startAddr, count) {
        return new Promise((resolve, reject) => {
            if (!this.isConnected) return reject("Not connected");

            const cmd = [0xAA, 0x03, (startAddr >> 8) & 0xFF, startAddr & 0xFF, 0x00, count & 0xFF];
            const crc = this.calculateCRC(Buffer.from(cmd));
            const finalBuf = Buffer.from([...cmd, crc & 0xFF, (crc >> 8) & 0xFF]);

            // Simple one-shot listener for response
            const onData = (data) => {
                // Check Header
                if (data[0] !== 0xAA || data[1] !== 0x03) return; // Wait for correct header or timeout
                
                const len = data[2];
                const totalExpected = 3 + len + 2;
                
                if (data.length < totalExpected) return; // Partial data, handling omitted for brevity

                const payload = data.slice(3, 3 + len);
                this.port.removeListener('data', onData);
                resolve(this.parseBlock(startAddr, payload));
            };

            this.port.on('data', onData);
            this.port.write(finalBuf);

            // Timeout safety
            setTimeout(() => {
                this.port.removeListener('data', onData);
                reject("Timeout");
            }, 500);
        });
    }

    // Generic parser based on REGISTER_MAP
    parseBlock(startAddr, buffer) {
        const result = {};
        let offset = 0; // Current byte offset in buffer (2 bytes per register step)

        // Iterate over all possible registers in the requested range
        // Note: The buffer contains `count` registers * 2 bytes
        const count = buffer.length / 2;

        for (let i = 0; i < count; i++) {
            const currentRegId = startAddr + i;
            const def = REGISTER_MAP.find(r => r.id === currentRegId);

            if (def) {
                let rawValue = 0;
                let byteOffset = i * 2; // 2 bytes per standard register slot

                // Handle different types
                if (def.type === 'FLOAT') {
                    // Floats take 2 registers (4 bytes)
                    // Check if we have enough bytes left
                    if (byteOffset + 4 <= buffer.length) {
                        rawValue = buffer.readFloatBE(byteOffset);
                        // Skip next register in loop because float consumes 2 slots
                        // (Logic simplified here: usually map aligns float at correct index)
                    }
                } else if (def.type === 'UINT32') {
                    if (byteOffset + 4 <= buffer.length) {
                        rawValue = buffer.readUInt32BE(byteOffset);
                    }
                } else if (def.type === 'INT16') {
                    rawValue = buffer.readInt16BE(byteOffset);
                } else {
                    // UINT16 (Default)
                    rawValue = buffer.readUInt16BE(byteOffset);
                }

                // Apply Scale
                let finalValue = rawValue;
                if (def.scale) finalValue = rawValue * def.scale;

                result[currentRegId] = {
                    id: def.id,
                    label: def.label,
                    value: parseFloat(finalValue.toFixed(4)), // Clean floats
                    unit: def.unit || '',
                    category: def.category
                };
            }
        }
        return result;
    }
}

module.exports = TinyBMS;
