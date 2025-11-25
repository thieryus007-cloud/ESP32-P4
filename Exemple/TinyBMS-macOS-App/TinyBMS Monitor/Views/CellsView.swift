import SwiftUI

struct CellsView: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Summary
                CellSummaryCard()

                // Grid of cells
                LazyVGrid(columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible()),
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ], spacing: 16) {
                    ForEach(0..<16) { index in
                        CellCard(
                            cellNumber: index + 1,
                            voltage: dataStore.liveData.cellVoltages[index],
                            isMin: dataStore.liveData.cellVoltages[index] == dataStore.liveData.minCellVoltage,
                            isMax: dataStore.liveData.cellVoltages[index] == dataStore.liveData.maxCellVoltage,
                            isBalancing: dataStore.liveData.balancingCells[index]
                        )
                    }
                }
            }
            .padding()
        }
        .navigationTitle("Cell Voltages")
    }
}

struct CellSummaryCard: View {
    @EnvironmentObject var dataStore: BMSDataStore

    var body: some View {
        VStack(spacing: 12) {
            Text("Cell Statistics")
                .font(.headline)

            HStack(spacing: 20) {
                StatBox(label: "Minimum", value: String(format: "%.4f V", dataStore.liveData.minCellVoltage), color: .blue)
                StatBox(label: "Maximum", value: String(format: "%.4f V", dataStore.liveData.maxCellVoltage), color: .red)
                StatBox(label: "Delta", value: String(format: "%.4f V", dataStore.liveData.deltaCellVoltage), color: .orange)
                StatBox(label: "Average", value: String(format: "%.4f V", averageVoltage), color: .green)
            }
        }
        .padding()
        .background(Color(.secondarySystemBackground))
        .cornerRadius(12)
    }

    private var averageVoltage: Double {
        let sum = dataStore.liveData.cellVoltages.reduce(0, +)
        return sum / Double(dataStore.liveData.cellVoltages.count)
    }
}

struct StatBox: View {
    let label: String
    let value: String
    let color: Color

    var body: some View {
        VStack(spacing: 4) {
            Text(label)
                .font(.caption)
                .foregroundColor(.secondary)
            Text(value)
                .font(.title3)
                .bold()
                .foregroundColor(color)
        }
        .frame(maxWidth: .infinity)
        .padding()
        .background(Color(.tertiarySystemBackground))
        .cornerRadius(8)
    }
}

struct CellCard: View {
    let cellNumber: Int
    let voltage: Double
    let isMin: Bool
    let isMax: Bool
    let isBalancing: Bool

    var body: some View {
        VStack(spacing: 8) {
            HStack {
                Text("Cell \(cellNumber)")
                    .font(.caption)
                    .foregroundColor(.secondary)
                Spacer()
                if isBalancing {
                    Image(systemName: "arrow.triangle.2.circlepath")
                        .font(.caption)
                        .foregroundColor(.orange)
                }
            }

            Text(String(format: "%.4f V", voltage))
                .font(.title3)
                .bold()
                .foregroundColor(cellColor)

            // Health indicator
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.gray.opacity(0.2))
                        .frame(height: 6)

                    RoundedRectangle(cornerRadius: 4)
                        .fill(cellColor)
                        .frame(width: geometry.size.width * healthPercentage, height: 6)
                }
            }
            .frame(height: 6)
        }
        .padding()
        .background(backgroundColor)
        .cornerRadius(12)
        .overlay(
            RoundedRectangle(cornerRadius: 12)
                .stroke(borderColor, lineWidth: isMin || isMax ? 2 : 0)
        )
    }

    private var healthPercentage: CGFloat {
        let minVoltage = 3.0
        let maxVoltage = 4.2
        let range = maxVoltage - minVoltage
        return CGFloat((voltage - minVoltage) / range)
    }

    private var cellColor: Color {
        if isMin {
            return .blue
        } else if isMax {
            return .red
        } else if voltage < 3.3 {
            return .orange
        }
        return .green
    }

    private var backgroundColor: Color {
        if isBalancing {
            return Color.orange.opacity(0.1)
        }
        return Color(.tertiarySystemBackground)
    }

    private var borderColor: Color {
        if isMin {
            return .blue
        } else if isMax {
            return .red
        }
        return .clear
    }
}
