# Features

Format: `[status] vX.X.X — beskrivelse`
Status: `planned` | `in-progress` | `done`

---

## Planlagte features

- [ ] planned — Basis Modbus RTU master på ét RS485-interface (UART1)
- [ ] planned — NVS-baseret konfigurationspersistens (baudrate, slave-adresser, polling-interval)
- [ ] planned — REST API: GET /api/interfaces og GET /api/interfaces/{id}/registers
- [ ] planned — Web frontend: monitoreringsside med live register-visning
- [ ] planned — WebSocket push ved register-ændringer
- [ ] planned — Web frontend: konfigurationsside (interface-parametre, slave-adresser)
- [x] done    v0.2.0 — Modbus slave mode pr. interface (HW-UART): konfigureres med `mode slave` + `addr <1-247>` i CLI; esp-modbus slave API med 128 holding/input/coil/discrete registre pr. interface
- [x] done    v0.2.0 — Dynamisk tilføjelse/sletning af Modbus interfaces via CLI (`interface modbus<N>` / `no interface modbus<N>`) op til maks. GATEWAY_MAX_IFACES (8)
- [ ] planned — Multi-interface support (UART1 + UART2)
- [ ] planned — SPIFFS-baseret data-backup (seneste kendte værdier ved strømfald)
- [x] done    — OTA firmware- og frontend-opdatering fra GitHub releases (REST API)
- [x] done    — CLI-værktøj (mbgw) til konfiguration og testning fra terminal (WiFi, interfaces, Modbus R/W, OTA)
- [ ] planned — Alarm/threshold-logik med notifikation via WebSocket
- [x] done    — Build-optimering: sdkconfig trim (b0019: 134KB flash sparet) + coredump deaktiveret (b0020)
- [ ] planned — REST API endpoints for slave register-bank (GET/PUT pr. interface)
- [ ] planned — SW-UART slave mode (custom bit-bang svar-implementering)
- [ ] planned — Build COMPONENTS-filtrering: PlatformIO's ESP-IDF integration understøtter ikke set(COMPONENTS ...) i rod-CMakeLists.txt (main-komponenten ekskluderes). Kræver dybere PlatformIO-specifik løsning.
