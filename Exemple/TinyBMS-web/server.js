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

// Identify USB port (Change this based on Mac Mini '/dev/tty.usb...' path)
const PORT_PATH = '/dev/tty.usbserial-0001'; // A CONFIGURER
const bms = new TinyBMS(PORT_PATH);

// API to list ports
app.get('/api/ports', async (req, res) => {
    const ports = await SerialPort.list();
    res.json(ports);
});

// API to connect
app.post('/api/connect', async (req, res) => {
    const { path } = req.body;
    bms.path = path;
    try {
        if(!bms.isConnected) await bms.connect();
        res.json({ success: true });
        startPolling();
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

let pollingInterval;

function startPolling() {
    if (pollingInterval) clearInterval(pollingInterval);
    
    pollingInterval = setInterval(async () => {
        if (!bms.isConnected) return;
        try {
            // Read block starting at 0, reading 57 registers (Live Data)
            // Note: Size depends on protocol version, simplified here
            const rawData = await bms.readRegisters(0, 57); 
            const parsedData = bms.parseLiveData(rawData);
            
            if(parsedData) {
                io.emit('bms-data', parsedData);
            }
        } catch (err) {
            console.error("Polling error:", err);
        }
    }, 1000); // 1Hz refresh
}

server.listen(3000, () => {
    console.log('Server running on http://localhost:3000');
});
