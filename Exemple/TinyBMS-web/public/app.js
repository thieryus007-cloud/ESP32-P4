const socket = io();

// State
let currentTab = 'dashboard';

// Init
document.addEventListener('DOMContentLoaded', () => {
    fetchPorts();
    generateCellCards();
});

// Navigation
function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    
    document.getElementById(tabId).classList.add('active');
    // Add active class to nav item logic here simplified
    currentTab = tabId;
}

// API
async function fetchPorts() {
    const res = await fetch('/api/ports');
    const ports = await res.json();
    const select = document.getElementById('portSelect');
    select.innerHTML = ports.map(p => `<option value="${p.path}">${p.path}</option>`).join('');
}

async function connectBMS() {
    const path = document.getElementById('portSelect').value;
    const res = await fetch('/api/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path })
    });
    const data = await res.json();
    if(data.success) {
        document.getElementById('connectionStatus').classList.add('connected');
    } else {
        alert('Connection failed');
    }
}

// Realtime Data Handling
socket.on('bms-data', (data) => {
    updateDashboard(data);
    updateCells(data.cells);
});

function updateDashboard(data) {
    // KPI
    document.getElementById('val-voltage').innerText = data.packVoltage.toFixed(2);
    document.getElementById('val-current').innerText = data.packCurrent.toFixed(2);
    document.getElementById('val-soc').innerText = data.soc.toFixed(1);
    document.getElementById('bar-soc').style.width = `${data.soc}%`;
    
    // Power P = U * I
    const power = data.packVoltage * data.packCurrent;
    document.getElementById('val-power').innerText = power.toFixed(0);

    // State
    const states = { 0x91: 'CHARGING', 0x92: 'FULL', 0x93: 'DISCHARGING', 0x97: 'IDLE' };
    document.getElementById('bmsState').innerText = states[data.state] || 'UNKNOWN';

    // Temps
    document.getElementById('temp-int').innerText = data.tempInternal + ' °C';
    document.getElementById('temp-ext1').innerText = data.tempExt1 + ' °C';
    document.getElementById('temp-ext2').innerText = data.tempExt2 + ' °C';
}

function generateCellCards() {
    const container = document.getElementById('cellsContainer');
    container.innerHTML = '';
    for(let i=1; i<=16; i++) {
        const div = document.createElement('div');
        div.className = 'cell-card';
        div.innerHTML = `
            <h4>Cell ${i}</h4>
            <div class="voltage" id="cell-v-${i-1}">0.000</div>
            <div class="cell-bar" id="cell-bar-${i-1}" style="width: 0%"></div>
        `;
        container.appendChild(div);
    }
}

function updateCells(cells) {
    if(!cells || cells.length === 0) return;

    let min = 99, max = 0;

    cells.forEach((v, index) => {
        const elV = document.getElementById(`cell-v-${index}`);
        const elBar = document.getElementById(`cell-bar-${index}`);
        
        if(elV) {
            elV.innerText = v.toFixed(3);
            // Visual mapping: 3.0V = 0%, 4.2V = 100%
            const pct = Math.max(0, Math.min(100, (v - 3.0) / 1.2 * 100));
            elBar.style.width = pct + '%';
            
            // Color coding for balancing (simplified logic)
            // Ideally we read Balancing Flags from BMS to color orange
        }

        if(v < min) min = v;
        if(v > max) max = v;
    });

    document.getElementById('cell-min').innerText = min.toFixed(3) + ' V';
    document.getElementById('cell-max').innerText = max.toFixed(3) + ' V';
    document.getElementById('cell-diff').innerText = (max - min).toFixed(3) + ' V';
}
