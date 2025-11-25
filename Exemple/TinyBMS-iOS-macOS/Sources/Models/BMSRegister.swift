import Foundation

/// Type de données pour les registres TinyBMS
enum RegisterType {
    case uint16
    case int16
    case uint32
    case float32
}

/// Catégorie de registre
enum RegisterCategory: String, Codable {
    case live = "Live"
    case stats = "Stats"
    case settings = "Settings"
    case version = "Version"
}

/// Groupe de settings pour l'organisation de l'interface
enum SettingsGroup: String, Codable {
    case battery
    case safety
    case balance
    case hardware
}

/// Définition d'un registre TinyBMS
struct BMSRegister: Identifiable, Codable {
    let id: Int
    let label: String
    let unit: String?
    let type: RegisterType
    let scale: Double?
    let category: RegisterCategory
    let group: SettingsGroup?

    enum CodingKeys: String, CodingKey {
        case id, label, unit, type, scale, category, group
    }

    init(id: Int, label: String, unit: String? = nil, type: RegisterType, scale: Double? = nil, category: RegisterCategory, group: SettingsGroup? = nil) {
        self.id = id
        self.label = label
        self.unit = unit
        self.type = type
        self.scale = scale
        self.category = category
        self.group = group
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(Int.self, forKey: .id)
        label = try container.decode(String.self, forKey: .label)
        unit = try container.decodeIfPresent(String.self, forKey: .unit)

        let typeString = try container.decode(String.self, forKey: .type)
        switch typeString {
        case "UINT16": type = .uint16
        case "INT16": type = .int16
        case "UINT32": type = .uint32
        case "FLOAT": type = .float32
        default: type = .uint16
        }

        scale = try container.decodeIfPresent(Double.self, forKey: .scale)
        category = try container.decode(RegisterCategory.self, forKey: .category)
        group = try container.decodeIfPresent(SettingsGroup.self, forKey: .group)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(id, forKey: .id)
        try container.encode(label, forKey: .label)
        try container.encodeIfPresent(unit, forKey: .unit)

        let typeString: String
        switch type {
        case .uint16: typeString = "UINT16"
        case .int16: typeString = "INT16"
        case .uint32: typeString = "UINT32"
        case .float32: typeString = "FLOAT"
        }
        try container.encode(typeString, forKey: .type)

        try container.encodeIfPresent(scale, forKey: .scale)
        try container.encode(category, forKey: .category)
        try container.encodeIfPresent(group, forKey: .group)
    }
}

/// Valeur lue d'un registre
struct BMSRegisterValue: Identifiable {
    let id: Int
    let label: String
    let value: Double
    let unit: String
    let category: RegisterCategory
    let group: SettingsGroup?

    var formattedValue: String {
        String(format: "%.4f", value)
    }

    var displayValue: String {
        if unit.isEmpty {
            return formattedValue
        }
        return "\(formattedValue) \(unit)"
    }
}
