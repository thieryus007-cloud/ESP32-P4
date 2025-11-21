// public/app.js
const socket = io();
let currentTab = 'dashboard';

// Instances ECharts
let chartSoc = null;
let chartCells = null;

document.addEventListener('DOMContentLoaded', () => {
    fetchPorts();
    initCharts(); // Initialisation des graphiques
    
    // Redimensionnement auto des graphiques si on change la taille de la fenêtre
    window.addEventListener('resize', () => {
        if(chartSoc) chartSoc.resize();
        if(chartCells) chartCells.resize();
    });
});

function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');
    
    // Gestion de la classe active dans le menu
    const navItems = document.querySelectorAll('.nav-links li');
    const tabs = ['dashboard', 'cells', 'stats', 'settings'];
    navItems[tabs.indexOf(tabId)].classList.add('active');

    // Important : ECharts doit être redimensionné quand l'onglet devient visible
    setTimeout(() => {
        if(tabId === 'dashboard' && chartSoc) chartSoc.resize();
        if(tabId === 'cells' && chartCells) chartCells.resize();
    }, 100);
}

// --- ECHARTS CONFIGURATION ---
function initCharts() {
    // 1. Jauge SOC (Dashboard)
    const socDom = document.getElementById('chart-soc');
    if(socDom) {
        chartSoc = echarts.init(socDom, 'dark', { renderer: 'canvas' });
        const optionSoc = {
            backgroundColor: 'transparent',
            series: [{
                type: 'gauge',
                startAngle: 180,
                endAngle: 0,
                min: 0,
                max: 100,
                splitNumber: 5,
                axisLine: {
                    lineStyle: {
                        width: 10,
                        color: [[0.2, '#ef4444'], [0.8, '#6366f1'], [1, '#10b981']]
                    }
                },
                pointer: { length: '50%', width: 4 },
                axisLabel: { color: '#a1a1aa', distance: 15, fontSize: 10 },
                detail: {
                    fontSize: 20,
                    offsetCenter: [0, '20%'],
                    valueAnimation: true,
                    formatter: '{value}%',
                    color: '#fff'
                },
                data: [{ value: 0 }]
            }]
        };
        chartSoc.setOption(optionSoc);
    }

    // 2. Bar Chart Cellules (Cells Tab)
    const cellsDom = document.getElementById('chart-cells');
    if(cellsDom) {
        chartCells = echarts.init(cellsDom, 'dark', { renderer: 'canvas' });
        const optionCells = {
            backgroundColor: 'transparent',
            tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
            grid: { top: 30, bottom: 30, left: 40, right: 20 },
            xAxis: {
                type: 'category',
                data: Array.from({length: 16}, (_, i) => `C${i+1}`),
                axisLine: { lineStyle: { color: '#555' } },
                axisLabel: { color: '#aaa' }
            },
            yAxis: {
                type: 'value',
                min: 2.5, // Zoom sur la plage utile Li-Ion
                max: 4.5,
                splitLine: { lineStyle: { color: '#333' } }
            },
            series: [{
                data: Array(16).fill(0),
                type: 'bar',
                showBackground: true,
                backgroundStyle: { color: 'rgba(180, 180, 180, 0.1)' },
                itemStyle: {
                    color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                        { offset: 0, color: '#6366f1' },
                        { offset: 1, color: '#4338ca' }
                    ]),
                    borderRadius: [4, 4, 0, 0]
                },
                label: {
                    show: true,
                    position: 'top',
                    formatter: '{c}v',
                    color: '#fff',
                    fontSize: 10,
                    rotate: 90,
                    align: 'left',
                    verticalAlign: 'middle',
                    distance: 10
                }
            }]
        };
        chartCells.setOption(optionCells);
    }
}

// --- API & CONNEXION (Standard) ---
async function fetchPorts() {
    try {
        const res = await fetch('/api/ports');
        const ports = await res.json();
        const select = document.getElementById('portSelect');
        select.innerHTML = ports.map(p => `<option value="${p.path}">${p.path}</option>`).join('');
    } catch(e) { console.error(e); }
}

async function connectBMS() {
    const path = document.getElementById('portSelect').value;
    const btn = document.querySelector('.port-selector button');
    btn.innerText = "Connecting...";
    try {
        const res = await fetch('/api/connect', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path })
        });
        const data = await res.json();
        if(data.success) {
            document.getElementById('connectionStatus').classList.add('connected');
            btn.innerText = "Connected";
            btn.style.background = "#1e1e24";
            btn.style.border = "1px solid #10b981";
        }
    } catch(e) {
        alert('Connection failed');
        btn.innerText = "Connect";
    }
}

// --- UPDATE LOGIC ---
socket.on('bms-live', (data) => {
    if(!data) return;
    const getVal = (id) => data[id] ? data[id].value : 0;

    // Text Updates
    document.getElementById('val-voltage').innerText = getVal(36).toFixed(2);
    document.getElementById('val-current').innerText = getVal(38).toFixed(2);
    const power = getVal(36) * getVal(38);
    document.getElementById('val-power').innerText = power.toFixed(0);
    
    const tempInt = getVal(48);
    document.getElementById('temp-int').innerText = tempInt + ' °C';
    document.getElementById('temp-ext1').innerText = getVal(42) + ' °C';
    document.getElementById('temp-ext2').innerText = getVal(43) + ' °C';

    // State Logic
    const stateVal = getVal(50);
    const states = { 0x91: 'CHARGING', 0x92: 'FULL', 0x93: 'DISCHARGING', 0x97: 'IDLE', 0x9B: 'FAULT' };
    const stateStr = states[stateVal] || 'UNKNOWN';
    const stateEl = document.getElementById('bmsState');
    stateEl.innerText = stateStr;
    
    // Coloration dynamique de l'état
    if(stateVal === 0x9B) stateEl.style.background = '#ef4444'; // Rouge
    else if(stateVal === 0x91) stateEl.style.background = '#10b981'; // Vert
    else stateEl.style.background = '#3f3f46'; // Gris

    // --- MISE A JOUR ECHARTS ---
    
    // 1. SOC Gauge
    const soc = getVal(46);
    if(chartSoc) {
        chartSoc.setOption({
            series: [{
                data: [{ value: soc.toFixed(1), name: 'SOC' }]
            }]
        });
    }

    // 2. Cells Bar Chart
    // Récupération des cellules (Reg 0 à 15)
    const cellData = [];
    let min = 99, max = 0;
    for(let i=0; i<16; i++) {
        let val = data[i] ? data[i].value : 0;
        if(val > 0) {
            if(val < min) min = val;
            if(val > max) max = val;
        }
        cellData.push(val);
    }

    if(chartCells) {
        // Adaptation dynamique de l'échelle Y pour mieux voir les différences
        const yMin = Math.max(0, min - 0.2); 
        const yMax = max + 0.2;
        
        chartCells.setOption({
            yAxis: { min: yMin.toFixed(1), max: yMax.toFixed(1) },
            series: [{ data: cellData }]
        });
    }

    // Min/Max Texte
    document.getElementById('cell-min-kpi').innerText = min.toFixed(3) + ' V';
    document.getElementById('cell-max-kpi').innerText = max.toFixed(3) + ' V';
    document.getElementById('cell-diff').innerText = (max - min).toFixed(3) + ' V';
});

// --- Autres écouteurs (Stats/Settings) restent similaires au code précédent ---
socket.on('bms-settings', (data) => {
    const container = document.getElementById('settings-form-container');
    if(container.innerHTML.includes("Loading")) container.innerHTML = "";
    
    // Optimisation: on reconstruit seulement si vide, sinon on update les valeurs
    // Pour simplifier le code ici, on reconstruit proprement
    let html = '';
    Object.values(data).forEach(reg => {
        html += `
            <div class="form-group">
                <label>${reg.label}</label>
                <div style="display:flex; gap:10px; align-items:center;">
                    <input type="number" step="0.001" value="${reg.value}" readonly 
                           style="opacity:0.7; cursor:not-allowed;">
                    <span style="color:#666; font-size:0.8em;">${reg.unit}</span>
                </div>
            </div>
        `;
    });
    container.innerHTML = html;
});

socket.on('bms-stats', (data) => {
    const container = document.getElementById('stats-container');
    let html = '<div class="settings-grid">';
    Object.values(data).forEach(reg => {
        html += `
            <div class="card mini">
                <div class="label" style="color:#aaa; font-size:0.8em; margin-bottom:5px;">${reg.label}</div>
                <div class="value" style="font-size:1.1em; font-weight:bold;">${reg.value} <span class="unit" style="font-size:0.7em; color:#666;">${reg.unit}</span></div>
            </div>
        `;
    });
    html += '</div>';
    container.innerHTML = html;
});
