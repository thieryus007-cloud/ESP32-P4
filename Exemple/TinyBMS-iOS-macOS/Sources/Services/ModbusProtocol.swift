import Foundation

/// Protocole Modbus pour TinyBMS
class ModbusProtocol {

    /// Calcul du CRC Modbus (Polynôme 0xA001)
    static func calculateCRC(_ buffer: Data) -> UInt16 {
        var crc: UInt16 = 0xFFFF

        for byte in buffer {
            crc ^= UInt16(byte)
            for _ in 0..<8 {
                if (crc & 0x0001) != 0 {
                    crc >>= 1
                    crc ^= 0xA001
                } else {
                    crc >>= 1
                }
            }
        }

        return crc
    }

    /// Créer une commande de lecture de registres (fonction 0x03)
    /// - Parameters:
    ///   - startAddress: Adresse de départ (Big Endian)
    ///   - count: Nombre de registres à lire
    /// - Returns: Data de la commande complète avec CRC
    static func createReadCommand(startAddress: UInt16, count: UInt8) -> Data {
        var command = Data()
        command.append(0xAA) // Header
        command.append(0x03) // Fonction Read
        command.append(UInt8((startAddress >> 8) & 0xFF)) // MSB address
        command.append(UInt8(startAddress & 0xFF)) // LSB address
        command.append(0x00) // MSB count (always 0 for count < 256)
        command.append(count) // LSB count

        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF)) // CRC LSB
        command.append(UInt8((crc >> 8) & 0xFF)) // CRC MSB

        return command
    }

    /// Créer une commande d'écriture de registre (fonction 0x10)
    /// - Parameters:
    ///   - address: Adresse du registre (Big Endian)
    ///   - value: Valeur brute à écrire
    ///   - isSigned: Si la valeur est signée (Int16)
    /// - Returns: Data de la commande complète avec CRC
    static func createWriteCommand(address: UInt16, value: UInt16, isSigned: Bool = false) -> Data {
        var command = Data()
        command.append(0xAA) // Header
        command.append(0x10) // Fonction Write Multiple Registers
        command.append(UInt8((address >> 8) & 0xFF)) // MSB address
        command.append(UInt8(address & 0xFF)) // LSB address
        command.append(0x00) // MSB register count (1 register)
        command.append(0x01) // LSB register count
        command.append(0x02) // Byte count (2 bytes)

        // Ajouter la valeur en Big Endian
        command.append(UInt8((value >> 8) & 0xFF)) // MSB value
        command.append(UInt8(value & 0xFF)) // LSB value

        let crc = calculateCRC(command)
        command.append(UInt8(crc & 0xFF)) // CRC LSB
        command.append(UInt8((crc >> 8) & 0xFF)) // CRC MSB

        return command
    }

    /// Parser une réponse de lecture
    /// - Parameters:
    ///   - data: Data reçue
    ///   - startAddress: Adresse de départ des registres lus
    /// - Returns: Dictionnaire [registerId: BMSRegisterValue]
    static func parseReadResponse(_ data: Data, startAddress: UInt16) -> [Int: BMSRegisterValue] {
        guard data.count >= 3 else { return [:] }
        guard data[0] == 0xAA && data[1] == 0x03 else { return [:] }

        let length = Int(data[2])
        guard data.count >= 3 + length + 2 else { return [:] }

        let payload = data.subdata(in: 3..<(3 + length))
        return parsePayload(payload, startAddress: Int(startAddress))
    }

    /// Parser le payload et décoder selon le RegisterMap
    private static func parsePayload(_ buffer: Data, startAddress: Int) -> [Int: BMSRegisterValue] {
        var result: [Int: BMSRegisterValue] = [:]
        let registerCount = buffer.count / 2

        for i in 0..<registerCount {
            let currentRegId = startAddress + i
            guard let registerDef = RegisterMap.register(for: currentRegId) else { continue }

            let byteOffset = i * 2
            guard byteOffset + 1 < buffer.count else { continue }

            var rawValue: Double = 0

            switch registerDef.type {
            case .float32:
                // Float32 occupe 4 octets (2 registres)
                if byteOffset + 3 < buffer.count {
                    let bytes = buffer.subdata(in: byteOffset..<(byteOffset + 4))
                    rawValue = Double(bytes.withUnsafeBytes { $0.load(fromByteOffset: 0, as: Float32.self).bitPattern }.bigEndian.bitPattern)
                }

            case .uint32:
                // UInt32 occupe 4 octets (2 registres)
                if byteOffset + 3 < buffer.count {
                    let bytes = buffer.subdata(in: byteOffset..<(byteOffset + 4))
                    rawValue = Double(bytes.withUnsafeBytes { $0.load(fromByteOffset: 0, as: UInt32.self).bigEndian })
                }

            case .int16:
                let value = Int16(bigEndian: buffer.withUnsafeBytes { $0.load(fromByteOffset: byteOffset, as: Int16.self) })
                rawValue = Double(value)

            case .uint16:
                let value = UInt16(bigEndian: buffer.withUnsafeBytes { $0.load(fromByteOffset: byteOffset, as: UInt16.self) })
                rawValue = Double(value)
            }

            // Appliquer le scale si présent
            var finalValue = rawValue
            if let scale = registerDef.scale {
                finalValue = rawValue * scale
            }

            // Arrondir à 4 décimales
            finalValue = round(finalValue * 10000) / 10000

            result[currentRegId] = BMSRegisterValue(
                id: currentRegId,
                label: registerDef.label,
                value: finalValue,
                unit: registerDef.unit ?? "",
                category: registerDef.category,
                group: registerDef.group
            )
        }

        return result
    }

    /// Encoder une valeur pour l'écriture
    static func encodeValue(_ value: Double, for register: BMSRegister) -> UInt16 {
        var rawValue = value
        if let scale = register.scale {
            rawValue = value / scale
        }
        return UInt16(rawValue.rounded())
    }
}
