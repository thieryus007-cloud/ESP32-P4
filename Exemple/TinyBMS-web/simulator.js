class BmsSimulator {
    constructor() {
        // Physique
        this.cells = Array.from({ length: 16 }, () => 3.8 + (Math.random() * 0.05));
        this.voltage = 0;
        this.current = 12.5;
        this.soc = 85.0;
        this.tempInt = 35.0;
        this.tempExt1 = 24.0;
        this.tempExt2 = 25.0;
        this.balMask = 0;

        // Settings (Imite la structure renvoyée par tinybms.js)
        this.settings = {};

        // Liste des registres à simuler pour peupler l'onglet Config
        const simRegs = [
            // Battery
            { id: 300, v: 4.20, g: 'battery', l: 'Fully Charged Voltage', u: 'V' },
            { id: 301, v: 2.80, g: 'battery', l: 'Fully Discharged Voltage', u: 'V' },
            { id: 306, v: 100, g: 'battery', l: 'Battery Capacity', u: 'Ah' },
            { id: 307, v: 16, g: 'battery', l: 'Series Cells Count', u: '' },
            { id: 322, v: 2000, g: 'battery', l: 'Max Cycles Count', u: '' },
            { id: 328, v: 100, g: 'battery', l: 'Set SOC Manually', u: '%' },
            // Safety
            { id: 315, v: 4.25, g: 'safety', l: 'Over-Voltage Cutoff', u: 'V' },
            { id: 316, v: 2.70, g: 'safety', l: 'Under-Voltage Cutoff', u: 'V' },
            { id: 317, v: 60, g: 'safety', l: 'Discharge Over-Current', u: 'A' },
            { id: 318, v: 30, g: 'safety', l: 'Charge Over-Current', u: 'A' },
            { id: 305, v: 150, g: 'safety', l: 'Peak Discharge Current', u: 'A' },
            { id: 319, v: 65, g: 'safety', l: 'Over-Heat Cutoff', u: '°C' },
            { id: 320, v: 0, g: 'safety', l: 'Low Temp Charge Cutoff', u: '°C' },
            // Balance
            { id: 303, v: 3.90, g: 'balance', l: 'Early Balancing Threshold', u: 'V' },
            { id: 304, v: 100, g: 'balance', l: 'Charge Finished Current', u: 'mA' },
            { id: 308, v: 20, g: 'balance', l: 'Allowed Disbalance', u: 'mV' },
            { id: 321, v: 90, g: 'balance', l: 'Charge Restart Level', u: '%' },
            { id: 332, v: 10, g: 'balance', l: 'Automatic Recovery', u: 's' },
            // Hardware
            { id: 310, v: 5, g: 'hardware', l: 'Charger Startup Delay', u: 's' },
            { id: 311, v: 5, g: 'hardware', l: 'Charger Disable Delay', u: 's' },
            { id: 312, v: 1000, g: 'hardware', l: 'Pulses Per Unit', u: '' },
            { id: 330, v: 1, g: 'hardware', l: 'Charger Type', u: '' },
            { id: 340, v: 0, g: 'hardware', l: 'Operation Mode', u: '' },
            { id: 343, v: 1, g: 'hardware', l: 'Protocol', u: '' }
        ];

        simRegs.forEach(d => {
            this.settings[d.id] = { id: d.id, value: d.v, label: d.l, unit: d.u, group: d.g };
        });
    }

    tick() {
        this.current += (Math.random() - 0.5);
        this.cells = this.cells.map(c => Math.min(4.2, Math.max(2.8, c + (Math.random() - 0.5) * 0.002)));
        // Simuler équilibrage aléatoire
        this.balMask = (Math.random() > 0.6 ? 4 : 0) | (Math.random() > 0.8 ? 32 : 0);
        this.voltage = this.cells.reduce((a, b) => a + b, 0);
        this.soc = Math.max(0, Math.min(100, this.soc - 0.01));
        this.tempInt = 30 + Math.sin(Date.now() / 10000) * 5;
    }

    getLiveData() {
        const data = {};
        this.cells.forEach((v, i) => data[i] = { value: v });
        data[36] = { value: this.voltage };
        data[38] = { value: this.current };
        data[40] = { value: Math.min(...this.cells) };
        data[41] = { value: Math.max(...this.cells) };
        data[42] = { value: this.tempExt1 };
        data[43] = { value: this.tempExt2 };
        data[45] = { value: 99.0 };
        data[46] = { value: this.soc };
        data[48] = { value: this.tempInt };
        data[50] = { value: 0x93 }; // Discharging status
        data[52] = { value: this.balMask };
        return data;
    }

    getSettings() { return this.settings; }
    writeSetting(id, val) {
        if (this.settings[id]) this.settings[id].value = val;
    }
}

module.exports = BmsSimulator;
