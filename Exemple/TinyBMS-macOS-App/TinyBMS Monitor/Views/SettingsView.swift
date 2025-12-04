import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var dataStore: BMSDataStore
    @EnvironmentObject var bmsService: TinyBMSService

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                Text("Configuration (Read/Write)")
                    .font(.title2)
                    .bold()

                // Battery Settings
                SettingsGroupCard(
                    title: "Battery",
                    icon: "battery.100",
                    group: .battery
                )

                // Safety Settings
                SettingsGroupCard(
                    title: "Safety",
                    icon: "shield.fill",
                    group: .safety
                )

                // Balance Settings
                SettingsGroupCard(
                    title: "Balance",
                    icon: "scale.3d",
                    group: .balance
                )

                // Hardware Settings
                SettingsGroupCard(
                    title: "Hardware",
                    icon: "cpu",
                    group: .hardware
                )
            }
            .padding()
        }
        .navigationTitle("Settings")
    }
}

struct SettingsGroupCard: View {
    @EnvironmentObject var dataStore: BMSDataStore
    @EnvironmentObject var bmsService: TinyBMSService

    let title: String
    let icon: String
    let group: SettingsGroup

    @State private var editedValues: [Int: String] = [:]
    @State private var isSaving = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: icon)
                    .foregroundColor(.blue)
                Text(title)
                    .font(.headline)

                Spacer()

                Button(action: saveChanges) {
                    if isSaving {
                        ProgressView()
                            .scaleEffect(0.8)
                    } else {
                        Text("Save")
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(editedValues.isEmpty || isSaving)
            }

            Divider()

            VStack(spacing: 12) {
                ForEach(RegisterMap.registers(for: group)) { register in
                    SettingRow(
                        register: register,
                        currentValue: getCurrentValue(for: register.id),
                        editedValue: Binding(
                            get: { editedValues[register.id] ?? "" },
                            set: { editedValues[register.id] = $0 }
                        )
                    )
                }
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }

    private func getCurrentValue(for registerId: Int) -> String {
        switch registerId {
        case 300: return String(format: "%.3f", dataStore.settings.fullyChargedVoltage)
        case 301: return String(format: "%.3f", dataStore.settings.fullyDischargedVoltage)
        case 306: return String(format: "%.2f", dataStore.settings.batteryCapacity)
        case 307: return "\(dataStore.settings.seriesCellsCount)"
        case 322: return "\(dataStore.settings.maxCyclesCount)"
        case 328: return String(format: "%.1f", dataStore.settings.manualSOCSet)

        case 315: return String(format: "%.3f", dataStore.settings.overVoltageCutoff)
        case 316: return String(format: "%.3f", dataStore.settings.underVoltageCutoff)
        case 317: return "\(dataStore.settings.dischargeOverCurrent)"
        case 318: return "\(dataStore.settings.chargeOverCurrent)"
        case 305: return "\(dataStore.settings.peakDischargeCurrent)"
        case 319: return "\(dataStore.settings.overHeatCutoff)"
        case 320: return "\(dataStore.settings.lowTempChargeCutoff)"

        case 303: return String(format: "%.3f", dataStore.settings.earlyBalancingThreshold)
        case 304: return "\(dataStore.settings.chargeFinishedCurrent)"
        case 308: return "\(dataStore.settings.allowedDisbalance)"
        case 321: return "\(dataStore.settings.chargeRestartLevel)"
        case 332: return "\(dataStore.settings.automaticRecovery)"

        case 310: return "\(dataStore.settings.chargerStartupDelay)"
        case 311: return "\(dataStore.settings.chargerDisableDelay)"
        case 312: return "\(dataStore.settings.pulsesPerUnit)"
        case 330: return "\(dataStore.settings.chargerType)"
        case 340: return "\(dataStore.settings.operationMode)"
        case 343: return "\(dataStore.settings.protocol)"

        default: return "--"
        }
    }

    private func saveChanges() {
        isSaving = true

        Task {
            do {
                let changes = editedValues.compactMap { (id, valueStr) -> (id: Int, value: Double)? in
                    guard let value = Double(valueStr) else { return nil }
                    return (id, value)
                }

                try await bmsService.writeRegisters(changes)

                await MainActor.run {
                    editedValues.removeAll()
                    isSaving = false
                }
            } catch {
                await MainActor.run {
                    dataStore.errorMessage = error.localizedDescription
                    isSaving = false
                }
            }
        }
    }
}

struct SettingRow: View {
    let register: BMSRegister
    let currentValue: String
    @Binding var editedValue: String

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text(register.label)
                    .font(.subheadline)
                Text("[\(register.id)]")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }

            Spacer()

            HStack {
                Text(currentValue)
                    .font(.body)
                    .foregroundColor(.secondary)
                    .frame(width: 80, alignment: .trailing)

                TextField("New value", text: $editedValue)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 100)
                    #if os(iOS)
                    .keyboardType(.decimalPad)
                    #endif

                if let unit = register.unit, !unit.isEmpty {
                    Text(unit)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .frame(width: 40, alignment: .leading)
                }
            }
        }
        .padding(.vertical, 4)
    }
}
