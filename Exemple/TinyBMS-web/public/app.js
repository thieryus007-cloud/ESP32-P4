const socket = io();
let chartSocSoh = null, chartTemps = null, chartCells = null, chartBattery = null;
let axisMin = 2.8, axisMax = 4.2;

document.addEventListener('DOMContentLoaded', () => {
    fetchPorts();
    initCharts();
    window.addEventListener('resize', resizeCharts);
});

function switchTab(tabId) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');
    const idx = tabId === 'dashboard' ? 0 : (tabId === 'settings' ? 1 : 2);
    document.querySelectorAll('.nav-links li')[idx].classList.add('active');
    if (tabId === 'dashboard') setTimeout(resizeCharts, 50);
}

function resizeCharts() { if(chartSocSoh) chartSocSoh.resize(); if(chartTemps) chartTemps.resize(); if(chartCells) chartCells.resize(); if(chartBattery) chartBattery.resize(); }

// Fonction pour décoder le registre 501 (Firmware Version + BMS Info)
function decodeRegister501(value) {
    const fwVersion = value & 0xFF;           // LSB = Version firmware publique
    const flags = (value >> 8) & 0xFF;        // MSB = Flags (BPT + BCS)

    // Bit 0: BMS Power Type
    const isHighPower = (flags & 0x01) !== 0;
    const powerType = isHighPower ? 'High Power' : 'Low Power';

    // Bits 1-2: BMS Current Sensor
    const sensorType = (flags >> 1) & 0x03;
    let currentSensor;
    switch (sensorType) {
        case 0x00: currentSensor = 'Internal Resistor'; break;
        case 0x01: currentSensor = 'Internal HALL'; break;
        case 0x02: currentSensor = 'External'; break;
        default:   currentSensor = 'Unknown'; break;
    }

    return { fwVersion, powerType, currentSensor };
}

// Fonction helper pour calculer les segments de couleur de l'arc
function getColoredAxisLine(values, min, max, defaultColor = '#2a2a3a') {
    // values: array de {value: number, color: string}
    // Trier les valeurs par ordre croissant
    const sorted = values.map(v => ({
        normalized: (v.value - min) / (max - min),
        color: v.color
    })).sort((a, b) => a.normalized - b.normalized);

    const segments = [];
    let prevNorm = 0;

    sorted.forEach((item) => {
        if (item.normalized > prevNorm) {
            segments.push([item.normalized, item.color]);
        }
        prevNorm = item.normalized;
    });

    // Ajouter le segment final avec la couleur par défaut si nécessaire
    if (prevNorm < 1) {
        segments.push([1, defaultColor]);
    }

    return segments;
}

function updateConnectionUI(mode) {
    const dot = document.getElementById('connectionDot');
    const badge = document.getElementById('modeBadge');
    const btn = document.querySelector('.port-selector button');
    dot.className = 'status-dot'; badge.className = 'mode-badge';
    if (mode === 'CONNECTED') { dot.classList.add('connected'); badge.classList.add('connected'); badge.innerText = 'ONLINE'; btn.innerText = "Disconnect"; }
    else if (mode === 'SIMULATION') { dot.classList.add('simulation'); badge.classList.add('simulation'); badge.innerText = 'SIMULATION'; btn.innerText = "Stop Sim"; }
    else { badge.classList.add('disconnected'); badge.innerText = 'DISCONNECTED'; btn.innerText = "Connect"; }
}

// --- SETTINGS & WRITE LOGIC ---
socket.on('bms-settings', (data) => {
    if(data[300]) axisMax = data[300].value;
    if(data[301]) axisMin = data[301].value;

    // Update dashboard configuration summary
    if(data[340]) {
        const modeValue = data[340].value;
        const modeText = modeValue === 0 ? 'Dual Port Mode' : modeValue === 1 ? 'Single Port Mode' : 'Unknown';
        document.getElementById('info-mode').innerText = modeText;
    }
    if(data[306]) {
        document.getElementById('info-capacity').innerText = `${data[306].value} ${data[306].unit}`;
    }
    if(data[315]) {
        document.getElementById('info-ov-cutoff').innerText = `${data[315].value} ${data[315].unit}`;
    }
    if(data[316]) {
        document.getElementById('info-uv-cutoff').innerText = `${data[316].value} ${data[316].unit}`;
    }
    if(data[317]) {
        document.getElementById('info-discharge-oc').innerText = `${data[317].value} ${data[317].unit}`;
    }
    if(data[318]) {
        document.getElementById('info-charge-oc').innerText = `${data[318].value} ${data[318].unit}`;
    }
    if(data[305]) {
        document.getElementById('info-peak-discharge').innerText = `${data[305].value} ${data[305].unit}`;
    }

    // Décodage du registre 501 (Firmware Version + BMS Info)
    if(data[501]) {
        const decoded = decodeRegister501(data[501].value);
        document.getElementById('info-fw-version').innerText = `v${decoded.fwVersion}`;
        document.getElementById('info-power-type').innerText = decoded.powerType;
        document.getElementById('info-current-sensor').innerText = decoded.currentSensor;
    }

    const groups = ['battery', 'safety', 'balance', 'hardware'];
    groups.forEach(gid => {
        const container = document.getElementById(`conf-${gid}`);
        if(container && container.innerHTML.includes("Loading")) container.innerHTML = "";
    });

    Object.values(data).forEach(reg => {
        const gid = reg.group || 'hardware';
        const container = document.getElementById(`conf-${gid}`);
        if (container) {
            if (!document.getElementById(`wrapper-${reg.id}`)) {
                const div = document.createElement('div');
                div.className = 'form-group';
                div.id = `wrapper-${reg.id}`;

                // Récupérer les contraintes pour ce registre
                const constraints = REGISTER_CONSTRAINTS[reg.id];
                let newInputHTML = '';

                if (constraints && constraints.type === 'select') {
                    // Créer un dropdown pour les valeurs énumérées
                    const options = constraints.options.map(opt =>
                        `<option value="${opt.value}">${opt.label}</option>`
                    ).join('');
                    newInputHTML = `<select id="new-${reg.id}" class="new-select"><option value="">--</option>${options}</select>`;
                } else {
                    // Créer un input avec contraintes min/max
                    const min = constraints?.min !== undefined ? `min="${constraints.min}"` : '';
                    const max = constraints?.max !== undefined ? `max="${constraints.max}"` : '';
                    const step = constraints?.step || 0.001;
                    newInputHTML = `<input type="number" id="new-${reg.id}" placeholder="--" step="${step}" ${min} ${max}>`;
                }

                div.innerHTML = `
                    <label>${reg.label} <span style="color:#555; font-size:0.7em">[${reg.id}]</span></label>
                    <div class="dual-input-wrapper">
                        <div class="input-field current-value">
                            <input type="text" id="current-${reg.id}" value="${reg.value} ${reg.unit}" readonly>
                        </div>
                        <div class="input-field new-value">
                            ${newInputHTML}
                            <span class="unit">${reg.unit}</span>
                        </div>
                    </div>`;
                container.appendChild(div);
            } else {
                const currentInput = document.getElementById(`current-${reg.id}`);
                const newInput = document.getElementById(`new-${reg.id}`);
                if (currentInput) currentInput.value = `${reg.value} ${reg.unit}`;
                if (newInput && document.activeElement !== newInput && newInput.tagName === 'INPUT') {
                    // Ne pas changer le placeholder si l'utilisateur est en train de saisir
                }
            }
        }
    });
});

async function saveSection(groupId) {
    const container = document.getElementById(`conf-${groupId}`);
    const newInputs = container.querySelectorAll('input[id^="new-"], select[id^="new-"]');
    const changes = [];
    newInputs.forEach(input => {
        // Seulement ajouter si une nouvelle valeur a été saisie
        if (input.value && input.value.trim() !== '' && input.value !== '--') {
            const id = input.id.replace('new-', '');
            const numValue = parseFloat(input.value);

            // Validation des contraintes
            const constraints = REGISTER_CONSTRAINTS[parseInt(id)];
            if (constraints && constraints.type !== 'select') {
                if (constraints.min !== undefined && numValue < constraints.min) {
                    alert(`Value for register ${id} is below minimum (${constraints.min})`);
                    return;
                }
                if (constraints.max !== undefined && numValue > constraints.max) {
                    alert(`Value for register ${id} is above maximum (${constraints.max})`);
                    return;
                }
            }

            changes.push({ id: id, value: input.value });
        }
    });
    if (changes.length === 0) {
        alert('No changes to save. Please enter new values.');
        return;
    }

    const btn = container.parentNode.querySelector('button');
    const oldText = btn.innerText;
    btn.innerText = "Sending..."; btn.disabled = true;

    try {
        const res = await fetch('/api/write-batch', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ changes })
        });
        const data = await res.json();
        if(data.success) {
            btn.innerText = "Saved ✅";
            // Vider les champs "new-" après succès
            newInputs.forEach(input => {
                if (input.tagName === 'SELECT') {
                    input.selectedIndex = 0; // Remettre au premier choix (vide)
                } else {
                    input.value = ''; // Vider l'input
                }
            });
        }
        else { alert("Error"); btn.innerText = "Error ❌"; }
    } catch(e) { alert(e.message); btn.innerText = "Error ❌"; }
    setTimeout(() => { btn.innerText = oldText; btn.disabled = false; }, 2000);
}

// --- LIVE DATA ---
socket.on('bms-live', (data) => {
    if(!data) return;
    const getVal = (id) => data[id] ? data[id].value : 0;

    const voltage = getVal(36);
    const current = getVal(38);
    const power = voltage * current;

    // Mise à jour du triple gauge
    if(chartBattery) {
        chartBattery.setOption({
            series: [
                { data: [{ value: voltage.toFixed(2) }] },
                { data: [{ value: power.toFixed(0) }] },
                { data: [{ value: current.toFixed(2) }] }
            ]
        });
    }

    const sVal = getVal(50);
    const sEl = document.getElementById('bmsState');
    sEl.innerText = {0x91:'CHARGING',0x92:'FULL',0x93:'DISCHARGING',0x96:'REGENERATION',0x97:'IDLE',0x9B:'FAULT'}[sVal] || 'UNKNOWN';
    sEl.style.color = sVal===0x9B?'var(--danger)':(sVal===0x91||sVal===0x96?'var(--success)':'#fff');

    // Mise à jour du gauge SOC/SOH
    if(chartSocSoh) {
        const socVal = parseFloat(getVal(46).toFixed(1));
        const sohVal = parseFloat(getVal(45).toFixed(1));
        const socSohColors = getColoredAxisLine([
            { value: socVal, color: '#6366f1' },
            { value: sohVal, color: '#10b981' }
        ], 20, 100);

        chartSocSoh.setOption({
            series: [
                {
                    data: [{ value: socVal, name: 'SOC' }],
                    axisLine: { lineStyle: { width: 4, color: socSohColors } }
                },
                { data: [{ value: sohVal, name: 'SOH' }] }
            ]
        });
    }

    // Mise à jour du gauge températures - Format de l'exemple ECharts
    if(chartTemps) {
        const tempInt = parseFloat(getVal(48).toFixed(1));
        const tempS1 = parseFloat(getVal(42).toFixed(1));
        const tempS2 = parseFloat(getVal(43).toFixed(1));

        const tempGaugeData = [
            {
                value: tempS2,
                name: 'S2',
                title: { offsetCenter: ['-80%', '55%'] },
                detail: { offsetCenter: ['-80%', '75%'] },
                itemStyle: { color: '#06b6d4' }
            },
            {
                value: tempS1,
                name: 'S1',
                title: { offsetCenter: ['0%', '55%'] },
                detail: { offsetCenter: ['0%', '75%'] },
                itemStyle: { color: '#ec4899' }
            },
            {
                value: tempInt,
                name: 'Int',
                title: { offsetCenter: ['80%', '55%'] },
                detail: { offsetCenter: ['80%', '75%'] },
                itemStyle: { color: '#f59e0b' }
            }
        ];

        chartTemps.setOption({
            series: [{
                data: tempGaugeData
            }]
        });
    }

    const voltages = [], balMask = getVal(52);
    let minV=99, maxV=0;
    for(let i=0; i<16; i++) {
        let v = data[i] ? data[i].value : 0;
        if(v>0.1) { voltages.push(v); if(v<minV) minV=v; if(v>maxV) maxV=v; } else voltages.push(0);
    }

    const seriesData = voltages.map((val, i) => {
        if(val<0.1) return { value:0, itemStyle:{color:'#333'} };
        let c = '#6366f1';
        if((balMask>>i)&1) c='#f59e0b'; else if(val===maxV) c='#ec4899'; else if(val===minV) c='#06b6d4';
        return { value:val, itemStyle:{color:c} };
    });

    if(chartCells) chartCells.setOption({
        yAxis: { min: axisMin, max: axisMax },
        series: [{ data: seriesData, markLine: { data: [{ yAxis: maxV, lineStyle:{color:'#ec4899'} }, { yAxis: minV, lineStyle:{color:'#06b6d4'} }] } }]
    });

    document.getElementById('cell-min-txt').innerText = minV.toFixed(3)+' V';
    document.getElementById('cell-max-txt').innerText = maxV.toFixed(3)+' V';
    document.getElementById('cell-diff-txt').innerText = (maxV-minV).toFixed(3)+' V';
});

function initCharts() {
    // Gauge combinée SOC/SOH
    chartSocSoh = echarts.init(document.getElementById('chart-soc-soh'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    const socSohColors = getColoredAxisLine([
        { value: 80, color: '#6366f1' },
        { value: 95, color: '#10b981' }
    ], 20, 100);

    chartSocSoh.setOption({
        series: [
            {
                type: 'gauge',
                radius: '90%',
                center: ['50%', '55%'],
                startAngle: 200,
                endAngle: -20,
                min: 20,
                max: 100,
                splitNumber: 4,
                itemStyle: { color: '#6366f1' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '65%',
                    width: 5,
                    itemStyle: { color: '#6366f1' }
                },
                axisLine: { lineStyle: { width: 4, color: socSohColors } },
                axisTick: {
                    show: true,
                    distance: 5,
                    length: 6,
                    splitNumber: 5,
                    lineStyle: { width: 1, color: '#888' }
                },
                splitLine: {
                    show: true,
                    distance: 5,
                    length: 12,
                    lineStyle: { width: 2, color: '#aaa' }
                },
                axisLabel: {
                    show: true,
                    distance: 10,
                    color: '#ddd',
                    fontSize: 10
                },
                anchor: { show: true, size: 10, itemStyle: { color: '#6366f1' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['-50%', '65%'], fontSize: 12, fontWeight: 'bold', formatter: 'SOC {value}%', color: '#6366f1' },
                data: [{ value: 80 }]
            },
            {
                type: 'gauge',
                radius: '90%',
                center: ['50%', '55%'],
                startAngle: 200,
                endAngle: -20,
                min: 20,
                max: 100,
                splitNumber: 4,
                itemStyle: { color: '#10b981' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '55%',
                    width: 4,
                    itemStyle: { color: '#10b981' }
                },
                axisLine: { show: false },
                axisTick: { show: false },
                splitLine: { show: false },
                axisLabel: { show: false },
                anchor: { show: true, size: 8, itemStyle: { color: '#10b981' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['50%', '65%'], fontSize: 12, fontWeight: 'bold', formatter: 'SOH {value}%', color: '#10b981' },
                data: [{ value: 95 }]
            }
        ]
    });

    // Gauge multi-températures - CODE SOURCE EXACT de l'exemple ECharts
    chartTemps = echarts.init(document.getElementById('chart-temps'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});

    const tempGaugeData = [
        {
            value: 25,
            name: 'S2',
            title: { offsetCenter: ['-80%', '60%'] },
            detail: { offsetCenter: ['-80%', '75%'] },
            itemStyle: { color: '#06b6d4' }
        },
        {
            value: 40,
            name: 'S1',
            title: { offsetCenter: ['0%', '60%'] },
            detail: { offsetCenter: ['0%', '75%'] },
            itemStyle: { color: '#ec4899' }
        },
        {
            value: 55,
            name: 'Int',
            title: { offsetCenter: ['80%', '60%'] },
            detail: { offsetCenter: ['80%', '75%'] },
            itemStyle: { color: '#f59e0b' }
        }
    ];

    chartTemps.setOption({
        series: [
            {
                type: 'gauge',
                center: ['50%', '55%'],
                radius: '90%',
                min: 0,
                max: 70,
                startAngle: 200,
                endAngle: -20,
                splitNumber: 7,
                anchor: {
                    show: true,
                    showAbove: true,
                    size: 4,
                    itemStyle: {
                        color: '#FAC858'
                    }
                },
                pointer: {
                    icon: 'path://M2.9,0.7L2.9,0.7c1.4,0,2.6,1.2,2.6,2.6v115c0,1.4-1.2,2.6-2.6,2.6l0,0c-1.4,0-2.6-1.2-2.6-2.6V3.3C0.3,1.9,1.4,0.7,2.9,0.7z',
                    width: 4,
                    length: '80%',
                    offsetCenter: [0, '8%']
                },
                progress: {
                    show: true,
                    overlap: true,
                    roundCap: true,
                    width: 5
                },
                axisLine: {
                    roundCap: true,
                    lineStyle: {
                        width: 5
                    }
                },
                axisTick: {
                    show: true,
                    splitNumber: 2
                },
                axisLabel: {
                    show: true,
                    fontSize: 10,
                    distance: 10
                },
                splitLine: {
                    show: true,
                    length: 10
                },
                data: tempGaugeData,
                title: {
                    fontSize: 12
                },
                detail: {
                    width: 25,
                    height: 12,
                    fontSize: 12,
                    color: '#fff',
                    backgroundColor: 'inherit',
                    borderRadius: 3,
                    formatter: '{value}°C'
                }
            }
        ]
    });

    // Triple gauge pour Voltage, Power, Current
    chartBattery = echarts.init(document.getElementById('chart-battery'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartBattery.setOption({
        series: [
            {
                type: 'gauge',
                radius: '55%',
                center: ['20%', '60%'],
                startAngle: 280,
                endAngle: 60,
                min: 40,
                max: 60,
                splitNumber: 4,
                itemStyle: { color: '#06b6d4' },
                progress: { show: true, width: 4 },
                pointer: { show: true, width: 2 },
                axisLine: { lineStyle: { width: 4, color: [[1, '#333']] } },
                axisTick: { distance: 4, splitNumber: 5, lineStyle: { width: 1, color: '#888' } },
                splitLine: { distance: 4, length: 10, lineStyle: { width: 1.5, color: '#aaa' } },
                axisLabel: { distance: 10, color: '#ddd', fontSize: 10 },
                anchor: { show: false },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: ['60%', '10%'], fontSize: 12, fontWeight: 'bolder', formatter: '{value} V', color: '#fff' },
                data: [{ value: 50 }]
            },
            {
                type: 'gauge',
                radius: '90%',
                center: ['50%', '60%'],
                startAngle: 160,
                endAngle: 20,
                min: -5000,
                max: 5000,
                splitNumber: 6,
                itemStyle: { color: '#6366f1' },
                progress: { show: true, width: 4 },
                pointer: {
                    show: true,
                    length: '65%',
                    width: 5,
                    itemStyle: { color: '#6366f1' }
                },
                axisLine: { lineStyle: { width: 5, color: [[1, '#333']] } },
                axisTick: { distance: 5, splitNumber: 5, lineStyle: { width: 1, color: '#888' } },
                splitLine: { distance: 5, length: 12, lineStyle: { width: 2, color: '#aaa' } },
                axisLabel: { distance: 10, color: '#ddd', fontSize: 10, formatter: (v) => v === 0 ? '0' : (v / 1000).toFixed(0) + 'k' },
                anchor: { show: true, size: 10, itemStyle: { color: '#6366f1' } },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: [0, '30%'], fontSize: 16, fontWeight: 'bolder', formatter: '{value} W', color: '#fff' },
                data: [{ value: 0 }]
            },
            {
                type: 'gauge',
                radius: '55%',
                center: ['80%', '60%'],
                startAngle: 120,
                endAngle: 280,
                min: -120,
                max: 120,
                splitNumber: 4,
                itemStyle: { color: '#10b981' },
                progress: { show: true, width: 4 },
                pointer: {
                    show: true,
                    length: '65%',
                    width: 4,
                    itemStyle: { color: '#10b981' }
                },
                axisLine: { lineStyle: { width: 4, color: [[1, '#333']] } },
                axisTick: { distance: 4, splitNumber: 5, lineStyle: { width: 1, color: '#888' } },
                splitLine: { distance: 4, length: 10, lineStyle: { width: 1.5, color: '#aaa' } },
                axisLabel: { distance: 10, color: '#ddd', fontSize: 10 },
                anchor: { show: true, size: 8, itemStyle: { color: '#10b981' } },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: ['-60%', '10%'], fontSize: 12, fontWeight: 'bolder', formatter: '{value} A', color: '#fff' },
                data: [{ value: 0 }]
            }
        ]
    });

    chartCells = echarts.init(document.getElementById('chart-cells'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartCells.setOption({
        tooltip: { trigger:'axis', formatter:(p)=>`C${p[0].name}: <b>${p[0].value} V</b>` },
        grid: {top:30, bottom:25, left:40, right:20}, xAxis: {type:'category', data:Array.from({length:16},(_,i)=>`${i+1}`), axisLine:{lineStyle:{color:'#555'}}},
        yAxis: {type:'value', min:2.8, max:4.2, splitLine:{lineStyle:{color:'#333', type:'dashed'}}},
        series: [{type:'bar', barWidth:'60%', data:[], itemStyle:{borderRadius:[4,4,0,0]}, markLine:{symbol:'none', label:{position:'end', color:'#fff'}, data:[]}}]
    });
}

socket.on('status-change', (d) => updateConnectionUI(d.mode));
socket.on('bms-stats', (d) => {
    const c = document.getElementById('stats-container');
    let h = ''; Object.values(d).forEach(r => h+=`<div class="card" style="padding:10px;"><div style="color:#aaa;font-size:0.8em;">${r.label}</div><div style="font-size:1.1em;font-weight:bold;">${r.value} <span style="font-size:0.7em;color:#666;">${r.unit}</span></div></div>`);
    c.innerHTML = h;
});

async function fetchPorts() { try { const r=await fetch('/api/ports'); const p=await r.json(); document.getElementById('portSelect').innerHTML=p.map(x=>`<option value="${x.path}">${x.path==='SIMULATION'?'🛠️ SIMULATION':x.path}</option>`).join(''); } catch(e){} }
async function connectBMS() {
    const path=document.getElementById('portSelect').value;
    const protocol=document.getElementById('protocolSelect').value;
    await fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path, protocol: parseInt(protocol)})});
}

// --- LOGS SYSTEM ---
const MAX_LOGS = 100; // Limite le nombre de logs affichés
let logCount = 0;

function addLog(message, type = 'info') {
    const container = document.getElementById('logs-container');
    if (!container) return;

    const time = new Date().toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const logEntry = document.createElement('div');
    logEntry.className = `log-entry log-${type}`;
    logEntry.innerHTML = `<span class="log-time">${time}</span><span class="log-message">${message}</span>`;

    container.appendChild(logEntry);
    logCount++;

    // Limiter le nombre de logs
    if (logCount > MAX_LOGS) {
        const firstLog = container.firstChild;
        if (firstLog) container.removeChild(firstLog);
        logCount--;
    }

    // Auto-scroll vers le bas
    container.scrollTop = container.scrollHeight;
}

function clearLogs() {
    const container = document.getElementById('logs-container');
    if (container) {
        container.innerHTML = '';
        logCount = 0;
        addLog('Logs cleared', 'info');
    }
}

// ======================
// BMS EVENTS MANAGEMENT
// ======================
let previousBmsStatus = null;
let previousBalancingCells = 0;

function addEvent(message, type = 'info', icon = 'ℹ️') {
    const container = document.getElementById('events-container');
    if (!container) return;

    const entry = document.createElement('div');
    entry.className = `event-entry event-${type}`;

    const time = new Date().toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit' });

    entry.innerHTML = `
        <span class="event-icon">${icon}</span>
        <span class="event-time">${time}</span>
        <span class="event-message">${message}</span>
    `;

    container.insertBefore(entry, container.firstChild);

    // Limiter à 50 événements
    while (container.children.length > 50) {
        container.removeChild(container.lastChild);
    }
}

function clearEvents() {
    const container = document.getElementById('events-container');
    if (container) container.innerHTML = '';
    addEvent('Events cleared', 'info', 'ℹ️');
}

// BMS Management Commands
async function clearBMSEvents() {
    if (!confirm('Clear all BMS events?\n\nThis will erase the event history stored in the TinyBMS.')) {
        return;
    }

    try {
        addLog('Sending Clear Events command to BMS...', 'info');
        const response = await fetch('/api/bms-command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: 'clear-events' })
        });

        const result = await response.json();
        if (result.success) {
            addLog('BMS events cleared successfully', 'success');
            addEvent('BMS Events cleared by user', 'success', '🗑️');
        } else {
            addLog(`Failed to clear events: ${result.error}`, 'error');
        }
    } catch (error) {
        addLog(`Error clearing events: ${error.message}`, 'error');
    }
}

async function clearBMSStatistics() {
    if (!confirm('Clear all BMS statistics?\n\nThis will reset all counters (distance, cycles, protection counts, etc.).')) {
        return;
    }

    try {
        addLog('Sending Clear Statistics command to BMS...', 'info');
        const response = await fetch('/api/bms-command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: 'clear-statistics' })
        });

        const result = await response.json();
        if (result.success) {
            addLog('BMS statistics cleared successfully', 'success');
            addEvent('BMS Statistics reset by user', 'success', '📊');
            // Recharger les statistiques
            setTimeout(() => {
                window.location.reload();
            }, 1000);
        } else {
            addLog(`Failed to clear statistics: ${result.error}`, 'error');
        }
    } catch (error) {
        addLog(`Error clearing statistics: ${error.message}`, 'error');
    }
}

async function resetBMS() {
    if (!confirm('⚠️ WARNING: Reset BMS?\n\nThis will restart the TinyBMS completely.\n\nThe connection will be lost and you will need to reconnect.\n\nContinue?')) {
        return;
    }

    try {
        addLog('Sending Reset command to BMS...', 'warning');
        addEvent('BMS Reset initiated by user', 'warning', '🔄');

        const response = await fetch('/api/bms-command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: 'reset-bms' })
        });

        const result = await response.json();
        if (result.success) {
            addLog('BMS reset command sent. BMS is restarting...', 'success');
            addEvent('BMS is restarting...', 'info', '⏳');

            // Afficher un message d'attente
            setTimeout(() => {
                addLog('Waiting for BMS to restart (5 seconds)...', 'info');
            }, 1000);

            // Après 6 secondes, suggérer de reconnecter
            setTimeout(() => {
                addLog('BMS should be ready. Please reconnect.', 'success');
                addEvent('BMS restart complete - Please reconnect', 'success', '✅');
            }, 6000);
        } else {
            addLog(`Failed to reset BMS: ${result.error}`, 'error');
        }
    } catch (error) {
        addLog(`Error resetting BMS: ${error.message}`, 'error');
    }
}

function updateBmsStatusBadges(data) {
    if (!data) return;

    // Status Balancing (registre 52)
    const balancingReg = data[52];
    if (balancingReg !== undefined) {
        const balancingValue = balancingReg.value || 0;
        const activeCells = balancingValue.toString(2).split('1').length - 1;

        document.getElementById('balancing-cells').textContent = activeCells > 0 ? `${activeCells} cells` : 'Inactive';
        const balancingBadge = document.getElementById('status-balancing');
        balancingBadge.classList.toggle('active', activeCells > 0);

        // Événement si changement
        if (activeCells > 0 && previousBalancingCells === 0) {
            addEvent(`Balancing started on ${activeCells} cell(s)`, 'info', '⚖️');
        } else if (activeCells === 0 && previousBalancingCells > 0) {
            addEvent('Balancing completed', 'success', '✅');
        }
        previousBalancingCells = activeCells;
    }

    // Status Charging (registre 50)
    const statusReg = data[50];
    if (statusReg !== undefined) {
        const statusValue = statusReg.value || 0;
        const statusMap = {
            0x91: { text: 'CHARGING', icon: '🔌', type: 'active' },
            0x92: { text: 'FULL', icon: '✅', type: 'active' },
            0x93: { text: 'DISCHARGING', icon: '⚡', type: 'active' },
            0x96: { text: 'REGENERATION', icon: '♻️', type: 'active' },
            0x97: { text: 'IDLE', icon: '💤', type: '' },
            0x9B: { text: 'FAULT', icon: '⚠️', type: 'error' }
        };

        const status = statusMap[statusValue] || { text: 'UNKNOWN', icon: '❓', type: '' };
        document.getElementById('charging-status').textContent = status.text;
        const chargingBadge = document.getElementById('status-charging');
        chargingBadge.className = 'status-badge';
        if (status.type) chargingBadge.classList.add(status.type);

        // Événement si changement de statut
        if (previousBmsStatus !== null && previousBmsStatus !== statusValue) {
            const eventType = statusValue === 0x9B ? 'alarm' : 'info';
            addEvent(`Status changed: ${status.text}`, eventType, status.icon);
        }
        previousBmsStatus = statusValue;

        // Update fault count
        document.getElementById('fault-count').textContent = statusValue === 0x9B ? '1' : '0';
        const faultBadge = document.getElementById('status-fault');
        faultBadge.classList.toggle('error', statusValue === 0x9B);
    }
}

// Écouter les événements Socket.IO
socket.on('connect', () => {
    addEvent('Connected to server', 'success', '🟢');
});

socket.on('disconnect', () => {
    addEvent('Disconnected from server', 'warning', '🔴');
});

socket.on('status-change', (data) => {
    const mode = data.mode;
    const icons = {
        'CONNECTED': '🔌',
        'SIMULATION': '🎮',
        'DISCONNECTED': '⏸️'
    };
    const types = {
        'CONNECTED': 'success',
        'SIMULATION': 'info',
        'DISCONNECTED': 'warning'
    };

    if (mode) {
        addEvent(`Mode: ${mode}`, types[mode] || 'info', icons[mode] || 'ℹ️');
    }
});

socket.on('bms-live', (data) => {
    if (!data) return;
    updateBmsStatusBadges(data);

    // Détecter les alarmes de tension
    const minCell = data[40];
    const maxCell = data[41];
    if (minCell && maxCell) {
        if (minCell.value < 2.8) {
            addEvent(`Under-voltage alarm: ${minCell.value.toFixed(3)}V`, 'alarm', '⚠️');
        }
        if (maxCell.value > 4.2) {
            addEvent(`Over-voltage alarm: ${maxCell.value.toFixed(3)}V`, 'alarm', '⚠️');
        }
    }
});

socket.on('bms-settings', (data) => {
    if (data && Object.keys(data).length > 0) {
        addEvent(`Settings loaded (${Object.keys(data).length} registers)`, 'info', '⚙️');
    }
});

// Log personnalisés du serveur
socket.on('log', (data) => {
    const icons = { info: 'ℹ️', success: '✅', warning: '⚠️', error: '❌', system: '🔧' };
    addEvent(data.message, data.type || 'info', icons[data.type] || 'ℹ️');
});

// Log initial au chargement de la page
document.addEventListener('DOMContentLoaded', () => {
    addLog('TinyBMS Monitor initialized', 'system');
});
