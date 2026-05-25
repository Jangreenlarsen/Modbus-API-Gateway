// Log-side: system log viewer + OTA

let logRefreshTimer = null;
const logBuffer = [];

function initLogPage() {
  setupLogRefresh();
  addLogEntry('I', 'log', 'Log viewer klar');
}

function setupLogRefresh() {
  if (logRefreshTimer) clearInterval(logRefreshTimer);
  const interval = parseInt(document.getElementById('log-refresh').value);
  if (interval > 0) logRefreshTimer = setInterval(pollLog, interval);
}

document.getElementById('log-refresh').addEventListener('change', setupLogRefresh);
document.getElementById('log-filter').addEventListener('change', renderLog);

// ESP32 sender log via WebSocket eller vi poller /api/v1/system/log
// For nu simulerer vi ved at tilføje entries manuelt fra API-kald
async function pollLog() {
  try {
    const sys = await API.system();
    addLogEntry('I', 'system', `Uptime ${sys.uptime_s}s  Heap ${(sys.free_heap/1024).toFixed(1)}KB  IP ${sys.ip}`);
  } catch (e) {
    addLogEntry('E', 'system', 'API fejl: ' + e.message);
  }
}

function addLogEntry(level, tag, msg) {
  const now = new Date().toLocaleTimeString('da-DK', { hour12: false });
  logBuffer.push({ time: now, level, tag, msg });
  if (logBuffer.length > 500) logBuffer.shift();
  renderLog();
}

function renderLog() {
  const filter  = document.getElementById('log-filter').value;
  const entries = filter ? logBuffer.filter(e => e.level === filter[0]) : logBuffer;
  const container = document.getElementById('log-entries');

  container.innerHTML = entries.slice(-200).map(e => `
    <div class="log-entry">
      <span class="log-time">${e.time}</span>
      <span class="log-level-${e.level}">[${e.level}]</span>
      <span class="log-msg"><b>${e.tag}:</b> ${e.msg}</span>
    </div>`).join('');

  container.scrollTop = container.scrollHeight;
}

function clearLogView() {
  logBuffer.length = 0;
  document.getElementById('log-entries').innerHTML = '';
}

// ── OTA ───────────────────────────────────────────────────────────────────────

let otaPolling = null;

async function checkOTA() {
  const res = document.getElementById('ota-result');
  const tbl = document.getElementById('ota-table');
  res.classList.add('hidden');
  addLogEntry('I', 'ota', 'Tjekker GitHub for opdatering…');

  try {
    const info = await API.otaCheck();
    const rows = [
      ['Nuværende version', info.current_version],
      ['Seneste version',   info.latest_version],
      ['Firmware opdatering', info.firmware_available ? '✅ Tilgængelig' : '✔ Opdateret'],
      ['Frontend opdatering', info.frontend_available ? '✅ Tilgængelig' : '✔ Opdateret'],
    ];
    if (info.release_notes)
      rows.push(['Release notes', info.release_notes.slice(0, 120) + (info.release_notes.length > 120 ? '…' : '')]);

    tbl.innerHTML = rows.map(([k,v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('');

    const btns = document.getElementById('ota-buttons');
    btns.innerHTML = '';
    if (info.firmware_available) {
      const b = document.createElement('button');
      b.className = 'btn-warn'; b.textContent = '⬆ Opdater firmware';
      b.onclick = () => startOTA('firmware');
      btns.appendChild(b);
    }
    if (info.frontend_available) {
      const b = document.createElement('button');
      b.className = 'btn-secondary'; b.textContent = '⬆ Opdater frontend';
      b.style.marginLeft = '8px';
      b.onclick = () => startOTA('frontend');
      btns.appendChild(b);
    }

    res.classList.remove('hidden');
    addLogEntry('I', 'ota', `Nuværende: ${info.current_version}  Seneste: ${info.latest_version}`);
  } catch (e) {
    addLogEntry('E', 'ota', 'OTA check fejl: ' + e.message);
    showToast('OTA check fejlede: ' + e.message, 'error');
  }
}

async function startOTA(target) {
  const prog = document.getElementById('ota-progress');
  const bar  = document.getElementById('ota-bar');
  const txt  = document.getElementById('ota-status-text');
  prog.classList.remove('hidden');

  addLogEntry('I', 'ota', `Starter ${target} OTA…`);

  try {
    if (target === 'firmware') await API.otaFirmware();
    else                       await API.otaFrontend();
  } catch (e) {
    // Firmware OTA genstarter ESP32 — connection reset er forventet
    if (target === 'firmware') {
      txt.textContent = 'Firmware flashet — gateway genstarter…';
      bar.style.width = '100%';
      addLogEntry('I', 'ota', 'Firmware OTA startet — gateway genstarter om ca. 10 sek');
      return;
    }
    addLogEntry('E', 'ota', 'OTA fejl: ' + e.message);
    return;
  }

  // Poll status
  if (otaPolling) clearInterval(otaPolling);
  otaPolling = setInterval(async () => {
    try {
      const st = await API.otaStatus();
      bar.style.width = st.progress_pct + '%';
      txt.textContent = `${st.state}  ${st.progress_pct}%`;

      if (st.state === 'done' || st.state === 'error') {
        clearInterval(otaPolling);
        addLogEntry(st.state === 'done' ? 'I' : 'E', 'ota',
          st.state === 'done' ? `${target} OTA fuldført` : 'OTA fejl: ' + st.error);
        if (st.state === 'done') showToast(`${target} opdateret!`, 'success');
      }
    } catch (_) {}
  }, 1000);
}
