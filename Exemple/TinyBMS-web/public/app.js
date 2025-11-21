const socket = io();

// Instances ECharts
let chartSoc = null;
let chartSoh = null;
let chartCells = null;

// Seuils par défaut pour le graphe (mis à jour par les Settings)
let axisMin = 2.8; 
let axisMax = 4.2;

document.addEventListener('DOMContentLoaded', () => {
    fetchPorts();
    initCharts();
    
    // Ajuster les graphiques lors du redimensionnement de la fenêtre
    window.addEventListener('resize', () => {
        resizeCharts();
    });
});

// --- NAVIGATION ONGLETS ---
function switchTab(tabId) {
    // 1. Cacher tous les contenus
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    
    // 2. Désactiver tous les liens du menu
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    
    // 3. Activer le contenu demandé
    document.getElementById(tabId).classList.add('active');
    
    // 4. Activer le lien du menu correspondant
    const navIndex = tabId === 'dashboard' ? 0 : 1;
    document.querySelectorAll('.nav-links li')[navIndex].classList.add('active');

    // 5. Redimensionner les graphiques car ils étaient cachés (display: none)
    if (tabId === 'dashboard') {
        setTimeout(() => resizeCharts(), 50);
    }
}

function resizeCharts() {
    if(chartSoc) chartSoc.resize();
    if(chartSoh) chartSoh.resize();
    if(chartCells) chartCells.resize();
}

// --- INITIALISATION ECHARTS ---
function initCharts() {
    const commonGaugeOpts = (color, name) => ({
        series: [{
            type: 'gauge',
            startAngle: 180, endAngle: 0,
            min: 0, max: 100,
            splitNumber: 5,
            radius: '100%',
            center: ['50%', '70%'],
            axisLine: { lineStyle: { width: 8, color: [[1, '#333']] } },
            progress: { show: true, width: 8, itemStyle: { color: color } },
            pointer: { show: false },
            axisLabel: { show: false },
            axisTick: { show: false },
            splitLine: { show: false },
            detail: {
                valueAnimation: true,
                offsetCenter: [0, '-20%'],
                fontSize: 24,
                fontWeight: 'bold',
                formatter: '{value}%',
                color: '#fff'
            },
            data: [{ value: 0, name: name }]
        }]
    });

    // 1. SOC
    const domSoc = document.getElementById('chart-soc');
    if(domSoc) {
        chartSoc = echarts.init(domSoc, 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
        chartSoc.setOption(commonGaugeOpts('#6366f1', 'SOC'));
    }

    // 2. SOH
    const domSoh = document.getElementById('chart-soh');
    if(domSoh) {
        chartSoh = echarts.init(domSoh, 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
        chartSoh.setOption(commonGaugeOpts('#10b981', 'SOH'));
    }

    // 3. CELLULES (Bar Chart)
    const domCells = document.getElementById('chart-cells');
    if(domCells) {
        chartCells = echarts.init(domCells, 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
        chartCells.setOption({
            tooltip: { 
                trigger: 'axis', 
                formatter: (params) => {
                    const p = params[0];
                    return `C${p.name}: <b>${p.value} V</b>`; 
                }
            },
            grid: { top: 30, bottom: 25, left: 40, right: 20 },
            xAxis: {
                type: 'category',
                data: Array.from({length: 16}, (_, i) => `${i+1}`),
                axisLine: { lineStyle: { color: '#555' } }
            },
            yAxis: {
                type: 'value',
                min: axisMin, 
                max: axisMax,
                splitLine: { lineStyle: { color: '#333', type: 'dashed' } }
            },
            series: [{
                type: 'bar',
                barWidth: '60%',
                data: [],
                itemStyle: { borderRadius: [4, 4, 0, 0] },
                // Lignes Min/Max
                markLine: {
                    symbol: 'none',
                    label: { position: 'end', color: '#fff', formatter: '{c}' },
                    data: [] 
                }
            }]
        });
    }
}

// --- API / CONNEXION ---
async function fetchPorts() {
    try {
        const res = await fetch('/api/ports');
        const ports = await res.json();
        const select = document.getElementById('portSelect');
        select.innerHTML = ports.map(p => `<option value="${p.path}">${p.path}</option>`).join('');
    } catch(e){}
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
            btn.style.background = "transparent";
            btn.style.border = "1px solid var(--success)";
        }
    } catch(e) {
        alert('Failed to connect');
        btn.innerText = "Connect";
    }
}

// --- RECEPTION DONNÉES LIVE (Dashboard) ---
socket.on('bms-live', (data) => {
    if(!data) return;
    const getVal = (id) => data[id] ? data[id].value : 0;

    // 1. KPI
    document.getElementById('val-voltage').innerText = getVal(36).toFixed(2);
    document.getElementById('val-current').innerText = getVal(38).toFixed(2);
    document.getElementById('val-power').innerText = (getVal(36) * getVal(38)).toFixed(0);
    
    // 2. États
    const stateVal = getVal(50);
    const states = { 0x91: 'CHARGING', 0x92: 'FULL', 0x93: 'DISCHARGING', 0x97: 'IDLE', 0x9B: 'FAULT' };
    const stateEl = document.getElementById('bmsState');
    stateEl.innerText = states[stateVal] || 'UNKNOWN';
    
    if(stateVal === 0x9B) stateEl.style.color = 'var(--danger)';
    else if(stateVal === 0x91) stateEl.style.color = 'var(--success)';
    else stateEl.style.color = 'white';

    // 3. Températures
    document.getElementById('temp-int').innerText = getVal(48) + '°';
    document.getElementById('temp-ext1').innerText = getVal(42) + '°';
    document.getElementById('temp-ext2').innerText = getVal(43) + '°';

    // 4. Jauges
    if(chartSoc) chartSoc.setOption({ series: [{ data: [{ value: getVal(46).toFixed(1), name:'SOC' }] }] });
    if(chartSoh) chartSoh.setOption({ series: [{ data: [{ value: getVal(45).toFixed(1), name:'SOH' }] }] });

    // 5. Graphique Cellules
    const voltages = [];
    let minV = 99, maxV = 0;
    
    for(let i=0; i<16; i++) {
        let v = data[i] ? data[i].value : 0;
        if(v > 0.1) {
            voltages.push(v);
            if(v < minV) minV = v;
            if(v > maxV) maxV = v;
        } else {
            voltages.push(0);
        }
    }

    const balancingMask = getVal(52); // Reg 52: Balancing flags

    // Couleurs conditionnelles
    const seriesData = voltages.map((val, index) => {
        if(val < 0.1) return { value: 0, itemStyle: { color: '#333' } };
        
        let color = '#6366f1'; // Default Indigo
        const isBal = (balancingMask >> index) & 1;
        
        if(isBal) color = '#f59e0b'; // Orange (Balancing)
        else if(val === maxV) color = '#ec4899'; // Pink (Max)
        else if(val === minV) color = '#06b6d4'; // Cyan (Min)
        
        return { value: val, itemStyle: { color: color } };
    });

    if(chartCells) {
        chartCells.setOption({
            yAxis: { min: axisMin, max: axisMax },
            series: [{
                data: seriesData,
                markLine: {
                    data: [
                        { yAxis: maxV, lineStyle: { color: '#ec4899' } },
                        { yAxis: minV, lineStyle: { color: '#06b6d4' } }
                    ]
                }
            }]
        });
    }

    // Stats textes
    document.getElementById('cell-min-txt').innerText = minV.toFixed(3) + ' V';
    document.getElementById('cell-max-txt').innerText = maxV.toFixed(3) + ' V';
    document.getElementById('cell-diff-txt').innerText = (maxV - minV).toFixed(3) + ' V';
});

// --- RECEPTION CONFIGURATION (Onglet Settings) ---
socket.on('bms-settings', (data) => {
    // Update graphique axis limits
    if(data[300]) axisMax = data[300].value; // Fully Charged
    if(data[301]) axisMin = data[301].value; // Fully Discharged

    // Remplissage du formulaire
    const container = document.getElementById('settings-form-container');
    if(container.innerHTML.includes("Waiting")) container.innerHTML = "";

    Object.values(data).forEach(reg => {
        // Check if input exists to update value without redraw
        let input = document.getElementById(`reg-${reg.id}`);
        
        if (!input) {
            // Create Element
            const div = document.createElement('div');
            div.className = 'form-group';
            div.innerHTML = `
                <label>${reg.label} <span style="color:#555; font-size:0.8em">ID:${reg.id}</span></label>
                <div class="input-wrapper">
                    <input type="number" id="reg-${reg.id}" value="${reg.value}" step="0.001">
                    <span>${reg.unit}</span>
                </div>
            `;
            container.appendChild(div);
        } else {
            // Update only if not focused (to allow user typing)
            if (document.activeElement !== input) {
                input.value = reg.value;
            }
        }
    });
});
