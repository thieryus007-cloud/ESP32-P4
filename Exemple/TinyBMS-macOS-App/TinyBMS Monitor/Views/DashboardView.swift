import SwiftUI

struct DashboardView: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Header Info Summary
                BMSInfoSummaryCard()

                // Gauges Row
                HStack(spacing: 16) {
                    BatteryStatusGauge()
                    BatteryMonitorGauge()
                    TemperatureGauge()
                }
                .frame(height: 200)

                // Pack Overview
                PackOverviewCard()

                // Cell Voltages Chart
                CellVoltagesCard()
            }
            .padding()
        }
        .navigationTitle("Dashboard")
    }
}

// MARK: - Info Summary Card
struct BMSInfoSummaryCard: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("BMS Information")
                .font(.headline)

            LazyVGrid(columns: [
                GridItem(.flexible()),
                GridItem(.flexible()),
                GridItem(.flexible()),
                GridItem(.flexible())
            ], spacing: 12) {
                InfoItem(label: "Operation Mode", value: dataStore.settings.operationModeString, register: "[340]")
                InfoItem(label: "Battery Capacity", value: String(format: "%.1f Ah", dataStore.settings.batteryCapacity), register: "[306]")
                InfoItem(label: "Over-Voltage Cutoff", value: String(format: "%.3f V", dataStore.settings.overVoltageCutoff), register: "[315]")
                InfoItem(label: "Under-Voltage Cutoff", value: String(format: "%.3f V", dataStore.settings.underVoltageCutoff), register: "[316]")
                InfoItem(label: "Discharge OC Limit", value: "\(dataStore.settings.dischargeOverCurrent) A", register: "[317]")
                InfoItem(label: "Charge OC Limit", value: "\(dataStore.settings.chargeOverCurrent) A", register: "[318]")
                InfoItem(label: "Peak Discharge", value: "\(dataStore.settings.peakDischargeCurrent) A", register: "[305]")
                InfoItem(label: "BMS Status", value: dataStore.liveData.bmsStatusString, register: "[50]", highlighted: true)
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct InfoItem: View {
    let label: String
    let value: String
    let register: String
    var highlighted: Bool = false

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
            Text(value)
                .font(.body)
                .bold()
                .foregroundColor(highlighted ? .blue : .primary)
            Text(register)
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        .padding(8)
        .background(Color(.tertiarySystemBackground))
        .cornerRadius(8)
    }
}

// MARK: - Gauges
struct BatteryStatusGauge: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack {
            Text("Battery Status")
                .font(.headline)

            ZStack {
                Circle()
                    .stroke(Color.gray.opacity(0.2), lineWidth: 20)

                Circle()
                    .trim(from: 0, to: dataStore.liveData.stateOfCharge / 100)
                    .stroke(
                        LinearGradient(gradient: Gradient(colors: [.green, .yellow, .red]),
                                       startPoint: .leading,
                                       endPoint: .trailing),
                        style: StrokeStyle(lineWidth: 20, lineCap: .round)
                    )
                    .rotationEffect(.degrees(-90))

                VStack {
                    Text(String(format: "%.1f%%", dataStore.liveData.stateOfCharge))
                        .font(.title2)
                        .bold()
                    Text("SOC")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            .padding()
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct BatteryMonitorGauge: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack {
            Text("Battery Monitor")
                .font(.headline)

            VStack(spacing: 8) {
                HStack {
                    VStack(alignment: .leading) {
                        Text("Voltage")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text(String(format: "%.2f V", dataStore.liveData.packVoltage))
                            .font(.title3)
                            .bold()
                    }
                    Spacer()
                }

                HStack {
                    VStack(alignment: .leading) {
                        Text("Current")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text(String(format: "%.2f A", dataStore.liveData.packCurrent))
                            .font(.title3)
                            .bold()
                            .foregroundColor(currentColor)
                    }
                    Spacer()
                }

                HStack {
                    VStack(alignment: .leading) {
                        Text("Power")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text(String(format: "%.1f W", dataStore.liveData.packVoltage * dataStore.liveData.packCurrent))
                            .font(.title3)
                            .bold()
                    }
                    Spacer()
                }
            }
            .padding()
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }

    private var currentColor: Color {
        if dataStore.liveData.packCurrent > 0 {
            return .green
        } else if dataStore.liveData.packCurrent < 0 {
            return .red
        }
        return .primary
    }
}

struct TemperatureGauge: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack {
            Text("Temperatures")
                .font(.headline)

            VStack(spacing: 8) {
                TempRow(label: "Sensor 1", value: dataStore.liveData.tempSensor1)
                TempRow(label: "Sensor 2", value: dataStore.liveData.tempSensor2)
                TempRow(label: "Internal", value: dataStore.liveData.internalTemp)
            }
            .padding()
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct TempRow: View {
    let label: String
    let value: Double

    var body: some View {
        HStack {
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
            Spacer()
            Text(String(format: "%.1f°C", value))
                .font(.body)
                .bold()
                .foregroundColor(tempColor)
        }
    }

    private var tempColor: Color {
        if value > 50 {
            return .red
        } else if value > 40 {
            return .orange
        }
        return .primary
    }
}

// MARK: - Pack Overview
struct PackOverviewCard: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Pack Overview")
                .font(.headline)

            HStack(spacing: 20) {
                VStack(alignment: .leading) {
                    Text("SOH")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Text(String(format: "%.1f%%", dataStore.liveData.stateOfHealth))
                        .font(.title2)
                        .bold()
                }

                Divider()

                VStack(alignment: .leading) {
                    Text("Balancing")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Text(dataStore.liveData.isBalancing ? "Active" : "Inactive")
                        .font(.title3)
                        .bold()
                        .foregroundColor(dataStore.liveData.isBalancing ? .orange : .secondary)
                }

                Spacer()
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

// MARK: - Cell Voltages Card
struct CellVoltagesCard: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Cell Voltages")
                    .font(.headline)

                Spacer()

                HStack(spacing: 12) {
                    LegendItem(color: .blue, label: "Min")
                    LegendItem(color: .red, label: "Max")
                    LegendItem(color: .orange, label: "Balancing")
                }
            }

            // Simplified bar chart
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(alignment: .bottom, spacing: 8) {
                    ForEach(0..<16) { index in
                        CellBar(
                            cellNumber: index + 1,
                            voltage: dataStore.liveData.cellVoltages[index],
                            isMin: dataStore.liveData.cellVoltages[index] == dataStore.liveData.minCellVoltage,
                            isMax: dataStore.liveData.cellVoltages[index] == dataStore.liveData.maxCellVoltage,
                            isBalancing: dataStore.liveData.balancingCells[index]
                        )
                    }
                }
                .padding(.vertical)
            }

            // Stats
            HStack {
                StatLabel(label: "Min", value: String(format: "%.4f V", dataStore.liveData.minCellVoltage), color: .blue)
                StatLabel(label: "Max", value: String(format: "%.4f V", dataStore.liveData.maxCellVoltage), color: .red)
                StatLabel(label: "Delta", value: String(format: "%.4f V", dataStore.liveData.deltaCellVoltage), color: .orange)
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct CellBar: View {
    let cellNumber: Int
    let voltage: Double
    let isMin: Bool
    let isMax: Bool
    let isBalancing: Bool

    var body: some View {
        VStack {
            Text(String(format: "%.3f", voltage))
                .font(.caption2)
                .foregroundColor(.secondary)

            RoundedRectangle(cornerRadius: 4)
                .fill(barColor)
                .frame(width: 30, height: normalizedHeight)

            Text("\(cellNumber)")
                .font(.caption)
                .foregroundColor(.secondary)
        }
    }

    private var normalizedHeight: CGFloat {
        let minVoltage = 3.0
        let maxVoltage = 4.2
        let range = maxVoltage - minVoltage
        let normalized = (voltage - minVoltage) / range
        return CGFloat(normalized) * 100 + 20
    }

    private var barColor: Color {
        if isMin {
            return .blue
        } else if isMax {
            return .red
        } else if isBalancing {
            return .orange
        }
        return .green
    }
}

struct LegendItem: View {
    let color: Color
    let label: String

    var body: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(color)
                .frame(width: 8, height: 8)
            Text(label)
                .font(.caption)
        }
    }
}

struct StatLabel: View {
    let label: String
    let value: String
    let color: Color

    var body: some View {
        HStack {
            Text("\(label):")
                .font(.caption)
                .foregroundColor(.secondary)
            Text(value)
                .font(.caption)
                .bold()
                .foregroundColor(color)
        }
    }
}
