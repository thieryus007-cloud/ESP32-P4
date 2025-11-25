import Foundation
#if os(macOS)
import ORSSerialPort
#endif

/// Protocole pour la communication avec le TinyBMS
protocol TinyBMSServiceProtocol {
    func connect(portPath: String) async throws
    func disconnect()
    func readRegisters(startAddress: UInt16, count: UInt8) async throws -> [Int: BMSRegisterValue]
    func writeRegister(id: Int, value: Double) async throws
    var isConnected: Bool { get }
}

/// Service principal de communication avec le TinyBMS
@MainActor
class TinyBMSService: ObservableObject {
    @Published var connectionStatus: BMSConnectionStatus = .disconnected
    @Published var availablePorts: [String] = []
    @Published var errorMessage: String?

    private var implementation: TinyBMSServiceProtocol?
    private var pollingTask: Task<Void, Never>?

    let dataStore: BMSDataStore

    init(dataStore: BMSDataStore) {
        self.dataStore = dataStore
    }

    /// Liste les ports série disponibles
    func refreshPorts() {
        #if os(macOS)
        availablePorts = SerialPortManager.shared.availablePorts()
        #else
        availablePorts = ["Simulation Mode"]
        #endif
    }

    /// Se connecter au BMS
    func connect(portPath: String) async {
        do {
            connectionStatus = .connecting
            dataStore.connectionStatus = .connecting

            #if os(macOS)
            if portPath == "Simulation" {
                implementation = BMSSimulator()
                connectionStatus = .simulation
                dataStore.connectionStatus = .simulation
            } else {
                let serialService = SerialBMSService()
                try await serialService.connect(portPath: portPath)
                implementation = serialService
                connectionStatus = .connected
                dataStore.connectionStatus = .connected
            }
            #else
            implementation = BMSSimulator()
            connectionStatus = .simulation
            dataStore.connectionStatus = .simulation
            #endif

            startPolling()
        } catch {
            errorMessage = error.localizedDescription
            dataStore.errorMessage = error.localizedDescription
            connectionStatus = .disconnected
            dataStore.connectionStatus = .disconnected
        }
    }

    /// Se déconnecter du BMS
    func disconnect() {
        pollingTask?.cancel()
        pollingTask = nil
        implementation?.disconnect()
        implementation = nil
        connectionStatus = .disconnected
        dataStore.connectionStatus = .disconnected
    }

    /// Démarrer le polling des données
    private func startPolling() {
        pollingTask?.cancel()

        pollingTask = Task { [weak self] in
            var cycle = 0

            while !Task.isCancelled {
                guard let self = self,
                      let impl = self.implementation else {
                    break
                }

                do {
                    // Lecture des données live (registres 0-56)
                    let liveData = try await impl.readRegisters(startAddress: 0, count: 57)
                    await self.dataStore.updateFromRegisterValues(liveData)

                    // Tous les 5 cycles, lire les statistiques et settings
                    if cycle % 5 == 0 {
                        // Statistics (100-119)
                        let stats = try await impl.readRegisters(startAddress: 100, count: 20)
                        await self.dataStore.updateFromRegisterValues(stats)

                        // Settings (300-344)
                        let settings = try await impl.readRegisters(startAddress: 300, count: 45)
                        await self.dataStore.updateFromRegisterValues(settings)
                    }

                    cycle += 1
                } catch {
                    await MainActor.run {
                        self.errorMessage = error.localizedDescription
                        self.dataStore.errorMessage = error.localizedDescription
                    }
                }

                // Attendre 1 seconde avant le prochain cycle
                try? await Task.sleep(nanoseconds: 1_000_000_000)
            }
        }
    }

    /// Écrire un registre
    func writeRegister(id: Int, value: Double) async throws {
        guard let impl = implementation else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        // Pause polling temporairement
        pollingTask?.cancel()

        try await impl.writeRegister(id: id, value: value)

        // Attendre un peu
        try await Task.sleep(nanoseconds: 200_000_000)

        // Reprendre le polling
        startPolling()
    }

    /// Écrire plusieurs registres
    func writeRegisters(_ changes: [(id: Int, value: Double)]) async throws {
        guard implementation != nil else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        // Pause polling
        pollingTask?.cancel()

        for change in changes {
            try await writeRegister(id: change.id, value: change.value)
            try await Task.sleep(nanoseconds: 100_000_000)
        }

        // Reprendre le polling
        startPolling()
    }
}

#if os(macOS)
/// Service de communication série pour macOS
class SerialBMSService: NSObject, TinyBMSServiceProtocol, ORSSerialPortDelegate {
    private var serialPort: ORSSerialPort?
    private var responseData = Data()
    private var responseContinuation: CheckedContinuation<Data, Error>?

    var isConnected: Bool {
        serialPort?.isOpen ?? false
    }

    func connect(portPath: String) async throws {
        guard let port = ORSSerialPort(path: portPath) else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Invalid port path"])
        }

        port.baudRate = 115200
        port.numberOfDataBits = 8
        port.parity = .none
        port.numberOfStopBits = 1
        port.delegate = self

        return try await withCheckedThrowingContinuation { continuation in
            port.open()
            if port.isOpen {
                self.serialPort = port
                continuation.resume()
            } else {
                continuation.resume(throwing: NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Failed to open port"]))
            }
        }
    }

    func disconnect() {
        serialPort?.close()
        serialPort = nil
    }

    func readRegisters(startAddress: UInt16, count: UInt8) async throws -> [Int: BMSRegisterValue] {
        guard let port = serialPort, port.isOpen else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        let command = ModbusProtocol.createReadCommand(startAddress: startAddress, count: count)

        let response = try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.responseContinuation = continuation
            self.responseData.removeAll()
            port.send(command)

            // Timeout après 800ms
            Task {
                try? await Task.sleep(nanoseconds: 800_000_000)
                if self.responseContinuation != nil {
                    self.responseContinuation?.resume(throwing: NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Timeout"]))
                    self.responseContinuation = nil
                }
            }
        }

        return ModbusProtocol.parseReadResponse(response, startAddress: startAddress)
    }

    func writeRegister(id: Int, value: Double) async throws {
        guard let port = serialPort, port.isOpen else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Not connected"])
        }

        guard let register = RegisterMap.register(for: id) else {
            throw NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Unknown register"])
        }

        let rawValue = ModbusProtocol.encodeValue(value, for: register)
        let command = ModbusProtocol.createWriteCommand(address: UInt16(id), value: rawValue)

        let _ = try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Data, Error>) in
            self.responseContinuation = continuation
            self.responseData.removeAll()
            port.send(command)

            // Timeout après 800ms
            Task {
                try? await Task.sleep(nanoseconds: 800_000_000)
                if self.responseContinuation != nil {
                    self.responseContinuation?.resume(throwing: NSError(domain: "TinyBMS", code: -1, userInfo: [NSLocalizedDescriptionKey: "Timeout"]))
                    self.responseContinuation = nil
                }
            }
        }
    }

    // MARK: - ORSSerialPortDelegate

    func serialPortWasOpened(_ serialPort: ORSSerialPort) {
        print("Serial port opened: \(serialPort.path)")
    }

    func serialPortWasClosed(_ serialPort: ORSSerialPort) {
        print("Serial port closed")
    }

    func serialPort(_ serialPort: ORSSerialPort, didReceive data: Data) {
        responseData.append(data)

        // Vérifier si on a reçu une réponse complète
        if responseData.count >= 3 {
            if responseData[0] == 0xAA && (responseData[1] == 0x03 || responseData[1] == 0x10) {
                let expectedLength: Int
                if responseData[1] == 0x03 {
                    expectedLength = 3 + Int(responseData[2]) + 2
                } else {
                    expectedLength = 8 // Write response is fixed 8 bytes
                }

                if responseData.count >= expectedLength {
                    responseContinuation?.resume(returning: responseData)
                    responseContinuation = nil
                }
            }
        }
    }

    func serialPort(_ serialPort: ORSSerialPort, didEncounterError error: Error) {
        print("Serial port error: \(error)")
        responseContinuation?.resume(throwing: error)
        responseContinuation = nil
    }
}

/// Manager pour les ports série
class SerialPortManager {
    static let shared = SerialPortManager()

    func availablePorts() -> [String] {
        var ports = ORSSerialPortManager.shared().availablePorts.map { $0.path }
        ports.insert("Simulation", at: 0)
        return ports
    }
}
#endif
