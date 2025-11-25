import Foundation

/// Simulateur de TinyBMS pour les tests et iOS
class BMSSimulator: TinyBMSServiceProtocol {
    private var _isConnected = false
    private var simulatedData: [Int: Double] = [:]
    private var simulationTime: TimeInterval = 0

    var isConnected: Bool {
        _isConnected
    }

    init() {
        initializeSimulatedData()
    }

    func connect(portPath: String) async throws {
        _isConnected = true
        // Pas besoin de vraie connexion en simulation
    }

    func disconnect() {
        _isConnected = false
    }

    func readRegisters(startAddress: UInt16, count: UInt8) async throws -> [Int: BMSRegisterValue] {
        guard _isConnected else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        // Mise à jour de la simulation
        tick()

        var result: [Int: BMSRegisterValue] = [:]

        for i in 0..<Int(count) {
            let regId = Int(startAddress) + i
            guard let register = RegisterMap.register(for: regId) else { continue }

            let value = simulatedData[regId] ?? 0.0

            result[regId] = BMSRegisterValue(
                id: regId,
                label: register.label,
                value: value,
                unit: register.unit ?? "",
                category: register.category,
                group: register.group
            )
        }

        return result
    }

    func writeRegister(id: Int, value: Double) async throws {
        guard _isConnected else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        simulatedData[id] = value
    }

    // MARK: - Simulation Logic

    private func initializeSimulatedData() {
        // Cellules (0-15) - Autour de 3.7V
        for i in 0..<16 {
            simulatedData[i] = 3.7 + Double.random(in: -0.05...0.05)
        }

        // Pack voltage (36)
        simulatedData[36] = 59.2

        // Pack current (38) - Varie entre -20A et +20A
        simulatedData[38] = 0.0

        // Min/Max cell (40, 41)
        simulatedData[40] = 3.65
        simulatedData[41] = 3.75

        // Températures (42, 43, 48)
        simulatedData[42] = 25.0
        simulatedData[43] = 24.5
        simulatedData[48] = 26.0

        // SOH (45)
        simulatedData[45] = 98.5

        // SOC (46)
        simulatedData[46] = 75.0

        // BMS Status (50) - 0 = Idle
        simulatedData[50] = 0

        // Balancing (52)
        simulatedData[52] = 0

        // Statistics
        simulatedData[101] = 1234.56 // Total distance
        simulatedData[105] = 2 // Under-voltage count
        simulatedData[106] = 1 // Over-voltage count
        simulatedData[111] = 45 // Charging count
        simulatedData[112] = 30 // Full charge count

        // Settings - Battery
        simulatedData[300] = 4.2 // Fully charged voltage
        simulatedData[301] = 3.0 // Fully discharged voltage
        simulatedData[306] = 100.0 // Battery capacity
        simulatedData[307] = 16 // Series cells count
        simulatedData[322] = 2000 // Max cycles
        simulatedData[328] = 75.0 // Manual SOC

        // Settings - Safety
        simulatedData[315] = 4.25 // Over-voltage cutoff
        simulatedData[316] = 2.8 // Under-voltage cutoff
        simulatedData[317] = 100 // Discharge over-current
        simulatedData[318] = 50 // Charge over-current
        simulatedData[305] = 150 // Peak discharge current
        simulatedData[319] = 60 // Over-heat cutoff
        simulatedData[320] = 0 // Low temp charge cutoff

        // Settings - Balance
        simulatedData[303] = 3.5 // Early balancing threshold
        simulatedData[304] = 200 // Charge finished current
        simulatedData[308] = 50 // Allowed disbalance
        simulatedData[321] = 95 // Charge restart level
        simulatedData[332] = 30 // Automatic recovery

        // Settings - Hardware
        simulatedData[310] = 5 // Charger startup delay
        simulatedData[311] = 10 // Charger disable delay
        simulatedData[312] = 1000 // Pulses per unit
        simulatedData[330] = 0 // Charger type
        simulatedData[340] = 0 // Operation mode (Dual)
        simulatedData[343] = 0 // Protocol

        // Version
        simulatedData[501] = 304 // Firmware version 3.04
    }

    private func tick() {
        simulationTime += 1

        // Simuler des variations dynamiques

        // Courant qui varie
        let currentPhase = sin(simulationTime * 0.1) * 15.0
        simulatedData[38] = currentPhase

        // SOC qui diminue/augmente selon le courant
        if let currentSOC = simulatedData[46] {
            let socDelta = -currentPhase * 0.001
            simulatedData[46] = max(0, min(100, currentSOC + socDelta))
        }

        // Tension des cellules qui fluctue légèrement
        for i in 0..<16 {
            if let voltage = simulatedData[i] {
                let noise = Double.random(in: -0.002...0.002)
                simulatedData[i] = max(3.0, min(4.2, voltage + noise))
            }
        }

        // Recalculer min/max
        let cellVoltages = (0..<16).compactMap { simulatedData[$0] }
        if let minV = cellVoltages.min() {
            simulatedData[40] = minV
        }
        if let maxV = cellVoltages.max() {
            simulatedData[41] = maxV
        }

        // Pack voltage = somme des cellules
        let packV = cellVoltages.reduce(0.0, +)
        simulatedData[36] = packV

        // Température qui varie légèrement
        if let temp1 = simulatedData[42] {
            simulatedData[42] = temp1 + Double.random(in: -0.1...0.1)
        }
        if let temp2 = simulatedData[43] {
            simulatedData[43] = temp2 + Double.random(in: -0.1...0.1)
        }
        if let tempInt = simulatedData[48] {
            simulatedData[48] = tempInt + Double.random(in: -0.1...0.1)
        }

        // BMS Status basé sur le courant
        if let current = simulatedData[38] {
            if current > 1.0 {
                simulatedData[50] = 1 // Charging
            } else if current < -1.0 {
                simulatedData[50] = 2 // Discharging
            } else {
                simulatedData[50] = 0 // Idle
            }
        }

        // Balancing aléatoire occasionnel
        if Int.random(in: 0..<20) == 0 {
            simulatedData[52] = UInt16.random(in: 0...0xFFFF)
        } else {
            simulatedData[52] = 0
        }
    }
}
