import Foundation

/// État de la connexion au BMS
enum BMSConnectionStatus: String {
    case disconnected = "Disconnected"
    case connecting = "Connecting"
    case connected = "Connected"
    case simulation = "Simulation"
}

/// Données en temps réel du BMS
struct BMSLiveData {
    var cellVoltages: [Double] = Array(repeating: 0.0, count: 16)
    var packVoltage: Double = 0.0
    var packCurrent: Double = 0.0
    var minCellVoltage: Double = 0.0
    var maxCellVoltage: Double = 0.0
    var deltaCellVoltage: Double {
        maxCellVoltage - minCellVoltage
    }
    var tempSensor1: Double = 0.0
    var tempSensor2: Double = 0.0
    var internalTemp: Double = 0.0
    var stateOfCharge: Double = 0.0
    var stateOfHealth: Double = 0.0
    var bmsStatus: UInt16 = 0
    var realBalancing: UInt16 = 0

    var bmsStatusString: String {
        switch bmsStatus {
        case 0: return "Idle"
        case 1: return "Charging"
        case 2: return "Discharging"
        case 3: return "Regeneration"
        case 4: return "Fault"
        default: return "Unknown"
        }
    }

    var isBalancing: Bool {
        realBalancing != 0
    }

    var balancingCells: [Bool] {
        (0..<16).map { bit in
            (realBalancing & (1 << bit)) != 0
        }
    }
}

/// Statistiques du BMS
struct BMSStatistics {
    var totalDistance: Double = 0.0
    var underVoltageCount: UInt16 = 0
    var overVoltageCount: UInt16 = 0
    var chargingCount: UInt16 = 0
    var fullChargeCount: UInt16 = 0
}

/// Configuration du BMS (read/write)
struct BMSSettings {
    // Battery Group
    var fullyChargedVoltage: Double = 0.0
    var fullyDischargedVoltage: Double = 0.0
    var batteryCapacity: Double = 0.0
    var seriesCellsCount: UInt16 = 0
    var maxCyclesCount: UInt16 = 0
    var manualSOCSet: Double = 0.0

    // Safety Group
    var overVoltageCutoff: Double = 0.0
    var underVoltageCutoff: Double = 0.0
    var dischargeOverCurrent: UInt16 = 0
    var chargeOverCurrent: UInt16 = 0
    var peakDischargeCurrent: UInt16 = 0
    var overHeatCutoff: Int16 = 0
    var lowTempChargeCutoff: Int16 = 0

    // Balance Group
    var earlyBalancingThreshold: Double = 0.0
    var chargeFinishedCurrent: UInt16 = 0
    var allowedDisbalance: UInt16 = 0
    var chargeRestartLevel: UInt16 = 0
    var automaticRecovery: UInt16 = 0

    // Hardware Group
    var chargerStartupDelay: UInt16 = 0
    var chargerDisableDelay: UInt16 = 0
    var pulsesPerUnit: UInt32 = 0
    var chargerType: UInt16 = 0
    var operationMode: UInt16 = 0
    var protocol: UInt16 = 0

    var operationModeString: String {
        operationMode == 0 ? "Dual" : "Single"
    }
}

/// Données complètes du BMS (Observable)
@MainActor
class BMSDataStore: ObservableObject {
    @Published var connectionStatus: BMSConnectionStatus = .disconnected
    @Published var liveData = BMSLiveData()
    @Published var statistics = BMSStatistics()
    @Published var settings = BMSSettings()
    @Published var firmwareVersion: UInt16 = 0
    @Published var lastUpdateTime = Date()
    @Published var errorMessage: String?

    func updateFromRegisterValues(_ values: [Int: BMSRegisterValue]) {
        for (id, regValue) in values {
            updateValue(registerId: id, value: regValue.value)
        }
        lastUpdateTime = Date()
    }

    private func updateValue(registerId: Int, value: Double) {
        // Cellules 0-15
        if registerId >= 0 && registerId < 16 {
            liveData.cellVoltages[registerId] = value
        }

        // Live Data
        switch registerId {
        case 36: liveData.packVoltage = value
        case 38: liveData.packCurrent = value
        case 40: liveData.minCellVoltage = value
        case 41: liveData.maxCellVoltage = value
        case 42: liveData.tempSensor1 = value
        case 43: liveData.tempSensor2 = value
        case 45: liveData.stateOfHealth = value
        case 46: liveData.stateOfCharge = value
        case 48: liveData.internalTemp = value
        case 50: liveData.bmsStatus = UInt16(value)
        case 52: liveData.realBalancing = UInt16(value)

        // Statistics
        case 101: statistics.totalDistance = value
        case 105: statistics.underVoltageCount = UInt16(value)
        case 106: statistics.overVoltageCount = UInt16(value)
        case 111: statistics.chargingCount = UInt16(value)
        case 112: statistics.fullChargeCount = UInt16(value)

        // Settings - Battery
        case 300: settings.fullyChargedVoltage = value
        case 301: settings.fullyDischargedVoltage = value
        case 306: settings.batteryCapacity = value
        case 307: settings.seriesCellsCount = UInt16(value)
        case 322: settings.maxCyclesCount = UInt16(value)
        case 328: settings.manualSOCSet = value

        // Settings - Safety
        case 315: settings.overVoltageCutoff = value
        case 316: settings.underVoltageCutoff = value
        case 317: settings.dischargeOverCurrent = UInt16(value)
        case 318: settings.chargeOverCurrent = UInt16(value)
        case 305: settings.peakDischargeCurrent = UInt16(value)
        case 319: settings.overHeatCutoff = Int16(value)
        case 320: settings.lowTempChargeCutoff = Int16(value)

        // Settings - Balance
        case 303: settings.earlyBalancingThreshold = value
        case 304: settings.chargeFinishedCurrent = UInt16(value)
        case 308: settings.allowedDisbalance = UInt16(value)
        case 321: settings.chargeRestartLevel = UInt16(value)
        case 332: settings.automaticRecovery = UInt16(value)

        // Settings - Hardware
        case 310: settings.chargerStartupDelay = UInt16(value)
        case 311: settings.chargerDisableDelay = UInt16(value)
        case 312: settings.pulsesPerUnit = UInt32(value)
        case 330: settings.chargerType = UInt16(value)
        case 340: settings.operationMode = UInt16(value)
        case 343: settings.protocol = UInt16(value)

        // Version
        case 501: firmwareVersion = UInt16(value)

        default: break
        }
    }
}
