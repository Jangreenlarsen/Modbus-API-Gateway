// Status-side: system info, WiFi, interface-status og direkte register-læsning

async function initStatusPage() {
  await Promise.all([refreshSystemCard(), refreshWifiCard(), refreshIfaceStatus()]);
}

async function refreshSystemCard() {
  try {
    const sys = await API.system();
    const h = Math.floor(sys.uptime_s / 3600);
    const m = Math.floor((sys.uptime_s % 3600) / 60);
    const rows = [
      ['Version',    'v' + sys.version],
      ['IP (Eth)',   sys.ip],
      ['Uptime',     `${h}t ${m}m`],
      ['Fri heap',   (sys.free_heap / 1024).toFixed(1) + ' KB'],
      ['Reset årsag',sys.reset_reason],
    ];
    document.getElementById('system-table').innerHTML =
      rows.map(([k,v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('');
  } catch (e) {
    document.getElementById('system-table').innerHTML =
      `<tr><td colspan="2" style="color:var(--error)">Fejl: ${e.message}</td></tr>`;
  }
}

async function refreshWifiCard() {
  try {
    const w = await API.wifiStatus();
    const stateLabel = { connected:'Forbundet', ap_mode:'AP Hotspot',
                          connecting:'Forbinder…', disabled:'Deaktiveret', error:'Fejl' };
    const rows = [
      ['Status', stateLabel[w.state] || w.state],
      ['SSID',   w.ssid || '—'],
      ['IP',     w.ip   || '—'],
      ['Signal', w.rssi ? w.rssi + ' dBm' : '—'],
    ];
    document.getElementById('wifi-table').innerHTML =
      rows.map(([k,v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join('');
  } catch (_) {
    document.getElementById('wifi-table').innerHTML =
      '<tr><td colspan="2" style="color:var(--text-muted)">WiFi ikke tilgængeligt</td></tr>';
  }
}

async function refreshIfaceStatus() {
  const ifaces = getIfaces();
  const list   = document.getElementById('iface-status-list');
  if (!ifaces.length) {
    list.innerHTML = '<div class="card" style="color:var(--text-muted)">Ingen interfaces konfigureret</div>';
    return;
  }
  list.innerHTML = ifaces.map(i => `
    <div class="iface-card">
      <div class="iface-header">
        <span class="iface-title">Interface ${i.id} — ${i.type}
          <span class="badge ${i.uart_mode === 'SW' ? 'badge-blue' : 'badge-gray'}" style="margin-left:6px">
            ${i.uart_mode === 'SW' ? 'SW-UART' : 'HW-UART'}
          </span>
        </span>
        <span class="badge ${i.enabled ? 'badge-ok' : 'badge-error'}">
          ${i.enabled ? 'Aktiv' : 'Deaktiveret'}
        </span>
      </div>
      <table class="info-table">
        <tr><td>Baudrate</td><td>${i.baudrate} bps</td></tr>
        <tr><td>Paritet</td><td>${['Ingen','Ulige','Lige'][i.parity] || i.parity}</td></tr>
        <tr><td>Timeout</td><td>${i.timeout_ms} ms</td></tr>
        <tr><td>Pins</td><td>TX:${i.tx_pin} RX:${i.rx_pin} DE:${i.rts_pin >= 0 ? i.rts_pin : '—'}</td></tr>
      </table>
    </div>`).join('');
}

async function readRegisters() {
  const iface = document.getElementById('rd-iface').value;
  const slave = document.getElementById('rd-slave').value;
  const type  = document.getElementById('rd-type').value;
  const start = parseInt(document.getElementById('rd-start').value);
  const count = parseInt(document.getElementById('rd-count').value);
  const res   = document.getElementById('rd-result');

  res.classList.add('hidden');
  try {
    const data = await API['read' + {
      'holding-registers': 'Holding',
      'input-registers':   'Input',
      'coils':             'Coils',
      'discrete-inputs':   'Discrete'
    }[type]](iface, slave, start, count);

    const vals = data.registers || data.coils || data.inputs || [];
    const isWord = type.includes('register');

    let html = `<b>FC0${data.function} svar — ${vals.length} ${isWord ? 'registers' : 'bits'}</b>\n\n`;
    html += '<table class="reg-table"><tr><th>Adresse</th><th>Dec</th>';
    if (isWord) html += '<th>Hex</th><th>Bin</th>';
    html += '</tr>';

    vals.forEach((v, i) => {
      html += `<tr><td>${start + i}</td><td>${v}</td>`;
      if (isWord) html += `<td>0x${v.toString(16).toUpperCase().padStart(4,'0')}</td><td>${v.toString(2).padStart(16,'0')}</td>`;
      html += '</tr>';
    });
    html += '</table>';
    res.innerHTML = html;
    res.classList.remove('hidden');
  } catch (e) {
    res.textContent = 'Fejl: ' + (e.data?.error || e.message);
    res.classList.remove('hidden');
  }
}
