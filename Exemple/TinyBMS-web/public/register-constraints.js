// Configuration des contraintes et validations pour chaque registre
const REGISTER_CONSTRAINTS = {
    // Battery Group
    300: { min: 3.0, max: 4.5, step: 0.001 }, // Fully Charged Voltage
    301: { min: 2.0, max: 3.5, step: 0.001 }, // Fully Discharged Voltage
    306: { min: 1, max: 500, step: 0.1 },     // Battery Capacity (Ah)
    307: { min: 1, max: 24, step: 1 },        // Series Cells Count
    322: { min: 100, max: 10000, step: 1 },   // Max Cycles Count
    328: { min: 0, max: 100, step: 1 },       // Manual SOC Set (%)

    // Safety Group
    315: { min: 3.5, max: 4.5, step: 0.001 }, // Over-Voltage Cutoff
    316: { min: 2.0, max: 3.2, step: 0.001 }, // Under-Voltage Cutoff
    317: { min: 0, max: 300, step: 1 },       // Discharge Over-Current (A)
    318: { min: 0, max: 100, step: 1 },       // Charge Over-Current (A)
    305: { min: 0, max: 500, step: 1 },       // Peak Discharge Current (A)
    319: { min: 40, max: 85, step: 1 },       // Over-Heat Cutoff (°C)
    320: { min: -20, max: 10, step: 1 },      // Low Temp Charge Cutoff (°C)

    // Balance Group
    303: { min: 3.5, max: 4.2, step: 0.001 }, // Early Balancing Threshold
    304: { min: 0, max: 1000, step: 1 },      // Charge Finished Current (mA)
    308: { min: 5, max: 100, step: 1 },       // Allowed Disbalance (mV)
    321: { min: 50, max: 99, step: 1 },       // Charge Restart Level (%)
    332: { min: 0, max: 300, step: 1 },       // Automatic Recovery (s)

    // Hardware Group - Dropdowns
    330: {
        type: 'select',
        options: [
            { value: 0, label: 'Type 0' },
            { value: 1, label: 'Type 1' },
            { value: 2, label: 'Type 2' }
        ]
    }, // Charger Type
    340: {
        type: 'select',
        options: [
            { value: 0, label: 'Dual Mode' },
            { value: 1, label: 'Single Mode' }
        ]
    }, // Operation Mode
    343: {
        type: 'select',
        options: [
            { value: 0, label: 'Protocol 0' },
            { value: 1, label: 'Protocol 1' }
        ]
    }, // Protocol

    // Hardware Group - Numeric
    310: { min: 0, max: 60, step: 1 },        // Charger Startup Delay (s)
    311: { min: 0, max: 60, step: 1 },        // Charger Disable Delay (s)
    312: { min: 100, max: 10000, step: 1 }    // Pulses Per Unit
};
