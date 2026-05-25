# Projekt: Modbus API Gateway

Dette er Claudes system-prompt for dette projekt. Den læses altid først og følges uden undtagelser.

---

## Projektbeskrivelse

En **Modbus-til-REST gateway** bygget på **ESP32** med **x antal RS485/RS232 interfaces** og **Ethernet-tilslutning**.

Enheden fungerer som Modbus RTU master over RS485/RS232 og eksponerer et **fuldt REST API over Ethernet** — man kan via REST udføre nøjagtigt de samme operationer som hvis man var direkte på en Modbus-node (læse/skrive coils, discrete inputs, input registers, holding registers med alle standard function codes).

Derudover kører der et **lokalt datacache** på ESP32'en (NVS/SPIFFS) der gemmer seneste kendte register-værdier, så data er tilgængeligt selv under kortvarige Modbus-fejl. En **web frontend** serveres direkte fra ESP32 til konfiguration og realtidsmonitorering.

**Primær use case**: Tilslut legacy Modbus-enheder (PLC'er, sensorer, aktuatorer) til et IT-netværk via standard HTTP REST — ingen specialsoftware nødvendig på klientsiden.

**Hardware-referenceplatform**: ESP32 (Espressif) — ESP-IDF v5.x framework  
**Ethernet**: LAN8720 (RMII) eller W5500 (SPI) — konfigureres pr. hardware-variant  
**Modbus-bibliotek**: esp-modbus (Espressif officiel)  
**Serial interfaces**: RS485 (half-duplex) og/eller RS232 (full-duplex) via UART  
**Protokol**: Modbus RTU (binær, CRC-16, 3.5 character-time frame-delimiter)

---

## Faste regler

1. **Versionering (UFRAVIGELIG)**: Projektet versioneres via [version.json](version.json). Denne fil er den **eneste** kilde til versionsnumre — alle andre steder (firmware, frontend, changelog) læser herfra.
   - Normalt format: `{ "version": "MAJOR.MINOR.PATCH", "build": "NNNN" }`
   - **build**: incrementeres med **1** ved HVERT commit med kodeændringer (0001 → 0002 → ...).
   - **PATCH**: incrementeres ved bug fixes (afsluttede). Build nulstilles IKKE.
   - **MINOR**: incrementeres ved nye features. PATCH sættes til 0.
   - **MAJOR**: incrementeres ved breaking changes eller store milepæle. MINOR og PATCH sættes til 0.
   - **Debugging-format (4 decimaler)**: Under aktiv fejlsøgning bruges `MAJOR.MINOR.PATCH.D` hvor D starter på 1 og incrementeres for hvert debug-commit: `v1.0.0.1`, `v1.0.0.2`, osv. Commit-beskeder præfikses `vX.X.X.D-bNNNN: debug: beskrivelse`.
   - **Afslutning af debugging**: Når Jan bekræfter *"nu virker det som det skal"*, incrementeres PATCH og D nulstilles: `v1.0.0.3` → `v1.0.1.0`. Commit markeres som `fix:` og afslutter debug-serien.
   - **Kun-dokumentations-commits** (RELEASE_NOTES.md, CHANGELOG.md, BUGS.md, FEATURES.md uden kodeændringer): bump IKKE version — lav commit uden versionsbump.
   - **RELEASE_NOTES.md skal opdateres ved ETHVERT commit der ændrer kode** — features, bugfixes og debug-afslutninger. Dokumentations-commits er undtaget. Glem aldrig dette.
   - Changelog-entries tagges med versionsnummer: `## [1.0.0 build 0001] — 2026-05-25 — beskrivelse`.
   - Claude **skal** opdatere `version.json` og vise den nye version i commit-beskeden.

2. **Ny funktionalitet (features)** skal ALTID registreres i [FEATURES.md](FEATURES.md) *før* implementering påbegyndes. Opdatér status når den er færdig.

3. **Bugs** skal ALTID registreres i [BUGS.md](BUGS.md) så snart de opdages. Opdatér med løsning når de er fikset.

4. **Alle kodeændringer** skal logges i [CHANGELOG.md](CHANGELOG.md) med version, dato, berørte filer og kort beskrivelse. Nyeste øverst.

5. **Lag-arkitekturen** beskrevet i [ARCHITECTURE.md](ARCHITECTURE.md) skal respekteres til enhver tid:
   - Frontend taler **kun** med ESP32's REST/WebSocket API — aldrig direkte med Modbus-laget.
   - API-laget kalder **kun** service-laget.
   - Service-laget kalder **kun** Modbus-laget og storage-laget.
   - Modbus-laget kalder **kun** UART/hardware-driverne.

6. **REST API 1:1 Modbus mapping (UFRAVIGELIG)**: Hvert Modbus function code skal have et tilsvarende REST endpoint. Se [ARCHITECTURE.md](ARCHITECTURE.md) for komplet endpoint-tabel. API'et må ikke tilbyde operationer der ikke findes i Modbus-standarden, og må ikke skjule Modbus-operationer bag højere abstraktioner.

7. **Modbus-reference**: [MODBUS_REFERENCE.md](MODBUS_REFERENCE.md) indeholder protokolspecifikation, function codes, register-typer, frame-struktur, CRC-beregning og RS485/RS232-timing. Konsulter ved al Modbus-integration og hold opdateret med nye fund.

8. **ESP32-reference**: [ESP32_REFERENCE.md](ESP32_REFERENCE.md) indeholder UART-konfiguration, GPIO-mapping, Ethernet-opsætning, NVS/SPIFFS API, task-model (FreeRTOS), power management og OTA-opdatering. Konsulter ved hardware-nær kode.

8. **Runtime-logging**: Firmware skal logge alle Modbus-operationer og fejl via ESP-IDF logging-systemet (`ESP_LOG*`). Log-niveau pr. komponent konfigureres i `sdkconfig`.

9. **Read/write rettigheder**: Claude har forhåndsgodkendelse (via [.claude/settings.local.json](.claude/settings.local.json)) til at læse, skrive og redigere filer i projektmappen.

10. **Versionskontrol**: Projektet er et git-repo. Efter enhver logisk afsluttet ændring skal Claude lave en git commit med en beskrivende commit-besked. Aldrig bulk-commits af urelaterede ændringer.

11. **GitHub branch-strategi (UFRAVIGELIG)**:
    - `dev` — aktiv udviklingsbranch. **Al ny kode commites hertil.** Claude arbejder altid på `dev`.
    - `main` — stabil release-branch. Kun opdateret via PR/merge fra `dev` når en release er klar. Produktion følger `main`.
    - Claude skal pushe til `origin dev` efter hvert commit — aldrig direkte til `main`.
    - Merge `dev` → `main` gøres manuelt af Jan når en release er godkendt.

12. **Push og merge efter commit (UFRAVIGELIG)**: Efter ethvert commit skal Claude automatisk:
    - Pushe til `origin dev`
    - Spørge Jan: *"Vil du også merge til `main` og pushe?"*
    - Hvis ja: merge `dev` → `main` med `--no-ff` og pushe `origin main`
    - Hvis nej: forblive på `dev` og informere om at `main` ikke er opdateret

---

## Workflow for enhver opgave

1. Tilføj entry i `FEATURES.md` (feature) eller `BUGS.md` (bug).
2. Implementer ændringen i det korrekte lag jf. `ARCHITECTURE.md`.
3. Opdater `version.json`: bump build (altid), bump version (hvis feature/bugfix/breaking).
4. Tilføj entry i `CHANGELOG.md` med `[version build NNNN]` prefix.
5. Opdater `RELEASE_NOTES.md` hvis kode er ændret.
6. Kør flash/test hvis relevant.
7. `git add` + `git commit` med besked der inkluderer version: `v1.0.0-b0001: beskrivelse`.
8. `git push origin dev` til GitHub.
9. Spørg Jan: *"Vil du også merge til `main`?"* — merge og push `origin main` hvis ja.

---

## Modbus RTU Quick Reference

### Frame-struktur
```
| Slave Addr (1B) | Function Code (1B) | Data (0–252B) | CRC-16 (2B) |
```
- Frame-delimiter: min. **3.5 character times** silence før og efter.
- Inter-character gap: max **1.5 character times** — ellers betragtes frame som afsluttet.
- Slave-adresser: **1–247** (0 = broadcast, 248–255 reserveret).

### Function Codes
| FC   | Navn                    | Registertype        | R/W |
|------|-------------------------|---------------------|-----|
| 0x01 | Read Coils              | Coil (1-bit)        | R   |
| 0x02 | Read Discrete Inputs    | Discrete Input (1-bit) | R |
| 0x03 | Read Holding Registers  | Holding Reg (16-bit)| R   |
| 0x04 | Read Input Registers    | Input Reg (16-bit)  | R   |
| 0x05 | Write Single Coil       | Coil (1-bit)        | W   |
| 0x06 | Write Single Register   | Holding Reg (16-bit)| W   |
| 0x0F | Write Multiple Coils    | Coil (1-bit)        | W   |
| 0x10 | Write Multiple Registers| Holding Reg (16-bit)| W   |

### Registertyper
| Type             | Adresse-prefix | Størrelse | Adgang      |
|------------------|---------------|-----------|-------------|
| Coil             | 0x            | 1 bit     | R/W         |
| Discrete Input   | 1x            | 1 bit     | Read-only   |
| Input Register   | 3x            | 16 bit    | Read-only   |
| Holding Register | 4x            | 16 bit    | R/W         |

### RS485 elektrisk
- Half-duplex, differentialt signal (A/B linje)
- Op til **247 enheder** pr. bus-segment
- Op til **1200 meter** uden repeater
- Typiske baud-rates: 9600 / 19200 / 38400 / 57600 / 115200 bps
- Terminering: 120 Ω modstand i begge ender af bus

---

## Projektstruktur

```
.
├── CLAUDE.md                  # denne fil — regler Claude altid følger
├── version.json               # SINGLE SOURCE OF TRUTH for version + build
├── ARCHITECTURE.md            # lag-struktur og arkitekturregler
├── MODBUS_REFERENCE.md        # Modbus RTU protokol-reference
├── ESP32_REFERENCE.md         # ESP32 hardware og ESP-IDF API-reference
├── FEATURES.md                # features (planned / in-progress / done)
├── BUGS.md                    # bugs (open / fixed)
├── CHANGELOG.md               # alle kodeændringer, nyeste øverst
├── RELEASE_NOTES.md           # brugervenlige release-noter
├── .claude/
│   └── settings.local.json    # Claude-rettigheder
├── firmware/                  # ESP32 firmware (ESP-IDF / C)
│   ├── main/
│   │   ├── core/              # system-init, config, watchdog, OTA
│   │   ├── modbus/            # Modbus RTU master pr. RS485-interface
│   │   │   ├── interface_N/   # én mappe pr. RS485-port (N = 0, 1, 2, ...)
│   │   │   └── modbus_manager.c  # koordinerer alle interfaces
│   │   ├── storage/           # NVS/SPIFFS: backup, config-persistens
│   │   ├── api/               # HTTP/WebSocket API-server (esp_http_server)
│   │   │   ├── routes/        # REST-endpoints pr. ressource
│   │   │   └── ws_handler.c   # WebSocket real-time push
│   │   └── main.c             # FreeRTOS app_main entry
│   ├── components/            # genanvendelige ESP-IDF komponenter
│   ├── sdkconfig              # ESP-IDF Kconfig (genereret)
│   └── CMakeLists.txt
└── frontend/                  # web UI — taler kun med ESP32 API
    ├── index.html
    ├── css/
    │   └── style.css
    └── js/
        ├── api.js             # al kommunikation med ESP32 API
        ├── config.js          # konfigurationsside
        └── monitor.js         # realtidsmonitorering via WebSocket
```

---

## Arkitektur-lag (overblik)

```
┌─────────────────────────────────────────┐
│           Web Frontend (Browser)         │  HTML/CSS/JS
│   config.js  monitor.js  api.js          │
└──────────────┬──────────────────────────┘
               │ HTTP REST / WebSocket
┌──────────────▼──────────────────────────┐
│         API-lag (ESP32 HTTP server)      │  esp_http_server
│   /api/interfaces  /api/registers  /ws   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Service-lag (forretningslogik)   │  FreeRTOS tasks
│   polling-scheduler  data-aggregator     │
└──────┬───────────────────────┬──────────┘
       │                       │
┌──────▼──────┐       ┌────────▼────────┐
│ Modbus-lag  │       │  Storage-lag    │
│ esp-modbus  │       │  NVS / SPIFFS   │
│ master RTU  │       │  backup + cfg   │
└──────┬──────┘       └─────────────────┘
       │ UART + RS485 driver
┌──────▼──────────────────────────────────┐
│     Hardware: ESP32 UART0..N + RS485    │
│     MAX485 / SN65HVD3082 transceivers   │
└─────────────────────────────────────────┘
```
