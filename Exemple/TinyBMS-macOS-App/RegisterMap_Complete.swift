import Foundation

/// Carte des registres TinyBMS COMPLÈTE basée sur Communication Protocols Rev D (2025-07-04)
struct RegisterMapComplete {

    /// Liste complète des registres
    static let registers: [BMSRegister] = {
        var regs: [BMSRegister] = []

        // ═══════════════════════════════════════════════════════════════════════
        // LIVE DATA (Registres 0-99) - Lecture seule
        // ═══════════════════════════════════════════════════════════════════════

        // --- CELLULES 1-16 (Registres 0-15) ---
        for i in 0..<16 {
            regs.append(BMSRegister(
                id: i,
                label: "Cell \(i + 1) Voltage",
                unit: "V",
                type: .uint16,
                scale: 0.0001,  // Resolution 0.1 mV → V
                category: .live,
                minValue: 0,
                maxValue: 5.0
            ))
        }

        // Registres 16-31: Reserved
        
        // --- Registres 32-55: Données Live ---
        regs.append(contentsOf: [
            // Lifetime Counter (32-33) - UINT32
            BMSRegister(id: 32, label: "BMS Lifetime Counter", unit: "s", type: .uint32, scale: 1, category: .live),
            // Note: Reg 33 est la partie haute du UINT32
            
            // Estimated Time Left (34-35) - UINT32
            BMSRegister(id: 34, label: "Estimated Time Left", unit: "s", type: .uint32, scale: 1, category: .live),
            // Note: Reg 35 est la partie haute du UINT32
            
            // Pack Voltage (36-37) - FLOAT
            BMSRegister(id: 36, label: "Battery Pack Voltage", unit: "V", type: .float32, category: .live),
            // Note: Reg 37 est la partie haute du FLOAT
            
            // Pack Current (38-39) - FLOAT
            BMSRegister(id: 38, label: "Battery Pack Current", unit: "A", type: .float32, category: .live),
            // Note: Reg 39 est la partie haute du FLOAT
            
            // Min/Max Cell Voltage
            BMSRegister(id: 40, label: "Minimal Cell Voltage", unit: "V", type: .uint16, scale: 0.001, category: .live),
            BMSRegister(id: 41, label: "Maximal Cell Voltage", unit: "V", type: .uint16, scale: 0.001, category: .live),
            
            // Températures
            BMSRegister(id: 42, label: "External Temp Sensor #1", unit: "°C", type: .int16, scale: 0.1, category: .live,
                       specialValue: -32768, specialValueMeaning: "Not Connected"),
            BMSRegister(id: 43, label: "External Temp Sensor #2", unit: "°C", type: .int16, scale: 0.1, category: .live,
                       specialValue: -32768, specialValueMeaning: "Not Connected"),
            
            // Distance Left
            BMSRegister(id: 44, label: "Distance Left To Empty", unit: "km", type: .uint16, scale: 1, category: .live),
            
            // State Of Health
            BMSRegister(id: 45, label: "State Of Health", unit: "%", type: .uint16, scale: 0.002, category: .live,
                       minValue: 0, maxValue: 100),
            
            // State Of Charge (46-47) - UINT32
            BMSRegister(id: 46, label: "State Of Charge", unit: "%", type: .uint32, scale: 0.000001, category: .live,
                       minValue: 0, maxValue: 100),
            // Note: Reg 47 est la partie haute du UINT32
            
            // Internal Temperature
            BMSRegister(id: 48, label: "BMS Internal Temperature", unit: "°C", type: .int16, scale: 0.1, category: .live),
            
            // Reg 49: Reserved
            
            // BMS Online Status
            BMSRegister(id: 50, label: "BMS Online Status", type: .uint16, category: .live,
                       enumValues: [
                           0x91: "Charging",
                           0x92: "Fully Charged",
                           0x93: "Discharging",
                           0x96: "Regeneration",
                           0x97: "Idle",
                           0x9B: "Fault"
                       ]),
            
            // Balancing Bits
            BMSRegister(id: 51, label: "Balancing Decision Bits", type: .uint16, category: .live,
                       isBitfield: true, bitfieldDescription: "First Cell = LSB Bit: 1=need balancing"),
            BMSRegister(id: 52, label: "Real Balancing Bits", type: .uint16, category: .live,
                       isBitfield: true, bitfieldDescription: "First Cell = LSB Bit: 1=balancing active"),
            
            // Number of Detected Cells
            BMSRegister(id: 53, label: "Number Of Detected Cells", type: .uint16, category: .live,
                       minValue: 4, maxValue: 16),
            
            // Speed (54-55) - FLOAT
            BMSRegister(id: 54, label: "Speed", unit: "km/h", type: .float32, category: .live),
            // Note: Reg 55 est la partie haute du FLOAT
        ])
        
        // Registres 56-99: Reserved

        // ═══════════════════════════════════════════════════════════════════════
        // STATISTICS (Registres 100-199) - Lecture seule
        // ═══════════════════════════════════════════════════════════════════════
        
        regs.append(contentsOf: [
            // Total Distance (100-101) - UINT32
            BMSRegister(id: 100, label: "Total Distance", unit: "km", type: .uint32, scale: 0.01, category: .stats),
            // Note: Reg 101 est la partie haute du UINT32
            
            // Currents maximaux enregistrés
            BMSRegister(id: 102, label: "Max Discharge Current", unit: "A", type: .uint16, scale: 0.1, category: .stats),
            BMSRegister(id: 103, label: "Max Charge Current", unit: "A", type: .uint16, scale: 0.1, category: .stats),
            
            // Voltage difference max
            BMSRegister(id: 104, label: "Max Cell Voltage Difference", unit: "mV", type: .uint16, scale: 0.1, category: .stats),
            
            // Compteurs de protection
            BMSRegister(id: 105, label: "Under-Voltage Protection Count", type: .uint16, category: .stats),
            BMSRegister(id: 106, label: "Over-Voltage Protection Count", type: .uint16, category: .stats),
            BMSRegister(id: 107, label: "Discharge Over-Current Protection Count", type: .uint16, category: .stats),
            BMSRegister(id: 108, label: "Charge Over-Current Protection Count", type: .uint16, category: .stats),
            BMSRegister(id: 109, label: "Over-Heat Protection Count", type: .uint16, category: .stats),
            
            // Reg 110: Reserved
            
            // Compteurs de charge
            BMSRegister(id: 111, label: "Charging Count", type: .uint16, category: .stats),
            BMSRegister(id: 112, label: "Full Charge Count", type: .uint16, category: .stats),
            
            // Pack Temperature extremes (packed in one register)
            BMSRegister(id: 113, label: "Min/Max Pack Temperature", type: .uint16, category: .stats,
                       packedFields: [
                           PackedField(name: "Min Pack Temp", bits: 0...7, scale: 1, unit: "°C", signed: true),
                           PackedField(name: "Max Pack Temp", bits: 8...15, scale: 1, unit: "°C", signed: true)
                       ]),
            
            // Last Reset/Wakeup events (packed)
            BMSRegister(id: 114, label: "Last Reset/Wakeup Events", type: .uint16, category: .stats,
                       packedFields: [
                           PackedField(name: "Last BMS Reset", bits: 0...7, enumValues: [
                               0x00: "Unknown",
                               0x01: "Low power reset",
                               0x02: "Window watchdog reset",
                               0x03: "Independent watchdog reset",
                               0x04: "Software reset",
                               0x05: "POR/PDR reset",
                               0x06: "PIN reset",
                               0x07: "Options bytes loading reset"
                           ]),
                           PackedField(name: "Last Wakeup", bits: 8...15, enumValues: [
                               0x00: "Charger connected",
                               0x01: "Ignition",
                               0x02: "Discharging detected",
                               0x03: "UART communication detected"
                           ])
                       ]),
            
            // Reg 115: Reserved
            
            // Statistics Last Cleared Timestamp (116-117) - UINT32
            BMSRegister(id: 116, label: "Statistics Last Cleared On", unit: "s", type: .uint32, scale: 1, category: .stats),
            // Note: Reg 117 est la partie haute du UINT32
        ])
        
        // Registres 118-199: Reserved

        // ═══════════════════════════════════════════════════════════════════════
        // EVENTS (Registres 200-299) - Lecture seule (via commandes spéciales)
        // ═══════════════════════════════════════════════════════════════════════
        
        // Les events sont mieux lus via les commandes 0x11 (newest) et 0x12 (all)
        // Structure: Timestamp[UINT24] + EventID[UINT8] par event
        // 49 events maximum (Event_0 à Event_48)
        
        for i in 0..<49 {
            let baseReg = 200 + (i * 2)
            regs.append(BMSRegister(
                id: baseReg,
                label: "Event \(i) Timestamp LSB",
                type: .uint16,
                category: .events
            ))
            regs.append(BMSRegister(
                id: baseReg + 1,
                label: "Event \(i) Timestamp MSB + ID",
                type: .uint16,
                category: .events
            ))
        }

        // ═══════════════════════════════════════════════════════════════════════
        // SETTINGS (Registres 300-399) - Lecture/Écriture
        // ═══════════════════════════════════════════════════════════════════════

        // --- Battery Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 300, label: "Fully Charged Voltage", unit: "V", type: .uint16, scale: 0.001,
                       category: .settings, group: .battery, minValue: 1.2, maxValue: 4.5),
            BMSRegister(id: 301, label: "Fully Discharged Voltage", unit: "V", type: .uint16, scale: 0.001,
                       category: .settings, group: .battery, minValue: 1.0, maxValue: 3.5),
            // Reg 302: Reserved
            BMSRegister(id: 303, label: "Early Balancing Threshold", unit: "V", type: .uint16, scale: 0.001,
                       category: .settings, group: .balance, minValue: 1.0, maxValue: 4.5),
            BMSRegister(id: 304, label: "Charge Finished Current", unit: "mA", type: .uint16, scale: 1,
                       category: .settings, group: .balance, minValue: 100, maxValue: 5000),
            BMSRegister(id: 305, label: "Peak Discharge Current Cutoff", unit: "A", type: .uint16, scale: 1,
                       category: .settings, group: .safety),
            BMSRegister(id: 306, label: "Battery Capacity", unit: "Ah", type: .uint16, scale: 0.01,
                       category: .settings, group: .battery, minValue: 0.1, maxValue: 655),
            BMSRegister(id: 307, label: "Number Of Series Cells", type: .uint16, scale: 1,
                       category: .settings, group: .battery, minValue: 4, maxValue: 16),
            BMSRegister(id: 308, label: "Allowed Disbalance", unit: "mV", type: .uint16, scale: 1,
                       category: .settings, group: .balance, minValue: 15, maxValue: 100),
            // Reg 309: Reserved
            BMSRegister(id: 310, label: "Charger Startup Delay", unit: "s", type: .uint16, scale: 1,
                       category: .settings, group: .hardware, minValue: 5, maxValue: 60),
            BMSRegister(id: 311, label: "Charger Disable Delay", unit: "s", type: .uint16, scale: 1,
                       category: .settings, group: .hardware, minValue: 0, maxValue: 60),
            BMSRegister(id: 312, label: "Pulses Per Unit", type: .uint32, scale: 1,
                       category: .settings, group: .hardware, minValue: 1, maxValue: 100000),
            // Note: Reg 313 est la partie haute du UINT32
            BMSRegister(id: 314, label: "Distance Unit Name", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [0x01: "Meter", 0x02: "Kilometer", 0x03: "Feet", 0x04: "Mile", 0x05: "Yard"]),
        ])

        // --- Safety Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 315, label: "Over-Voltage Cutoff", unit: "V", type: .uint16, scale: 0.001,
                       category: .settings, group: .safety, minValue: 1.2, maxValue: 4.5),
            BMSRegister(id: 316, label: "Under-Voltage Cutoff", unit: "V", type: .uint16, scale: 0.001,
                       category: .settings, group: .safety, minValue: 0.8, maxValue: 3.5),
            BMSRegister(id: 317, label: "Discharge Over-Current Cutoff", unit: "A", type: .uint16, scale: 1,
                       category: .settings, group: .safety, minValue: 1, maxValue: 750),
            BMSRegister(id: 318, label: "Charge Over-Current Cutoff", unit: "A", type: .uint16, scale: 1,
                       category: .settings, group: .safety, minValue: 1, maxValue: 750),
            BMSRegister(id: 319, label: "Over-Heat Cutoff", unit: "°C", type: .int16, scale: 1,
                       category: .settings, group: .safety, minValue: 20, maxValue: 90),
            BMSRegister(id: 320, label: "Low Temperature Charger Cutoff", unit: "°C", type: .int16, scale: 1,
                       category: .settings, group: .safety, minValue: -40, maxValue: 10),
            BMSRegister(id: 321, label: "Charge Restart Level", unit: "%", type: .uint16, scale: 1,
                       category: .settings, group: .balance, minValue: 60, maxValue: 95),
            BMSRegister(id: 322, label: "Battery Maximum Cycles Count", type: .uint16, scale: 1,
                       category: .settings, group: .battery, minValue: 10, maxValue: 65000),
            BMSRegister(id: 323, label: "State Of Health (Write)", unit: "%", type: .uint16, scale: 0.002,
                       category: .settings, group: .battery, minValue: 0, maxValue: 100),
            // Reg 324-327: Reserved
            BMSRegister(id: 328, label: "State Of Charge (Manual Set)", unit: "%", type: .uint16, scale: 0.002,
                       category: .settings, group: .battery, minValue: 0, maxValue: 100),
        ])

        // --- Configuration Bits (Reg 329) ---
        regs.append(BMSRegister(id: 329, label: "Configuration Bits", type: .uint16,
                               category: .settings, group: .hardware, isBitfield: true,
                               bitfieldDescription: """
                                   Bit 0: Invert External Current Sensor Direction (0-1)
                                   Bit 1: Disable Load/Charger Switch Diagnostics (0-1)
                                   Bit 2: Enable Charger Restart Level (0-1)
                                   Bits 3-15: Reserved
                               """))

        // --- Hardware Configuration ---
        regs.append(contentsOf: [
            // Reg 330: Charger Type + Discharge OC Timeout (packed)
            BMSRegister(id: 330, label: "Charger Type / Discharge OC Timeout", type: .uint16,
                       category: .settings, group: .hardware,
                       packedFields: [
                           PackedField(name: "Charger Type", bits: 0...7, enumValues: [
                               0x00: "Variable (Reserved)",
                               0x01: "CC/CV",
                               0x02: "CAN (Reserved)"
                           ]),
                           PackedField(name: "Discharge OC Timeout", bits: 8...15, scale: 1, unit: "s", minValue: 0, maxValue: 30)
                       ]),
            
            // Reg 331: Load Switch Type
            BMSRegister(id: 331, label: "Load Switch Type", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "FET", 0x01: "AIDO1", 0x02: "AIDO2", 0x03: "DIDO1", 0x04: "DIDO2",
                           0x05: "AIHO1 Active Low", 0x06: "AIHO1 Active High",
                           0x07: "AIHO2 Active Low", 0x08: "AIHO2 Active High"
                       ]),
            
            // Reg 332: Automatic Recovery
            BMSRegister(id: 332, label: "Automatic Recovery", unit: "s", type: .uint16, scale: 1,
                       category: .settings, group: .balance, minValue: 1, maxValue: 30),
            
            // Reg 333: Charger Switch Type
            BMSRegister(id: 333, label: "Charger Switch Type", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x01: "Charge FET", 0x02: "AIDO1", 0x03: "AIDO2", 0x04: "DIDO1", 0x05: "DIDO2",
                           0x06: "AIHO1 Active Low", 0x07: "AIHO1 Active High",
                           0x08: "AIHO2 Active Low", 0x09: "AIHO2 Active High"
                       ]),
            
            // Reg 334: Ignition
            BMSRegister(id: 334, label: "Ignition", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "Disabled", 0x01: "AIDO1", 0x02: "AIDO2", 0x03: "DIDO1",
                           0x04: "DIDO2", 0x05: "AIHO1", 0x06: "AIHO2"
                       ]),
            
            // Reg 335: Charger Detection
            BMSRegister(id: 335, label: "Charger Detection", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x01: "Internal", 0x02: "AIDO1", 0x03: "AIDO2", 0x04: "DIDO1",
                           0x05: "DIDO2", 0x06: "AIHO1", 0x07: "AIHO2"
                       ]),
            
            // Reg 336: Speed Sensor Input
            BMSRegister(id: 336, label: "Speed Sensor Input", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [0x00: "Disabled", 0x01: "DIDO1", 0x02: "DIDO2"]),
            
            // Reg 337: Precharge Pin
            BMSRegister(id: 337, label: "Precharge Pin", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "Disabled", 0x02: "Discharge FET", 0x03: "AIDO1", 0x04: "AIDO2",
                           0x05: "DIDO1", 0x06: "DIDO2", 0x07: "AIHO1 Active Low", 0x08: "AIHO1 Active High",
                           0x09: "AIHO2 Active Low", 0x10: "AIHO2 Active High"
                       ]),
            
            // Reg 338: Precharge Duration
            BMSRegister(id: 338, label: "Precharge Duration", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "0.1 sec", 0x01: "0.2 sec", 0x02: "0.5 sec", 0x03: "1 sec",
                           0x04: "2 sec", 0x05: "3 sec", 0x06: "4 sec", 0x07: "5 sec"
                       ]),
            
            // Reg 339: Temperature Sensor Type
            BMSRegister(id: 339, label: "Temperature Sensor Type", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [0x00: "Dual 10K NTC", 0x01: "Multipoint Active Sensor"]),
            
            // Reg 340: BMS Operation Mode
            BMSRegister(id: 340, label: "BMS Operation Mode", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [0x00: "Dual Port Operation", 0x01: "Single Port Operation"]),
            
            // Reg 341: Single Port Switch Type
            BMSRegister(id: 341, label: "Single Port Switch Type", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "FET", 0x01: "AIDO1", 0x02: "AIDO2", 0x03: "DIDO1", 0x04: "DIDO2",
                           0x05: "AIHO1 Active Low", 0x06: "AIHO1 Active High",
                           0x07: "AIHO2 Active Low", 0x08: "AIHO2 Active High"
                       ]),
            
            // Reg 342: Broadcast Time
            BMSRegister(id: 342, label: "Broadcast Time", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [
                           0x00: "Disabled", 0x01: "0.1 sec", 0x02: "0.2 sec", 0x03: "0.5 sec",
                           0x04: "1 sec", 0x05: "2 sec", 0x06: "5 sec", 0x07: "10 sec"
                       ]),
            
            // Reg 343: Protocol
            BMSRegister(id: 343, label: "Protocol", type: .uint16,
                       category: .settings, group: .hardware,
                       enumValues: [0x00: "CA V3", 0x01: "ASCII", 0x02: "SOC BAR"]),
        ])
        
        // Registres 344-399: Reserved

        // ═══════════════════════════════════════════════════════════════════════
        // VERSION (Registres 500-599) - Lecture seule
        // ═══════════════════════════════════════════════════════════════════════
        
        regs.append(contentsOf: [
            // Reg 500: Hardware versions (packed)
            BMSRegister(id: 500, label: "Hardware Version", type: .uint16, category: .version,
                       packedFields: [
                           PackedField(name: "Hardware Version", bits: 0...7),
                           PackedField(name: "Hardware Changes Version", bits: 8...15)
                       ]),
            
            // Reg 501: Firmware public + type info (packed)
            BMSRegister(id: 501, label: "Firmware Version", type: .uint16, category: .version,
                       packedFields: [
                           PackedField(name: "FW Public Version", bits: 0...7),
                           PackedField(name: "BMS Power Type", bits: 8...8, enumValues: [0: "Low Power", 1: "High Power"]),
                           PackedField(name: "BMS Current Sensor", bits: 9...10, enumValues: [
                               0: "Internal Resistor", 1: "Internal HALL", 2: "External"
                           ])
                       ]),
            
            // Reg 502: Firmware Internal Version
            BMSRegister(id: 502, label: "Firmware Internal Version", type: .uint16, category: .version),
            
            // Reg 503: Bootloader + Profile (packed)
            BMSRegister(id: 503, label: "Bootloader / Profile Version", type: .uint16, category: .version,
                       packedFields: [
                           PackedField(name: "Bootloader Version", bits: 0...7),
                           PackedField(name: "Profile Version", bits: 8...15)
                       ]),
            
            // Regs 504-509: Product Serial Number (96 bits = 6 x 16 bits)
            BMSRegister(id: 504, label: "Serial Number (Part 1)", type: .uint16, category: .version),
            BMSRegister(id: 505, label: "Serial Number (Part 2)", type: .uint16, category: .version),
            BMSRegister(id: 506, label: "Serial Number (Part 3)", type: .uint16, category: .version),
            BMSRegister(id: 507, label: "Serial Number (Part 4)", type: .uint16, category: .version),
            BMSRegister(id: 508, label: "Serial Number (Part 5)", type: .uint16, category: .version),
            BMSRegister(id: 509, label: "Serial Number (Part 6)", type: .uint16, category: .version),
        ])

        return regs
    }()

    // ═══════════════════════════════════════════════════════════════════════
    // Helper Methods
    // ═══════════════════════════════════════════════════════════════════════

    /// Dictionnaire pour un accès rapide par ID
    static let registersByID: [Int: BMSRegister] = {
        Dictionary(uniqueKeysWithValues: registers.map { ($0.id, $0) })
    }()

    /// Obtenir un registre par son ID
    static func register(for id: Int) -> BMSRegister? {
        registersByID[id]
    }

    /// Obtenir tous les registres d'une catégorie
    static func registers(for category: RegisterCategory) -> [BMSRegister] {
        registers.filter { $0.category == category }
    }

    /// Obtenir tous les registres d'un groupe de settings
    static func registers(for group: SettingsGroup) -> [BMSRegister] {
        registers.filter { $0.group == group }
    }
    
    /// Obtenir les registres cellules actives (basé sur le nombre détecté)
    static func activeCellRegisters(detectedCells: Int) -> [BMSRegister] {
        registers.filter { $0.id >= 0 && $0.id < detectedCells }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Extended Models
// ═══════════════════════════════════════════════════════════════════════

/// Catégorie de registre étendue
enum RegisterCategory: String, Codable {
    case live = "Live"
    case stats = "Stats"
    case events = "Events"
    case settings = "Settings"
    case version = "Version"
}

/// Structure pour les champs packés dans un registre
struct PackedField {
    let name: String
    let bits: ClosedRange<Int>
    var scale: Double?
    var unit: String?
    var signed: Bool = false
    var enumValues: [Int: String]?
    var minValue: Double?
    var maxValue: Double?
}

/// Définition d'un registre TinyBMS étendue
struct BMSRegister: Identifiable {
    let id: Int
    let label: String
    var unit: String?
    let type: RegisterType
    var scale: Double?
    let category: RegisterCategory
    var group: SettingsGroup?
    
    // Validation
    var minValue: Double?
    var maxValue: Double?
    
    // Valeur spéciale (ex: -32768 = non connecté)
    var specialValue: Int?
    var specialValueMeaning: String?
    
    // Pour les registres enum
    var enumValues: [Int: String]?
    
    // Pour les registres bitfield
    var isBitfield: Bool = false
    var bitfieldDescription: String?
    
    // Pour les registres avec champs packés
    var packedFields: [PackedField]?
    
    /// Vérifie si une valeur est dans les limites
    func isValueValid(_ value: Double) -> Bool {
        if let min = minValue, value < min { return false }
        if let max = maxValue, value > max { return false }
        return true
    }
    
    /// Retourne la description enum pour une valeur
    func enumDescription(for value: Int) -> String? {
        enumValues?[value]
    }
    
    /// Vérifie si la valeur est une valeur spéciale
    func isSpecialValue(_ rawValue: Int) -> Bool {
        rawValue == specialValue
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Event Types (Chapter 4)
// ═══════════════════════════════════════════════════════════════════════

enum BMSEventType: UInt8 {
    // Faults (0x01 to 0x30)
    case underVoltageCutoff = 0x02
    case overVoltageCutoff = 0x03
    case overTemperatureCutoff = 0x04
    case dischargingOverCurrentCutoff = 0x05
    case chargingOverCurrentCutoff = 0x06
    case regenerationOverCurrentCutoff = 0x07
    case lowTemperatureCutoff = 0x0A
    case chargerSwitchError = 0x0B
    case loadSwitchError = 0x0C
    case singlePortSwitchError = 0x0D
    case externalCurrentSensorDisconnected = 0x0E
    case externalCurrentSensorConnected = 0x0F
    
    // Warnings (0x31 to 0x60)
    case fullyDischargedCutoff = 0x31
    case lowTempChargingCutoff = 0x37
    case chargingDoneVoltageTooHigh = 0x38
    case chargingDoneVoltageTooLow = 0x39
    
    // Info (0x61 to 0x90)
    case systemStarted = 0x61
    case chargingStarted = 0x62
    case chargingDone = 0x63
    case chargerConnected = 0x64
    case chargerDisconnected = 0x65
    case dualPortModeActivated = 0x66
    case singlePortModeActivated = 0x67
    case recoveredFromOverTemp = 0x73
    case recoveredFromLowTempWarning = 0x74
    case recoveredFromLowTempFault = 0x75
    case recoveredFromChargeOverCurrent = 0x76
    case recoveredFromDischargeOverCurrent = 0x77
    case recoveredFromRegenOverCurrent = 0x78
    case recoveredFromOverVoltage = 0x79
    case recoveredFromFullyDischarged = 0x7A
    case recoveredFromUnderVoltage = 0x7B
    case extCurrentSensorConnected = 0x7C
    case extCurrentSensorDisconnected = 0x7D
    
    var severity: EventSeverity {
        switch rawValue {
        case 0x01...0x30: return .fault
        case 0x31...0x60: return .warning
        case 0x61...0x90: return .info
        default: return .unknown
        }
    }
    
    var description: String {
        switch self {
        case .underVoltageCutoff: return "Under-Voltage Cutoff Occurred"
        case .overVoltageCutoff: return "Over-Voltage Cutoff Occurred"
        case .overTemperatureCutoff: return "Over-Temperature Cutoff Occurred"
        case .dischargingOverCurrentCutoff: return "Discharging Over-Current Cutoff Occurred"
        case .chargingOverCurrentCutoff: return "Charging Over-Current Cutoff Occurred"
        case .regenerationOverCurrentCutoff: return "Regeneration Over-Current Cutoff Occurred"
        case .lowTemperatureCutoff: return "Low Temperature Cutoff Occurred"
        case .chargerSwitchError: return "Charger Switch Error Detected"
        case .loadSwitchError: return "Load Switch Error Detected"
        case .singlePortSwitchError: return "Single Port Switch Error Detected"
        case .externalCurrentSensorDisconnected: return "External Current Sensor Disconnected (BMS restart required)"
        case .externalCurrentSensorConnected: return "External Current Sensor Connected (BMS restart required)"
        case .fullyDischargedCutoff: return "Fully Discharged Cutoff Occurred"
        case .lowTempChargingCutoff: return "Low Temperature Charging Cutoff Occurred"
        case .chargingDoneVoltageTooHigh: return "Charging Done (Charger voltage too high)"
        case .chargingDoneVoltageTooLow: return "Charging Done (Charger voltage too low)"
        case .systemStarted: return "System Started"
        case .chargingStarted: return "Charging Started"
        case .chargingDone: return "Charging Done"
        case .chargerConnected: return "Charger Connected"
        case .chargerDisconnected: return "Charger Disconnected"
        case .dualPortModeActivated: return "Dual Port Operation Mode Activated"
        case .singlePortModeActivated: return "Single Port Operation Mode Activated"
        case .recoveredFromOverTemp: return "Recovered From Over-Temperature Fault Condition"
        case .recoveredFromLowTempWarning: return "Recovered From Low Temperature Warning Condition"
        case .recoveredFromLowTempFault: return "Recovered From Low Temperature Fault Condition"
        case .recoveredFromChargeOverCurrent: return "Recovered From Charging Over-Current Fault Condition"
        case .recoveredFromDischargeOverCurrent: return "Recovered From Discharging Over-Current Fault Condition"
        case .recoveredFromRegenOverCurrent: return "Recovered From Regeneration Over-Current Fault Condition"
        case .recoveredFromOverVoltage: return "Recovered From Over-Voltage Fault Condition"
        case .recoveredFromFullyDischarged: return "Recovered From Fully Discharged Voltage Warning Condition"
        case .recoveredFromUnderVoltage: return "Recovered From Under-Voltage Fault Condition"
        case .extCurrentSensorConnected: return "External Current Sensor Connected"
        case .extCurrentSensorDisconnected: return "External Current Sensor Disconnected"
        }
    }
}

enum EventSeverity {
    case fault
    case warning
    case info
    case unknown
}

struct BMSEvent: Identifiable {
    let id = UUID()
    let timestamp: UInt32 // Seconds since BMS start
    let eventType: BMSEventType
    
    var formattedTimestamp: String {
        let hours = timestamp / 3600
        let minutes = (timestamp % 3600) / 60
        let seconds = timestamp % 60
        return String(format: "%02d:%02d:%02d", hours, minutes, seconds)
    }
}
