import Foundation

/// Carte des registres TinyBMS basée sur Communication Protocols Rev D
struct RegisterMap {

    /// Liste complète des registres
    static let registers: [BMSRegister] = {
        var regs: [BMSRegister] = []

        // --- CELLULES 1-16 (Registres 0-15) ---
        for i in 0..<16 {
            regs.append(BMSRegister(
                id: i,
                label: "Cell \(i + 1)",
                unit: "V",
                type: .uint16,
                scale: 0.0001,
                category: .live
            ))
        }

        // --- LIVE DATA (16-99) ---
        regs.append(contentsOf: [
            BMSRegister(id: 36, label: "Pack Voltage", unit: "V", type: .float32, category: .live),
            BMSRegister(id: 38, label: "Pack Current", unit: "A", type: .float32, category: .live),
            BMSRegister(id: 40, label: "Min Cell Voltage", unit: "V", type: .uint16, scale: 0.001, category: .live),
            BMSRegister(id: 41, label: "Max Cell Voltage", unit: "V", type: .uint16, scale: 0.001, category: .live),
            BMSRegister(id: 42, label: "Temp Sensor 1", unit: "°C", type: .int16, scale: 0.1, category: .live),
            BMSRegister(id: 43, label: "Temp Sensor 2", unit: "°C", type: .int16, scale: 0.1, category: .live),
            BMSRegister(id: 45, label: "State Of Health", unit: "%", type: .uint16, scale: 0.002, category: .stats),
            BMSRegister(id: 46, label: "State Of Charge", unit: "%", type: .uint32, scale: 0.000001, category: .live),
            BMSRegister(id: 48, label: "Internal Temp", unit: "°C", type: .int16, scale: 0.1, category: .live),
            BMSRegister(id: 50, label: "BMS Status", type: .uint16, category: .live),
            BMSRegister(id: 52, label: "Real Balancing", type: .uint16, category: .live),
        ])

        // --- STATISTICS (100-199) ---
        regs.append(contentsOf: [
            BMSRegister(id: 101, label: "Total Distance", unit: "km", type: .uint32, scale: 0.01, category: .stats),
            BMSRegister(id: 105, label: "Under-Voltage Count", type: .uint16, category: .stats),
            BMSRegister(id: 106, label: "Over-Voltage Count", type: .uint16, category: .stats),
            BMSRegister(id: 111, label: "Charging Count", type: .uint16, category: .stats),
            BMSRegister(id: 112, label: "Full Charge Count", type: .uint16, category: .stats),
        ])

        // --- SETTINGS - Battery Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 300, label: "Fully Charged Voltage", unit: "V", type: .uint16, scale: 0.001, category: .settings, group: .battery),
            BMSRegister(id: 301, label: "Fully Discharged Voltage", unit: "V", type: .uint16, scale: 0.001, category: .settings, group: .battery),
            BMSRegister(id: 306, label: "Battery Capacity", unit: "Ah", type: .uint16, scale: 0.01, category: .settings, group: .battery),
            BMSRegister(id: 307, label: "Series Cells Count", type: .uint16, category: .settings, group: .battery),
            BMSRegister(id: 322, label: "Max Cycles Count", type: .uint16, category: .settings, group: .battery),
            BMSRegister(id: 328, label: "Manual SOC Set", unit: "%", type: .uint16, scale: 0.002, category: .settings, group: .battery),
        ])

        // --- SETTINGS - Safety Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 315, label: "Over-Voltage Cutoff", unit: "V", type: .uint16, scale: 0.001, category: .settings, group: .safety),
            BMSRegister(id: 316, label: "Under-Voltage Cutoff", unit: "V", type: .uint16, scale: 0.001, category: .settings, group: .safety),
            BMSRegister(id: 317, label: "Discharge Over-Current", unit: "A", type: .uint16, scale: 1, category: .settings, group: .safety),
            BMSRegister(id: 318, label: "Charge Over-Current", unit: "A", type: .uint16, scale: 1, category: .settings, group: .safety),
            BMSRegister(id: 305, label: "Peak Discharge Current", unit: "A", type: .uint16, scale: 1, category: .settings, group: .safety),
            BMSRegister(id: 319, label: "Over-Heat Cutoff", unit: "°C", type: .int16, scale: 1, category: .settings, group: .safety),
            BMSRegister(id: 320, label: "Low Temp Charge Cutoff", unit: "°C", type: .int16, scale: 1, category: .settings, group: .safety),
        ])

        // --- SETTINGS - Balance Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 303, label: "Early Balancing Threshold", unit: "V", type: .uint16, scale: 0.001, category: .settings, group: .balance),
            BMSRegister(id: 304, label: "Charge Finished Current", unit: "mA", type: .uint16, scale: 1, category: .settings, group: .balance),
            BMSRegister(id: 308, label: "Allowed Disbalance", unit: "mV", type: .uint16, scale: 1, category: .settings, group: .balance),
            BMSRegister(id: 321, label: "Charge Restart Level", unit: "%", type: .uint16, category: .settings, group: .balance),
            BMSRegister(id: 332, label: "Automatic Recovery", unit: "s", type: .uint16, category: .settings, group: .balance),
        ])

        // --- SETTINGS - Hardware Group ---
        regs.append(contentsOf: [
            BMSRegister(id: 310, label: "Charger Startup Delay", unit: "s", type: .uint16, category: .settings, group: .hardware),
            BMSRegister(id: 311, label: "Charger Disable Delay", unit: "s", type: .uint16, category: .settings, group: .hardware),
            BMSRegister(id: 312, label: "Pulses Per Unit", type: .uint32, category: .settings, group: .hardware),
            BMSRegister(id: 330, label: "Charger Type", type: .uint16, category: .settings, group: .hardware),
            BMSRegister(id: 340, label: "Operation Mode", type: .uint16, category: .settings, group: .hardware),
            BMSRegister(id: 343, label: "Protocol", type: .uint16, category: .settings, group: .hardware),
        ])

        // --- VERSION ---
        regs.append(BMSRegister(id: 501, label: "Firmware Version", type: .uint16, category: .version))

        return regs
    }()

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
}
