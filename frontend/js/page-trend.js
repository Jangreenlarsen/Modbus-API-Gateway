// Trend-side: enkelt-register tidsserie + multi-register sammenligning

let trendChart = null, multiChart = null;
let trendTimer = null, multiTimer = null;
const trendData = { labels: [], values: [] };

const CHART_COLORS = ['#4f8ef7','#27c96e','#f5a623','#f05252','#7c5cfc','#00d4ff','#ff6b6b','#51cf66'];

function makeTrendChart() {
  const ctx = document.getElementById('trend-chart').getContext('2d');
  if (trendChart) trendChart.destroy();
  trendChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels:   trendData.labels,
      datasets: [{
        label:       'Register',
        data:        trendData.values,
        borderColor: CHART_COLORS[0],
        backgroundColor: CHART_COLORS[0] + '22',
        borderWidth: 2,
        pointRadius: 2,
        fill: true,
        tension: 0.3,
      }]
    },
    options: {
      responsive: true, maintainAspectRatio: false, animation: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { ticks: { color: '#8892a4', maxTicksLimit: 10 }, grid: { color: '#2e3350' } },
        y: { ticks: { color: '#8892a4' }, grid: { color: '#2e3350' } },
      }
    }
  });
}

function updateTrendStats() {
  const v = trendData.values;
  if (!v.length) return;
  const cur = v[v.length - 1];
  const min = Math.min(...v), max = Math.max(...v);
  const avg = (v.reduce((a, b) => a + b, 0) / v.length).toFixed(2);
  document.getElementById('tr-current').innerHTML = `Aktuel: <b>${cur}</b>`;
  document.getElementById('tr-min').innerHTML     = `Min: <b>${min}</b>`;
  document.getElementById('tr-max').innerHTML     = `Max: <b>${max}</b>`;
  document.getElementById('tr-avg').innerHTML     = `Avg: <b>${avg}</b>`;
}

async function trendTick() {
  const iface    = document.getElementById('tr-iface').value;
  const slave    = parseInt(document.getElementById('tr-slave').value);
  const reg      = parseInt(document.getElementById('tr-reg').value);
  const maxPts   = parseInt(document.getElementById('tr-points').value);

  try {
    const data = await API.readHolding(iface, slave, reg, 1);
    const val  = data.registers?.[0] ?? 0;
    const now  = new Date().toLocaleTimeString('da-DK');

    trendData.labels.push(now);
    trendData.values.push(val);
    if (trendData.labels.length > maxPts) {
      trendData.labels.shift();
      trendData.values.shift();
    }

    if (!trendChart) makeTrendChart();
    trendChart.update('none');
    updateTrendStats();
  } catch (e) {
    console.warn('Trend fejl:', e.message);
  }
}

function toggleTrend() {
  const btn = document.getElementById('tr-toggle');
  if (trendTimer) {
    clearInterval(trendTimer);
    trendTimer = null;
    btn.textContent = 'Start';
    btn.classList.remove('btn-danger');
    btn.classList.add('btn-primary');
  } else {
    const interval = parseInt(document.getElementById('tr-interval').value) * 1000;
    if (!trendChart) makeTrendChart();
    trendTick();
    trendTimer = setInterval(trendTick, interval);
    btn.textContent = 'Stop';
    btn.classList.remove('btn-primary');
    btn.classList.add('btn-danger');
  }
}

function clearTrend() {
  trendData.labels = []; trendData.values = [];
  if (trendChart) trendChart.update();
  document.getElementById('tr-current').innerHTML = 'Aktuel: —';
  document.getElementById('tr-min').innerHTML     = 'Min: —';
  document.getElementById('tr-max').innerHTML     = 'Max: —';
  document.getElementById('tr-avg').innerHTML     = 'Avg: —';
}

// ── Multi-register chart ──────────────────────────────────────────────────────

let multiData = {};  // { reg_addr: { labels: [], values: [] } }

function makeMultiChart(registerAddrs) {
  const ctx = document.getElementById('multi-chart').getContext('2d');
  if (multiChart) multiChart.destroy();
  multiChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: registerAddrs.map((addr, i) => ({
        label: `Reg ${addr}`,
        data: [],
        borderColor: CHART_COLORS[i % CHART_COLORS.length],
        backgroundColor: 'transparent',
        borderWidth: 2,
        pointRadius: 2,
        tension: 0.3,
      }))
    },
    options: {
      responsive: true, maintainAspectRatio: false, animation: false,
      plugins: {
        legend: { display: true, labels: { color: '#e2e8f0', boxWidth: 12 } }
      },
      scales: {
        x: { ticks: { color: '#8892a4', maxTicksLimit: 10 }, grid: { color: '#2e3350' } },
        y: { ticks: { color: '#8892a4' }, grid: { color: '#2e3350' } },
      }
    }
  });
}

async function multiTick() {
  const iface  = document.getElementById('mc-iface').value;
  const slave  = parseInt(document.getElementById('mc-slave').value);
  const start  = parseInt(document.getElementById('mc-start').value);
  const count  = parseInt(document.getElementById('mc-count').value);

  try {
    const data = await API.readHolding(iface, slave, start, count);
    const vals = data.registers || [];
    const now  = new Date().toLocaleTimeString('da-DK');
    const MAX  = 60;

    if (!multiChart || multiChart.data.datasets.length !== count) {
      const addrs = Array.from({ length: count }, (_, i) => start + i);
      makeMultiChart(addrs);
    }

    multiChart.data.labels.push(now);
    if (multiChart.data.labels.length > MAX) multiChart.data.labels.shift();

    vals.forEach((v, i) => {
      const ds = multiChart.data.datasets[i];
      if (ds) {
        ds.data.push(v);
        if (ds.data.length > MAX) ds.data.shift();
      }
    });
    multiChart.update('none');
  } catch (e) {
    console.warn('Multi-chart fejl:', e.message);
  }
}

function toggleMultiChart() {
  const btn = document.getElementById('mc-toggle');
  if (multiTimer) {
    clearInterval(multiTimer);
    multiTimer = null;
    btn.textContent = 'Start';
    btn.classList.replace('btn-danger', 'btn-primary');
  } else {
    const interval = parseInt(document.getElementById('mc-interval').value) * 1000;
    multiTick();
    multiTimer = setInterval(multiTick, interval);
    btn.textContent = 'Stop';
    btn.classList.replace('btn-primary', 'btn-danger');
  }
}
