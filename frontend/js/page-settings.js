// Settings-side: Ethernet, WiFi, Modbus interfaces, system

async function initSettingsPage() {
  await Promise.all([loadEthernetSettings(), loadWifiSettings(), renderIfaceSettings()]);
}

// ── Ethernet ─────────────────────────────────────────────────────────────────

async function loadEthernetSettings() {
  try {
    const sys = await API.system();
    // Ethernet-config hentes fra /api/v1/interfaces endpoint — læs bare fra system
    // For nu viser vi hvad vi har
    document.getElementById('eth-ip').value   = sys.ip || 'dhcp';
  } catch (_) {}
}

async function saveEthernet() {
  const ip   = document.getElementById('eth-ip').value.trim();
  const gw   = document.getElementById('eth-gw').value.trim();
  const mask = document.getElementById('eth-mask').value.trim();
  try {
    await fetch('/api/v1/system/ethernet', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ip, gw, netmask: mask }),
    });
    showToast('Ethernet gemt — genstart for at anvende', 'success');
  } catch (e) {
    showToast('Fejl: ' + e.message, 'error');
  }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────

async function loadWifiSettings() {
  try {
    const st = await API.wifiStatus();
    document.getElementById('wifi-enabled').checked = st.state !== 'disabled';
    document.getElementById('wifi-ssid').value = st.ssid || '';
    toggleWifiFields();
  } catch (_) {}
}

function toggleWifiFields() {
  const enabled = document.getElementById('wifi-enabled').checked;
  document.getElementById('wifi-fields').classList.toggle('hidden', !enabled);
}

async function scanWifi() {
  const container = document.getElementById('wifi-scan-results');
  container.innerHTML = '<div class="scan-item" style="color:var(--text-muted)">Scanner…</div>';
  container.classList.remove('hidden');
  try {
    const networks = await API.wifiScan();
    if (!networks.length) {
      container.innerHTML = '<div class="scan-item" style="color:var(--text-muted)">Ingen netværk fundet</div>';
      return;
    }
    container.innerHTML = networks
      .sort((a, b) => b.rssi - a.rssi)
      .map(n => `
        <div class="scan-item" onclick="selectNetwork('${n.ssid.replace(/'/g,"\\'")}')">
          <span>${n.ssid} ${n.open ? '<span style="color:var(--text-muted);font-size:.75rem">(åben)</span>' : ''}</span>
          <span class="scan-rssi">${n.rssi} dBm  CH${n.channel}</span>
        </div>`).join('');
  } catch (e) {
    container.innerHTML = `<div class="scan-item" style="color:var(--error)">Fejl: ${e.message}</div>`;
  }
}

function selectNetwork(ssid) {
  document.getElementById('wifi-ssid').value = ssid;
  document.getElementById('wifi-scan-results').classList.add('hidden');
  document.getElementById('wifi-password').focus();
}

async function saveWifi() {
  const cfg = {
    enabled:     document.getElementById('wifi-enabled').checked,
    ssid:        document.getElementById('wifi-ssid').value.trim(),
    password:    document.getElementById('wifi-password').value,
    ip:          document.getElementById('wifi-ip').value.trim() || 'dhcp',
    ap_fallback: document.getElementById('wifi-ap-fallback').checked,
    ap_ssid:     document.getElementById('wifi-ap-ssid').value.trim(),
    ap_password: document.getElementById('wifi-ap-password').value,
  };
  try {
    await API.wifiSave(cfg);
    showToast('WiFi konfiguration gemt og genanvendt', 'success');
    addLogEntry('I', 'wifi', `WiFi konfigureret: ${cfg.ssid}`);
  } catch (e) {
    showToast('Fejl: ' + e.message, 'error');
  }
}

// ── Modbus Interfaces ─────────────────────────────────────────────────────────

async function renderIfaceSettings() {
  const ifaces = getIfaces();
  const list   = document.getElementById('iface-settings-list');

  list.innerHTML = ifaces.map(iface => `
    <div class="iface-settings-card" id="iface-card-${iface.id}">
      <div class="iface-settings-header" onclick="toggleIfaceCard(${iface.id})">
        <span>
          <span class="badge ${iface.enabled ? 'badge-ok' : 'badge-error'}" style="margin-right:8px">
            ${iface.enabled ? 'Aktiv' : 'Slukket'}
          </span>
          Interface ${iface.id} — ${iface.type}
          <span class="badge badge-blue" style="margin-left:6px">${iface.uart_mode}</span>
        </span>
        <span style="color:var(--text-muted)">▾</span>
      </div>
      <div class="iface-settings-body" id="iface-body-${iface.id}">
        <div class="form-grid">
          <label>Type
            <select id="if${iface.id}-type">
              <option value="0" ${iface.type==='RS485'?'selected':''}>RS485</option>
              <option value="1" ${iface.type==='RS232'?'selected':''}>RS232</option>
            </select>
          </label>
          <label>UART Mode
            <select id="if${iface.id}-mode" onchange="updateModeHint(${iface.id})">
              <option value="0" ${iface.uart_mode!=='SW'?'selected':''}>HW UART (≤115200 baud)</option>
              <option value="1" ${iface.uart_mode==='SW'?'selected':''}>SW UART (≤9600 baud)</option>
            </select>
          </label>
          <label id="if${iface.id}-uart-label" ${iface.uart_mode==='SW'?'style="opacity:.4"':''}>
            UART nr. (HW mode)
            <select id="if${iface.id}-uart">
              <option value="1" ${iface.uart_num===1?'selected':''}>UART1</option>
              <option value="2" ${iface.uart_num===2?'selected':''}>UART2</option>
            </select>
          </label>
          <label>Baudrate
            <select id="if${iface.id}-baud">
              ${[1200,2400,4800,9600,19200,38400,57600,115200].map(b =>
                `<option value="${b}" ${iface.baudrate===b?'selected':''}>${b}</option>`
              ).join('')}
            </select>
          </label>
          <label>Paritet
            <select id="if${iface.id}-parity">
              <option value="0" ${iface.parity===0?'selected':''}>Ingen</option>
              <option value="1" ${iface.parity===1?'selected':''}>Ulige</option>
              <option value="2" ${iface.parity===2?'selected':''}>Lige</option>
            </select>
          </label>
          <label>Stop bits
            <select id="if${iface.id}-stop">
              <option value="1" ${iface.stop_bits===1?'selected':''}>1</option>
              <option value="2" ${iface.stop_bits===2?'selected':''}>2</option>
            </select>
          </label>
          <label>Timeout (ms)
            <input type="number" id="if${iface.id}-timeout" value="${iface.timeout_ms}" min="50" max="5000">
          </label>
        </div>
        <h4 style="margin-top:14px">GPIO Pins</h4>
        <div class="form-grid">
          <label>TX Pin<input type="number" id="if${iface.id}-tx" value="${iface.tx_pin}" min="0" max="39"></label>
          <label>RX Pin<input type="number" id="if${iface.id}-rx" value="${iface.rx_pin}" min="0" max="39"></label>
          <label>DE/RE Pin (RS485, -1=ingen)
            <input type="number" id="if${iface.id}-de" value="${iface.rts_pin}" min="-1" max="39">
          </label>
        </div>
        <div class="form-row" style="margin-top:14px">
          <label class="toggle-label">
            <input type="checkbox" id="if${iface.id}-enabled" ${iface.enabled?'checked':''}>
            Aktiveret
          </label>
          <button class="btn-primary" onclick="saveInterface(${iface.id})">Gem</button>
          <button class="btn-danger"  onclick="removeInterface(${iface.id})" style="margin-left:auto">Fjern</button>
        </div>
      </div>
    </div>`).join('');
}

function toggleIfaceCard(id) {
  const body = document.getElementById(`iface-body-${id}`);
  body.classList.toggle('open');
}

function updateModeHint(id) {
  const isSW = document.getElementById(`if${id}-mode`).value === '1';
  const label = document.getElementById(`if${id}-uart-label`);
  if (label) label.style.opacity = isSW ? '0.4' : '1';
  if (isSW) {
    const baudSel = document.getElementById(`if${id}-baud`);
    // Begræns baudrate til max 9600 i SW mode
    Array.from(baudSel.options).forEach(opt => {
      opt.disabled = parseInt(opt.value) > 9600;
    });
    if (parseInt(baudSel.value) > 9600) baudSel.value = '9600';
  } else {
    const baudSel = document.getElementById(`if${id}-baud`);
    Array.from(baudSel.options).forEach(opt => { opt.disabled = false; });
  }
}

async function saveInterface(id) {
  const cfg = {
    type:       parseInt(document.getElementById(`if${id}-type`).value),
    uart_mode:  parseInt(document.getElementById(`if${id}-mode`).value),
    uart_num:   parseInt(document.getElementById(`if${id}-uart`).value),
    baudrate:   parseInt(document.getElementById(`if${id}-baud`).value),
    parity:     parseInt(document.getElementById(`if${id}-parity`).value),
    stop_bits:  parseInt(document.getElementById(`if${id}-stop`).value),
    timeout_ms: parseInt(document.getElementById(`if${id}-timeout`).value),
    tx_pin:     parseInt(document.getElementById(`if${id}-tx`).value),
    rx_pin:     parseInt(document.getElementById(`if${id}-rx`).value),
    rts_pin:    parseInt(document.getElementById(`if${id}-de`).value),
    enabled:    document.getElementById(`if${id}-enabled`).checked,
  };
  try {
    await API.saveInterface(id, cfg);
    showToast(`Interface ${id} gemt`, 'success');
    await loadInterfaces();
    populateIfaceSelects();
    renderIfaceSettings();
  } catch (e) {
    showToast('Fejl: ' + e.message, 'error');
  }
}

async function addInterface() {
  const ifaces = getIfaces();
  const newId  = ifaces.length;
  const cfg = {
    type: 0, uart_mode: 1, uart_num: 1, baudrate: 9600,
    parity: 0, stop_bits: 1, timeout_ms: 500,
    tx_pin: 25, rx_pin: 26, rts_pin: 27, enabled: true,
  };
  try {
    await API.saveInterface(newId, cfg);
    showToast(`Interface ${newId} tilføjet`, 'success');
    await loadInterfaces();
    renderIfaceSettings();
  } catch (e) {
    showToast('Fejl: ' + e.message, 'error');
  }
}

async function removeInterface(id) {
  if (!confirm(`Fjern interface ${id}?`)) return;
  try {
    await API.saveInterface(id, { enabled: false });
    showToast(`Interface ${id} deaktiveret`, 'warn');
    await loadInterfaces();
    renderIfaceSettings();
  } catch (e) {
    showToast('Fejl: ' + e.message, 'error');
  }
}

// ── System ────────────────────────────────────────────────────────────────────

async function reboot() {
  if (!confirm('Genstart gatewayen?')) return;
  try {
    await API.reboot();
    showToast('Gateway genstarter…', 'warn');
    addLogEntry('W', 'system', 'Manuel genstart udløst');
  } catch (_) {
    // Connection reset er forventet
    showToast('Gateway genstarter…', 'warn');
  }
}
