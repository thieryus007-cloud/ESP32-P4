const socket = io();
let chartSoc = null, chartSoh = null, chartCells = null;
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
    const idx = tabId === 'dashboard' ? 0 : (tabId === 'stats' ? 1 : 2);
    document.querySelectorAll('.nav-links li')[idx].classList.add('active');
    if (tabId === 'dashboard') setTimeout(resizeCharts, 50);
}

function resizeCharts() {
    if(chartSoc) chartSoc.resize(); if(chartSoh) chartSoh.resize(); if(chartCells) chartCells.resize();
}

// --- CHARTS INIT ---
function initCharts() {
    const gaugeOpts = (color, name) => ({
        series: [{
            type: 'gauge', startAngle: 180, endAngle: 0, min: 0, max: 100, splitNumber: 5, radius: '100%', center: ['50%', '70%'],
            axisLine: { lineStyle: { width: 8, color: [[1, '#333']] } },
            progress: { show: true, width: 8, itemStyle: { color } },
            pointer: { show: false }, axisLabel: { show: false }, axisTick: { show: false }, splitLine: { show: false },
            detail: { valueAnimation: true, offsetCenter: [0, '-20%'], fontSize: 24, formatter: '{value}%', color: '#fff' },
            data: [{ value: 0, name }]
        }]
    });

    chartSoc = echarts.init(document.getElementById('chart-soc'), 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
    chartSoc.setOption(gaugeOpts('#6366f1', 'SOC'));

    chartSoh = echarts.init(document.getElementById('chart-soh'), 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
    chartSoh.setOption(gaugeOpts('#10b981', 'SOH'));

    chartCells = echarts.init(document.getElementById('chart-cells'), 'dark', { renderer: 'canvas', backgroundColor: 'transparent' });
    chartCells.setOption({
        tooltip: { trigger: 'axis', formatter: (p) => `C${p[0].name}: <b>${p[0].value} V</b>` },
        grid: { top: 30, bottom: 25, left: 40, right: 20 },
        xAxis: { type: 'category', data: Array.from({length: 16}, (_, i) => `${i+1}`), axisLine: { lineStyle: { color: '#555' } } },
        yAxis: { type: 'value', min: axisMin, max: axisMax, splitLine: { lineStyle: { color: '#333', type: 'dashed' } } },
        series: [{ type: 'bar', barWidth: '60%', data: [], itemStyle: { borderRadius: [4, 4, 0, 0] }, markLine: { symbol: 'none', label: { position: 'end', color: '#fff' }, data: [] } }]
    });
}

// --- DATA HANDLING ---
socket.on('bms-live', (data) => {
    if(!data) return;
    const getVal = (id) => data[id] ? data[id].value : 0;

    document.getElementById('val-voltage').innerText = getVal(36).toFixed(2);
    document.getElementById('val-current').innerText = getVal(38).toFixed(2);
    document.getElementById('val-power').innerText = (getVal(36) * getVal(38)).toFixed(0);
    
    const stateMap = { 0x91: 'CHARGING', 0x92: 'FULL', 0x93: 'DISCHARGING', 0x97: 'IDLE', 0x9B: 'FAULT' };
    const stateVal = getVal(50);
    const stateEl = document.getElementById('bmsState');
    stateEl.innerText = stateMap[stateVal] || 'UNKNOWN';
    stateEl.style.color = stateVal === 0x9B ? '#ef4444' : (stateVal === 0x91 ? '#10b981' : '#fff');

    document.getElementById('temp-int').innerText = getVal(48) + '°';
    document.getElementById('temp-ext1').innerText = getVal(42) + '°';
    document.getElementById('temp-ext2').innerText = getVal(43) + '°';

    if(chartSoc) chartSoc.setOption({ series: [{ data: [{ value: getVal(46).toFixed(1), name:'SOC' }] }] });
    if(chartSoh) chartSoh.setOption({ series: [{ data: [{ value: getVal(45).toFixed(1), name:'SOH' }] }] });

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

    const balMask = getVal(52);
    const seriesData = voltages.map((val, index) => {
        if(val < 0.1) return { value: 0, itemStyle: { color: '#333' } };
        let color = '#6366f1';
        if((balMask >> index) & 1) color = '#f59e0b';
        else if(val === maxV) color = '#ec4899';
        else if(val === minV) color = '#06b6d4';
        return { value: val, itemStyle: { color } };
    });

    if(chartCells) {
        chartCells.setOption({
            yAxis: { min: axisMin, max: axisMax },
            series: [{ data: seriesData, markLine: { data: [{ yAxis: maxV, lineStyle: { color: '#ec4899' } }, { yAxis: minV, lineStyle: { color: '#06b6d4' } }] } }]
        });
    }

    document.getElementById('cell-min-txt').innerText = minV.toFixed(3) + ' V';
    document.getElementById('cell-max-txt').innerText = maxV.toFixed(3) + ' V';
    document.getElementById('cell-diff-txt').innerText = (maxV - minV).toFixed(3) + ' V';
});

// --- STATS & SETTINGS ---
socket.on('bms-stats', (data) => {
    const container = document.getElementById('stats-container');
    let html = '';
    Object.values(data).forEach(reg => {
        html += `<div class="card mini"><div style="color:#aaa; font-size:0.8em;">${reg.label}</div><div style="font-size:1.1em; font-weight:bold;">${reg.value} <span style="font-size:0.7em; color:#666;">${reg.unit}</span></div></div>`;
    });
    container.innerHTML = html;
});

socket.on('bms-settings', (data) => {
    if(data[300]) axisMax = data[300].value;
    if(data[301]) axisMin = data[301].value;

    const container = document.getElementById('settings-form-container');
    if(container.innerHTML.includes("Loading")) container.innerHTML = "";

    Object.values(data).forEach(reg => {
        if (!document.getElementById(`wrapper-${reg.id}`)) {
            const div = document.createElement('div');
            div.className = 'form-group';
            div.id = `wrapper-${reg.id}`;
            div.innerHTML = `
                <label>${reg.label} <span style="color:#555; font-size:0.8em">ID:${reg.id}</span></label>
                <div class="input-wrapper">
                    <input type="number" id="reg-${reg.id}" value="${reg.value}" step="0.001">
                    <span style="line-height: 30px; min-width:30px; font-size:0.8em; color:#888;">${reg.unit}</span>
                    <button class="btn-save" onclick="saveSetting(${reg.id})">Save</button>
                </div>`;
            container.appendChild(div);
        } else {
            const input = document.getElementById(`reg-${reg.id}`);
            if (document.activeElement !== input) input.value = reg.value;
        }
    });
});

async function saveSetting(id) {
    const input = document.getElementById(`reg-${id}`);
    const btn = document.querySelector(`#wrapper-${id} .btn-save`);
    const originalText = btn.innerText;
    btn.innerText = "⏳"; btn.disabled = true;

    try {
        const res = await fetch('/api/write', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id, value: input.value })
        });
        const data = await res.json();
        if(data.success) btn.innerText = "✅";
        else alert("Write failed: " + data.error);
    } catch(e) { alert("Error: " + e.message); }
    
    setTimeout(() => { btn.innerText = originalText; btn.disabled = false; }, 1500);
}

// --- CONNECT ---
async function fetchPorts() {
    const res = await fetch('/api/ports');
    const ports = await res.json();
    document.getElementById('portSelect').innerHTML = ports.map(p => `<option value="${p.path}">${p.path}</option>`).join('');
}

async function connectBMS() {
    const path = document.getElementById('portSelect').value;
    await fetch('/api/connect', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ path }) });
    document.getElementById('connectionStatus').classList.add('connected');
}
