# Features

Format: `[status] vX.X.X — beskrivelse`
Status: `planned` | `in-progress` | `done`

---

## Færdige features

- [x] done    v0.9.0 b0095 — Forside + indlejret manual: ny landingsside på `GET /` (var tidligere 404 — ingen rute eksisterede) med live systemstatus og navigationskort til Management/Manual/API/GitHub, samt `GET /manual` — hele MANUAL.md konverteret til en pæn, navigerbar HTML-side (samme visuelle stil som /mgmt) med indholdsfortegnelse, GPIO pin-tildelingstabeller for RS485/RS232, wiring-diagrammer og komplet REST API-guide. Begge indlejret i firmwaren (ingen SPIFFS-afhængighed, samme mønster som /mgmt). Kryds-navigation tilføjet i /mgmt-headeren.
- [x] done    v0.8.0 b0093 — Dekodet Modbus-log i web-GUI: ring-buffer (100 entries) over bus-transaktioner, hooket i modbus_manager. `GET /api/v1/modbus/log?since=N` + `POST /api/v1/modbus/log/clear`. Ny "Modbus Log"-fane i /mgmt med live-poll, FC-navne, slave/adr/antal/status/værdi, pause + ryd.
- [x] done    v0.7.0 b0092 — Interface loopback-selvtest: `POST /api/v1/interfaces/{key}/selftest {"mode":"internal"|"external"}`. HW-UART master sender en FC03-forespørgsel med (intern) UART-loopback og verificerer at telegrammet modtages retur på RX (TIMEOUT=fejl, ellers OK). Test-knap pr. interface i /mgmt. SW-UART understøttes ikke (bit-bang deler timer mellem TX/RX); slave-mode heller ikke. CLI udestår.
- [x] done    v0.6.0 b0091 — GPIO-oversigt på interface-konfig-siden: `GET /api/v1/system/gpio` beregner pr. GPIO om den kan bruges som TX/RX/DE ift. board-variant + aktiv Ethernet-config (reserverede/optagne pins markeres). Farvekodet chip-grid i /mgmt interfaces-fanen.

- [x] done    v0.1.0 — Basis Modbus RTU master på ét RS485-interface (UART1)
- [x] done    v0.1.0 — NVS-baseret konfigurationspersistens (alle interface-parametre)
- [x] done    v0.1.0 — REST API: fuld FC01–FC10/FC0F mapping med 1:1 Modbus-operationer
- [x] done    v0.1.0 — CLI-værktøj (IOS-stil) til konfiguration via UART0 (Ethernet, WiFi, interfaces, API)
- [x] done    v0.1.0 — OTA firmware- og frontend-opdatering fra GitHub releases (REST API + web GUI)
- [x] done    v0.1.0 — Build-optimering: sdkconfig trim (b0019: 134KB flash sparet) + coredump deaktiveret (b0020)
- [x] done    v0.1.0 — Web management page `/mgmt` (status, OTA, RS485 config)
- [x] done    v0.2.0 — Modbus slave mode pr. interface (HW-UART): `mode slave` + `addr <1-247>` i CLI; esp-modbus slave API med 128 holding/input/coil/discrete registre pr. interface
- [x] done    v0.2.0 — Dynamisk tilføjelse/sletning af Modbus interfaces (`interface modbus<N>` / `no interface modbus<N>`) op til GATEWAY_MAX_IFACES (8)
- [x] done    v0.2.1 — W5500 SPI Ethernet performance fix: konfigurerbar SPI clock (default 10MHz), pull-up advarsel, lru_purge_enable, ISR-miss workaround-task
- [x] done    v0.3.0 — Master/slave + tilføj/slet/konfigurer interfaces fra web GUI (`/mgmt`)
- [x] done    v0.3.0 — Navn-alias pr. interface: `floor1`, `pumpestation`, ... kan bruges som URL-segment i alle interface- og FC-routes
- [x] done    v0.3.0 — GPIO pin-konfiguration (TX, RX, DE) i web GUI + komplet CLI-support (`name`, `tx`, `rx`, `de`, `type`, `uart`, `format`, `timeout`)
- [x] done    v0.3.0 — FC01-FC10 REST master dispatcher: alle Modbus function codes virker nu via REST (var broken pga. ESP-IDF httpd midt-wildcard begrænsning)
- [x] done    v0.3.0 — POST /api/v1/interfaces (opret) og DELETE /api/v1/interfaces/{key} (slet+renummerér)
- [x] done    v0.4.0 — Modbus register cache: read-through, TTL-baseret freshness, LRU eviction (256 entries), hit/miss/error stats, write-through på succes / invalidering ved fejl. Inspireret af Modbus_server_slave_ESP32's async cache.
- [x] done    v0.4.0 — Cache REST API: `GET /api/v1/cache/stats|entries`, `PUT /api/v1/cache/config`, `POST /api/v1/cache/clear|reset-stats`
- [x] done    v0.4.0 — Cache tab i /mgmt: live stats, entries-tabel, TTL/enable toggles, clear/reset knapper
- [x] done    v0.4.0 — CLI `cache` kommandoer (`show cache`, `cache enable/disable/ttl/clear/reset-stats/entries`)

---

## Planlagte features

- [ ] planned — SPIFFS-baseret data-backup (seneste kendte værdier ved strømfald)
- [ ] planned — Lokal register-cache i RAM/NVS (overlever korte Modbus-fejl)
- [ ] planned — Web frontend monitoreringsside: live register-visning pr. slave med polling-konfiguration
- [ ] planned — WebSocket push ved register-ændringer (real-time til monitoreringsklienter)
- [x] done    v0.4.0 b0063 — Cache fase 2: baggrunds-refresh task der holder hot data varmt (scanner cache for stale entries, refresher op til 8/cycle). NB: simpler end Modbus_server_slave_ESP32's priority queue + adaptive backoff design — fase 3 kan tilføje det hvis nødvendigt.
- [x] done    v0.4.0 b0063 — Historisk metrics: ringbuffer af 60 samples × 10s = 10 min historik. SVG sparkline-graf i /mgmt med hit rate, requests/s, refreshes/s. REST `GET /api/v1/cache/history`.
- [x] done    v0.4.0 b0062 — Cache NVS-persistens (TTL, enabled, refresh-config overlever reboot)
- [ ] planned — Cache fase 3 (kun ved behov): per-slave adaptive backoff, priority queue (writes > fresh reads > refreshes), full async decoupling med PENDING-state
- [ ] planned — REST API endpoints for slave register-bank (GET/PUT pr. interface for slave-mode-data)
- [ ] planned — SW-UART slave mode (custom bit-bang svar-implementering)
- [ ] planned — Alarm/threshold-logik med notifikation via WebSocket
- [ ] planned — REST API key per role (read-only vs read/write)
- [ ] planned — Build COMPONENTS-filtrering: PlatformIO's ESP-IDF integration understøtter ikke `set(COMPONENTS ...)` i rod-CMakeLists.txt
