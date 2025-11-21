// server.js
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

// -- Configuration --
// NOTE: Sur Mac, c'est souvent /dev/tty.usbserial-XXXX ou /dev/tty.usbmodemXXXX
// L'utilisateur pourra choisir via l'interface.
const bms = new TinyBMS(''); 

// -- Routes API --
app.get('/api/ports', async (req, res) => {
    const ports = await SerialPort.list();
    res.json(ports);
});

app.post('/api/connect', async (req, res) => {
    const { path } = req.body;
    bms.path = path;
    try {
        if (bms.isConnected && bms.port) {
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

// -- Polling Logic --
let pollTimer;
let pollCycle = 0;

function startPolling() {
    if (pollTimer) clearInterval(pollTimer);

    pollTimer = setInterval(async () => {
        if (!bms.isConnected) return;

        try {
            // Strategy: 
            // Every 1s: Poll LIVE data (0-56)
            // Every 5s: Poll STATS (100-117)
            // Every 10s: Poll SETTINGS (300-343) + Version (500-506)
            
            // 1. Always Poll Live Data (Reg 0 to 56)
            // Size = 57 registers
            const liveData = await bms.readRegisterBlock(0, 57);
            io.emit('bms-live', liveData);

            // 2. Poll Statistics (Cycle 0, 5, 10...) -> Every 5 seconds
            if (pollCycle % 5 === 0) {
                const statsData = await bms.readRegisterBlock(100, 20); // Reg 100-119
                io.emit('bms-stats', statsData);
            }

            // 3. Poll Settings (Cycle 0, 10, 20...) -> Every 10 seconds
            if (pollCycle % 10 === 0) {
                const settingsData = await bms.readRegisterBlock(300, 45); // Reg 300-344
                io.emit('bms-settings', settingsData);
                
                // Poll Version occasionally
                const versionData = await bms.readRegisterBlock(500, 10); // Reg 500+
                io.emit('bms-version', versionData);
            }

            pollCycle++;
            if (pollCycle > 100) pollCycle = 1; 

        } catch (err) {
            console.error("Polling Error:", err);
        }
    }, 1000); // 1 Second base interval
}

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});
