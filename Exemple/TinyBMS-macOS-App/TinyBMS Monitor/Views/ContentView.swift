import SwiftUI

struct ContentView: View {
    @EnvironmentObject var dataStore: BMSDataStore
    @EnvironmentObject var bmsService: TinyBMSService
    @State private var selectedTab = 0

    var body: some View {
        #if os(macOS)
        NavigationSplitView {
            SidebarView(selectedTab: $selectedTab)
        } detail: {
            TabContentView(selectedTab: selectedTab)
        }
        #else
        TabView(selection: $selectedTab) {
            DashboardView()
                .tabItem {
                    Label("Dashboard", systemImage: "chart.bar.fill")
                }
                .tag(0)

            CellsView()
                .tabItem {
                    Label("Cells", systemImage: "battery.100")
                }
                .tag(1)

            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gearshape.fill")
                }
                .tag(2)

            StatsView()
                .tabItem {
                    Label("Stats", systemImage: "chart.line.uptrend.xyaxis")
                }
                .tag(3)
        }
        #endif
    }
}

// MARK: - macOS Sidebar
struct SidebarView: View {
    @EnvironmentObject var dataStore: BMSDataStore
    @EnvironmentObject var bmsService: TinyBMSService
    @Binding var selectedTab: Int

    var body: some View {
        VStack(spacing: 0) {
            // Header
            VStack(spacing: 8) {
                HStack {
                    Image(systemName: "bolt.fill")
                        .font(.title)
                        .foregroundColor(.yellow)
                    Text("TinyBMS")
                        .font(.title2)
                        .bold()
                }
                .padding(.top)

                ConnectionStatusBadge(status: dataStore.connectionStatus)
            }
            .padding(.bottom)

            Divider()

            // Navigation
            List(selection: $selectedTab) {
                Label("Dashboard", systemImage: "chart.bar.fill")
                    .tag(0)
                Label("Cells", systemImage: "battery.100")
                    .tag(1)
                Label("Settings", systemImage: "gearshape.fill")
                    .tag(2)
                Label("Statistics", systemImage: "chart.line.uptrend.xyaxis")
                    .tag(3)
            }
            .listStyle(.sidebar)

            Divider()

            // Connection Controls
            ConnectionControlsView()
                .padding()
        }
        .frame(minWidth: 200)
    }
}

// MARK: - Tab Content
struct TabContentView: View {
    let selectedTab: Int

    var body: some View {
        Group {
            switch selectedTab {
            case 0: DashboardView()
            case 1: CellsView()
            case 2: SettingsView()
            case 3: StatsView()
            default: DashboardView()
            }
        }
    }
}

// MARK: - Connection Status Badge
struct ConnectionStatusBadge: View {
    let status: BMSConnectionStatus

    var body: some View {
        HStack {
            Circle()
                .fill(statusColor)
                .frame(width: 10, height: 10)
            Text(status.rawValue.uppercased())
                .font(.caption)
                .bold()
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(statusColor.opacity(0.2))
        .cornerRadius(12)
    }

    private var statusColor: Color {
        switch status {
        case .connected: return .green
        case .simulation: return .blue
        case .connecting: return .orange
        case .disconnected: return .gray
        }
    }
}

// MARK: - Connection Controls
struct ConnectionControlsView: View {
    @EnvironmentObject var bmsService: TinyBMSService
    @State private var selectedPort: String = ""
    @State private var showingPortPicker = false

    var body: some View {
        VStack(spacing: 12) {
            Button(action: {
                bmsService.refreshPorts()
                showingPortPicker = true
            }) {
                HStack {
                    Image(systemName: "externaldrive.connected.to.line.below")
                    Text(selectedPort.isEmpty ? "Select Port" : selectedPort)
                        .lineLimit(1)
                }
                .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)

            if bmsService.connectionStatus == .disconnected {
                Button("Connect") {
                    Task {
                        await bmsService.connect(portPath: selectedPort.isEmpty ? "Simulation" : selectedPort)
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(selectedPort.isEmpty)
            } else {
                Button("Disconnect") {
                    bmsService.disconnect()
                }
                .buttonStyle(.bordered)
                .tint(.red)
            }
        }
        .sheet(isPresented: $showingPortPicker) {
            PortPickerView(selectedPort: $selectedPort, isPresented: $showingPortPicker)
        }
        .onAppear {
            bmsService.refreshPorts()
        }
    }
}

// MARK: - Port Picker
struct PortPickerView: View {
    @EnvironmentObject var bmsService: TinyBMSService
    @Binding var selectedPort: String
    @Binding var isPresented: Bool

    var body: some View {
        NavigationStack {
            List(bmsService.availablePorts, id: \.self) { port in
                Button(action: {
                    selectedPort = port
                    isPresented = false
                }) {
                    HStack {
                        Text(port)
                        Spacer()
                        if selectedPort == port {
                            Image(systemName: "checkmark")
                                .foregroundColor(.blue)
                        }
                    }
                }
                .buttonStyle(.plain)
            }
            .navigationTitle("Select Port")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        isPresented = false
                    }
                }
            }
        }
        .frame(minWidth: 300, minHeight: 400)
    }
}
