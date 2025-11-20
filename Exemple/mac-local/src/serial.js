import { SerialPort } from 'serialport';

const DEFAULT_BAUDRATE = 115200;
const DEFAULT_TIMEOUT_MS = 750;

class Mutex {
  constructor() {
    this._current = Promise.resolve();
  }

  runExclusive(task) {
    const run = this._current.then(() => task());
    this._current = run.then(
      () => undefined,
      () => undefined,
    );
    return run;
  }
}

function crc16(buffer) {
  let crc = 0xffff;
  for (let i = 0; i < buffer.length; i += 1) {
    crc ^= buffer[i];
    for (let bit = 0; bit < 8; bit += 1) {
      if (crc & 0x0001) {
        crc = ((crc >> 1) ^ 0xa001) & 0xffff;
      } else {
        crc = (crc >> 1) & 0xffff;
      }
    }
  }
  return crc & 0xffff;
}

function buildReadFrame(address) {
  const frame = Buffer.alloc(7);
  frame[0] = 0xaa;              // Preamble
  frame[1] = 0x09;              // Command: Read Individual (Rev D)
  frame[2] = 0x02;              // Payload Length: 2 bytes (address)
  frame[3] = address & 0xff;    // Address LSB
  frame[4] = (address >> 8) & 0xff; // Address MSB
  const crc = crc16(frame.subarray(0, 5));
  frame[5] = crc & 0xff;        // CRC LSB
  frame[6] = (crc >> 8) & 0xff; // CRC MSB
  return frame;
}

function buildWriteFrame(address, rawValue) {
  const frame = Buffer.alloc(9);
  frame[0] = 0xaa;                  // Preamble
  frame[1] = 0x0d;                  // Command: Write Individual (Rev D)
  frame[2] = 0x04;                  // Payload Length: 4 bytes (addr + data)
  frame[3] = address & 0xff;        // Address LSB
  frame[4] = (address >> 8) & 0xff; // Address MSB
  frame[5] = rawValue & 0xff;       // Data LSB
  frame[6] = (rawValue >> 8) & 0xff;// Data MSB
  const crc = crc16(frame.subarray(0, 7));
  frame[7] = crc & 0xff;            // CRC LSB
  frame[8] = (crc >> 8) & 0xff;     // CRC MSB
  return frame;
}

/**
 * Build a reset command frame (Command 0x02 with option 0x05)
 * Conforme à la section 1.1.8 du protocole TinyBMS Rev D
 *
 * Frame format (6 bytes):
 * [0xAA] [0x02] [0x01] [0x05] [CRC_LSB] [CRC_MSB]
 */
function buildRestartFrame() {
  const frame = Buffer.alloc(6);
  frame[0] = 0xaa;  // Preamble
  frame[1] = 0x02;  // Command: Reset
  frame[2] = 0x01;  // Payload Length: 1 byte
  frame[3] = 0x05;  // Option: Reset BMS
  const crc = crc16(frame.subarray(0, 4));
  frame[4] = crc & 0xff;        // CRC LSB
  frame[5] = (crc >> 8) & 0xff; // CRC MSB
  return frame;
}

/**
 * Build a read block frame (Command 0x07 - Proprietary)
 *
 * Frame format (8 bytes):
 * [0xAA] [0x07] [0x03] [Start:LSB] [Start:MSB] [Count] [CRC_LSB] [CRC_MSB]
 */
function buildReadBlockFrame(startAddress, count) {
  const frame = Buffer.alloc(8);
  frame[0] = 0xaa;                      // Preamble
  frame[1] = 0x07;                      // Command: Read Block
  frame[2] = 0x03;                      // Payload Length: 3 bytes
  frame[3] = startAddress & 0xff;       // Start address LSB
  frame[4] = (startAddress >> 8) & 0xff;// Start address MSB
  frame[5] = count & 0xff;              // Register count
  const crc = crc16(frame.subarray(0, 6));
  frame[6] = crc & 0xff;                // CRC LSB
  frame[7] = (crc >> 8) & 0xff;         // CRC MSB
  return frame;
}

/**
 * Build a write block frame (Command 0x0B - Proprietary)
 *
 * Frame format (variable):
 * [0xAA] [0x0B] [PL] [Start:LSB] [Start:MSB] [Count] [Data...] [CRC_LSB] [CRC_MSB]
 */
function buildWriteBlockFrame(startAddress, values) {
  const count = values.length;
  const payloadLength = 3 + (count * 2);
  const frameLength = 3 + payloadLength + 2;
  const frame = Buffer.alloc(frameLength);

  frame[0] = 0xaa;                      // Preamble
  frame[1] = 0x0b;                      // Command: Write Block
  frame[2] = payloadLength;             // Payload Length
  frame[3] = startAddress & 0xff;       // Start address LSB
  frame[4] = (startAddress >> 8) & 0xff;// Start address MSB
  frame[5] = count & 0xff;              // Register count

  // Write register values (little-endian)
  for (let i = 0; i < count; i += 1) {
    const offset = 6 + (i * 2);
    frame[offset] = values[i] & 0xff;         // Data LSB
    frame[offset + 1] = (values[i] >> 8) & 0xff; // Data MSB
  }

  const crcOffset = 6 + (count * 2);
  const crc = crc16(frame.subarray(0, crcOffset));
  frame[crcOffset] = crc & 0xff;        // CRC LSB
  frame[crcOffset + 1] = (crc >> 8) & 0xff; // CRC MSB

  return frame;
}

/**
 * Build a MODBUS read frame (Command 0x03)
 *
 * Frame format (9 bytes):
 * [0xAA] [0x03] [0x04] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [CRC_LSB] [CRC_MSB]
 */
function buildModbusReadFrame(startAddress, quantity) {
  const frame = Buffer.alloc(9);
  frame[0] = 0xaa;                      // Preamble
  frame[1] = 0x03;                      // Command: MODBUS Read
  frame[2] = 0x04;                      // Payload Length: 4 bytes
  frame[3] = startAddress & 0xff;       // Start address LSB
  frame[4] = (startAddress >> 8) & 0xff;// Start address MSB
  frame[5] = quantity & 0xff;           // Quantity LSB
  frame[6] = (quantity >> 8) & 0xff;    // Quantity MSB
  const crc = crc16(frame.subarray(0, 7));
  frame[7] = crc & 0xff;                // CRC LSB
  frame[8] = (crc >> 8) & 0xff;         // CRC MSB
  return frame;
}

/**
 * Build a MODBUS write frame (Command 0x10)
 *
 * Frame format (variable):
 * [0xAA] [0x10] [PL] [Start:LSB] [Start:MSB] [Qty:LSB] [Qty:MSB] [ByteCount] [Data...] [CRC_LSB] [CRC_MSB]
 */
function buildModbusWriteFrame(startAddress, values) {
  const quantity = values.length;
  const byteCount = quantity * 2;
  const payloadLength = 5 + byteCount;
  const frameLength = 3 + payloadLength + 2;
  const frame = Buffer.alloc(frameLength);

  frame[0] = 0xaa;                      // Preamble
  frame[1] = 0x10;                      // Command: MODBUS Write
  frame[2] = payloadLength;             // Payload Length
  frame[3] = startAddress & 0xff;       // Start address LSB
  frame[4] = (startAddress >> 8) & 0xff;// Start address MSB
  frame[5] = quantity & 0xff;           // Quantity LSB
  frame[6] = (quantity >> 8) & 0xff;    // Quantity MSB
  frame[7] = byteCount;                 // Byte count

  // Write register values (big-endian for MODBUS)
  for (let i = 0; i < quantity; i += 1) {
    const offset = 8 + (i * 2);
    frame[offset] = (values[i] >> 8) & 0xff;  // Data MSB (big-endian)
    frame[offset + 1] = values[i] & 0xff;     // Data LSB
  }

  const crcOffset = 8 + byteCount;
  const crc = crc16(frame.subarray(0, crcOffset));
  frame[crcOffset] = crc & 0xff;        // CRC LSB
  frame[crcOffset + 1] = (crc >> 8) & 0xff; // CRC MSB

  return frame;
}

function extractFrame(buffer) {
  if (!buffer || buffer.length === 0) {
    return { frame: null, buffer: Buffer.alloc(0) };
  }

  let working = buffer;
  const preambleIndex = working.indexOf(0xaa);
  if (preambleIndex === -1) {
    return { frame: null, buffer: Buffer.alloc(0) };
  }

  if (preambleIndex > 0) {
    working = working.slice(preambleIndex);
  }

  if (working.length < 5) {
    return { frame: null, buffer: working };
  }

  const payloadLength = working[2];
  const frameLength = 3 + payloadLength + 2;
  if (working.length < frameLength) {
    return { frame: null, buffer: working };
  }

  const frame = working.slice(0, frameLength);
  const expected = frame.readUInt16LE(frameLength - 2);
  const computed = crc16(frame.subarray(0, frameLength - 2));
  if (expected !== computed) {
    return extractFrame(working.slice(1));
  }

  return { frame, buffer: working.slice(frameLength) };
}

export async function listSerialPorts() {
  const ports = await SerialPort.list();
  return ports.map((port) => ({
    path: port.path,
    manufacturer: port.manufacturer || null,
    serialNumber: port.serialNumber || null,
    vendorId: port.vendorId || null,
    productId: port.productId || null,
  }));
}

export class TinyBmsSerial {
  constructor() {
    this.port = null;
    this._readBuffer = Buffer.alloc(0);
    this._pending = [];
    this._mutex = new Mutex();
    this._portInfo = null;
    this._onData = this._handleData.bind(this);
    this._onError = this._handleError.bind(this);
  }

  isOpen() {
    return Boolean(this.port && this.port.isOpen);
  }

  info() {
    if (!this.isOpen()) {
      return null;
    }
    return this._portInfo;
  }

  async open(path, { baudRate = DEFAULT_BAUDRATE } = {}) {
    if (!path) {
      throw new Error('Aucun port série fourni');
    }

    if (this.isOpen()) {
      await this.close();
    }

    const port = new SerialPort({
      path,
      baudRate,
      autoOpen: false,
    });

    await new Promise((resolve, reject) => {
      port.open((err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });

    port.on('data', this._onData);
    port.on('error', this._onError);

    this.port = port;
    this._readBuffer = Buffer.alloc(0);
    this._pending = [];
    this._portInfo = { path, baudRate };
    return this._portInfo;
  }

  async close() {
    if (!this.port) {
      return;
    }

    await new Promise((resolve) => {
      this.port.removeListener('data', this._onData);
      this.port.removeListener('error', this._onError);
      this.port.close(() => resolve());
    });

    this._rejectAll(new Error('Port série fermé'));
    this.port = null;
    this._portInfo = null;
    this._readBuffer = Buffer.alloc(0);
  }

  async readRegister(address, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(() => this._readRegisterLocked(address, timeoutMs));
  }

  async writeRegister(address, rawValue, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(() => this._writeRegisterLocked(address, rawValue, timeoutMs));
  }

  async restartTinyBms(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const frame = buildRestartFrame();
      // ACK/NACK format (Rev D): Byte2 = 0x01 (ACK) or 0x00 (NACK)
      const ackPromise = this._waitForFrame((received) => received[1] === 0x01 || received[1] === 0x00, timeoutMs);
      try {
        await this._writeFrame(frame);
      } catch (error) {
        if (typeof ackPromise.cancel === 'function') {
          ackPromise.cancel();
        }
        throw error;
      }
      const ack = await ackPromise;
      // NACK is Byte2 = 0x00 (not 0x81)
      if (ack[1] === 0x00) {
        const errorCode = ack.length > 3 ? ack[3] : 0;
        throw new Error(`TinyBMS a renvoyé un NACK pour la commande de redémarrage (code: 0x${errorCode.toString(16).padStart(2, '0')})`);
      }
      return true;
    });
  }

  async readBlock(startAddress, count, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = buildReadBlockFrame(startAddress, count);
      // Response format: [0xAA][0x07][PL][Start:LSB][Start:MSB][Data...][CRC:LSB][CRC:MSB]
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x07, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      if (frame.length < 8) {
        throw new Error('Réponse TinyBMS invalide pour read block');
      }

      const payloadLength = frame[2];
      const dataBytes = payloadLength - 2; // Minus start address (2 bytes)
      const registerCount = dataBytes / 2;
      const values = [];

      for (let i = 0; i < registerCount; i += 1) {
        const offset = 5 + (i * 2); // Skip header (3) + start addr (2)
        const value = frame[offset] | (frame[offset + 1] << 8);
        values.push(value);
      }

      return values;
    });
  }

  async writeBlock(startAddress, values, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const frame = buildWriteBlockFrame(startAddress, values);
      // ACK/NACK format: Byte2 = 0x01 (ACK) or 0x00 (NACK)
      const ackPromise = this._waitForFrame((received) => received[1] === 0x01 || received[1] === 0x00, timeoutMs);
      try {
        await this._writeFrame(frame);
      } catch (error) {
        if (typeof ackPromise.cancel === 'function') {
          ackPromise.cancel();
        }
        throw error;
      }
      const ack = await ackPromise;
      if (ack[1] === 0x00) {
        const errorCode = ack.length > 3 ? ack[3] : 0;
        throw new Error(`TinyBMS NACK on write block (code 0x${errorCode.toString(16).padStart(2, '0')})`);
      }
      return true;
    });
  }

  async modbusRead(startAddress, quantity, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = buildModbusReadFrame(startAddress, quantity);
      // Response format: [0xAA][0x03][PL][ByteCount][Data...][CRC:LSB][CRC:MSB]
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x03, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      if (frame.length < 6) {
        throw new Error('Réponse TinyBMS invalide pour MODBUS read');
      }

      const byteCount = frame[3];
      const registerCount = byteCount / 2;
      const values = [];

      for (let i = 0; i < registerCount; i += 1) {
        const offset = 4 + (i * 2); // Skip header (3) + byte count (1)
        const value = (frame[offset] << 8) | frame[offset + 1]; // Big-endian
        values.push(value);
      }

      return values;
    });
  }

  async modbusWrite(startAddress, values, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const frame = buildModbusWriteFrame(startAddress, values);
      // ACK/NACK format: Byte2 = 0x01 (ACK) or 0x00 (NACK)
      const ackPromise = this._waitForFrame((received) => received[1] === 0x01 || received[1] === 0x00, timeoutMs);
      try {
        await this._writeFrame(frame);
      } catch (error) {
        if (typeof ackPromise.cancel === 'function') {
          ackPromise.cancel();
        }
        throw error;
      }
      const ack = await ackPromise;
      if (ack[1] === 0x00) {
        const errorCode = ack.length > 3 ? ack[3] : 0;
        throw new Error(`TinyBMS NACK on MODBUS write (code 0x${errorCode.toString(16).padStart(2, '0')})`);
      }
      return true;
    });
  }

  /**
   * Build a simple command frame (no payload)
   * Format: [0xAA] [CMD] [0x00] [CRC:LSB] [CRC:MSB]
   */
  _buildSimpleCommandFrame(command) {
    const frame = Buffer.alloc(5);
    frame[0] = 0xaa;
    frame[1] = command;
    frame[2] = 0x00; // PL = 0
    const crc = crc16(frame.subarray(0, 3));
    frame[3] = crc & 0xff;
    frame[4] = (crc >> 8) & 0xff;
    return frame;
  }

  /**
   * Parse a simple uint16 response
   * Format: [0xAA] [CMD] [PL] [Data:LSB] [Data:MSB] [CRC:LSB] [CRC:MSB]
   */
  _parseSimpleUint16Response(frame) {
    if (frame.length < 7) {
      throw new Error('Simple response too short');
    }
    return frame[3] | (frame[4] << 8);
  }

  /**
   * Parse a simple int16 response (signed)
   */
  _parseSimpleInt16Response(frame) {
    const unsigned = this._parseSimpleUint16Response(frame);
    return unsigned > 0x7fff ? unsigned - 0x10000 : unsigned;
  }

  /**
   * Parse a multi-value response
   * Format: [0xAA] [CMD] [PL] [Data...] [CRC:LSB] [CRC:MSB]
   */
  _parseMultiValueResponse(frame) {
    if (frame.length < 5) {
      throw new Error('Multi-value response too short');
    }
    const payloadLength = frame[2];
    const valueCount = payloadLength / 2;
    const values = [];
    for (let i = 0; i < valueCount; i += 1) {
      const offset = 3 + (i * 2);
      values.push(frame[offset] | (frame[offset + 1] << 8));
    }
    return values;
  }

  async readNewestEvents(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x11);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x11, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseMultiValueResponse(frame);
    });
  }

  async readAllEvents(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x12);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x12, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseMultiValueResponse(frame);
    });
  }

  async readPackVoltage(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x14);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x14, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readPackCurrent(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x15);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x15, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleInt16Response(frame);
    });
  }

  async readMaxCellVoltage(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x16);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x16, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readMinCellVoltage(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x17);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x17, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readOnlineStatus(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x18);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x18, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readLifetimeCounter(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x19);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x19, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readEstimatedSoc(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1a);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1a, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseSimpleUint16Response(frame);
    });
  }

  async readTemperatures(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1b);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1b, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseMultiValueResponse(frame);
    });
  }

  async readCellVoltages(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1c);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1c, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseMultiValueResponse(frame);
    });
  }

  async readSettingsValues(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1d);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1d, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      return this._parseMultiValueResponse(frame);
    });
  }

  async readVersion(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1e);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1e, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      if (frame.length < 8) {
        throw new Error('Version response too short');
      }
      return {
        major: frame[3],
        minor: frame[4],
        patch: frame[5],
      };
    });
  }

  async readExtendedVersion(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x1f);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x1f, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      if (frame.length < 8) {
        throw new Error('Extended version response too short');
      }
      return {
        major: frame[3],
        minor: frame[4],
        patch: frame[5],
      };
    });
  }

  async readSpeedDistance(timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      await this._prepareTransaction();
      const request = this._buildSimpleCommandFrame(0x20);
      const responsePromise = this._waitForFrame((frame) => frame[1] === 0x20, timeoutMs);
      try {
        await this._writeFrame(request);
      } catch (error) {
        if (typeof responsePromise.cancel === 'function') {
          responsePromise.cancel();
        }
        throw error;
      }
      const frame = await responsePromise;
      const values = this._parseMultiValueResponse(frame);
      if (values.length < 2) {
        throw new Error('Speed/distance response too short');
      }
      return {
        speed: values[0],
        distance: values[1],
      };
    });
  }

  async readCatalogue(descriptors, timeoutMs = DEFAULT_TIMEOUT_MS) {
    return this._mutex.runExclusive(async () => {
      const results = [];
      for (const descriptor of descriptors) {
        const raw = await this._readRegisterLocked(descriptor.address, timeoutMs);
        results.push({ descriptor, raw });
      }
      return results;
    });
  }

  async _readRegisterLocked(address, timeoutMs) {
    await this._prepareTransaction();
    const request = buildReadFrame(address);
    // Response format (9 bytes): [0xAA][0x09][PL][Addr:LSB][Addr:MSB][Data:LSB][Data:MSB][CRC:LSB][CRC:MSB]
    const responsePromise = this._waitForFrame((frame) => frame[1] === 0x09, timeoutMs);
    try {
      await this._writeFrame(request);
    } catch (error) {
      if (typeof responsePromise.cancel === 'function') {
        responsePromise.cancel();
      }
      throw error;
    }
    const frame = await responsePromise;
    // Validate response: minimum 9 bytes, payload length should be 4
    if (frame.length < 9 || frame[2] !== 0x04) {
      throw new Error('Réponse TinyBMS invalide (expected 9 bytes with PL=0x04)');
    }
    // Data is at bytes 5-6 (not 3-4)
    const raw = frame[5] | (frame[6] << 8);
    return raw;
  }

  async _writeRegisterLocked(address, rawValue, timeoutMs) {
    await this._prepareTransaction();
    const frame = buildWriteFrame(address, rawValue);
    // ACK/NACK format (Rev D): Byte2 = 0x01 (ACK) or 0x00 (NACK)
    const ackPromise = this._waitForFrame((received) => received[1] === 0x01 || received[1] === 0x00, timeoutMs);
    try {
      await this._writeFrame(frame);
    } catch (error) {
      if (typeof ackPromise.cancel === 'function') {
        ackPromise.cancel();
      }
      throw error;
    }
    const ack = await ackPromise;
    // NACK is Byte2 = 0x00 (not 0x81)
    if (ack[1] === 0x00) {
      const errorCode = ack.length > 3 ? ack[3] : 0;
      throw new Error(`TinyBMS NACK (code 0x${errorCode.toString(16).padStart(2, '0')})`);
    }
    // Verify write by reading back the register
    const readback = await this._readRegisterLocked(address, timeoutMs);
    return readback;
  }

  async _prepareTransaction() {
    if (!this.isOpen()) {
      throw new Error('Port série non connecté');
    }
    this._readBuffer = Buffer.alloc(0);
    await new Promise((resolve, reject) => {
      this.port.flush((err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
  }

  async _writeFrame(frame) {
    if (!this.isOpen()) {
      throw new Error('Port série non connecté');
    }
    await new Promise((resolve, reject) => {
      this.port.write(frame, (err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
    await new Promise((resolve, reject) => {
      this.port.drain((err) => {
        if (err) {
          reject(err);
        } else {
          resolve();
        }
      });
    });
  }

  _waitForFrame(matcher, timeoutMs) {
    let pendingRef = null;
    const promise = new Promise((resolve, reject) => {
      const pending = {
        matcher,
        resolve: (frame) => {
          clearTimeout(pending.timeoutHandle);
          resolve(frame);
        },
        reject: (error) => {
          clearTimeout(pending.timeoutHandle);
          reject(error);
        },
        timeoutHandle: null,
      };

      pending.timeoutHandle = setTimeout(() => {
        this._removePending(pending);
        reject(new Error('Timeout de réponse TinyBMS'));
      }, timeoutMs);

      pendingRef = pending;
      this._pending.push(pending);
    });

    promise.cancel = () => {
      if (pendingRef) {
        clearTimeout(pendingRef.timeoutHandle);
        this._removePending(pendingRef);
        pendingRef = null;
      }
    };

    return promise;
  }

  _handleData(data) {
    this._readBuffer = Buffer.concat([this._readBuffer, data]);
    let parsing = true;
    while (parsing) {
      const { frame, buffer } = extractFrame(this._readBuffer);
      this._readBuffer = buffer;
      if (!frame) {
        parsing = false;
        break;
      }
      this._dispatchFrame(frame);
    }
  }

  _handleError(error) {
    this._rejectAll(error);
  }

  _dispatchFrame(frame) {
    if (this._pending.length === 0) {
      return;
    }
    for (let i = 0; i < this._pending.length; i += 1) {
      const entry = this._pending[i];
      let matches = false;
      try {
        matches = Boolean(entry.matcher(frame));
      } catch (error) {
        entry.reject(error);
        this._pending.splice(i, 1);
        return;
      }
      if (matches) {
        this._pending.splice(i, 1);
        entry.resolve(frame);
        return;
      }
    }
  }

  _removePending(pending) {
    const index = this._pending.indexOf(pending);
    if (index >= 0) {
      this._pending.splice(index, 1);
    }
  }

  _rejectAll(error) {
    while (this._pending.length > 0) {
      const pending = this._pending.shift();
      if (pending) {
        pending.reject(error);
      }
    }
  }
}

export default TinyBmsSerial;
