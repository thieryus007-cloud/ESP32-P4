const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const TinyBMS = require('./tinybms');
const { SerialPort } = require('serialport');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static('public'));
app.use(express.json());

const bms = new TinyBMS('');

// --- ETAT DU SYSTEME ---
let currentMode = 'DISCONNECTED'; // 'CONNECTED' | 'SIMULATION'
let pollTimer;
let simTimer;

// --- SIMULATEUR ---
class BmsSimulator {
    constructor() {
        this.cells = Array.from({length: 16}, () => 3.8 + (Math.random() * 0.05));
        this.voltage = 0;
        this.current = 12.5;
        this.soc = 85.0;
        this.tempInt = 35.0;
        this.tempExt1 = 24.0;
        this.tempExt2 = 25.0;
        this.balMask = 0;
        
        // Map des réglages simulés
        this.settings = {};
        // On pré-remplit quelques valeurs pour l'exemple
        this.settings[300] = { id: 300, label: 'Fully Charged Voltage', value: 4.20, unit: 'V' };
        this.settings[301] = { id: 301, label: 'Fully Discharged Voltage', value: 2.80, unit: 'V' };
        this.settings[315] = { id: 315, label: 'Over-Voltage Cutoff', value: 4.25, unit: 'V' };
        this.settings[317] = { id: 317, label: 'Discharge Over-Current', value: 60, unit: 'A' };
        this.settings[306] = { id: 306, label: 'Battery Capacity', value: 100, unit: 'Ah' };
    }

    tick() {
        // Simulation physique
        this.current += (Math.random() - 0.5);
        this.cells = this.cells.map(c => {
            let change = (Math.random() - 0.5) * 0.002;
            return Math.min(4.2, Math.max(2.8, c + change));
        });
        this.cells[3] -= 0.0005; // Une cellule faible
        this.balMask = (Math.random() > 0.6 ? 4 : 0) | (Math.random() > 0.8 ? 32 : 0);
        this.voltage = this.cells.reduce((a, b) => a + b, 0);
        if(this.current > 0) this.soc -= 0.005;
        if(this.soc < 0) this.soc = 0;
        this.tempInt += (Math.random() - 0.4) * 0.1;
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
        data[50] = { value: 0x93 };
        data[52] = { value: this.balMask };
        return data;
    }

    getSettings() { return this.settings; }
    writeSetting(id, val) {
        if(this.settings[id]) this.settings[id].value = val;
        else this.settings[id] = { id, value: val, label: 'Simulated Reg', unit: '' };
    }
}

const simulator = new BmsSimulator();

// --- ROUTES API ---
app.get('/api/ports', async (req, res) => {
    const ports = await SerialPort.list();
    const result = [{ path: 'SIMULATION', manufacturer: 'Virtual Device' }, ...ports];
    res.json(result);
});

app.post('/api/connect', async (req, res) => {
    const { path } = req.body;
    stopAll();

    if (path === 'SIMULATION') {
        currentMode = 'SIMULATION';
        startSimulation();
        res.json({ success: true, mode: 'SIMULATION' });
        io.emit('status-change', { mode: 'SIMULATION' });
    } else {
        bms.path = path;
        try {
            if (bms.isConnected && bms.port && bms.port.isOpen) {
                await new Promise(resolve => bms.port.close(resolve));
            }
            await bms.connect();
            currentMode = 'CONNECTED';
            startRealPolling();
            res.json({ success: true, mode: 'CONNECTED' });
            io.emit('status-change', { mode: 'CONNECTED' });
        } catch (e) {
            res.status(500).json({ error: e.message });
        }
    }
});

app.post('/api/write', async (req, res) => {
    const { id, value } = req.body;

    if (currentMode === 'SIMULATION') {
        simulator.writeSetting(parseInt(id), parseFloat(value));
        io.emit('bms-settings', simulator.getSettings());
        return res.json({ success: true });
    } 
    
    if (currentMode === 'CONNECTED' && bms.isConnected) {
        try {
            clearInterval(pollTimer); // Pause polling
            await bms.writeRegister(parseInt(id), parseFloat(value));
            setTimeout(startRealPolling, 500); // Resume
            res.json({ success: true });
        } catch (e) {
            startRealPolling();
            res.status(500).json({ error: e.message });
        }
    } else {
        res.status(400).json({ error: "Not connected" });
    }
});

// --- LOOPS ---
function stopAll() {
    if(pollTimer) clearInterval(pollTimer);
    if(simTimer) clearInterval(simTimer);
    currentMode = 'DISCONNECTED';
    io.emit('status-change', { mode: 'DISCONNECTED' });
}

function startSimulation() {
    console.log("Starting Simulation...");
    let cycle = 0;
    simTimer = setInterval(() => {
        simulator.tick();
        io.emit('bms-live', simulator.getLiveData());
        
        if(cycle % 5 === 0) {
            io.emit('bms-settings', simulator.getSettings());
            // On utilise les settings comme stats bidon pour la démo
            io.emit('bms-stats', simulator.getSettings());
        }
        cycle++;
    }, 1000);
}

function startRealPolling() {
    console.log("Starting Real Hardware Polling...");
    let cycle = 0;
    pollTimer = setInterval(async () => {
        if (!bms.isConnected) return stopAll();
        try {
            const live = await bms.readRegisterBlock(0, 57);
            io.emit('bms-live', live);

            if (cycle % 5 === 0) {
                const stats = await bms.readRegisterBlock(100, 20);
                io.emit('bms-stats', stats);
            }
            if (cycle % 10 === 0) {
                const settings = await bms.readRegisterBlock(300, 45);
                io.emit('bms-settings', settings);
            }
            cycle++;
        } catch (err) {
            // Log minimal pour éviter le spam
        }
    }, 1000);
}

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});
