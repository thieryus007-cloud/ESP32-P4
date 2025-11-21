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

let currentMode = 'DISCONNECTED';
let pollTimer;
let simTimer;

// --- SIMULATEUR COMPLET (Avec Settings 300-343) ---
class BmsSimulator {
    constructor() {
        this.cells = Array.from({length: 16}, () => 3.8 + (Math.random() * 0.05));
        this.voltage = 0;
        this.current = 12.5;
        this.soc = 85.0;
        this.tempInt = 35.0;
        this.balMask = 0;
        
        this.settings = {};
        const defaults = [
            {id:300, v:4.20}, {id:301, v:2.80}, {id:303, v:3.90}, {id:304, v:100}, {id:305, v:150},
            {id:306, v:100}, {id:307, v:16}, {id:308, v:20}, {id:310, v:5}, {id:311, v:5},
            {id:312, v:1000}, {id:315, v:4.25}, {id:316, v:2.70}, {id:317, v:60}, {id:318, v:30},
            {id:319, v:65}, {id:320, v:0}, {id:321, v:90}, {id:322, v:2000}, {id:328, v:85},
            {id:330, v:1}, {id:332, v:10}, {id:340, v:0}, {id:343, v:1}
        ];
        
        defaults.forEach(d => {
            // Mock structure returned by parser
            this.settings[d.id] = { id: d.id, value: d.v, label: 'Simulated', unit: '', group: 'sim' };
        });
    }

    tick() {
        this.current += (Math.random() - 0.5);
        this.cells = this.cells.map(c => Math.min(4.2, Math.max(2.8, c + (Math.random()-0.5)*0.002)));
        this.cells[3] -= 0.0005; 
        this.voltage = this.cells.reduce((a, b) => a + b, 0);
        this.balMask = (Math.random() > 0.6 ? 4 : 0) | (Math.random() > 0.8 ? 32 : 0);
    }

    getLiveData() {
        const data = {};
        this.cells.forEach((v, i) => data[i] = { value: v });
        data[36] = { value: this.voltage };
        data[38] = { value: this.current };
        data[40] = { value: Math.min(...this.cells) };
        data[41] = { value: Math.max(...this.cells) };
        data[42] = { value: 24.0 };
        data[43] = { value: 25.5 };
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
    }
}

const simulator = new BmsSimulator();

// --- API ---
app.get('/api/ports', async (req, res) => {
    const ports = await SerialPort.list();
    res.json([{ path: 'SIMULATION', manufacturer: 'Virtual Device' }, ...ports]);
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

// BATCH WRITE (Ecriture par lot)
app.post('/api/write-batch', async (req, res) => {
    const { changes } = req.body; // Array of {id, value}

    if (currentMode === 'SIMULATION') {
        changes.forEach(c => simulator.writeSetting(c.id, parseFloat(c.value)));
        io.emit('bms-settings', simulator.getSettings());
        return res.json({ success: true });
    } 
    
    if (currentMode === 'CONNECTED' && bms.isConnected) {
        try {
            clearInterval(pollTimer); // Pause polling
            
            // Sequential Write
            for (const change of changes) {
                console.log(`Writing Reg ${change.id} -> ${change.value}`);
                await bms.writeRegister(parseInt(change.id), parseFloat(change.value));
                // Petit délai pour laisser respirer le BMS
                await new Promise(r => setTimeout(r, 100));
            }

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

function stopAll() {
    if(pollTimer) clearInterval(pollTimer);
    if(simTimer) clearInterval(simTimer);
    currentMode = 'DISCONNECTED';
    io.emit('status-change', { mode: 'DISCONNECTED' });
}

function startSimulation() {
    let cycle = 0;
    simTimer = setInterval(() => {
        simulator.tick();
        io.emit('bms-live', simulator.getLiveData());
        if(cycle % 5 === 0) {
            io.emit('bms-settings', simulator.getSettings());
            io.emit('bms-stats', simulator.getSettings()); // Mock stats
        }
        cycle++;
    }, 1000);
}

function startRealPolling() {
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
        } catch (err) { }
    }, 1000);
}

server.listen(3000, () => console.log('Server running on http://localhost:3000'));
