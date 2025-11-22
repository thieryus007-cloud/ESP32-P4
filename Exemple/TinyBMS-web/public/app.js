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
    sEl.innerText = {0x91:'CHARGING',0x92:'FULL',0x93:'DISCHARGING',0x97:'IDLE',0x9B:'FAULT'}[sVal] || 'UNKNOWN';
    sEl.style.color = sVal===0x9B?'var(--danger)':(sVal===0x91?'var(--success)':'#fff');

    // Mise à jour du gauge SOC/SOH
    if(chartSocSoh) {
        chartSocSoh.setOption({
            series: [
                { data: [{ value: getVal(46).toFixed(1), name: 'SOC' }] },
                { data: [{ value: getVal(45).toFixed(1), name: 'SOH' }] }
            ]
        });
    }

    // Mise à jour du gauge températures
    if(chartTemps) {
        chartTemps.setOption({
            series: [
                { data: [{ value: getVal(48).toFixed(1), name: 'Internal' }] },
                { data: [{ value: getVal(42).toFixed(1), name: 'Sensor 1' }] },
                { data: [{ value: getVal(43).toFixed(1), name: 'Sensor 2' }] }
            ]
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
    chartSocSoh.setOption({
        series: [
            {
                type: 'gauge',
                radius: '80%',
                center: ['50%', '45%'],
                startAngle: 200,
                endAngle: -20,
                min: 20,
                max: 100,
                splitNumber: 4,
                itemStyle: { color: '#6366f1' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 5,
                    itemStyle: { color: '#6366f1' }
                },
                axisLine: { lineStyle: { width: 5, color: [[1, '#2a2a3a']] } },
                axisTick: {
                    show: true,
                    distance: -18,
                    length: 6,
                    splitNumber: 5,
                    lineStyle: { width: 1, color: '#555' }
                },
                splitLine: {
                    show: true,
                    distance: -18,
                    length: 12,
                    lineStyle: { width: 2, color: '#888' }
                },
                axisLabel: {
                    show: true,
                    distance: -5,
                    color: '#999',
                    fontSize: 9
                },
                anchor: { show: true, size: 10, itemStyle: { color: '#6366f1' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['-50%', '100%'], fontSize: 13, fontWeight: 'bold', formatter: 'SOC {value}%', color: '#6366f1' },
                data: [{ value: 80 }]
            },
            {
                type: 'gauge',
                radius: '60%',
                center: ['50%', '45%'],
                startAngle: 200,
                endAngle: -20,
                min: 20,
                max: 100,
                splitNumber: 4,
                itemStyle: { color: '#10b981' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 4,
                    itemStyle: { color: '#10b981' }
                },
                axisLine: { lineStyle: { width: 4, color: [[1, '#2a2a3a']] } },
                axisTick: {
                    show: true,
                    distance: -14,
                    length: 5,
                    splitNumber: 5,
                    lineStyle: { width: 1, color: '#555' }
                },
                splitLine: {
                    show: true,
                    distance: -14,
                    length: 10,
                    lineStyle: { width: 2, color: '#888' }
                },
                axisLabel: {
                    show: true,
                    distance: -2,
                    color: '#999',
                    fontSize: 8
                },
                anchor: { show: true, size: 8, itemStyle: { color: '#10b981' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['50%', '133%'], fontSize: 13, fontWeight: 'bold', formatter: 'SOH {value}%', color: '#10b981' },
                data: [{ value: 95 }]
            }
        ]
    });

    // Gauge multi-températures
    chartTemps = echarts.init(document.getElementById('chart-temps'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartTemps.setOption({
        series: [
            {
                type: 'gauge',
                radius: '80%',
                center: ['50%', '45%'],
                startAngle: 200,
                endAngle: -20,
                min: 0,
                max: 70,
                splitNumber: 7,
                itemStyle: { color: '#f59e0b' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 4,
                    itemStyle: { color: '#f59e0b' }
                },
                axisLine: { lineStyle: { width: 4, color: [[1, '#2a2a3a']] } },
                axisTick: {
                    show: true,
                    distance: -16,
                    length: 5,
                    splitNumber: 2,
                    lineStyle: { width: 1, color: '#555' }
                },
                splitLine: {
                    show: true,
                    distance: -16,
                    length: 10,
                    lineStyle: { width: 2, color: '#888' }
                },
                axisLabel: {
                    show: true,
                    distance: -3,
                    color: '#999',
                    fontSize: 8
                },
                anchor: { show: true, size: 8, itemStyle: { color: '#f59e0b' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['-65%', '100%'], fontSize: 12, fontWeight: 'bold', formatter: 'Int {value}°', color: '#f59e0b' },
                data: [{ value: 25 }]
            },
            {
                type: 'gauge',
                radius: '60%',
                center: ['50%', '45%'],
                startAngle: 200,
                endAngle: -20,
                min: 0,
                max: 70,
                splitNumber: 7,
                itemStyle: { color: '#ec4899' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 3,
                    itemStyle: { color: '#ec4899' }
                },
                axisLine: { lineStyle: { width: 3, color: [[1, '#2a2a3a']] } },
                axisTick: {
                    show: true,
                    distance: -12,
                    length: 4,
                    splitNumber: 2,
                    lineStyle: { width: 1, color: '#555' }
                },
                splitLine: {
                    show: true,
                    distance: -12,
                    length: 8,
                    lineStyle: { width: 1.5, color: '#888' }
                },
                axisLabel: {
                    show: true,
                    distance: 0,
                    color: '#999',
                    fontSize: 7
                },
                anchor: { show: true, size: 6, itemStyle: { color: '#ec4899' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['0%', '133%'], fontSize: 12, fontWeight: 'bold', formatter: 'S1 {value}°', color: '#ec4899' },
                data: [{ value: 23 }]
            },
            {
                type: 'gauge',
                radius: '40%',
                center: ['50%', '45%'],
                startAngle: 200,
                endAngle: -20,
                min: 0,
                max: 70,
                splitNumber: 7,
                itemStyle: { color: '#06b6d4' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 2,
                    itemStyle: { color: '#06b6d4' }
                },
                axisLine: { lineStyle: { width: 2.5, color: [[1, '#2a2a3a']] } },
                axisTick: {
                    show: true,
                    distance: -10,
                    length: 3,
                    splitNumber: 2,
                    lineStyle: { width: 1, color: '#555' }
                },
                splitLine: {
                    show: true,
                    distance: -10,
                    length: 6,
                    lineStyle: { width: 1.5, color: '#888' }
                },
                axisLabel: {
                    show: true,
                    distance: 2,
                    color: '#999',
                    fontSize: 6
                },
                anchor: { show: true, size: 5, itemStyle: { color: '#06b6d4' } },
                title: { show: false },
                detail: { valueAnimation: true, offsetCenter: ['65%', '200%'], fontSize: 12, fontWeight: 'bold', formatter: 'S2 {value}°', color: '#06b6d4' },
                data: [{ value: 22 }]
            }
        ]
    });

    // Triple gauge pour Voltage, Power, Current
    chartBattery = echarts.init(document.getElementById('chart-battery'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartBattery.setOption({
        series: [
            {
                type: 'gauge',
                radius: '45%',
                center: ['20%', '55%'],
                startAngle: 200,
                endAngle: -20,
                min: 40,
                max: 60,
                splitNumber: 4,
                itemStyle: { color: '#06b6d4' },
                progress: { show: true, width: 4 },
                pointer: { show: false },
                axisLine: { lineStyle: { width: 4, color: [[1, '#333']] } },
                axisTick: { distance: -18, splitNumber: 5, lineStyle: { width: 1, color: '#555' } },
                splitLine: { distance: -20, length: 8, lineStyle: { width: 1.5, color: '#555' } },
                axisLabel: { distance: -8, color: '#999', fontSize: 9 },
                anchor: { show: false },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: [0, '85%'], fontSize: 14, fontWeight: 'bolder', formatter: '{value} V', color: '#fff' },
                data: [{ value: 50 }]
            },
            {
                type: 'gauge',
                radius: '55%',
                center: ['50%', '55%'],
                startAngle: 180,
                endAngle: 0,
                min: -6000,
                max: 6000,
                splitNumber: 6,
                itemStyle: { color: '#6366f1' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 4,
                    itemStyle: { color: '#6366f1' }
                },
                axisLine: { lineStyle: { width: 5, color: [[1, '#333']] } },
                axisTick: { distance: -22, splitNumber: 5, lineStyle: { width: 1, color: '#555' } },
                splitLine: { distance: -24, length: 10, lineStyle: { width: 1.5, color: '#555' } },
                axisLabel: { distance: -10, color: '#999', fontSize: 10, formatter: (v) => v === 0 ? '0' : (v / 1000).toFixed(0) + 'k' },
                anchor: { show: true, size: 8, itemStyle: { color: '#6366f1' } },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: [0, '85%'], fontSize: 14, fontWeight: 'bolder', formatter: '{value} W', color: '#fff' },
                data: [{ value: 0 }]
            },
            {
                type: 'gauge',
                radius: '45%',
                center: ['80%', '55%'],
                startAngle: 180,
                endAngle: 0,
                min: -120,
                max: 120,
                splitNumber: 4,
                itemStyle: { color: '#10b981' },
                progress: { show: false },
                pointer: {
                    show: true,
                    length: '70%',
                    width: 3,
                    itemStyle: { color: '#10b981' }
                },
                axisLine: { lineStyle: { width: 4, color: [[1, '#333']] } },
                axisTick: { distance: -18, splitNumber: 5, lineStyle: { width: 1, color: '#555' } },
                splitLine: { distance: -20, length: 8, lineStyle: { width: 1.5, color: '#555' } },
                axisLabel: { distance: -8, color: '#999', fontSize: 9 },
                anchor: { show: true, size: 6, itemStyle: { color: '#10b981' } },
                title: { show: false },
                detail: { valueAnimation: true, width: '60%', lineHeight: 18, borderRadius: 8, offsetCenter: [0, '85%'], fontSize: 14, fontWeight: 'bolder', formatter: '{value} A', color: '#fff' },
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
async function connectBMS() { const path=document.getElementById('portSelect').value; await fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({path})}); }
