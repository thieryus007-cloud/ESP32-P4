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

const bms = new TinyBMS(''); // Path défini via l'interface

// --- API ---
app.get('/api/ports', async (req, res) => {
    const ports = await SerialPort.list();
    res.json(ports);
});

app.post('/api/connect', async (req, res) => {
    const { path } = req.body;
    bms.path = path;
    try {
        if (bms.isConnected && bms.port && bms.port.isOpen) {
            await new Promise(resolve => bms.port.close(resolve));
        }
        await bms.connect();
        res.json({ success: true });
        startPolling();
    } catch (e) {
        console.error(e);
        res.status(500).json({ error: e.message });
    }
});

// WRITE Route (Avec mise en pause du polling)
app.post('/api/write', async (req, res) => {
    const { id, value } = req.body;
    
    if (!bms.isConnected) return res.status(400).json({ error: "Not connected" });

    try {
        clearInterval(pollTimer); // Pause polling
        console.log(`Writing Reg ${id} -> ${value}`);
        
        await bms.writeRegister(parseInt(id), parseFloat(value));
        
        setTimeout(startPolling, 500); // Resume polling
        res.json({ success: true });
    } catch (e) {
        console.error("Write Error:", e);
        startPolling();
        res.status(500).json({ error: e.message });
    }
});

// --- POLLING ---
let pollTimer;
let cycle = 0;

function startPolling() {
    if (pollTimer) clearInterval(pollTimer);

    pollTimer = setInterval(async () => {
        if (!bms.isConnected) return;

        try {
            // 1. Live Data (Tous les registres 0-56) - Chaque seconde
            const liveData = await bms.readRegisterBlock(0, 57);
            io.emit('bms-live', liveData);

            // 2. Stats (Reg 100+) - Toutes les 5 sec
            if (cycle % 5 === 0) {
                const stats = await bms.readRegisterBlock(100, 20);
                io.emit('bms-stats', stats);
            }

            // 3. Settings (Reg 300+) - Toutes les 5 sec pour réactivité UI
            if (cycle % 5 === 0) {
                const settings = await bms.readRegisterBlock(300, 45);
                io.emit('bms-settings', settings);
            }

            cycle++;
            if(cycle > 100) cycle = 0;

        } catch (err) {
            // Ignorer erreurs de timeout ponctuelles
        }
    }, 1000);
}

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});
