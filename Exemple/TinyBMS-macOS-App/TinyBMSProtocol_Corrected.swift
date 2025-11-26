import Foundation

// ═══════════════════════════════════════════════════════════════════════
// TinyBMS Protocol Implementation
// Based on Enepaq Communication Protocols Rev D (2025-07-04)
// ═══════════════════════════════════════════════════════════════════════

/// Erreurs du protocole TinyBMS
enum TinyBMSError: Error, LocalizedError {
    case invalidResponse
    case nackReceived(cmd: UInt8, error: TinyBMSErrorCode)
    case crcMismatch(expected: UInt16, received: UInt16)
    case timeout
    case notConnected
    case invalidRegisterAddress
    case sleepModeWakeupRequired
    case unknownCommand
    
    var errorDescription: String? {
        switch self {
        case .invalidResponse:
            return "Invalid response from BMS"
        case .nackReceived(let cmd, let error):
            return "NACK received for command 0x\(String(format: "%02X", cmd)): \(error.description)"
        case .crcMismatch(let expected, let received):
            return "CRC mismatch: expected 0x\(String(format: "%04X", expected)), received 0x\(String(format: "%04X", received))"
        case .timeout:
            return "Communication timeout"
        case .notConnected:
            return "Not connected to BMS"
        case .invalidRegisterAddress:
            return "Invalid register address"
        case .sleepModeWakeupRequired:
            return "BMS is in sleep mode, wakeup required"
        case .unknownCommand:
            return "Unknown command"
        }
    }
}

/// Codes d'erreur TinyBMS
enum TinyBMSErrorCode: UInt8 {
    case cmdError = 0x00
    case crcError = 0x01
    case unknown = 0xFF
    
    var description: String {
        switch self {
        case .cmdError: return "Command Error"
        case .crcError: return "CRC Error"
        case .unknown: return "Unknown Error"
        }
    }
}

/// Protocole de communication TinyBMS
class TinyBMSProtocol {
    
    // ═══════════════════════════════════════════════════════════════════════
    // CRC Table from Protocol Documentation (Pages 11-12)
    // Polynomial: x^16 + x^15 + x^2 + 1 (0x8005)
    // ═══════════════════════════════════════════════════════════════════════
    
    private static let crcTable: [UInt16] = [
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    ]
    
    /// Calcul du CRC16 avec la table de lookup
    static func calculateCRC(_ data: Data) -> UInt16 {
        var crc: UInt16 = 0xFFFF
        for byte in data {
            let index = Int(UInt8(crc & 0xFF) ^ byte)
            crc = (crc >> 8) ^ crcTable[index]
        }
        return crc
    }
    
    /// Vérifie le CRC d'une réponse
    static func verifyCRC(_ data: Data) -> Bool {
        guard data.count >= 3 else { return false }
        let payloadLength = data.count - 2
        let payload = data.prefix(payloadLength)
        let expectedCRC = calculateCRC(Data(payload))
        let receivedCRC = UInt16(data[payloadLength]) | (UInt16(data[payloadLength + 1]) << 8)
        return expectedCRC == receivedCRC
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // MODBUS Commands (Chapter 1.1.6 & 1.1.7)
    // ═══════════════════════════════════════════════════════════════════════
    
    /// Créer une commande de lecture MODBUS (fonction 0x03)
    static func createModbusReadCommand(startAddress: UInt16, count: UInt8) -> Data {
        var command = Data()
        command.append(0xAA)                              // Header
        command.append(0x03)                              // Function: Read
        command.append(UInt8((startAddress >> 8) & 0xFF)) // Address MSB
        command.append(UInt8(startAddress & 0xFF))        // Address LSB
        command.append(0x00)                              // Count MSB (always 0)
        command.append(count)                             // Count LSB (max 0x7F)
        
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))                 // CRC LSB
        command.append(UInt8((crc >> 8) & 0xFF))          // CRC MSB
        
        return command
    }
    
    /// Créer une commande d'écriture MODBUS (fonction 0x10)
    static func createModbusWriteCommand(startAddress: UInt16, values: [UInt16]) -> Data {
        var command = Data()
        command.append(0xAA)                              // Header
        command.append(0x10)                              // Function: Write Multiple
        command.append(UInt8((startAddress >> 8) & 0xFF)) // Address MSB
        command.append(UInt8(startAddress & 0xFF))        // Address LSB
        command.append(0x00)                              // Register count MSB
        command.append(UInt8(values.count))               // Register count LSB
        command.append(UInt8(values.count * 2))           // Byte count
        
        // Values in Big Endian
        for value in values {
            command.append(UInt8((value >> 8) & 0xFF))    // Value MSB
            command.append(UInt8(value & 0xFF))           // Value LSB
        }
        
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        
        return command
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Proprietary Commands (Chapter 1.1.8 - 1.1.23)
    // ═══════════════════════════════════════════════════════════════════════
    
    /// Créer une commande simple (sans paramètres)
    private static func createSimpleCommand(_ cmd: UInt8) -> Data {
        var command = Data([0xAA, cmd])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    /// Reset BMS / Clear Events / Clear Statistics (0x02)
    static func createResetCommand(option: ResetOption) -> Data {
        var command = Data([0xAA, 0x02, option.rawValue])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    enum ResetOption: UInt8 {
        case clearEvents = 0x01
        case clearStatistics = 0x02
        case resetBMS = 0x05
    }
    
    /// Read Newest Events (0x11)
    static func createReadNewestEventsCommand() -> Data {
        return createSimpleCommand(0x11)
    }
    
    /// Read All Events (0x12)
    static func createReadAllEventsCommand() -> Data {
        return createSimpleCommand(0x12)
    }
    
    /// Read Pack Voltage (0x14) - Returns FLOAT
    static func createReadPackVoltageCommand() -> Data {
        return createSimpleCommand(0x14)
    }
    
    /// Read Pack Current (0x15) - Returns FLOAT
    static func createReadPackCurrentCommand() -> Data {
        return createSimpleCommand(0x15)
    }
    
    /// Read Max Cell Voltage (0x16) - Returns UINT16
    static func createReadMaxCellVoltageCommand() -> Data {
        return createSimpleCommand(0x16)
    }
    
    /// Read Min Cell Voltage (0x17) - Returns UINT16
    static func createReadMinCellVoltageCommand() -> Data {
        return createSimpleCommand(0x17)
    }
    
    /// Read Online Status (0x18) - Returns status code
    static func createReadOnlineStatusCommand() -> Data {
        return createSimpleCommand(0x18)
    }
    
    /// Read Lifetime Counter (0x19) - Returns UINT32
    static func createReadLifetimeCounterCommand() -> Data {
        return createSimpleCommand(0x19)
    }
    
    /// Read SOC (0x1A) - Returns UINT32
    static func createReadSOCCommand() -> Data {
        return createSimpleCommand(0x1A)
    }
    
    /// Read Temperatures (0x1B) - Returns 3 x INT16
    static func createReadTemperaturesCommand() -> Data {
        return createSimpleCommand(0x1B)
    }
    
    /// Read Cell Voltages (0x1C) - Returns N x UINT16
    static func createReadCellVoltagesCommand() -> Data {
        return createSimpleCommand(0x1C)
    }
    
    /// Read Settings (0x1D)
    static func createReadSettingsCommand(option: SettingsOption, count: UInt8) -> Data {
        var command = Data([0xAA, 0x1D, option.rawValue, 0x00, count])
        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF))
        command.append(UInt8((crc >> 8) & 0xFF))
        return command
    }
    
    enum SettingsOption: UInt8 {
        case minSettings = 0x01
        case maxSettings = 0x02
        case defaultSettings = 0x03
        case currentSettings = 0x04
    }
    
    /// Read Version (0x1E)
    static func createReadVersionCommand() -> Data {
        return createSimpleCommand(0x1E)
    }
    
    /// Read Extended Version (0x1F)
    static func createReadExtendedVersionCommand() -> Data {
        return createSimpleCommand(0x1F)
    }
    
    /// Read Speed/Distance/Time (0x20)
    static func createReadSpeedDistanceTimeCommand() -> Data {
        return createSimpleCommand(0x20)
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Response Parsing
    // ═══════════════════════════════════════════════════════════════════════
    
    /// Parse la réponse et vérifie NACK/CRC
    static func validateResponse(_ data: Data, expectedCmd: UInt8) throws {
        guard data.count >= 3 else {
            throw TinyBMSError.invalidResponse
        }
        
        // Check header
        guard data[0] == 0xAA else {
            throw TinyBMSError.invalidResponse
        }
        
        // Check for NACK response
        if data[1] == 0x00 {
            let errorCode = data.count > 3 ? TinyBMSErrorCode(rawValue: data[3]) ?? .unknown : .unknown
            throw TinyBMSError.nackReceived(cmd: data[2], error: errorCode)
        }
        
        // Verify CRC
        guard verifyCRC(data) else {
            let payloadLength = data.count - 2
            let expectedCRC = calculateCRC(data.prefix(payloadLength))
            let receivedCRC = UInt16(data[payloadLength]) | (UInt16(data[payloadLength + 1]) << 8)
            throw TinyBMSError.crcMismatch(expected: expectedCRC, received: receivedCRC)
        }
    }
    
    /// Parse MODBUS read response
    static func parseModbusReadResponse(_ data: Data, startAddress: UInt16) throws -> [Int: ParsedRegisterValue] {
        try validateResponse(data, expectedCmd: 0x03)
        
        guard data.count >= 4 else {
            throw TinyBMSError.invalidResponse
        }
        
        let payloadLength = Int(data[2])
        guard data.count >= 3 + payloadLength + 2 else {
            throw TinyBMSError.invalidResponse
        }
        
        let payload = data.subdata(in: 3..<(3 + payloadLength))
        return parsePayload(payload, startAddress: Int(startAddress))
    }
    
    /// Parse le payload des registres (Big Endian MODBUS)
    private static func parsePayload(_ buffer: Data, startAddress: Int) -> [Int: ParsedRegisterValue] {
        var result: [Int: ParsedRegisterValue] = [:]
        var byteOffset = 0
        var currentRegId = startAddress
        
        while byteOffset < buffer.count {
            guard let registerDef = RegisterMapComplete.register(for: currentRegId) else {
                // Skip unknown register
                byteOffset += 2
                currentRegId += 1
                continue
            }
            
            var parsedValue: ParsedRegisterValue?
            
            switch registerDef.type {
            case .float32:
                if byteOffset + 3 < buffer.count {
                    // MODBUS Big Endian: MSB first
                    let byte0 = buffer[byteOffset]     // MSB
                    let byte1 = buffer[byteOffset + 1]
                    let byte2 = buffer[byteOffset + 2]
                    let byte3 = buffer[byteOffset + 3] // LSB
                    
                    let bits = UInt32(byte0) << 24 | UInt32(byte1) << 16 | UInt32(byte2) << 8 | UInt32(byte3)
                    let floatValue = Float(bitPattern: bits)
                    
                    parsedValue = ParsedRegisterValue(
                        id: currentRegId,
                        rawValue: Double(floatValue),
                        scaledValue: Double(floatValue), // Float already in correct unit
                        register: registerDef
                    )
                    byteOffset += 4
                    currentRegId += 2 // FLOAT32 uses 2 registers
                }
                
            case .uint32:
                if byteOffset + 3 < buffer.count {
                    let byte0 = buffer[byteOffset]
                    let byte1 = buffer[byteOffset + 1]
                    let byte2 = buffer[byteOffset + 2]
                    let byte3 = buffer[byteOffset + 3]
                    
                    let value = UInt32(byte0) << 24 | UInt32(byte1) << 16 | UInt32(byte2) << 8 | UInt32(byte3)
                    let scaled = registerDef.scale != nil ? Double(value) * registerDef.scale! : Double(value)
                    
                    parsedValue = ParsedRegisterValue(
                        id: currentRegId,
                        rawValue: Double(value),
                        scaledValue: scaled,
                        register: registerDef
                    )
                    byteOffset += 4
                    currentRegId += 2
                }
                
            case .int16:
                if byteOffset + 1 < buffer.count {
                    let byte0 = buffer[byteOffset]
                    let byte1 = buffer[byteOffset + 1]
                    let rawValue = Int16(bitPattern: UInt16(byte0) << 8 | UInt16(byte1))
                    
                    // Check for special value (e.g., -32768 = not connected)
                    if let specialVal = registerDef.specialValue, Int(rawValue) == specialVal {
                        parsedValue = ParsedRegisterValue(
                            id: currentRegId,
                            rawValue: Double(rawValue),
                            scaledValue: Double(rawValue),
                            register: registerDef,
                            isSpecialValue: true
                        )
                    } else {
                        let scaled = registerDef.scale != nil ? Double(rawValue) * registerDef.scale! : Double(rawValue)
                        parsedValue = ParsedRegisterValue(
                            id: currentRegId,
                            rawValue: Double(rawValue),
                            scaledValue: scaled,
                            register: registerDef
                        )
                    }
                    byteOffset += 2
                    currentRegId += 1
                }
                
            case .uint16:
                if byteOffset + 1 < buffer.count {
                    let byte0 = buffer[byteOffset]
                    let byte1 = buffer[byteOffset + 1]
                    let rawValue = UInt16(byte0) << 8 | UInt16(byte1)
                    
                    let scaled = registerDef.scale != nil ? Double(rawValue) * registerDef.scale! : Double(rawValue)
                    
                    parsedValue = ParsedRegisterValue(
                        id: currentRegId,
                        rawValue: Double(rawValue),
                        scaledValue: scaled,
                        register: registerDef
                    )
                    byteOffset += 2
                    currentRegId += 1
                }
            }
            
            if let value = parsedValue {
                result[value.id] = value
            }
        }
        
        return result
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Specialized Response Parsers
    // ═══════════════════════════════════════════════════════════════════════
    
    /// Parse Pack Voltage response (0x14)
    static func parsePackVoltageResponse(_ data: Data) throws -> Float {
        try validateResponse(data, expectedCmd: 0x14)
        guard data.count >= 8 else { throw TinyBMSError.invalidResponse }
        
        // Bytes 2-5: FLOAT (Little Endian for proprietary commands)
        let bits = UInt32(data[2]) | UInt32(data[3]) << 8 | UInt32(data[4]) << 16 | UInt32(data[5]) << 24
        return Float(bitPattern: bits)
    }
    
    /// Parse Pack Current response (0x15)
    static func parsePackCurrentResponse(_ data: Data) throws -> Float {
        try validateResponse(data, expectedCmd: 0x15)
        guard data.count >= 8 else { throw TinyBMSError.invalidResponse }
        
        let bits = UInt32(data[2]) | UInt32(data[3]) << 8 | UInt32(data[4]) << 16 | UInt32(data[5]) << 24
        return Float(bitPattern: bits)
    }
    
    /// Parse Online Status response (0x18)
    static func parseOnlineStatusResponse(_ data: Data) throws -> BMSOnlineStatus {
        try validateResponse(data, expectedCmd: 0x18)
        guard data.count >= 6 else { throw TinyBMSError.invalidResponse }
        
        let statusCode = UInt16(data[2]) | UInt16(data[3]) << 8
        return BMSOnlineStatus(rawValue: statusCode) ?? .unknown
    }
    
    /// Parse Temperatures response (0x1B)
    static func parseTemperaturesResponse(_ data: Data) throws -> (internal: Double, sensor1: Double?, sensor2: Double?) {
        try validateResponse(data, expectedCmd: 0x1B)
        guard data.count >= 11 else { throw TinyBMSError.invalidResponse }
        
        let payloadLength = Int(data[2])
        guard payloadLength == 6 else { throw TinyBMSError.invalidResponse }
        
        // Little Endian for proprietary commands
        let temp1Raw = Int16(bitPattern: UInt16(data[3]) | UInt16(data[4]) << 8)
        let temp2Raw = Int16(bitPattern: UInt16(data[5]) | UInt16(data[6]) << 8)
        let temp3Raw = Int16(bitPattern: UInt16(data[7]) | UInt16(data[8]) << 8)
        
        let internalTemp = Double(temp1Raw) * 0.1
        let sensor1: Double? = temp2Raw == -32768 ? nil : Double(temp2Raw) * 0.1
        let sensor2: Double? = temp3Raw == -32768 ? nil : Double(temp3Raw) * 0.1
        
        return (internalTemp, sensor1, sensor2)
    }
    
    /// Parse Cell Voltages response (0x1C)
    static func parseCellVoltagesResponse(_ data: Data) throws -> [Double] {
        try validateResponse(data, expectedCmd: 0x1C)
        guard data.count >= 4 else { throw TinyBMSError.invalidResponse }
        
        let payloadLength = Int(data[2])
        let cellCount = payloadLength / 2
        
        var voltages: [Double] = []
        for i in 0..<cellCount {
            let offset = 3 + (i * 2)
            guard offset + 1 < data.count - 2 else { break }
            
            // Little Endian
            let rawValue = UInt16(data[offset]) | UInt16(data[offset + 1]) << 8
            let voltage = Double(rawValue) * 0.0001 // Resolution 0.1mV → V
            voltages.append(voltage)
        }
        
        return voltages
    }
    
    /// Parse Version response (0x1E)
    static func parseVersionResponse(_ data: Data) throws -> BMSVersion {
        try validateResponse(data, expectedCmd: 0x1E)
        guard data.count >= 10 else { throw TinyBMSError.invalidResponse }
        
        return BMSVersion(
            hardwareVersion: data[3],
            hardwareChanges: data[4],
            firmwarePublic: data[5],
            firmwareInternal: UInt16(data[6]) | UInt16(data[7]) << 8
        )
    }
    
    /// Parse Extended Version response (0x1F)
    static func parseExtendedVersionResponse(_ data: Data) throws -> BMSExtendedVersion {
        try validateResponse(data, expectedCmd: 0x1F)
        guard data.count >= 12 else { throw TinyBMSError.invalidResponse }
        
        return BMSExtendedVersion(
            hardwareVersion: data[3],
            hardwareChanges: data[4],
            firmwarePublic: data[5],
            firmwareInternal: UInt16(data[6]) | UInt16(data[7]) << 8,
            bootloaderVersion: data[8],
            registerMapVersion: data[9]
        )
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Encoding for Write Operations
    // ═══════════════════════════════════════════════════════════════════════
    
    /// Encode une valeur pour l'écriture dans un registre
    static func encodeValue(_ value: Double, for register: BMSRegister) -> UInt16 {
        var rawValue = value
        if let scale = register.scale, scale != 0 {
            rawValue = value / scale
        }
        
        // Clamp to valid range
        if let min = register.minValue {
            rawValue = max(rawValue, min / (register.scale ?? 1))
        }
        if let max = register.maxValue {
            rawValue = min(rawValue, max / (register.scale ?? 1))
        }
        
        return UInt16(clamping: Int(rawValue.rounded()))
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Supporting Types
// ═══════════════════════════════════════════════════════════════════════

/// Valeur parsée d'un registre
struct ParsedRegisterValue {
    let id: Int
    let rawValue: Double
    let scaledValue: Double
    let register: BMSRegister
    var isSpecialValue: Bool = false
    
    var displayValue: String {
        if isSpecialValue, let meaning = register.specialValueMeaning {
            return meaning
        }
        
        if let enumValues = register.enumValues, let description = enumValues[Int(rawValue)] {
            return description
        }
        
        let formatted: String
        if scaledValue == scaledValue.rounded() {
            formatted = String(format: "%.0f", scaledValue)
        } else if abs(scaledValue) < 1 {
            formatted = String(format: "%.4f", scaledValue)
        } else {
            formatted = String(format: "%.2f", scaledValue)
        }
        
        if let unit = register.unit, !unit.isEmpty {
            return "\(formatted) \(unit)"
        }
        return formatted
    }
}

/// Statut en ligne du BMS
enum BMSOnlineStatus: UInt16 {
    case charging = 0x91
    case fullyCharged = 0x92
    case discharging = 0x93
    case regeneration = 0x96
    case idle = 0x97
    case fault = 0x9B
    case unknown = 0xFFFF
    
    var description: String {
        switch self {
        case .charging: return "Charging"
        case .fullyCharged: return "Fully Charged"
        case .discharging: return "Discharging"
        case .regeneration: return "Regeneration"
        case .idle: return "Idle"
        case .fault: return "Fault"
        case .unknown: return "Unknown"
        }
    }
    
    var isFault: Bool { self == .fault }
    var isCharging: Bool { self == .charging || self == .fullyCharged }
}

/// Version du BMS
struct BMSVersion {
    let hardwareVersion: UInt8
    let hardwareChanges: UInt8
    let firmwarePublic: UInt8
    let firmwareInternal: UInt16
    
    var displayString: String {
        "HW: \(hardwareVersion).\(hardwareChanges) / FW: \(firmwarePublic).\(firmwareInternal)"
    }
}

/// Version étendue du BMS
struct BMSExtendedVersion {
    let hardwareVersion: UInt8
    let hardwareChanges: UInt8
    let firmwarePublic: UInt8
    let firmwareInternal: UInt16
    let bootloaderVersion: UInt8
    let registerMapVersion: UInt8
    
    var displayString: String {
        """
        Hardware: \(hardwareVersion).\(hardwareChanges)
        Firmware: \(firmwarePublic).\(firmwareInternal)
        Bootloader: \(bootloaderVersion)
        Register Map: \(registerMapVersion)
        """
    }
}
