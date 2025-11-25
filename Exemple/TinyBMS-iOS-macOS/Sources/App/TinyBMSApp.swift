import SwiftUI

@main
struct TinyBMSApp: App {
    @StateObject private var dataStore = BMSDataStore()
    @StateObject private var bmsService: TinyBMSService

    init() {
        let dataStore = BMSDataStore()
        _dataStore = StateObject(wrappedValue: dataStore)
        _bmsService = StateObject(wrappedValue: TinyBMSService(dataStore: dataStore))
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(dataStore)
                .environmentObject(bmsService)
        }
    }
}
