# Release Notes

---

## v0.1.0 build 0013 — 2026-05-27 — Boot uden Ethernet + version i CLI

Gateway booter nu stabilt selv uden Ethernet PHY tilsluttet. Ved manglende PHY logges en advarsel og systemet kører videre på WiFi alene — ingen reboot-loop.

Serial CLI boot-display og `status`-kommando viser nu fuld version: `v0.1.0 b0013`.

---

## v0.1.0 build 0012 — 2026-05-27 — Hurtigere build

Bluetooth, TLS-server og IPv6 er nu deaktiveret i `sdkconfig.defaults`. Build-tid falder fra over 5 minutter til ~4.3 minutter. Flash-footprint reduceret.

---

## v0.1.0 build 0009 — 2026-05-27 — Kompilerer rent med PlatformIO

Alle kompileringsfejl under ESP-IDF v5.5 + PlatformIO er rettet. `pio run` giver nu `[SUCCESS]`.

---

## v0.1.0 build 0008 — 2026-05-27 — PlatformIO support

Projektet bygger nu med PlatformIO IDE i VS Code.

**Kom i gang:**
```bash
# Installer PlatformIO IDE extension i VS Code
# Åbn projektet — PlatformIO genkender automatisk platformio.ini

# Byg
pio run

# Flash firmware
pio run -t upload

# Upload web frontend til SPIFFS
pio run -t uploadfs

# Serial monitor (Ctrl+C for at afslutte)
pio device monitor
```

**Første gang:**
PlatformIO downloader automatisk ESP-IDF toolchain og `espressif/esp-modbus` komponenten. Det tager nogle minutter første gang.

---

## v0.1.0 build 0007 — 2026-05-25 — Fix: WiFi statisk IP

WiFi statisk IP-konfiguration virkede ikke — gatewayen brugte altid DHCP uanset hvad der var konfigureret. Rettet nu.

**WiFi IP-konfiguration:**
- **DHCP** (standard): sæt `ip` til `"dhcp"` eller lad feltet være tomt
- **Statisk IP**: udfyld `ip`, `gw` og `netmask` — DHCP-klienten deaktiveres automatisk

Via CLI:
```bash
# DHCP (standard)
mbgw wifi set --ssid MitNet --password s3cr3t

# Statisk IP
mbgw wifi set --ssid MitNet --password s3cr3t --ip 192.168.1.50
# (gw og netmask konfigureres via web-frontend eller direkte via PUT /api/v1/system/wifi)
```

---

## v0.1.0 build 0006 — 2026-05-25 — CLI-værktøj (mbgw)

Terminal-CLI til direkte konfiguration og testning af gatewayen — ingen browser nødvendig.

**Installation:**
```bash
cd cli
pip install -r requirements.txt
# Kør direkte:
python mbgw.py --help
# Eller installér globalt som 'mbgw':
pip install -e .
```

**Hurtig start:**
```bash
# Gem gateway-IP én gang
mbgw config set host 192.168.1.100

# Tjek system-status
mbgw status

# Konfigurer WiFi
mbgw wifi scan
mbgw wifi set --ssid MitNetværk --password hemmeligt

# Læs 10 holding registers fra slave 1
mbgw read holding 0 1 0 10

# Skriv værdi 1234 til register 100
mbgw write holding 0 1 100 1234

# Tjek og opdater firmware
mbgw ota check
mbgw ota firmware
```

**JSON-output til scripting:**
```bash
mbgw --json status | python -c "import json,sys; d=json.load(sys.stdin); print(d['ip'])"
mbgw --json read holding 0 1 0 5 | jq '.registers[]'
```

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
