// Configuration des contraintes et validations pour chaque registre
// Basé sur TinyBMS_service.ts et TinyBMS Communication Protocols Rev D
const REGISTER_CONSTRAINTS = {
    // Battery Group
    300: { min: 3, max: 4.5, step: 0.001 },   // Fully Charged Voltage
    301: { min: 2.0, max: 3.5, step: 0.001 },   // Fully Discharged Voltage
    306: { min: 1.0, max: 500, step: 0.01 }, // Battery Capacity (Ah) - UINT16 scale 0.01
    307: { min: 4, max: 16, step: 1 },          // Series Cells Count (doc spec)
    322: { min: 100, max: 10000, step: 1 },       // Max Cycles Count - UINT16
    328: { min: 0, max: 100, step: 0.002 },     // Manual SOC Set (%) - scale 0.002

    // Safety Group
    315: { min: 1.2, max: 4.5, step: 0.001 },   // Over-Voltage Cutoff
    316: { min: 1.0, max: 3.5, step: 0.001 },   // Under-Voltage Cutoff
    317: { min: 0, max: 65535, step: 1 },       // Discharge Over-Current (A) - UINT16
    318: { min: 0, max: 65535, step: 1 },       // Charge Over-Current (A) - UINT16
    305: { min: 0, max: 65535, step: 1 },       // Peak Discharge Current (A) - UINT16
    319: { min: -32768, max: 32767, step: 1 },  // Over-Heat Cutoff (°C) - INT16
    320: { min: -32768, max: 32767, step: 1 },  // Low Temp Charge Cutoff (°C) - INT16

    // Balance Group
    303: { min: 3.5, max: 4.5, step: 0.001 },   // Early Balancing Threshold
    304: { min: 0, max: 65535, step: 1 },       // Charge Finished Current (mA) - UINT16
    308: { min: 0, max: 65535, step: 1 },       // Allowed Disbalance (mV) - UINT16
    321: { min: 0, max: 100, step: 1 },         // Charge Restart Level (%)
    332: { min: 0, max: 65535, step: 1 },       // Automatic Recovery (s) - UINT16

    // Hardware Group - Numeric
    310: { min: 0, max: 65535, step: 1 },       // Charger Startup Delay (s) - UINT16
    311: { min: 0, max: 65535, step: 1 },       // Charger Disable Delay (s) - UINT16
    312: { min: 0, max: 4294967295, step: 1 },  // Pulses Per Unit - UINT32

    // Hardware Group - Dropdowns
    314: {
        type: 'select',
        options: [
            { value: 1, label: 'Meters (m)' },
            { value: 2, label: 'Kilometers (km)' },
            { value: 3, label: 'Feet (ft)' },
            { value: 4, label: 'Miles (mi)' },
            { value: 5, label: 'Yards (yd)' }
        ]
    }, // Distance Unit Name

    // === ADVANCED CONFIGURATION (329-343) ===

    // Current Sensor (329)
    329: {
        type: 'select',
        options: [
            { value: 0, label: 'Normal' },
            { value: 1, label: 'Inverted' }
        ],
        help: 'Invert external current sensor polarity (for LEM DHAB 133, etc.)'
    },

    // Charger Setup (330, 333, 335)
    330: {
        type: 'select',
        options: [
            { value: 0, label: 'Variable (Reserved)' },
            { value: 1, label: 'Constant Current' }
        ],
        help: 'Charger behavior type'
    },
    333: {
        type: 'select',
        options: [
            { value: 1, label: 'Charge FET (Internal)' },
            { value: 2, label: 'AIDO1' },
            { value: 3, label: 'AIDO2' },
            { value: 4, label: 'DIDO1' },
            { value: 5, label: 'DIDO2' },
            { value: 6, label: 'AIHO1 Active Low' },
            { value: 7, label: 'AIHO1 Active High' },
            { value: 8, label: 'AIHO2 Active Low' },
            { value: 9, label: 'AIHO2 Active High' }
        ],
        help: 'Output pin controlling the charger switch'
    },
    335: {
        type: 'select',
        options: [
            { value: 1, label: 'Internal' },
            { value: 2, label: 'AIDI1' },
            { value: 3, label: 'AIDI2' },
            { value: 4, label: 'DIDI1' },
            { value: 5, label: 'DIDI2' },
            { value: 6, label: 'AIHI1' },
            { value: 7, label: 'AIHI2' }
        ],
        help: 'Input pin for charger presence detection'
    },

    // Load & Discharge (331, 334)
    331: {
        type: 'select',
        options: [
            { value: 0, label: 'FET (Internal)' },
            { value: 1, label: 'AIDO1' },
            { value: 2, label: 'AIDO2' },
            { value: 3, label: 'DIDO1' },
            { value: 4, label: 'DIDO2' },
            { value: 5, label: 'AIHO1 Active Low' },
            { value: 6, label: 'AIHO1 Active High' },
            { value: 7, label: 'AIHO2 Active Low' },
            { value: 8, label: 'AIHO2 Active High' }
        ],
        help: 'Output pin controlling the load/discharge switch'
    },
    334: {
        type: 'select',
        options: [
            { value: 0, label: 'Disabled' },
            { value: 1, label: 'AIDI1' },
            { value: 2, label: 'AIDI2' },
            { value: 3, label: 'DIDI1' },
            { value: 4, label: 'DIDI2' },
            { value: 5, label: 'AIHI1' },
            { value: 6, label: 'AIHI2' }
        ],
        help: 'Input pin for ignition/key detection'
    },

    // Precharge (337, 338)
    337: {
        type: 'select',
        options: [
            { value: 0, label: 'Disabled' },
            { value: 2, label: 'Discharge FET' },
            { value: 3, label: 'AIDO1' },
            { value: 4, label: 'AIDO2' },
            { value: 5, label: 'DIDO1' },
            { value: 6, label: 'DIDO2' },
            { value: 7, label: 'AIHO1 Active Low' },
            { value: 8, label: 'AIHO1 Active High' },
            { value: 9, label: 'AIHO2 Active Low' },
            { value: 10, label: 'AIHO2 Active High' }
        ],
        help: 'Output pin for precharge resistor control'
    },
    338: {
        type: 'select',
        options: [
            { value: 0, label: '0.1 s' },
            { value: 1, label: '0.2 s' },
            { value: 2, label: '0.5 s' },
            { value: 3, label: '1 s' },
            { value: 4, label: '2 s' },
            { value: 5, label: '3 s' },
            { value: 6, label: '4 s' },
            { value: 7, label: '5 s' }
        ],
        help: 'Duration of precharge phase before closing main contactor'
    },

    // System Settings (332, 339, 340, 341, 342, 343)
    332: {
        min: 1,
        max: 30,
        step: 1,
        help: 'Number of automatic recovery attempts (1-30)'
    },
    339: {
        type: 'select',
        options: [
            { value: 0, label: 'Dual 10K NTC' },
            { value: 1, label: 'Multipoint Active Sensor' }
        ],
        help: 'Temperature sensor type'
    },
    340: {
        type: 'select',
        options: [
            { value: 0, label: 'Dual Port (Separate Charge/Discharge)' },
            { value: 1, label: 'Single Port (Combined Charge/Discharge)' }
        ],
        help: 'BMS operation mode - Use Single Port for Victron Easysolar II'
    },
    341: {
        type: 'select',
        options: [
            { value: 0, label: 'FET (Internal)' },
            { value: 1, label: 'AIDO1' },
            { value: 2, label: 'AIDO2' },
            { value: 3, label: 'DIDO1' },
            { value: 4, label: 'DIDO2' },
            { value: 5, label: 'AIHO1 Active Low' },
            { value: 6, label: 'AIHO1 Active High' },
            { value: 7, label: 'AIHO2 Active Low' },
            { value: 8, label: 'AIHO2 Active High' }
        ],
        help: 'Output pin for single port switch (only used in Single Port Mode)'
    },
    342: {
        min: 0,
        max: 65535,
        step: 1,
        help: 'CAN broadcast interval in milliseconds'
    },
    343: {
        type: 'select',
        options: [
            { value: 0, label: 'MODBUS' },
            { value: 1, label: 'ASCII (Recommended)' }
        ],
        help: 'Communication protocol - ASCII is recommended for most applications'
    }
};
