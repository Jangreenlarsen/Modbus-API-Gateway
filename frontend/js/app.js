// App-initialisering, navigation og header-opdatering

let _ifaces = [];

function showToast(msg, type = 'success') {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = `toast ${type}`;
  t.classList.remove('hidden');
  clearTimeout(t._timer);
  t._timer = setTimeout(() => t.classList.add('hidden'), 3000);
}

function getIfaces() { return _ifaces; }

function populateIfaceSelects() {
  ['rd-iface','tr-iface','mc-iface'].forEach(id => {
    const sel = document.getElementById(id);
    if (!sel) return;
    const cur = sel.value;
    sel.innerHTML = _ifaces.map(i =>
      `<option value="${i.id}">${i.id}: ${i.type} ${i.uart_mode === 'SW' ? '(SW)' : ''} ${i.baudrate}bd</option>`
    ).join('');
    if (cur) sel.value = cur;
  });
}

async function refreshHeader() {
  try {
    const sys = await API.system();
    document.getElementById('hdr-version').textContent = 'v' + sys.version;
    document.getElementById('hdr-ip').textContent = sys.ip;
    const h = Math.floor(sys.uptime_s / 3600);
    const m = Math.floor((sys.uptime_s % 3600) / 60);
    const s = sys.uptime_s % 60;
    document.getElementById('hdr-uptime').textContent =
      `↑ ${h}h ${m}m ${s}s`;
  } catch (_) {}

  try {
    const wifi = await API.wifiStatus();
    const icon = document.getElementById('hdr-wifi');
    const icons = { connected:'📶', ap_mode:'📡', connecting:'⏳', disabled:'📵', error:'❌' };
    icon.textContent = icons[wifi.state] || '📵';
    icon.title = `WiFi: ${wifi.state}  ${wifi.ssid ? wifi.ssid + '  ' : ''}${wifi.rssi ? wifi.rssi + 'dBm' : ''}`;
  } catch (_) {}
}

async function loadInterfaces() {
  try {
    _ifaces = await API.interfaces();
    populateIfaceSelects();
  } catch (_) {}
}

// ── Navigation ────────────────────────────────────────────────────────────────

document.querySelectorAll('.nav-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('page-' + btn.dataset.page).classList.add('active');

    // Trigger page-specifikke init
    if (btn.dataset.page === 'status')   initStatusPage();
    if (btn.dataset.page === 'settings') initSettingsPage();
    if (btn.dataset.page === 'log')      initLogPage();
  });
});

// ── Boot ──────────────────────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', async () => {
  await loadInterfaces();
  refreshHeader();
  setInterval(refreshHeader, 10000);
  initStatusPage();
});
