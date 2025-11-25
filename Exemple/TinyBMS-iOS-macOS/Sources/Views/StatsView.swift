import SwiftUI

struct StatsView: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                Text("Statistics")
                    .font(.title2)
                    .bold()

                // General Stats
                StatCard(
                    title: "General",
                    icon: "chart.bar.fill",
                    items: [
                        StatItem(label: "Total Distance", value: String(format: "%.2f km", dataStore.statistics.totalDistance), icon: "road.lanes"),
                        StatItem(label: "State of Health", value: String(format: "%.1f%%", dataStore.liveData.stateOfHealth), icon: "heart.fill"),
                        StatItem(label: "Firmware Version", value: formatFirmwareVersion(dataStore.firmwareVersion), icon: "info.circle"),
                    ]
                )

                // Fault Statistics
                StatCard(
                    title: "Fault History",
                    icon: "exclamationmark.triangle.fill",
                    items: [
                        StatItem(label: "Over-Voltage Events", value: "\(dataStore.statistics.overVoltageCount)", icon: "bolt.fill", color: .red),
                        StatItem(label: "Under-Voltage Events", value: "\(dataStore.statistics.underVoltageCount)", icon: "bolt.slash.fill", color: .orange),
                    ]
                )

                // Charging Statistics
                StatCard(
                    title: "Charging",
                    icon: "battery.100.bolt",
                    items: [
                        StatItem(label: "Total Charge Cycles", value: "\(dataStore.statistics.chargingCount)", icon: "arrow.clockwise"),
                        StatItem(label: "Full Charge Cycles", value: "\(dataStore.statistics.fullChargeCount)", icon: "checkmark.circle.fill", color: .green),
                    ]
                )

                // Live Statistics
                StatCard(
                    title: "Current Session",
                    icon: "clock.fill",
                    items: [
                        StatItem(label: "Last Update", value: formatDate(dataStore.lastUpdateTime), icon: "clock"),
                        StatItem(label: "Connection Status", value: dataStore.connectionStatus.rawValue, icon: "antenna.radiowaves.left.and.right", color: connectionColor),
                    ]
                )
            }
            .padding()
        }
        .navigationTitle("Statistics")
    }

    private func formatFirmwareVersion(_ version: UInt16) -> String {
        let major = version / 100
        let minor = version % 100
        return "\(major).\(String(format: "%02d", minor))"
    }

    private func formatDate(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.dateStyle = .none
        formatter.timeStyle = .medium
        return formatter.string(from: date)
    }

    private var connectionColor: Color {
        switch dataStore.connectionStatus {
        case .connected: return .green
        case .simulation: return .blue
        case .connecting: return .orange
        case .disconnected: return .gray
        }
    }
}

struct StatCard: View {
    let title: String
    let icon: String
    var items: [StatItem]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: icon)
                    .foregroundColor(.blue)
                Text(title)
                    .font(.headline)
            }

            Divider()

            VStack(spacing: 12) {
                ForEach(items) { item in
                    StatItemRow(item: item)
                }
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }
}

struct StatItem: Identifiable {
    let id = UUID()
    let label: String
    let value: String
    let icon: String
    var color: Color = .primary
}

struct StatItemRow: View {
    let item: StatItem

    var body: some View {
        HStack {
            Image(systemName: item.icon)
                .foregroundColor(item.color)
                .frame(width: 24)

            Text(item.label)
                .font(.subheadline)

            Spacer()

            Text(item.value)
                .font(.body)
                .bold()
                .foregroundColor(item.color)
        }
        .padding(.vertical, 4)
    }
}
