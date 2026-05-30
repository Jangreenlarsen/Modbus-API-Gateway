# Features

Format: `[status] vX.X.X — beskrivelse`
Status: `planned` | `in-progress` | `done`

---

## Færdige features

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

---

## Planlagte features

- [ ] planned — SPIFFS-baseret data-backup (seneste kendte værdier ved strømfald)
- [ ] planned — Lokal register-cache i RAM/NVS (overlever korte Modbus-fejl)
- [ ] planned — Web frontend monitoreringsside: live register-visning pr. slave med polling-konfiguration
- [ ] planned — WebSocket push ved register-ændringer (real-time til monitoreringsklienter)
- [ ] planned — REST API endpoints for slave register-bank (GET/PUT pr. interface for slave-mode-data)
- [ ] planned — SW-UART slave mode (custom bit-bang svar-implementering)
- [ ] planned — Alarm/threshold-logik med notifikation via WebSocket
- [ ] planned — REST API key per role (read-only vs read/write)
- [ ] planned — Build COMPONENTS-filtrering: PlatformIO's ESP-IDF integration understøtter ikke `set(COMPONENTS ...)` i rod-CMakeLists.txt
