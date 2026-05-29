# Release Notes

---

## v0.1.0 build 0024 — 2026-05-29 — CLI show: komplet konfigurationsvisning

`show` kommandoen viser nu **al gemt konfiguration** opdelt i tre sektioner:

```
--------------------------------
ETHERNET
  IP      : 192.168.1.100
  Gateway : 192.168.1.1
  Netmask : 255.255.255.0

WIFI STA
  Aktiv   : ja
  SSID    : MitNetværk
  Password: *** (sat)
  IP      : dhcp
  Gateway : (dhcp)
  Netmask : (dhcp)

WIFI AP FALLBACK
  Aktiv   : ja
  SSID    : ModbusGW-XXXXXX (auto)
  Password: (åben)

MODBUS INTERFACES  (1 konfigureret)
  [0] RS485  HW  UART1
       Baud    : 9600
       Format  : 8N1  paritet=ingen
       Timeout : 500 ms
       Pins    : TX=17  RX=16  DE/RTS=4
       Status  : aktiv
--------------------------------
```

---

## v0.1.0 build 0023 — 2026-05-29 — API endpoint-oversigt på /api og /api/v1

`GET http://ip/api` eller `GET http://ip/api/v1/` returnerer nu en komplet liste over alle tilgængelige endpoints:

```json
{
  "api": "Modbus API Gateway",
  "version": "0.1.0",
  "build": "0023",
  "base": "/api/v1",
  "endpoints": [
    {"method": "GET",  "path": "/api/v1/system", "description": "System info..."},
    ...
  ]
}
```

Alle 21 endpoints er beskrevet med metode, sti og dansk beskrivelse. Fungerer som API-dokumentation direkte fra gatewayen.

---

## v0.1.0 build 0022 — 2026-05-29 — CLI: wifi status og wifi mode

To nye subkommandoer under `wifi`:

**`wifi status`** — live WiFi-status:
- Tilstand: deaktiveret / forbinder / forbundet / AP hotspot / fejl
- WiFi mode: klient (STA) / AP / klient+AP (APSTA)
- MAC-adresse (STA-interface)
- Ved forbundet: SSID, IP-adresse, signalstyrke (RSSI i dBm), kanal, kryptering (WPA2 etc.), BSSID
- Ved AP fallback: AP-netværksnavn, IP (192.168.4.1), AP MAC-adresse

**`wifi mode`** — kort tilstandsvisning:
- Viser om gatewayen kører som WiFi-klient, AP hotspot, eller begge (APSTA fallback)
- Viser SSID ved aktiv STA-forbindelse

---

## v0.1.0 build 0021 — 2026-05-29 — CLI: kommandohistorik og cursor-bevægelse

Serial CLI understøtter nu pile-taster fuldt ud:
- **↑/↓**: navigér de seneste 20 kommandoer
- **←/→**: flyt cursor inden i aktuel kommando
- **Home/End** og **Ctrl+A/E**: hop til linjens start/slut
- **Ctrl+W**: slet ord bagud
- **Ctrl+K**: slet til linjeslut

---

## v0.1.0 build 0020 — 2026-05-29 — Core dump deaktiveret

Core dump er deaktiveret (ingen coredump-partition i partitionstabellen). Reducerer espcoredump-komponentens overhead.

---

## v0.1.0 build 0019 — 2026-05-29 — Build-optimering: 134 KB flash sparet + hurtigere version-bumps

Firmware-image reduceret fra 77,5% (1219 KB) til 69,0% (1085 KB) — 134 KB frigjort. Ubrugte komponenter fjernet fra build: Bluetooth (Bluedroid + NimBLE), TLS 1.0/1.1, sjældne elliptiske kurver, AES-CCM, PKCS12, DHE-PSK, SLIP, PPP og overflødige SPI flash-drivere. Boya flash-chip nu korrekt identificeret (ingen boot-advarsel).

Version og build-nummer er flyttet til `version.h` (inkluderes kun af 4 filer mod tidligere 13). Fremtidige version-bumps recompilerer nu kun ~4 filer i stedet for alle 13 — byggetid for version-only ændringer: ca. 6 sekunder.

---

## v0.1.0 build 0018 — 2026-05-29 — CLI-prompt vises kun én gang

CLI-prompten `gw>` vises nu kun én gang efter hvert Enter — ikke to gange. Rod-årsag: ESP-IDF's standard UART RX-mode (`CR`) oversætter `\r`→`\n`, men Windows-terminaler sender `\r\n` (to tegn) som begge tolkes som Enter. Løst ved at skifte til `CRLF`-mode der konsumerer `\r\n` som ét enkelt `\n`.

---

## v0.1.0 build 0017 — 2026-05-28 — Stabil boot + Modbus starter korrekt

Gateway booter nu stabilt og Modbus RS485 starter korrekt ved hvert boot — også efter save+reboot. Rod-årsag til boot-loop var en ugyldig `uart_num=-1` i NVS-config fra en tidligere build. Ny `config_sanitize()` retter automatisk ugyldige værdier ved load. `mb_interface_init` er nu fuldt non-fatal — ingen `ESP_ERROR_CHECK` der kan forårsage panic.

CLI testet og verificeret: `help`, `status`, `show`, `eth`, `wifi`, `save`, `reboot` — alle fungerer korrekt.

---

## v0.1.0 build 0015 — 2026-05-28 — Stabil boot efter save+reboot

Gateway booter nu stabilt selv efter save+reboot. Alle tidligere årsager til boot-loop er rettet: NVS-fejl håndteres gracefully, Modbus/API init-fejl forårsager ikke længere panic, og UART RS485-mode sættes i korrekt rækkefølge.

---

## v0.1.0 build 0014 — 2026-05-27 — Serial CLI fungerer nu korrekt

Serial CLI blokkerer nu korrekt på brugerinput — ingen "gw>" prompt-spam mere. Rod-årsag: `esp_console_init()` installerer ikke UART-driveren i ESP-IDF v5.x, så stdin kørte non-blocking. Løst ved at bruge `esp_console_new_repl_uart()` som er den korrekte v5.x API.

Du kan nu bruge CLI normalt: `help`, `show`, `status`, `wifi`, `eth`, `save`, `reboot`.

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
