// tinybms.js
const { SerialPort } = require('serialport');
const { ByteLength } = require('@serialport/parser-byte-length');

class TinyBMS {
    constructor(path, baudRate = 115200) {
        this.path = path;
        this.baudRate = baudRate;
        this.port = null;
        this.isConnected = false;
        this.parser = null;
        this.pendingRequests = [];
    }

    async connect() {
        return new Promise((resolve, reject) => {
            this.port = new SerialPort({ path: this.path, baudRate: this.baudRate, autoOpen: false });
            
            this.port.open((err) => {
                if (err) return reject(err);
                this.isConnected = true;
                console.log(`Connected to TinyBMS on ${this.path}`);
                
                // TinyBMS responses usually have specific headers, 
                // but for simplicity we handle raw data chunks here
                this.port.on('data', (data) => this.handleData(data));
                resolve();
            });

            this.port.on('error', (err) => {
                console.error('Serial Error:', err);
                this.isConnected = false;
            });
        });
    }

    // CRC16-MODBUS calculation (Polynomial 0xA001)
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

    // Build Read Command (Live Data Block)
    // Based on manual: AA 03 ADDR_H ADDR_L 00 COUNT CRC_L CRC_H
    readRegisters(startAddr, count) {
        return new Promise((resolve, reject) => {
            const buf = Buffer.alloc(8);
            buf[0] = 0xAA; // Start Byte
            buf[1] = 0x03; // Read Command
            buf.writeUInt16BE(startAddr, 2);
            buf[2] = 0x00; // Hardcoded 00 in manual often
            buf.writeUInt16BE(startAddr, 2); // Correct Modbus structure usually: AA 03 ADDR_H ADDR_L 00 CNT_L CRC...
            // Let's stick to standard Modbus over UART structure visible in docs:
            // Frame: AA 03 [AddrHi] [AddrLo] 00 [Count] [CrcLo] [CrcHi]
            
            // Re-implementing strictly based on common TinyBMS libs logic:
            const cmd = [0xAA, 0x03, (startAddr >> 8) & 0xFF, startAddr & 0xFF, 0x00, count & 0xFF];
            const crc = this.calculateCRC(Buffer.from(cmd));
            const finalBuf = Buffer.from([...cmd, crc & 0xFF, (crc >> 8) & 0xFF]);

            this.send(finalBuf, (response) => {
                // Parse response: AA 03 LEN [DATA...] CRC
                if (response[0] !== 0xAA || response[1] !== 0x03) return reject('Invalid Response Header');
                const len = response[2];
                const data = response.slice(3, 3 + len);
                // Validate CRC here ideally
                resolve(data);
            });
        });
    }

    send(buffer, callback) {
        if (!this.isConnected) return;
        // Clear previous listener for simplicity in this single-threaded polling example
        this.port.removeAllListeners('data'); 
        
        this.port.write(buffer);
        
        // Wait for response
        this.port.once('data', (data) => {
            callback(data);
        });
    }

    // Parse Live Data Block (0-56 registers typically)
    parseLiveData(data) {
        if (data.length < 30) return null; // Safety check
        
        // Mapping based on Register Map (Chapter 3)
        // Note: Data is Big Endian (UInt16BE)
        const getU16 = (idx) => data.readUInt16BE(idx * 2);
        const getI16 = (idx) => data.readInt16BE(idx * 2);
        const getU32 = (idx) => data.readUInt32BE(idx * 2);
        const getFloat = (idx) => data.readFloatBE(idx * 2); 

        // Values need scaling (e.g. 0.1mV -> V)
        const cells = [];
        for(let i=0; i<16; i++) {
            cells.push(getU16(i) * 0.0001); // Cells 1-16
        }

        return {
            cells: cells,
            packVoltage: getFloat(36), // Reg 36
            packCurrent: getFloat(38), // Reg 38
            minCellVoltage: getU16(40) * 0.001,
            maxCellVoltage: getU16(41) * 0.001,
            tempInternal: getI16(48) / 10,
            tempExt1: getI16(42) / 10,
            tempExt2: getI16(43) / 10,
            soc: getU32(46) / 1000000, // High precision SOC
            state: getU16(50) // 0x91 Charging, 0x92 Fully Charged, 0x93 Discharging, 0x97 Idle
        };
    }
}

module.exports = TinyBMS;
