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
    const idx = tabId === 'dashboard' ? 0 : (tabId === 'settings' ? 1 : 2);
    document.querySelectorAll('.nav-links li')[idx].classList.add('active');
    if (tabId === 'dashboard') setTimeout(resizeCharts, 50);
}

function resizeCharts() { if(chartSoc) chartSoc.resize(); if(chartSoh) chartSoh.resize(); if(chartCells) chartCells.resize(); }

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
                div.innerHTML = `
                    <label>${reg.label} <span style="color:#555; font-size:0.7em">[${reg.id}]</span></label>
                    <div class="dual-input-wrapper">
                        <div class="input-field current-value">
                            <span class="field-label">Current</span>
                            <input type="text" id="current-${reg.id}" value="${reg.value}" readonly>
                            <span class="unit">${reg.unit}</span>
                        </div>
                        <div class="input-field new-value">
                            <span class="field-label">New</span>
                            <input type="number" id="new-${reg.id}" placeholder="${reg.value}" step="0.001">
                            <span class="unit">${reg.unit}</span>
                        </div>
                    </div>`;
                container.appendChild(div);
            } else {
                const currentInput = document.getElementById(`current-${reg.id}`);
                const newInput = document.getElementById(`new-${reg.id}`);
                if (currentInput) currentInput.value = reg.value;
                if (newInput && document.activeElement !== newInput) newInput.placeholder = reg.value;
            }
        }
    });
});

async function saveSection(groupId) {
    const container = document.getElementById(`conf-${groupId}`);
    const newInputs = container.querySelectorAll('input[id^="new-"]');
    const changes = [];
    newInputs.forEach(input => {
        // Seulement ajouter si une nouvelle valeur a été saisie
        if (input.value && input.value.trim() !== '') {
            const id = input.id.replace('new-', '');
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
            newInputs.forEach(input => { if(input.value) input.value = ''; });
        }
        else { alert("Error"); btn.innerText = "Error ❌"; }
    } catch(e) { alert(e.message); btn.innerText = "Error ❌"; }
    setTimeout(() => { btn.innerText = oldText; btn.disabled = false; }, 2000);
}

// --- LIVE DATA ---
socket.on('bms-live', (data) => {
    if(!data) return;
    const getVal = (id) => data[id] ? data[id].value : 0;

    document.getElementById('val-voltage').innerText = getVal(36).toFixed(2);
    document.getElementById('val-current').innerText = getVal(38).toFixed(2);
    document.getElementById('val-power').innerText = (getVal(36) * getVal(38)).toFixed(0);
    
    const sVal = getVal(50);
    const sEl = document.getElementById('bmsState');
    sEl.innerText = {0x91:'CHARGING',0x92:'FULL',0x93:'DISCHARGING',0x97:'IDLE',0x9B:'FAULT'}[sVal] || 'UNKNOWN';
    sEl.style.color = sVal===0x9B?'var(--danger)':(sVal===0x91?'var(--success)':'#fff');

    document.getElementById('temp-int').innerText = getVal(48).toFixed(1)+'°';
    document.getElementById('temp-ext1').innerText = getVal(42).toFixed(1)+'°';
    document.getElementById('temp-ext2').innerText = getVal(43).toFixed(1)+'°';

    if(chartSoc) chartSoc.setOption({ series: [{ data: [{ value: getVal(46).toFixed(1), name:'SOC' }] }] });
    if(chartSoh) chartSoh.setOption({ series: [{ data: [{ value: getVal(45).toFixed(1), name:'SOH' }] }] });

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
    const gauge = (col, name) => ({
        series: [{ type: 'gauge', radius:'100%', center:['50%','70%'], startAngle:180, endAngle:0, min:0, max:100, splitNumber:5, axisLine:{lineStyle:{width:8,color:[[1,'#333']]}}, progress:{show:true,width:8,itemStyle:{color:col}}, pointer:{show:false}, axisLabel:{show:false}, axisTick:{show:false}, splitLine:{show:false}, detail:{valueAnimation:true,offsetCenter:[0,'-20%'],fontSize:24,formatter:'{value}%',color:'#fff'}, data:[{value:0,name}] }]
    });
    chartSoc = echarts.init(document.getElementById('chart-soc'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartSoc.setOption(gauge('#6366f1','SOC'));
    chartSoh = echarts.init(document.getElementById('chart-soh'), 'dark', {renderer:'canvas', backgroundColor:'transparent'});
    chartSoh.setOption(gauge('#10b981','SOH'));
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
