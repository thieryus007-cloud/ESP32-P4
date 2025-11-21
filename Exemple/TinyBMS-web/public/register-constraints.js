// Configuration des contraintes et validations pour chaque registre
// Basé sur TinyBMS_service.ts et TinyBMS Communication Protocols Rev D
const REGISTER_CONSTRAINTS = {
    // Battery Group
    300: { min: 1.2, max: 4.5, step: 0.001 },   // Fully Charged Voltage
    301: { min: 1.0, max: 3.5, step: 0.001 },   // Fully Discharged Voltage
    306: { min: 0.01, max: 655.35, step: 0.01 }, // Battery Capacity (Ah) - UINT16 scale 0.01
    307: { min: 4, max: 16, step: 1 },          // Series Cells Count (doc spec)
    322: { min: 0, max: 65535, step: 1 },       // Max Cycles Count - UINT16
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
    303: { min: 1.2, max: 4.5, step: 0.001 },   // Early Balancing Threshold
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
    330: { min: 0, max: 65535, step: 1 },       // Charger Type / Dischg Timeout (packed)
    331: {
        type: 'select',
        options: [
            { value: 0, label: 'NO/NC Switch' },
            { value: 1, label: 'N-Channel MOSFET' },
            { value: 2, label: 'P-Channel MOSFET' }
        ]
    }, // Load Switch Type
    333: {
        type: 'select',
        options: [
            { value: 0, label: 'NO/NC Switch' },
            { value: 1, label: 'N-Channel MOSFET' },
            { value: 2, label: 'P-Channel MOSFET' }
        ]
    }, // Charger Switch Type
    338: {
        type: 'select',
        options: [
            { value: 0, label: '0.1 seconds' },
            { value: 1, label: '0.2 seconds' },
            { value: 2, label: '0.5 seconds' },
            { value: 3, label: '1 second' },
            { value: 4, label: '2 seconds' },
            { value: 5, label: '5 seconds' },
            { value: 6, label: '10 seconds' },
            { value: 7, label: '20 seconds' }
        ]
    }, // Precharge Duration
    339: {
        type: 'select',
        options: [
            { value: 0, label: 'NTC 10k' },
            { value: 1, label: 'NTC 100k' },
            { value: 2, label: 'Dallas DS18B20' }
        ]
    }, // Temp Sensor Type
    340: {
        type: 'select',
        options: [
            { value: 0, label: 'Dual Port Mode' },
            { value: 1, label: 'Single Port Mode' }
        ]
    }, // BMS Operation Mode
    343: {
        type: 'select',
        options: [
            { value: 0, label: 'UART Protocol' },
            { value: 1, label: 'CAN Bus Protocol' }
        ]
    } // Protocol
};
