const elements = {
  portSelect: document.getElementById('serial-port'),
  baudRate: document.getElementById('baud-rate'),
  refreshPorts: document.getElementById('refresh-ports'),
  connect: document.getElementById('connect-port'),
  disconnect: document.getElementById('disconnect-port'),
  badge: document.getElementById('connection-badge'),
  details: document.getElementById('connection-details'),
  error: document.getElementById('connection-error'),
  configLoading: document.getElementById('config-loading-message'),
  dashboardCard: document.getElementById('dashboard-card'),
  registersCard: document.getElementById('registers-card'),
  registersTableBody: document.querySelector('#registers-table tbody'),
  refreshRegisters: document.getElementById('refresh-registers'),
  refreshDashboard: document.getElementById('refresh-dashboard'),
  resetBms: document.getElementById('reset-bms'),
  groupFilter: document.getElementById('group-filter'),
  feedback: document.getElementById('registers-feedback'),
  packVoltage: document.getElementById('pack-voltage'),
  packCurrent: document.getElementById('pack-current'),
  soc: document.getElementById('soc'),
  temperature: document.getElementById('temperature'),
  cellsChart: document.getElementById('cells-chart'),
};

const state = {
  registers: [],
  connected: false,
  currentGroup: '__all__',
  cellsChart: null,
  dashboardInterval: null,
};

function isWritable(access) {
  const normalized = String(access || '').toLowerCase();
  if (!normalized) {
    return false;
  }
  return normalized.includes('w');
}

async function fetchJSON(url, options = {}) {
  const response = await fetch(url, options);
  let payload = null;
  try {
    payload = await response.json();
  } catch (error) {
    payload = null;
  }
  if (!response.ok) {
    const message = payload?.error || response.statusText;
    throw new Error(message);
  }
  return payload;
}

function setButtonsState({ connected }) {
  elements.connect.disabled = connected;
  elements.disconnect.disabled = !connected;
  elements.portSelect.disabled = connected;
  elements.baudRate.disabled = connected;
  elements.refreshRegisters.disabled = !connected;
}

function setStatus({ connected, port }) {
  state.connected = Boolean(connected);
  if (connected) {
    elements.badge.className = 'badge bg-success';
    elements.badge.textContent = 'Connecté';
    elements.details.textContent = `${port.path} • ${port.baudRate} bauds`;
    startDashboardRefresh();
  } else {
    elements.badge.className = 'badge bg-secondary';
    elements.badge.textContent = 'Déconnecté';
    elements.details.textContent = 'Aucun périphérique connecté.';
    setFeedback('');
    state.currentGroup = '__all__';
    elements.groupFilter.value = '__all__';
    elements.configLoading.hidden = true;
    stopDashboardRefresh();
  }
  setButtonsState({ connected });
  elements.dashboardCard.hidden = !connected;
  elements.registersCard.hidden = !connected;
}

function showError(message) {
  elements.error.textContent = message;
  elements.error.hidden = !message;
}

function setLoadingRegisters(isLoading) {
  elements.configLoading.hidden = !isLoading;
  elements.refreshRegisters.disabled = isLoading || !state.connected;
  elements.registersTableBody.classList.toggle('opacity-50', isLoading);
}

function setFeedback(message, type = 'info') {
  if (!message) {
    elements.feedback.hidden = true;
    return;
  }
  elements.feedback.hidden = false;
  elements.feedback.className = `alert alert-${type}`;
  elements.feedback.textContent = message;
}

function renderGroupFilter(registers) {
  const uniqueGroups = Array.from(
    new Set(registers.map((reg) => reg.group).filter((group) => group))
  ).sort((a, b) => a.localeCompare(b));
  const current = elements.groupFilter.value || state.currentGroup;
  elements.groupFilter.innerHTML = '';
  const allOption = document.createElement('option');
  allOption.value = '__all__';
  allOption.textContent = 'Tous les groupes';
  elements.groupFilter.appendChild(allOption);
  uniqueGroups.forEach((group) => {
    const option = document.createElement('option');
    option.value = group;
    option.textContent = group;
    elements.groupFilter.appendChild(option);
  });
  if (uniqueGroups.includes(current)) {
    elements.groupFilter.value = current;
    state.currentGroup = current;
  } else {
    elements.groupFilter.value = '__all__';
    state.currentGroup = '__all__';
  }
}

function createEnumField(register) {
  const select = document.createElement('select');
  select.className = 'form-select form-select-sm';
  register.enum.forEach((entry) => {
    const option = document.createElement('option');
    option.value = entry.value;
    option.textContent = `${entry.value} — ${entry.label}`;
    if (String(entry.value) === String(register.current_user_value)) {
      option.selected = true;
    }
    select.appendChild(option);
  });
  select.dataset.key = register.key;
  return select;
}

function createNumberField(register) {
  const input = document.createElement('input');
  input.type = 'number';
  input.className = 'form-control form-control-sm';
  input.value = register.current_user_value ?? '';
  if (typeof register.min === 'number') {
    input.min = register.min;
  }
  if (typeof register.max === 'number') {
    input.max = register.max;
  }
  if (typeof register.step === 'number' && !Number.isNaN(register.step)) {
    input.step = register.step;
  } else {
    input.step = 'any';
  }
  input.dataset.key = register.key;
  return input;
}

function renderRegistersTable() {
  const tbody = elements.registersTableBody;
  tbody.innerHTML = '';
  const group = state.currentGroup;
  const filtered =
    group === '__all__'
      ? state.registers
      : state.registers.filter((register) => register.group === group);

  if (filtered.length === 0) {
    const emptyRow = document.createElement('tr');
    const cell = document.createElement('td');
    cell.colSpan = 7;
    cell.className = 'text-center text-secondary';
    cell.textContent = 'Aucun registre à afficher pour ce groupe.';
    emptyRow.appendChild(cell);
    tbody.appendChild(emptyRow);
    return;
  }

  filtered.forEach((register) => {
    const row = document.createElement('tr');
    row.dataset.key = register.key;

    const addressCell = document.createElement('td');
    addressCell.textContent = register.address_hex || register.address;
    row.appendChild(addressCell);

    const labelCell = document.createElement('td');
    labelCell.innerHTML = `<div class="fw-semibold">${register.label}</div><div class="text-secondary small">${register.key}</div>`;
    row.appendChild(labelCell);

    const valueCell = document.createElement('td');
    valueCell.className = 'value-cell';
    let field;
    if (Array.isArray(register.enum) && register.enum.length > 0) {
      field = createEnumField(register);
    } else {
      field = createNumberField(register);
    }
    const writable = isWritable(register.access);
    field.disabled = !writable;
    field.dataset.address = register.address;
    field.dataset.unit = register.unit || '';
    valueCell.appendChild(field);
    row.appendChild(valueCell);

    const unitCell = document.createElement('td');
    unitCell.textContent = register.unit || '—';
    row.appendChild(unitCell);

    const accessCell = document.createElement('td');
    accessCell.textContent = register.access ? String(register.access).toUpperCase() : '—';
    row.appendChild(accessCell);

    const descriptionCell = document.createElement('td');
    descriptionCell.className = 'description';
    descriptionCell.textContent = register.comment || '';
    row.appendChild(descriptionCell);

    const actionsCell = document.createElement('td');
    actionsCell.className = 'text-end';
    if (!isWritable(register.access)) {
      actionsCell.innerHTML = '<span class="text-secondary">Lecture seule</span>';
    } else {
      const button = document.createElement('button');
      button.className = 'btn btn-primary btn-sm apply-register';
      button.textContent = 'Appliquer';
      button.dataset.key = register.key;
      actionsCell.appendChild(button);
    }
    row.appendChild(actionsCell);

    tbody.appendChild(row);
  });
}

function updateRegisterInState(updated) {
  const index = state.registers.findIndex((register) => register.key === updated.key);
  if (index !== -1) {
    state.registers[index] = {
      ...state.registers[index],
      ...updated,
      current_user_value: updated.value,
    };
  }
}

async function loadPorts() {
  try {
    showError('');
    elements.refreshPorts.disabled = true;
    const data = await fetchJSON('/api/ports');
    const { ports } = data;
    elements.portSelect.innerHTML = '';
    if (!ports || ports.length === 0) {
      const option = document.createElement('option');
      option.value = '';
      option.textContent = 'Aucun port détecté';
      elements.portSelect.appendChild(option);
      elements.portSelect.disabled = true;
      return;
    }
    ports.forEach((port) => {
      const option = document.createElement('option');
      option.value = port.path;
      option.textContent = port.path + (port.manufacturer ? ` • ${port.manufacturer}` : '');
      elements.portSelect.appendChild(option);
    });
    elements.portSelect.disabled = false;
  } catch (error) {
    showError(error.message);
  } finally {
    elements.refreshPorts.disabled = false;
  }
}

async function queryRegisters({ silent = false } = {}) {
  if (!state.connected) {
    return;
  }
  try {
    if (!silent) {
      setFeedback('Lecture des registres en cours…', 'info');
    } else {
      setFeedback('');
    }
    setLoadingRegisters(true);
    const data = await fetchJSON('/api/registers');
    state.registers = (data.registers || []).slice().sort((a, b) => {
      if (typeof a.address === 'number' && typeof b.address === 'number') {
        return a.address - b.address;
      }
      return String(a.key).localeCompare(String(b.key));
    });
    renderGroupFilter(state.registers);
    renderRegistersTable();
    if (!silent) {
      setFeedback(`Lecture terminée (${state.registers.length} registres).`, 'success');
    }
  } catch (error) {
    setFeedback(`Erreur lors du chargement des registres : ${error.message}`, 'danger');
  } finally {
    setLoadingRegisters(false);
  }
}

async function connectPort() {
  try {
    showError('');
    setFeedback('');
    const path = elements.portSelect.value;
    const baudRate = Number.parseInt(elements.baudRate.value, 10) || 115200;
    if (!path) {
      throw new Error('Veuillez sélectionner un port série.');
    }
    elements.connect.disabled = true;
    await fetchJSON('/api/connection/open', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path, baudRate }),
    });
    setStatus({ connected: true, port: { path, baudRate } });
    await queryRegisters();
  } catch (error) {
    showError(error.message);
    setStatus({ connected: false });
  } finally {
    elements.connect.disabled = false;
  }
}

async function disconnectPort() {
  try {
    showError('');
    setFeedback('');
    elements.disconnect.disabled = true;
    await fetchJSON('/api/connection/close', { method: 'POST' });
    setStatus({ connected: false });
    state.registers = [];
    state.currentGroup = '__all__';
    elements.groupFilter.value = '__all__';
    renderRegistersTable();
  } catch (error) {
    showError(error.message);
  } finally {
    elements.disconnect.disabled = false;
  }
}

async function refreshStatus() {
  try {
    const data = await fetchJSON('/api/connection/status');
    setStatus(data);
    if (data.connected) {
      await queryRegisters({ silent: true });
    } else {
      state.registers = [];
      renderRegistersTable();
    }
  } catch (error) {
    showError(error.message);
  }
}

async function applyRegister(key, value) {
  const numericValue = Number(value);
  if (Number.isNaN(numericValue)) {
    setFeedback('Valeur numérique invalide.', 'danger');
    return;
  }
  try {
    setFeedback(`Écriture du registre ${key}…`, 'info');
    const payload = await fetchJSON('/api/registers', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ key, value: numericValue }),
    });
    updateRegisterInState(payload);
    renderRegistersTable();
    setFeedback(`Registre ${key} mis à jour (valeur actuelle : ${payload.value}).`, 'success');
  } catch (error) {
    setFeedback(`Erreur lors de l’écriture : ${error.message}`, 'danger');
  }
}

function initCellsChart() {
  if (state.cellsChart) {
    state.cellsChart.destroy();
  }

  const ctx = elements.cellsChart.getContext('2d');
  state.cellsChart = new Chart(ctx, {
    type: 'bar',
    data: {
      labels: [],
      datasets: [{
        label: 'Tension (mV)',
        data: [],
        backgroundColor: 'rgba(59, 130, 246, 0.7)',
        borderColor: 'rgba(59, 130, 246, 1)',
        borderWidth: 1,
      }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: true,
      plugins: {
        legend: {
          display: false,
        },
        tooltip: {
          callbacks: {
            label: (context) => `${context.parsed.y} mV`,
          },
        },
      },
      scales: {
        y: {
          beginAtZero: false,
          ticks: {
            color: '#94a3b8',
          },
          grid: {
            color: 'rgba(148, 163, 184, 0.1)',
          },
        },
        x: {
          ticks: {
            color: '#94a3b8',
          },
          grid: {
            color: 'rgba(148, 163, 184, 0.1)',
          },
        },
      },
    },
  });
}

function updateCellsChart(cellVoltages) {
  if (!state.cellsChart) {
    initCellsChart();
  }

  const labels = cellVoltages.map((_, index) => `C${index + 1}`);
  state.cellsChart.data.labels = labels;
  state.cellsChart.data.datasets[0].data = cellVoltages;
  state.cellsChart.update();
}

async function refreshDashboard() {
  if (!state.connected) {
    return;
  }

  try {
    const data = await fetchJSON('/api/monitoring/live');

    if (data.packVoltage !== null) {
      elements.packVoltage.textContent = `${(data.packVoltage / 1000).toFixed(2)} V`;
    } else {
      elements.packVoltage.textContent = '-- V';
    }

    if (data.packCurrent !== null) {
      const current = data.packCurrent / 10;
      elements.packCurrent.textContent = `${current.toFixed(1)} A`;
    } else {
      elements.packCurrent.textContent = '-- A';
    }

    if (data.estimatedSoc !== null) {
      elements.soc.textContent = `${(data.estimatedSoc / 10).toFixed(1)} %`;
    } else {
      elements.soc.textContent = '-- %';
    }

    if (data.temperatures && data.temperatures.length > 0) {
      const avgTemp = data.temperatures.reduce((sum, t) => sum + t, 0) / data.temperatures.length;
      elements.temperature.textContent = `${(avgTemp / 10).toFixed(1)} °C`;
    } else {
      elements.temperature.textContent = '-- °C';
    }

    if (data.cellVoltages && data.cellVoltages.length > 0) {
      updateCellsChart(data.cellVoltages);
    }
  } catch (error) {
    console.error('Erreur lors du rafraîchissement du dashboard:', error);
  }
}

function startDashboardRefresh() {
  stopDashboardRefresh();
  refreshDashboard();
  state.dashboardInterval = setInterval(refreshDashboard, 2000);
}

function stopDashboardRefresh() {
  if (state.dashboardInterval) {
    clearInterval(state.dashboardInterval);
    state.dashboardInterval = null;
  }
}

async function resetBms() {
  if (!confirm('Êtes-vous sûr de vouloir redémarrer le TinyBMS ?')) {
    return;
  }

  try {
    elements.resetBms.disabled = true;
    await fetchJSON('/api/system/restart', { method: 'POST' });
    setFeedback('Le TinyBMS redémarre...', 'success');
    setTimeout(() => {
      refreshDashboard();
    }, 3000);
  } catch (error) {
    setFeedback(`Erreur lors du redémarrage : ${error.message}`, 'danger');
  } finally {
    elements.resetBms.disabled = false;
  }
}

elements.refreshPorts.addEventListener('click', (event) => {
  event.preventDefault();
  loadPorts();
});

elements.connect.addEventListener('click', (event) => {
  event.preventDefault();
  connectPort();
});

elements.disconnect.addEventListener('click', (event) => {
  event.preventDefault();
  disconnectPort();
});

elements.refreshRegisters.addEventListener('click', (event) => {
  event.preventDefault();
  queryRegisters();
});

elements.groupFilter.addEventListener('change', (event) => {
  state.currentGroup = event.target.value;
  renderRegistersTable();
});

elements.refreshDashboard.addEventListener('click', (event) => {
  event.preventDefault();
  refreshDashboard();
});

elements.resetBms.addEventListener('click', (event) => {
  event.preventDefault();
  resetBms();
});

elements.registersTableBody.addEventListener('click', (event) => {
  const target = event.target;
  if (target.classList.contains('apply-register')) {
    event.preventDefault();
    const key = target.dataset.key;
    const row = target.closest('tr');
    const field = row.querySelector('[data-key]');
    if (!key || !field) {
      setFeedback("Impossible de trouver le champ du registre.", 'danger');
      return;
    }
    applyRegister(key, field.value);
  }
});

window.addEventListener('DOMContentLoaded', async () => {
  initCellsChart();
  await loadPorts();
  await refreshStatus();
});

window.addEventListener('beforeunload', () => {
  stopDashboardRefresh();
});
