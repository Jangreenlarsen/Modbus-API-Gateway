# Release Notes

---

## v0.1.0 build 0005 — 2026-05-25 — WiFi STA/AP support + komplet web frontend

Gatewayen har nu fuldt WiFi-understøttelse og en komplet web-brugergrænseflade tilgængelig direkte fra browseren.

**WiFi:**
- STA-tilstand med automatisk AP-fallback hotspot (`ModbusGW-XXXXXX`) hvis STA ikke kan oprette forbindelse efter 5 forsøg
- Konfigureres og genanvendes via REST API — ingen genstart nødvendig
- WiFi scan returnerer netværksliste med RSSI, kanal og åben/lukket status

**Web frontend (SPIFFS):**
- **Status-side**: System-info (version, uptime, heap, IP), WiFi-status, Modbus interface-kort med HW/SW badge, on-demand register-læser
- **Trend-side**: Enkelt-register trend med Chart.js (min/max/avg-statistik) + multi-register sammenligning
- **Log-side**: Live log-viewer med niveau-filter (I/W/E), OTA check og opdateringstrigger med progress bar
- **Indstillinger-side**: Ethernet, WiFi (scan-knap, netværksvælger, AP-fallback), Modbus interface-cards (baudrate, paritet, GPIO pins, HW/SW mode), tilføj/fjern interface, genstart

**Eksempel — WiFi-konfiguration:**
```bash
# Tjek WiFi-status
curl http://192.168.1.100/api/v1/system/wifi

# Konfigurer og aktivér WiFi
curl -X PUT http://192.168.1.100/api/v1/system/wifi \
  -H 'Content-Type: application/json' \
  -d '{"enabled":true,"ssid":"MyNetwork","password":"secret","ap_fallback":true}'

# Scan efter netværk
curl http://192.168.1.100/api/v1/system/wifi/scan
```

---

## v0.1.0 build 0003 — 2026-05-25 — OTA opdatering fra GitHub releases

Firmware og frontend kan nu opdateres direkte fra GitHub releases over Ethernet — ingen USB-kabel nødvendig.

**Sådan virker det:**
1. Tag en ny GitHub release med `firmware.bin` og `frontend.bin` som assets
2. Kald `GET /api/v1/system/ota/check` — gatewayen sammenligner nuværende version med seneste release
3. Kald `POST /api/v1/system/ota/firmware` for at starte firmware-opdatering (genstarter automatisk)
4. Kald `POST /api/v1/system/ota/frontend` for at opdatere webgrænsefladen på SPIFFS
5. Følg fremdrift via `GET /api/v1/system/ota/status`

**Eksempel:**
```bash
# Tjek om opdatering er tilgængelig
curl http://192.168.1.100/api/v1/system/ota/check

# Start firmware-opdatering
curl -X POST http://192.168.1.100/api/v1/system/ota/firmware

# Følg status
curl http://192.168.1.100/api/v1/system/ota/status
```

---

## v0.1.0 — 2026-05-25 — Projektinitialisering

Projektstruktur og dokumentation oprettet.

- Komplet projektdefinition i `CLAUDE.md` med versioneringsregler, workflow og arkitekturprincipper
- Lagdelt arkitektur defineret: Hardware → Modbus RTU → Storage/Service → API → Frontend
- Modbus RTU protokolreference inkl. alle function codes, register-typer og RS485-specifikation
- ESP32/ESP-IDF reference inkl. UART-konfiguration, NVS, SPIFFS, HTTP-server og WebSocket
- Feature-backlog oprettet med 10 planlagte features

**Næste skridt**: Opret ESP-IDF projekt i `firmware/` og implementer basis Modbus RTU master.
