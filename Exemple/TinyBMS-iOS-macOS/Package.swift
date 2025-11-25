// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "TinyBMS",
    platforms: [
        .iOS(.v16),
        .macOS(.v13)
    ],
    products: [
        .library(
            name: "TinyBMS",
            targets: ["TinyBMS"]
        ),
    ],
    dependencies: [
        // ORSSerialPort pour macOS seulement
        .package(url: "https://github.com/armadsen/ORSSerialPort.git", from: "2.1.0")
    ],
    targets: [
        .target(
            name: "TinyBMS",
            dependencies: [
                .product(name: "ORSSerialPort", package: "ORSSerialPort", condition: .when(platforms: [.macOS]))
            ],
            path: "Sources"
        ),
        .testTarget(
            name: "TinyBMSTests",
            dependencies: ["TinyBMS"]
        ),
    ]
)
