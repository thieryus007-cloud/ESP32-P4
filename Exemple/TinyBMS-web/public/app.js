// public/app.js
const socket = io();
let currentTab = 'dashboard';

document.addEventListener('DOMContentLoaded', () => {
    fetchPorts();
    generateCellCards();
});

function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    
    document.getElementById(tabId).classList.add('active');
    // Trouver le li correspondant (simplifié)
    const navItems = document.querySelectorAll('.nav-links li');
    if(tabId === 'dashboard') navItems[0].classList.add('active');
    if(tabId === 'cells') navItems[1].classList.add('active');
    if(tabId === 'settings') navItems[2].classList.add('active');
    if(tabId === 'stats') navItems[3].classList.add('active'); // Nouvel onglet Stats
}

async function fetchPorts() {
    const res = await fetch('/api/ports');
    const ports = await res.json();
    const select = document.getElementById('portSelect');
    select.innerHTML = ports.map(p => `<option value="${p.path}">${p.path}</option>`).join('');
}

async function connectBMS() {
    const path = document.getElementById('portSelect').value;
    const btn = document.querySelector('.port-selector button');
    btn.innerText = "Connecting...";
    
    const res = await fetch('/api/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path })
    });
    const data = await res.json();
    if(data.success) {
        document.getElementById('connectionStatus').classList.add('connected');
        btn.innerText = "Connected";
        btn.style.background = "var(--bg-panel)";
    } else {
        alert('Connection failed');
        btn.innerText = "Connect";
    }
}

// --- LIVE DATA HANDLING ---
socket.on('bms-live', (data) => {
    if(!data) return;

    // Helper to safely get val
    const getVal = (id) => data[id] ? data[id].value : 0;

    // KPI Dashboard
    updateText('val-voltage', getVal(36).toFixed(2));
    updateText('val-current', getVal(38).toFixed(2));
    const soc = getVal(46);
    updateText('val-soc', soc.toFixed(1));
    document.getElementById('bar-soc').style.width = `${soc}%`;
    
    // Power Calculation
    const power = getVal(36) * getVal(38);
    updateText('val-power', power.toFixed(0));

    // Temperatures
    updateText('temp-int', getVal(48) + ' °C');
    updateText('temp-ext1', getVal(42) + ' °C');
    updateText('temp-ext2', getVal(43) + ' °C');

    // Cells (Reg 0 to 15)
    const cells = [];
    for(let i=0; i<16; i++) {
        if(data[i]) cells.push(data[i].value);
    }
    updateCells(cells);

    // Min/Max Cell Data (Reg 40, 41)
    updateText('cell-min-kpi', getVal(40).toFixed(3) + ' V');
    updateText('cell-max-kpi', getVal(41).toFixed(3) + ' V');
    updateText('cell-diff', (getVal(41) - getVal(40)).toFixed(3) + ' V');
});

// --- SETTINGS HANDLING ---
socket.on('bms-settings', (data) => {
    const container = document.getElementById('settings-form-container');
    // On ne régénère pas le formulaire à chaque fois s'il est déjà rempli pour éviter de bloquer la saisie
    // Pour cette démo, on met à jour les valeurs affichées
    // Une implémentation plus complexe vérifierait si l'input est 'focus'
    
    if(container.innerHTML === "") {
        // Génération initiale
        let html = '';
        Object.values(data).forEach(reg => {
            html += `
                <div class="form-group">
                    <label>${reg.label} (${reg.unit}) [ID:${reg.id}]</label>
                    <input type="number" step="0.001" value="${reg.value}" id="setting-${reg.id}">
                </div>
            `;
        });
        container.innerHTML = html;
    } else {
        // Mise à jour douce des valeurs
        Object.values(data).forEach(reg => {
            const input = document.getElementById(`setting-${reg.id}`);
            if(input && document.activeElement !== input) {
                input.value = reg.value;
            }
        });
    }
});

// --- STATS HANDLING ---
socket.on('bms-stats', (data) => {
    const container = document.getElementById('stats-container');
    let html = '<div class="settings-grid">';
    Object.values(data).forEach(reg => {
        html += `
            <div class="card mini">
                <div class="label">${reg.label}</div>
                <div class="value">${reg.value} <span class="unit">${reg.unit}</span></div>
            </div>
        `;
    });
    html += '</div>';
    container.innerHTML = html;
});

// Helpers
function updateText(id, text) {
    const el = document.getElementById(id);
    if(el) el.innerText = text;
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
    cells.forEach((v, index) => {
        const elV = document.getElementById(`cell-v-${index}`);
        const elBar = document.getElementById(`cell-bar-${index}`);
        if(elV) {
            elV.innerText = v.toFixed(3);
            const pct = Math.max(0, Math.min(100, (v - 3.0) / 1.2 * 100));
            elBar.style.width = pct + '%';
            // Change color based on voltage levels logic could go here
        }
    });
}
