// Al kommunikation med ESP32 REST API
// Bruges af alle page-*.js moduler

const API = (() => {
  const BASE = '/api/v1';

  async function req(method, path, body) {
    const opts = { method, headers: { 'Content-Type': 'application/json' } };
    if (body !== undefined) opts.body = JSON.stringify(body);
    const r = await fetch(BASE + path, opts);
    if (!r.ok) {
      const err = await r.json().catch(() => ({ error: r.statusText }));
      throw Object.assign(new Error(err.error || r.statusText), { status: r.status, data: err });
    }
    return r.json();
  }

  return {
    // System
    system:        ()      => req('GET',  '/system'),
    reboot:        ()      => req('POST', '/system/reboot'),

    // WiFi
    wifiStatus:    ()      => req('GET',  '/system/wifi'),
    wifiSave:      cfg     => req('PUT',  '/system/wifi', cfg),
    wifiScan:      ()      => req('GET',  '/system/wifi/scan'),

    // OTA
    otaCheck:      ()      => req('GET',  '/system/ota/check'),
    otaFirmware:   ()      => req('POST', '/system/ota/firmware'),
    otaFrontend:   ()      => req('POST', '/system/ota/frontend'),
    otaStatus:     ()      => req('GET',  '/system/ota/status'),

    // Interfaces
    interfaces:    ()      => req('GET',  '/interfaces'),
    interface:     id      => req('GET',  `/interfaces/${id}`),
    saveInterface: (id, d) => req('PUT',  `/interfaces/${id}/config`, d),

    // Modbus reads
    readHolding:   (iface, slave, start, count) =>
      req('GET', `/interfaces/${iface}/slaves/${slave}/holding-registers?start=${start}&count=${count}`),
    readInput:     (iface, slave, start, count) =>
      req('GET', `/interfaces/${iface}/slaves/${slave}/input-registers?start=${start}&count=${count}`),
    readCoils:     (iface, slave, start, count) =>
      req('GET', `/interfaces/${iface}/slaves/${slave}/coils?start=${start}&count=${count}`),
    readDiscrete:  (iface, slave, start, count) =>
      req('GET', `/interfaces/${iface}/slaves/${slave}/discrete-inputs?start=${start}&count=${count}`),

    // Modbus writes
    writeRegister: (iface, slave, reg, value) =>
      req('PUT',  `/interfaces/${iface}/slaves/${slave}/holding-registers/${reg}`, { value }),
    writeCoil:     (iface, slave, addr, value) =>
      req('PUT',  `/interfaces/${iface}/slaves/${slave}/coils/${addr}`, { value }),
  };
})();
