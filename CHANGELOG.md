# Changelog

Nyeste øverst. Format: `## [version build NNNN] — YYYY-MM-DD — beskrivelse`

---

## [0.5.5 build 0086] — 2026-07-12 — fix: HW-slave guard (N1, komplet K1)

**Filer ændret:**
- `firmware/main/modbus/modbus_manager.c` — N1: guarden tracker nu både `hw_master_up` og `hw_slave_up`. esp-modbus' slave-controller er også en global singleton, så to HW-slaves kolliderede tidligere lydløst. Der tillades nu højst én HW-master og højst én HW-slave (kan sameksistere).
- `version.json`, `version.h` — bump til 0.5.5 b0086

---

## [0.5.4 build 0084] — 2026-07-11 — chore: fjern død kode + cache-lås (L1, L4)

**Filer ændret:**
- `firmware/main/modbus/mb_rtu_sw.c`, `mb_rtu_sw.h` — L1: fjernet den døde `mb_rtu_sw_transaction` (+ `rx_cb`/`rx_ctx_t`) der aldrig blev kaldt og lækkede en queue.
- `firmware/main/storage/register_cache.c` — L4: `cache_lookup` læser `enabled` inde i mutex-låsen.
- L2, L3, L7 markeret [accepted] i BUGS.md (ingen kodeændring — dokumenteret rationale).
- `version.json`, `version.h` — bump til 0.5.4 b0084

---

## [0.5.3 build 0083] — 2026-07-11 — fix: OTA-robusthed (M4, M5)

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — M4: frontend-OTA validerer modtaget størrelse mod `content_len`; ufuldstændig download → ERROR (ikke "done"). Ny `ota_report_error()`.
- `firmware/main/ota/ota_manager.h` — deklaration af `ota_report_error()`.
- `firmware/main/api/routes/ota.c` — M5: GitHub-URL-opslag flyttet til `ota_task` (baggrund); handleren blokerer ikke længere. Handleren svarer straks; fejl rapporteres via OTA-status.
- `version.json`, `version.h` — bump til 0.5.3 b0083

---

## [0.5.2 build 0082] — 2026-07-11 — fix: robuste request-bodies + input-validering (M3, L5, L6)

**Filer ændret:**
- `firmware/main/api/routes/fc_common.h/.c` — `api_recv_body()` (robust recv-loop) + `api_query_u16()` (clampet query-parse).
- `firmware/main/api/routes/coils.c`, `holding_regs.c` — writes bruger `api_recv_body`; FC05 afviser tom/ugyldig body (400).
- `firmware/main/api/routes/coils.c`, `discrete.c`, `holding_regs.c`, `input_regs.c` — reads bruger `api_query_u16` (negativ→0, >65535→65535).
- `firmware/main/api/routes/interfaces.c` — bruger delt `api_recv_body`; egen recv-loop fjernet.
- `version.json`, `version.h` — bump til 0.5.2 b0082

---

## [0.5.1 build 0081] — 2026-07-11 — fix: ensret Modbus fejl-respons (H3, H2)

**Filer ændret:**
- `firmware/main/api/routes/fc_common.h/.c` — ny `api_mb_ok()`: fælles fejl-respons for alle FC-routes (`modbus_timeout` 504 / `modbus_exception` 400 / `modbus_error` 400) med exception-beskrivelser.
- `firmware/main/api/routes/coils.c`, `discrete.c`, `holding_regs.c`, `input_regs.c` — bruger `api_mb_ok()`; duplikeret/inkonsistent fejlkode fjernet. FC01/02/04 returnerer nu også det dokumenterede exception-format.
- `firmware/main/CMakeLists.txt` — tilføjet `api/routes/fc_common.c`.
- H2: esp-modbus v1.x-begrænsning (ingen HW exception-kode) dokumenteret i `fc_common.c`.
- `version.json`, `version.h` — bump til 0.5.1 b0081

---

## [0.5.0 build 0080] — 2026-07-11 — refactor: reelt service-lag + interface-routing (M1, H1, M2)

**Filer ændret:**
- `firmware/main/service/gateway_service.h/.c` — M1: service-laget er nu funktionelt. Ejer `gw_resolve_iface()` (opslag mod kørende config) + `gw_*` Modbus-operationer. API-laget kalder KUN dette lag.
- `firmware/main/main.c` — kalder `gateway_service_init(&cfg)` med kørende config.
- `firmware/main/api/routes/coils.c`, `discrete.c`, `holding_regs.c`, `input_regs.c` — kalder `gw_*` i stedet for `mb_*`; inkluderer `gateway_service.h` i stedet for `modbus_manager.h`.
- `firmware/main/api/routes/interfaces.c` — H1/M2: FC-dispatchers resolver mod kørende config (ingen NVS på hot-path); POST/PUT/DELETE returnerer `reboot_required`.
- `version.json`, `version.h` — bump til 0.5.0 b0080

---

## [0.4.6 build 0079] — 2026-07-11 — fix: kritiske robusthedsfejl (K1, K2, K3)

**Filer ændret:**
- `firmware/main/storage/config_store.c` — K3: `config_store_save` returnerer fejl ved NVS-open-fejl i stedet for `ESP_ERROR_CHECK` (kunne panikke enheden fra en ekstern PUT).
- `firmware/main/modbus/mb_rtu_sw.c` — K2: `mb_sw_read_coils`/`mb_sw_read_discrete` clamper kopieret byte-antal til `(count+7)/8` → ingen stack-overflow fra en slave med for stort byte-count.
- `firmware/main/modbus/modbus_manager.c` — K1: kun én HW-UART master initialiseres (esp-modbus global controller); yderligere HW-masters deaktiveres. Per-interface fejl er ikke længere fatal. `get_iface()` afviser ikke-ready interfaces.
- `firmware/main/modbus/interface.h/.c` — nyt `ready`-felt; sættes efter vellykket init.
- `version.json`, `version.h` — bump til 0.4.6 b0079

---

## [0.4.5 build 0078] — 2026-05-31 — feat: live API log tab i management-siden

**Filer ændret:**
- `firmware/main/api/api_log.h/.c` — ring buffer (100 entries, RAM, seq-numre). Ignorerer egne log-endpoint kald.
- `firmware/main/api/server.c` — `reg()` wrapper der intercepter alle HTTP-requests via `user_ctx`. Ingen per-handler ændringer nødvendige.
- `firmware/main/api/routes/system.h/.c` — `GET /api/v1/system/log?since=N` + `POST /api/v1/system/log/clear`
- `firmware/main/api/routes/mgmt.c` — ny "API Log" tab: live tabel, auto-scroll, pause-knap, ryd-knap, metode-badge farver
- `firmware/main/CMakeLists.txt` — tilføjet `api/api_log.c`
- `version.json`, `version.h` — bump til b0078

---

## [0.4.5 build 0077] — 2026-05-31 — fix: OTA Installer-knap reagerer ikke (2 bugs)

**Filer ændret:**
- `firmware/main/ota/ota_manager.h` — `ota_update_firmware` og `ota_update_frontend` tager ikke længere `status*` parameter
- `firmware/main/ota/ota_manager.c` — begge update-funktioner opdaterer nu `s_status` direkte (den globale der returneres af `ota_get_status()`). Asset-matching finder nu ethvert `.bin`-asset, ikke kun eksakt "firmware.bin"
- `firmware/main/api/routes/ota.c` — `ota_task` bruger ikke længere lokal `status`-variabel
- `version.json`, `version.h` — bump til 0.4.5 b0077

**Bug 1 (knap gør ingenting)**: `OTA_FIRMWARE_ASSET="firmware.bin"` matchede aldrig release-asset navne som `modbus-gateway-v0.4.5-b0076.bin` → `firmware_url` altid tom → JS returnerede tidligt.

**Bug 2 (progress opdateres aldrig)**: `ota_task` oprettede lokal `ota_status_t status` og sendte den til `ota_update_firmware`. Men `ota_get_status()` returnerer global `s_status` — som aldrig blev opdateret. Polleren så altid `state:"idle"`.

---

## [0.4.5 build 0076] — 2026-05-31 — fix: PATCH-bump for at bootstrappe OTA forbi version-sammenligning bug

**Filer ændret:**
- `version.json`, `version.h` — bump til 0.4.5 b0076

**Årsag**: Enheder der kører ≤b0074 kan ikke detektere b0075 via OTA (buggy version_newer ignorerer build-nummer). En PATCH-bump fra 0.4.4 → 0.4.5 detekteres korrekt selv med gammel kode.

---

## [0.4.4 build 0075] — 2026-05-31 — fix: OTA version-sammenligning ignorerede build-nummer

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — `version_newer()` parser nu `-bNNNN` suffix; `ota_check()` sammenligner med "VERSION-bBUILD" lokalt
- `version.json`, `version.h` — bump til b0075

**Årsag**: GitHub release-tags har format `v0.4.4-b0074`. `sscanf("%d.%d.%d.%d")` stopper ved `-` → begge sider parsede til 0.4.4.0 → "ingen opdatering".

---

## [0.4.4 build 0074] — 2026-05-31 — feat: OTA-side med trin-indikatorer og auto-reconnect

**Filer ændret:**
- `firmware/main/api/routes/mgmt.c` — OTA-fanen redesignet: trin-indikatorer (Download → Flash → Genstart → Online), live progress, auto-reconnect polling efter reboot, viser ny version efter opdatering. `waitForReconnect()` poller `/api/v1/system` hvert 2. sekund til enheden er online igen.
- `version.json`, `version.h` — bump til b0074

---

## [0.4.3 build 0072] — 2026-05-31 — test: OTA verificeringsrelease

**Filer ændret:**
- `version.json`, `version.h` — bump til 0.4.3 b0072 for OTA-test

---

## [0.4.2 build 0071] — 2026-05-31 — fix: partition-tabel tilføjer otadata + ota_1 (OTA virker nu)

**Filer ændret:**
- `firmware/partitions.csv` — tilføjet `otadata` (0x10000) og `ota_1` (0x1A0000), fjernet `factory`, `ota_0` rykket til 0x20000. NVS forbliver på 0x9000 (data bevaret)
- `version.json`, `version.h` — bump til b0071

**VIGTIGT**: Kræver USB-flash én gang for at skrive ny partition-tabel. Herefter virker OTA.

---

## [0.4.2 build 0070] — 2026-05-31 — feat: OTA viser build-nummer + OTA test release

**Filer ændret:**
- `firmware/main/api/routes/ota.c` — `build` felt tilføjet til OTA check JSON response
- `firmware/main/ota/ota_manager.h/.c` — `current_build` felt i `ota_info_t`
- `firmware/main/api/routes/mgmt.c` — OTA "Installeret" viser nu "vX.X.X bNNNN"
- `version.json`, `version.h` — bump til 0.4.2 b0070

---

## [0.4.1 build 0069] — 2026-05-31 — fix: OTA firmware-download HTTP buffer for lille (512B → 4KB)

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — `esp_https_ota` HTTP buffer 512B → 4096B (GitHub redirect-headers ~2-3KB), timeout 30s → 60s
- `firmware/main/core/version.h`, `version.json` — bump til b0069

---

## [0.4.1 build 0068] — 2026-05-31 — fix: OTA GitHub buffer for lille (4KB → 16KB)

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — `HTTP_BUF_SIZE` 4096 → 16384 (GitHub API-svar > 4KB ved release med assets), log ved JSON parse-fejl
- `firmware/main/core/version.h`, `version.json` — bump til b0068

---

## [0.4.1 build 0067] — 2026-05-31 — release: v0.4.1 — GPIO presets, board variant, OTA fix, cache fase 2

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — `version_newer()` understøtter nu MAJOR.MINOR.PATCH.D (4 komponenter)
- `firmware/main/core/version.h`, `version.json` — bump til 0.4.1 b0067 (afslutter debug-serie 0.4.0.x)

**Indeholder alt fra debug-serien 0.4.0.1–0.4.0.3:**
- GPIO preset-tabel (30-pin + 38-pin board variant)
- Cache fase 2: async refresh-task + historisk metrics SVG sparkline
- OTA-siden viser "—" ved ingen opdatering
- W5500 ISR-miss workaround
- Interface navn-alias i REST API

---

## [0.4.0.3 build 0066] — 2026-05-31 — debug: GPIO preset + board variant (30/38-pin)

**Filer ændret:**
- `firmware/main/core/config.h` — `board_variant_t` enum, `board_variant` felt i `gateway_config_t`, `config_get_gpio_preset()` decl, `CONFIG_STRUCT_VERSION` → 12
- `firmware/main/core/config.c` — GPIO preset tabeller (30-pin + 38-pin), `config_get_gpio_preset()`, defaults bruger preset
- `firmware/main/api/routes/system.h/.c` — `GET/PUT /api/v1/system/hardware` (board variant + preset-tabel), `board_variant` i GET /system response
- `firmware/main/api/server.c` — registrér nye hardware routes
- `firmware/main/api/routes/mgmt.c` — board variant selector, JS preset-tabel, `applyPreset()`, `toggleType()` → auto-preset, GPIO output-validering, "GPIO Preset"-knap, board vist på status-siden
- `version.json`, `version.h` — bump til 0.4.0.3 b0066

---

## [0.4.0.2 build 0065] — 2026-05-31 — debug: OTA-side viser korrekt version

**Filer ændret:**
- `firmware/main/api/routes/mgmt.c` — "Tilgængelig" viser "—" når ingen opdatering tilgængelig (undgår forvirring ved ældre GitHub-release)
- `version.json`, `version.h` — bump til 0.4.0.2 b0065

---

## [0.4.0 build 0063] — 2026-05-31 — feat: cache fase 2 — async refresh-task + historisk metrics

**Filer ændret:**
- `firmware/main/storage/register_cache.h` + `.c` — ringbuffer `cache_history_sample_t[60]` (cumulative counter snapshots), `cache_history_sample()` / `cache_history_get()`, `cache_get_stale_entries()` (sorteret efter age desc), `cache_record_refresh()`. Stats udvidet med `refresh_done`/`refresh_failed`.
- `firmware/main/core/config.h` — `cache_config_t` udvidet med `refresh_enabled`, `refresh_interval_ms`, `refresh_threshold_pct`, `history_interval_ms`. `CONFIG_STRUCT_VERSION` 10→11.
- `firmware/main/core/config.c` — defaults: refresh on, 200ms interval, 75% threshold, 10s history-interval. Sanitize ranges.
- `firmware/main/modbus/modbus_manager.c` — to nye FreeRTOS-tasks: `refresh_task` (scanner cache hver `refresh_interval_ms` for entries hvor age > TTL × threshold_pct, refresher op til 8/cycle), `history_task` (sampler stats hver `history_interval_ms`).
- `firmware/main/api/routes/cache.c` + `.h` — ny `GET /api/v1/cache/history` endpoint. `cache_routes_set_cfg()` så PUT /cache/config kan opdatere refresh-felter. Stats-respons inkluderer refresh-tællere + config-felter.
- `firmware/main/api/routes/mgmt.c` — Cache tab udvidet med refresh-toggle og **SVG sparkline-graf** der viser hit-rate (blå), requests/s (rød), refreshes/s (grøn). Computer deltas mellem samples for periode-rater.
- `firmware/main/api/server.c` — registrér `route_get_cache_history`; api_index opdateret.
- `firmware/main/main.c` — `cache_routes_set_cfg(&cfg)` før `api_server_start()`.
- `firmware/main/core/serial_cli.c` — `cache refresh on|off|interval|threshold` kommandoer; `show cache` viser refresh-status + tællere; `show config` viser cache-sektion med refresh-felter.
- `firmware/main/core/version.h` — 0.4.0 build 0063
- `version.json` — 0.4.0 build 0063

**Effekt:**
- Bus-trafik bliver konsistent — refresh-task holder cache varm så klienter får ~0ms hits selv med lavt TTL.
- `/mgmt` → Cache tab viser nu live sparkline over de seneste 10 minutter (60 samples × 10s).
- Refresh kører age-baseret round-robin — mest stale entries refreshes først.

---

## [0.4.0 build 0062] — 2026-05-31 — feat: cache NVS-persistens (overlever reboot)

**Filer ændret:**
- `firmware/main/core/config.h` — `cache_config_t {enabled, ttl_ms}` tilføjet til `gateway_config_t.cache`; `CONFIG_STRUCT_VERSION` 9→10
- `firmware/main/core/config.c` — defaults `enabled=1, ttl_ms=1000`; sanitize
- `firmware/main/storage/register_cache.h` — `register_cache_init(gateway_config_t *cfg)` (var void). Forward-decl undgår cirkulær include
- `firmware/main/storage/register_cache.c` — gemmer `s_cfg` pointer; init læser fra `cfg->cache`; `cache_set_enabled()` og `cache_set_ttl_ms()` opdaterer både runtime-state OG `cfg->cache` så næste `save` persisterer
- `firmware/main/main.c` — `register_cache_init(&cfg)`
- `firmware/main/core/serial_cli.c` — `show config` viser `Cache`-sektion; cache CLI-kommandoer nævner `save+reboot for at persistere`
- `firmware/main/core/version.h` — 0.4.0 build 0062
- `version.json` — 0.4.0 build 0062

**Effekt:** TTL og enabled-flag overlever nu reboot. Ændringer via CLI eller REST modificerer den in-RAM cfg-struct, og næste `save` (CLI) persisterer til NVS. NVS invalideres på grund af CONFIG_STRUCT_VERSION mismatch ved første boot efter opgradering → defaults indlæses.

---

## [0.4.0 build 0061] — 2026-05-30 — feat: Modbus register cache + stats/metrics side

**Inspireret af** `Modbus_server_slave_ESP32`'s async cache. Denne version er synchronous (ingen baggrundstask, ingen priority queue) — fase 2 kan tilføje det.

**Filer ændret:**
- `firmware/main/storage/register_cache.c` + `.h` — fuldt implementeret (var stub før). 256 entries × 16 bytes = 4KB. Linear search find_entry, LRU eviction, TTL-baseret freshness, mutex-beskyttet. Per-entry: iface/slave/fc/addr/value/status/hits/last_update_ms. Stats: hits, misses, errors, evictions, entries_used, ttl_ms, enabled.
- `firmware/main/modbus/modbus_manager.c` — alle read/write-wrappers tjekker cache først. Reads: hvis ALLE adresser i en multi-register operation er fresh → returnér uden bus-trafik. Misses → kald esp-modbus, gem resultat. Writes: opdatér cache med succesfuldt skrevet værdi, invalidér ved fejl.
- `firmware/main/api/routes/cache.c` + `.h` — REST endpoints: `GET /cache/stats`, `GET /cache/entries`, `PUT /cache/config`, `POST /cache/clear`, `POST /cache/reset-stats`
- `firmware/main/api/routes/mgmt.c` — ny **Cache** tab: stats-tabel, entries-tabel med iface/slave/FC/addr/value/status/hits/age, TTL-input, enable-toggle, Tøm/Reset knapper
- `firmware/main/api/server.c` — cache routes registreret; api_index opdateret
- `firmware/main/core/serial_cli.c` — `cache` top-level kommando: `cache show|enable|disable|ttl|clear|reset-stats|entries`; `show cache` alias
- `firmware/main/main.c` — `register_cache_init()` kaldes før `modbus_manager_init()`
- `firmware/main/CMakeLists.txt` — `api/routes/cache.c` tilføjet
- `firmware/main/core/version.h` — 0.4.0 build 0061
- `version.json` — 0.4.0 build 0061
- `FEATURES.md`, `RELEASE_NOTES.md` — cache-features dokumenteret

**Default TTL: 1000ms.** Cache kan deaktiveres helt via CLI eller REST hvis behov for altid-fresh reads.

---

## [0.3.0 build 0060] — 2026-05-30 — fix: FC01-FC10 REST routes via master dispatcher

**Filer ændret:**
- `firmware/main/api/routes/coils.c` + `.h` — handlers konverteret til kaldbar funktion-signatur `api_fc01_read_coils(req, iface, slave)`, `api_fc05_write_coil(req, iface, slave, addr)`, `api_fc0f_write_coils(req, iface, slave)`. Fjernet `static` og route-definitioner.
- `firmware/main/api/routes/discrete.c` + `.h` — `api_fc02_read_discrete_inputs(req, iface, slave)`
- `firmware/main/api/routes/holding_regs.c` + `.h` — `api_fc03_*`, `api_fc06_*`, `api_fc10_*`
- `firmware/main/api/routes/input_regs.c` + `.h` — `api_fc04_read_input_regs`
- `firmware/main/api/routes/interfaces.c` — `master_get_dispatcher` og `master_put_dispatcher` parser URI: hvis URI indeholder `/slaves/N/op[/addr]` → kald den rette FC-funktion; ellers → config GET/PUT. `find_fc_op()` helper. Begge dispatchers håndterer navn-alias for `key`.
- `firmware/main/api/server.c` — kun master-routes registreres nu; individual FC-routes og deres includes fjernet
- `firmware/main/core/version.h` — build 0060
- `version.json` — build 0060

**Effekt:**
- FC01 (Read Coils), FC02 (Read Discrete), FC03 (Read Holding), FC04 (Read Input), FC05 (Write Coil), FC06 (Write Register), FC0F (Write Coils), FC10 (Write Registers) virker alle via REST nu
- Navn-alias virker for ALLE Modbus-operationer: `GET /api/v1/interfaces/floor1/slaves/3/holding-registers?start=0&count=10`

---

## [0.3.0 build 0059] — 2026-05-30 — fix: PUT 405-fejl + feat: navn-alias + GPIO pins i GUI

**Root cause for 405-fejl:** ESP-IDF `httpd_uri_match_wildcard` behandler kun `*` ved slutningen af URI-mønstre som wildcard. `/api/v1/interfaces/*/config` matchede aldrig nogen request → PUT save returnerede 405 Method Not Allowed.

**Filer ændret:**
- `firmware/main/core/config.h` — `name[24]` felt i `iface_config_t`; `CONFIG_STRUCT_VERSION` 8→9
- `firmware/main/core/config.c` — default-navne `modbus0`, `modbus1`, ...; sanitize sikrer null-termination
- `firmware/main/api/routes/interfaces.c` — trailing wildcard `/api/v1/interfaces/*` for GET/PUT/DELETE; `resolve_iface()` accepterer både numerisk ID og navn-alias; `is_config_request()` undgår at fange FC-routes; PUT håndterer både `/{key}` og `/{key}/config`; udvidet med `name`, `type`, `uart_mode`, `tx_pin`, `rx_pin`, `rts_pin` felter; bedre body-recv (loop indtil content_len bytes); memory leak fix i iface_to_json response
- `firmware/main/api/server.c` — api_index opdateret med `:key` notation
- `firmware/main/api/routes/mgmt.c` — Navn (API alias) felt, RS485/RS232 Type-selector, TX/RX/DE GPIO felter pr. interface; status-tab viser navn; saveIfc sender alle nye felter til PUT `/interfaces/{id}`
- `firmware/main/core/serial_cli.c` — `name <navn>` kommando i CTX_MODBUS; show config viser `Name`; show status viser navn ved siden af ID
- `firmware/main/core/version.h` — 0.3.0 build 0059
- `version.json` — 0.3.0 build 0059

**Kendt issue:** FC01-FC10 routes (`/api/v1/interfaces/N/slaves/M/...`) har samme middle-wildcard problem og matcher aldrig. Notereret i BUGS.md som åben — kræver master dispatcher-handler.

---

## [0.3.0 build 0058] — 2026-05-30 — feat: master/slave + add/delete interface i web GUI

**Filer ændret:**
- `firmware/main/api/routes/interfaces.c` — `iface_to_json` udvidet med `mode`, `slave_addr`, `uart_mode`, `tx_pin`, `rx_pin`, `rts_pin`; PUT-handler accepterer `mode`, `slave_addr`, `type`, GPIO-pins; ny POST `/api/v1/interfaces` (opretter SW-UART master, defaults) og DELETE `/api/v1/interfaces/{id}` (sletter + renummererer)
- `firmware/main/api/routes/interfaces.h` — nye route-symboler eksporteret
- `firmware/main/api/server.c` — nye routes registreret; api_index opdateret
- `firmware/main/api/routes/mgmt.c` — RS485 Config-tab: Rolle-selector (Master/Slave) med dynamisk Slave-adr felt; Tilføj/Slet-knapper; interface-counter (N/8); save sender mode + slave_addr; toast minder om "reboot for at aktivere"
- `firmware/main/core/version.h` — 0.3.0 build 0058
- `version.json` — 0.3.0 build 0058

---

## [0.2.1 build 0057] — 2026-05-30 — fix: mgmt-side JavaScript syntax error

**Filer ændret:**
- `firmware/main/api/routes/mgmt.c` — interfaces-API-kald hængte udenfor en funktion + ekstra `}` brød hele scriptet → `SyntaxError` ved load → ingen API-kald → siden "frosset" på initiale "Indlæser..."-tekster. Fix: interface-summary flyttet ud i `loadIfcSummary()`, kaldes fra `loadStatus()`
- `firmware/main/core/version.h` — build 0057
- `version.json` — build 0057

---

## [0.2.1 build 0056] — 2026-05-30 — fix: W5500 ISR-miss workaround (port fra Modbus_server_slave_ESP32)

**Filer ændret:**
- `firmware/main/core/ethernet.c` — `w5500_int_poll_task()` baggrundstask: poller INT-pin hvert 2ms og sender `xTaskNotifyGive()` direkte til "w5500_tsk" når INT er LOW. Omgår ESP-IDF's edge-triggered GPIO ISR der misser frames når W5500 har multiple pakker i RX-bufferen
- `firmware/main/core/serial_cli.c` — CTX_ETH parser manglede `spi-clock` og `poll-ms` kommandoer (kun cmd_eth havde dem) → "Ukendt" fejl ved konfiguration
- `firmware/main/core/version.h` — build 0056
- `version.json` — build 0056

**Root cause:** ESP-IDF W5500 driver's RX-task ("w5500_tsk") venter på `ulTaskNotifyTake()` med 1000ms timeout. Når GPIO faldende-flanke-ISR misser (multi-frame queue holder INT LOW, ingen ny flanke), vågner tasken kun ved 1000ms timeout → 700-1300ms ping-latency med faldende mønster (88% pakketab).
**Fix:** Eksakt samme workaround som vi lavede i `Modbus_server_slave_ESP32` projektet — manuel INT-polling task der bypass'er GPIO ISR-laget. Forventet resultat: konsistent ~5ms ping.

---

## [0.2.1 build 0055] — 2026-05-30 — fix: W5500 init crash — interrupt og polling er mutuelt eksklusive

**Filer ændret:**
- `firmware/main/core/ethernet.c` — adskil interrupt-mode og polling-mode: enten `int_gpio_num >= 0` med `poll_period_ms=0`, eller `int_gpio_num=-1` med `poll_period_ms=cfg->spi_poll_ms`. ESP-IDF returnerer `invalid configuration argument combination` hvis begge sættes
- `firmware/main/core/version.h` — build 0055
- `version.json` — build 0055

**Root cause:** b0054 satte både `int_gpio_num=34` og `poll_period_ms=10` → `esp_eth_mac_new_w5500()` afviste konfigurationen ved boot → ingen Ethernet.

---

## [0.2.1 build 0054] — 2026-05-30 — fix: W5500 interrupt edge-problem + lwIP stack + poll-ms CLI

**Filer ændret:**
- `firmware/main/core/config.h` — `spi_poll_ms` (uint8_t) i `eth_config_t`; `CONFIG_STRUCT_VERSION` 7→8
- `firmware/main/core/config.c` — default `spi_poll_ms=10`; sanitize 1–100ms
- `firmware/main/core/ethernet.c` — `max_transfer_sz=4096` (eksplicit); `poll_period_ms` fra config; advarsel om pull-up krav ved INT-mode
- `firmware/main/core/serial_cli.c` — `poll-ms <1-100>` kommando; show ethernet + show config viser poll-ms; `?`-hjælp opdateret; note om manglende intern pull-up på GPIO 34–39
- `sdkconfig.defaults` — `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE=4096`; `CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=64`
- `firmware/main/core/version.h` — build 0054
- `version.json` — build 0054

**Root cause:** W5500 ESP-IDF driver bruger edge-triggered interrupt (falling edge). Når W5500 har 2+ frames i RX-bufferen behandles kun første frame, men INT-linjen holdes LOW — ingen ny faldende flanke → ISR fyrer ikke igen → frames 2..N sidder fast.
**Workaround:** polling-mode med 5ms interval: `int -1`, `poll-ms 5`, save, reboot → konsistent ~2.5ms latency.

---

## [0.2.1 build 0053] — 2026-05-30 — fix: W5500 hardware-pins som system-default

**Filer ændret:**
- `firmware/main/core/config.c` — default Ethernet: W5500 (CS=23 MOSI=13 MISO=12 SCLK=14 RST=33 INT=34 10MHz) i stedet for LAN8720
- `firmware/main/core/version.h` — build 0053
- `version.json` — build 0053

---

## [0.2.1 build 0052] — 2026-05-30 — fix: W5500 SPI clock + polling-advarsel + httpd lru_purge

**Filer ændret:**
- `firmware/main/core/config.h` — `spi_clock_mhz` (uint8_t) tilføjet til `eth_config_t`; `CONFIG_STRUCT_VERSION` 6→7
- `firmware/main/core/config.c` — default `spi_clock_mhz=10`; sanitize 1–36 MHz
- `firmware/main/core/ethernet.c` — SPI clock bruger `cfg->spi_clock_mhz` (var hardkodet 20 MHz); LOGW advarsel ved polling-mode (INT<0); forbedret init-log med MHz og INT-mode
- `firmware/main/api/server.c` — `lru_purge_enable=true` i httpd config
- `firmware/main/core/serial_cli.c` — `spi-clock <1-36>` kommando i ETH-kontekst; `int`-kommando viser advarsel ved -1; `show ethernet` viser SPI clock og polling-advarsel; `show config` viser `spi-clock`; inline `?`-hjælp opdateret
- `firmware/main/core/version.h` — 0.2.1 build 0052
- `version.json` — 0.2.1 build 0052

**Root cause:** W5500 SPI kørt på 20 MHz (for højt til prototype-ledninger → stille bitfejl → TCP-fejl) og INT pin ikke tilsluttet → polling-mode med ~10ms pakkelatency. Ping-tider på 100–800ms bekræftede problemet.

---

## [0.2.0 build 0051] — 2026-05-30 — feat: Modbus slave mode + dynamisk interface-tilføjelse/sletning

**Filer ændret:**
- `firmware/main/core/config.h` — `iface_mode_t` enum (MASTER/SLAVE); `mode` og `slave_addr` felter i `iface_config_t`; `CONFIG_STRUCT_VERSION` 5→6
- `firmware/main/core/config.c` — defaults `mode=MASTER, slave_addr=1`; sanitize validerer slave_addr (1–247) og mode
- `firmware/main/modbus/interface.h` — `SLAVE_*_COUNT` konstanter; slave register-lager (`slave_holding/input/coils/discrete`) i `mb_interface_t`
- `firmware/main/modbus/interface.c` — `#include esp_modbus_slave.h`; `init_hw_master()` og `init_hw_slave()` udtrukket; `mb_interface_init()` dispatcher på `cfg->mode`; slave sætter 4 register-areas og kalder `mbc_slave_set_descriptor()`; SW-UART slave returnerer `ESP_ERR_NOT_SUPPORTED`
- `firmware/main/core/serial_cli.c` — CTX_MODBUS: `mode master|slave` og `addr <1-247>` kommandoer; CTX_TOP: `interface modbus<N>` opretter nyt interface hvis N == interface_count; `no interface modbus<N>` sletter og renummererer; `show_running_config` viser Mode + Addr pr. interface; `cfg_help_modbus/top` opdateret
- `firmware/main/core/version.h` — 0.2.0 build 0051
- `version.json` — 0.2.0 build 0051

**Design:**
- esp-modbus master og slave er separate singletons (separate `mbc_master_*` / `mbc_slave_*` contexts) — der kan køre én master HW-UART og én slave HW-UART simultant
- Slave register-lager allokeres statisk i `mb_interface_t` (~580 bytes pr. interface)
- SW-UART slave er ikke implementeret (kræver custom bit-bang svar-logik)
- NVS invalideres ved opstart (CONFIG_STRUCT_VERSION mismatch) → defaults indlæses

---

## [0.1.0 build 0050] — 2026-05-30 — fix: /mgmt netværk viser Ethernet + WiFi separat

**Filer ændret:**
- `firmware/main/api/routes/mgmt.c` — `loadStatus()` omskrevet: Ethernet og WiFi vises som separate rækker; Ethernet-IP fra `/api/v1/system`; WiFi-detaljer kun vist ved `connected`; "Deaktiveret" badge er neutral (ingen farve)
- `firmware/main/core/version.h` — build 0050
- `version.json` — build 0050

**Problem:** Netværk-sektionen viste kun WiFi-tilstand. Ethernet-IP lå i System-sektionen. WiFi "Deaktiveret" var vildledende når Ethernet var oppe.

---

## [0.1.0 build 0049] — 2026-05-30 — fix: OTA check crash — stack overflow i httpd task

**Filer ændret:**
- `firmware/main/ota/ota_manager.c` — `char buf[4096]` ændret til `malloc(4096)` (fjerner 4KB fra stack)
- `firmware/main/api/server.c` — `hcfg.stack_size = 16384` (var 4096 default)
- `firmware/main/core/version.h` — build 0049
- `version.json` — build 0049

**Årsag:** `ota_check()` køres synkront i httpd-tasken. Stack-forbrug: 4KB response-buffer + ~1KB `ota_info_t` + ~8-12KB TLS-handshake mod GitHub = overflow → crash → boot. **Fix:** buffer på heap + httpd stack øget til 16KB.

---

## [0.1.0 build 0048] — 2026-05-30 — fix: /mgmt build-fejl — HTML embedded som C-streng

**Filer ændret:**
- `firmware/main/api/routes/mgmt.c` — HTML embedded som `static const char mgmt_html[]` (C-streng)
- `firmware/main/CMakeLists.txt` — `EMBED_TXTFILES` fjernet (PlatformIO kopierer ikke .html til build-dir)
- `firmware/main/core/version.h` — build 0048
- `version.json` — build 0048

**Problem:** PlatformIO kopierer kun `.c`/`.h`-filer til build-direktoriet. `EMBED_TXTFILES "mgmt_page.html"` kunne ikke finde filen og fejlede med `Source not found`. **Fix:** HTML embedded direkte i `mgmt.c` som statisk C-streng — ingen ekstern fil nødvendig.

---

## [0.1.0 build 0047] — 2026-05-30 — feat: /mgmt web management side

**Filer ændret:**
- `firmware/main/mgmt_page.html` — selvstændig management HTML (embedded i firmware)
- `firmware/main/api/routes/mgmt.c` + `mgmt.h` — `GET /mgmt` route
- `firmware/main/api/server.c` — route_get_mgmt registreret; `#include "routes/mgmt.h"`
- `firmware/main/api/routes/system.c` — `build`-felt tilføjet til GET /api/v1/system response
- `firmware/main/CMakeLists.txt` — `EMBED_TXTFILES "mgmt_page.html"` + `mgmt.c` i SRCS
- `firmware/main/core/version.h` — build 0047
- `version.json` — build 0047

**Ny endpoint:** `GET /mgmt` — leverer embedded HTML management side med:
- **Status**: system (version, build, uptime, heap, IP), netværk (WiFi tilstand/SSID/IP/RSSI), Modbus interfaces overblik
- **OTA**: tjek GitHub, viser installeret vs tilgængelig version, install-knap med progress bar
- **RS485 Config**: per-interface kort med baudrate/paritet/stop bits/timeout/enabled toggle og gem

HTML er embedded i firmware (EMBED_TXTFILES) — ingen SPIFFS nødvendig for /mgmt.

---

## [0.1.0 build 0046] — 2026-05-30 — feat: W5500 SPI Ethernet driver implementeret

**Filer ændret:**
- `firmware/main/core/ethernet.c` — omskrevet: `init_lan8720()` + `init_w5500()` separeret; dispatcher på `cfg->hw_type`; RST-puls, SPI2 bus init, W5500 MAC+PHY oprettelse, fejlhåndtering
- `sdkconfig.defaults` — `CONFIG_ETH_USE_SPI_ETHERNET=y` og `CONFIG_ETH_SPI_ETHERNET_W5500=y` tilføjet
- `sdkconfig.esp32dev` — `CONFIG_ETH_SPI_ETHERNET_W5500=y` aktiveret
- `firmware/main/core/version.h` — build 0046
- `version.json` — build 0046

**Fejlårsag:** `ethernet.c` var hardcodet til LAN8720 RMII og ignorerede `cfg->hw_type`. W5500 GPIO-konfiguration fra config blev aldrig brugt. `CONFIG_ETH_SPI_ETHERNET_W5500` var ikke aktiveret i sdkconfig.

**Implementering:** SPI2 (HSPI) bus + W5500 MAC (`esp_eth_mac_new_w5500`) + PHY (`esp_eth_phy_new_w5500`). Hardware RST-puls på konfigurerbar GPIO. INT-pin (-1 = polling). 20 MHz SPI clock.

---

## [0.1.0 build 0045] — 2026-05-30 — fix: build-fejl stdbool + show config paste-kompatibilitet

**Filer ændret:**
- `firmware/main/core/ethernet.h` — `#include <stdbool.h>` tilføjet (fix: unknown type 'bool')
- `firmware/main/core/serial_cli.c` — `show_running_config()`: fjernet `mode STA` (ingen CLI-kommando); AP PSK vises kun hvis sat, ellers `! PSK (ingen — åbent netværk)`
- `firmware/main/core/version.h` — build 0045
- `version.json` — build 0045

**Problem:** `ethernet.h` brugte `bool` uden `stdbool.h` → compile-fejl. `show config` printede `mode STA` og `PSK none (åben)` som ikke er gyldige CLI-kommandoer.

---

## [0.1.0 build 0044] — 2026-05-30 — feat: show ethernet kommando med live status og GPIO-detaljer

**Filer ændret:**
- `firmware/main/core/ethernet.h` — `ethernet_is_available()` tilføjet
- `firmware/main/core/ethernet.c` — `ethernet_is_available()` implementeret
- `firmware/main/core/serial_cli.c` — `show_eth_detail()` + `show ethernet` / `show eth` i `cmd_show()`
- `firmware/main/core/version.h` — build 0044
- `version.json` — build 0044

**Ny kommando:** `show ethernet` (alias `show eth`) viser:
- Live tilstand: forbundet / afventer IP / ikke tilgængeligt
- IP, IP-mode (dhcp/statisk), gateway, netmask
- Hardware-type: LAN8720 eller W5500
- GPIO pins for valgt hardware (PHY addr/MDC/MDIO/PHY-RST for LAN8720; CS/MOSI/MISO/SCLK/RST/INT for W5500)

---

## [0.1.0 build 0043] — 2026-05-30 — feat: W5500 RST GPIO pin i config, CLI og show config

**Filer ændret:**
- `firmware/main/core/config.h` — `spi_rst_gpio` tilføjet til `eth_config_t`; CONFIG_STRUCT_VERSION → 5
- `firmware/main/core/config.c` — default `spi_rst_gpio = -1`
- `firmware/main/core/serial_cli.c` — `rst <gpio>` kommando i CTX_ETH (W5500); hjælp og show config opdateret
- `firmware/main/core/version.h` — build 0043
- `version.json` — build 0043

**Ændring:** W5500 hardware-reset pin kan nu konfigureres via `rst <gpio|-1>` i `interface ethernet`-konteksten. Default -1 (ikke tilsluttet). CONFIG_STRUCT_VERSION bumped → NVS config nulstilles ved næste boot.

---

## [0.1.0 build 0042] — 2026-05-30 — fix: show config Ethernet GPIO-linjer matcher CLI-kommandoer

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `show_running_config()`: GPIO-linjer for LAN8720/W5500 ændret til CLI-format
- `firmware/main/core/version.h` — build 0042
- `version.json` — build 0042

**Problem:** `show config` printede `SPI-CS   GPIO 23` og `MDC      GPIO 23`. CLI-kommandoerne er `cs 23` og `mdc 23`. Copy-paste fra show config gav `atoi("GPIO") = 0`.

**Fix:** Output er nu identisk med CLI-kommandoerne — `cs`, `mosi`, `miso`, `sclk`, `int`, `mdc`, `mdio`, `phy-addr`, `phy-rst` uden "GPIO"-præfix og "SPI-"-præfix.

---

## [0.1.0 build 0041] — 2026-05-30 — feat: show status/version/wifi kommandoer

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `show_status()`, `show_version()`, `show_wifi_detail()` + omskrevet `cmd_show()` + `cmd_status()`
- `firmware/main/core/version.h` — build 0041
- `version.json` — build 0041

**Nye CLI-kommandoer:**
- `show status` — system (version, uptime d/h/m/s, heap), netværk (Eth+WiFi+RSSI), API-server, Modbus-interfaces
- `show version` — firmware v/build, ESP-IDF version, chip-model/revision/cores, flash-type
- `show wifi` — detaljeret WiFi (tilstand, mode, MAC, SSID, IP, RSSI, kanal, auth, BSSID)
- `show config` — uændret (IOS-stil konfiguration)
- `status` — alias for `show status`
- `wifi status` — alias for `show wifi`

---

## [0.1.0 build 0040] — 2026-05-30 — feat: API server config + CLI interface api + auth

**Filer ændret:**
- `firmware/main/core/config.h` — ny `api_config_t` struct; `CONFIG_STRUCT_VERSION` 3→4
- `firmware/main/core/config.c` — defaults (enabled, port 80, auth off) + sanitize
- `firmware/main/api/server.h` — opdateret signatur + `api_auth_ok()` declaration
- `firmware/main/api/server.c` — bruger port fra config, enabled-check, auth helper
- `firmware/main/main.c` — sender `&cfg.api` til `api_server_start()`
- `firmware/main/core/serial_cli.c` — `CTX_API` + `cfg_help_api()` + show config + prompt
- `firmware/main/core/version.h` — build 0040
- `version.json` — build 0040

**NB: CONFIG_STRUCT_VERSION bumped til 4** — eksisterende NVS-config slettes ved første boot. Rekonfigurér WiFi, Ethernet mv. efter flash.

**Ny CLI-sektion:**
```
gw(config)# interface api
gw(config-api)# enable
gw(config-api)# port 80
gw(config-api)# auth on
gw(config-api)# key MinHemligeNøgle
gw(config-api)# exit
```
Auth bruger `X-API-Key: <nøgle>` header. Deaktiveret som standard.

---

## [0.1.0 build 0039] — 2026-05-30 — fix: status viser WiFi IP og tilstand

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `cmd_status()`: tilføjet WiFi tilstand og IP
- `firmware/main/core/version.h` — build 0039
- `version.json` — build 0039

**Ændring:** `status`-kommandoen viste kun Ethernet-IP. Nu vises også WiFi-tilstand og IP når WiFi er forbundet.

---

## [0.1.0 build 0038] — 2026-05-30 — fix: WiFi DHCP kører ikke efter connect (korrupt IP-felt i NVS)

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — IP-validering + eksplicit DHCP-restart ved STA_CONNECTED
- `firmware/main/core/version.h` — build 0038
- `version.json` — build 0038

**Rodårsag:** NVS-korruption (pre-b0030 dangling pointer) kunne sætte `wifi.ip`-feltet til garbage — ikke "dhcp", ikke tom streng. `wifi_manager_init()` ramte da static-IP-branchen og kaldte `esp_netif_dhcpc_stop()`. WiFi associerede korrekt (PSK OK), men DHCP-klienten kørte ikke → klient ses på WLC men får aldrig en IP-adresse.

**Fix 1 — IP-validering:** `ip4addr_aton()` bruges til at verificere at `ip`-feltet er en gyldig IPv4 inden DHCP stoppes. Ugyldig/korrupt værdi logger en advarsel og falder tilbage til DHCP.

**Fix 2 — Eksplicit DHCP-restart ved `WIFI_EVENT_STA_CONNECTED`:** Selv hvis DHCP-tilstanden er uklar fra tidligere init-runs, genstartes DHCP-klienten eksplicit efter 4-way handshake — inden `IP_EVENT_STA_GOT_IP` kan udsendes.

---

## [0.1.0 build 0037] — 2026-05-30 — fix: show config viser PSK i clear text

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `show_running_config()`: PSK vises i klartekst (WiFi STA + AP)
- `firmware/main/core/version.h` — build 0037
- `version.json` — build 0037

**Ændring:** `show config` viste tidligere `*** (sat)` for WiFi-passwords. Nu vises den faktiske PSK-værdi fra NVS, så man kan verificere at korrekt password er gemt.

---

## [0.1.0 build 0036] — 2026-05-30 — feat: debug/no debug kommandoer — runtime log-niveau styring

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `cmd_debug()` og `cmd_no()` tilføjet + registreret
- `firmware/main/core/version.h` — build 0036
- `version.json` — build 0036

**Nye kommandoer:**
- `debug` — sætter alle komponenter til VERBOSE
- `debug wifi` — kun WiFi-driver + wifi_mgr → VERBOSE
- `debug <tag>` — specifik ESP-IDF komponent → VERBOSE
- `no debug` — alle komponenter → WARN (stille tilstand)
- `no debug wifi` — WiFi-relaterede komponenter → WARN
- `no debug <tag>` — specifik komponent → WARN
- `debug ?` / `no debug ?` — inline hjælp

Bruger `esp_log_level_set()` til at justere log-niveau pr. komponent-tag uden genstart.

---

## [0.1.0 build 0035] — 2026-05-29 — fix: WiFi 30s backoff efter max retries — undgår log-spam

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — `esp_timer` backoff: 30s pause efter 5 fejlede forsøg
- `firmware/main/core/version.h` — build 0035
- `version.json` — build 0035

**Problem:** WiFi retry-loop kørte hvert ~3. sekund og flooded terminalen med log-linjer, umuliggjorde CLI-brug. **Fix:** `retry_schedule()` starter en 30-sekunders `esp_timer` efter de første 5 retries. Ét loglinje hvert 30. sekund i stedet for hvert 3. sekund. `retry_cancel()` annullerer timeren ved forbundet/stop/reconfigure. WIFI_EVENT_STA_START nulstiller counter og annullerer timer.

---

## [0.1.0 build 0034] — 2026-05-29 — fix: WiFi threshold WPA_PSK → WPA2_PSK (WLC CCMP-krav)

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — threshold tilbage til `WIFI_AUTH_WPA2_PSK`
- `firmware/main/core/version.h` — build 0034
- `version.json` — build 0034

**Årsag:** `WIFI_AUTH_WPA_PSK` (indført i b0031) fik ESP32 til at inkludere WPA1/TKIP i association request. Enterprise WLC'er der kun accepterer CCMP kan afvise dette under 4-way handshake → `4WAY_HANDSHAKE_TIMEOUT (reason 15)`. `WPA2_PSK` threshold sikrer at ESP32 udelukkende annoncerer WPA2/CCMP.

---

## [0.1.0 build 0033] — 2026-05-29 — fix: WiFi PMF fjernet — SA_QUERY_TIMEOUT på enterprise WLC

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — `pmf_cfg.capable = true` fjernet
- `firmware/main/core/version.h` — build 0033
- `version.json` — build 0033

**Rodårsag:** b0031 tilføjede `pmf_cfg.capable = true` som fortæller AP'en at ESP32 understøtter PMF (Protected Management Frames). Enterprise WLC'er starter SA-query mekanisme ved PMF-capable klienter. ESP32 timer ud på disse SA queries → `WIFI_REASON_SA_QUERY_TIMEOUT (reason 205)` og `WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT (reason 15)`. WLC ser enheden som associated (assoc lykkedes) men WPA2-handshaken fejler i `run`-tilstanden. DHCP når aldrig at køre.

**Fix:** `pmf_cfg` ikke sat (defaults `{0,0}` = ikke capable, ikke required). WLC laver ikke SA queries mod non-PMF klienter.

---

## [0.1.0 build 0032] — 2026-05-29 — diagnose: WiFi disconnect reason + factory-reset kommando

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — disconnect-log viser nu reason-kode (reason 15/204 = forkert PSK)
- `firmware/main/core/serial_cli.c` — ny `factory-reset` kommando (sletter NVS + reboot)
- `firmware/main/core/version.h` — build 0032
- `version.json` — build 0032

**Baggrund:** WiFi HANDSHAKE_TIMEOUT (reason 204) opstod fordi NVS indeholdt garbage-password fra pre-b0030-perioden (dangling pointer korrupterede `save`). Reason-koden er nu synlig i loggen for fremtidig diagnose. `factory-reset` giver nem recovery ved korrupt NVS.

---

## [0.1.0 build 0031] — 2026-05-29 — fix: WiFi STA — retry, auth threshold, double-init, quotes

**Filer ændret:**
- `firmware/main/core/wifi_manager.c` — 5 rettelser (se nedenfor)
- `firmware/main/core/serial_cli.c` — cfg_tokenize: quote-support til SSID/PSK med mellemrum
- `firmware/main/core/version.h` — build 0031
- `version.json` — build 0031

**Rettelser i wifi_manager.c:**
1. **Threshold.authmode**: `WIFI_AUTH_WPA2_PSK` → `WIFI_AUTH_WPA_PSK` — accepterer nu WPA og stærkere. WPA2_PSK afviste WPA-only AP'er og visse WPA2/WPA3 transition-modes.
2. **Infinite retry**: `esp_wifi_connect()` kaldes nu altid ved disconnect — gateway giver ikke op efter 5 forsøg. FAIL_BIT sættes stadig (for AP-fallback trigger), men STA fortsætter med at forsøge forbindelsen.
3. **Double-init guard**: `s_initialized` flag forhindrer at `esp_netif_create_default_wifi_sta/ap` + `esp_wifi_init` + event handler registrering kaldes to gange ved `wifi_manager_reconfigure()`.
4. **FAIL_BIT ryddes**: `xEventGroupClearBits(FAIL_BIT)` tilføjet ved succesfuld forbindelse — korrekt state ved reconnect efter AP-fallback.
5. **start_ap_fallback**: tilføjet `esp_wifi_stop() + esp_wifi_start()` — påkrævet for korrekt APSTA-mode aktivering i ESP-IDF v5.x.

**Rettelse i serial_cli.c:**
- `cfg_tokenize`: understøtter nu `"quoted strings"` — SSID og PSK med mellemrum gemmes korrekt i configure-mode.

---

## [0.1.0 build 0030] — 2026-05-29 — fix: dangling pointer — cfg static i app_main

**Filer ændret:**
- `firmware/main/main.c` — `gateway_config_t cfg` gjort `static` så det lever i BSS, ikke på app_main's stack
- `firmware/main/core/version.h` — build 0030
- `version.json` — build 0030

**Rodårsag fikset:** `app_main` returnerer efter at have startet alle tasks. FreeRTOS sletter main-tasken og frigiver dens stack. `s_cfg` i `serial_cli.c` pegede på stack-allokeret `cfg` og blev dangling pointer. Al efterfølgende gem/læs via `s_cfg` arbejdede på frigjort hukommelse → data-korruption. `static` placerer `cfg` i BSS-segmentet (levetid = hele programmets kørsel).

---

## [0.1.0 build 0029] — 2026-05-29 — ETH GPIO type-gates: kun relevante pins vises og accepteres

**Filer ændret:**
- `firmware/main/core/config.h` — `ETH_HW_NONE` tilføjet til `eth_hw_t` enum
- `firmware/main/core/serial_cli.c` — `eth_print_help(hw_type)` fælles funktion. `show config` og `?`-hjælp viser kun GPIO-pins for valgt type. Configure mode afviser LAN8720-pins ved W5500 og omvendt. `eth type none` understøttet.
- `firmware/main/core/version.h` — build 0029
- `version.json` — build 0029

---

## [0.1.0 build 0028] — 2026-05-29 — fix: NVS struct-version + WiFi SSID altid vist

**Filer ændret:**
- `firmware/main/core/config.h` — `CONFIG_STRUCT_VERSION 3` + `uint32_t version` felt i `gateway_config_t`
- `firmware/main/core/config.c` — `config_set_defaults` sætter `cfg->version = CONFIG_STRUCT_VERSION`
- `firmware/main/storage/config_store.c` — load checker `cfg->version != CONFIG_STRUCT_VERSION` → defaults
- `firmware/main/core/serial_cli.c` — `show config` WIFI-blok viser altid SSID og PSK (med fallback-tekst). PSK vises aldrig i klartekst.
- `firmware/main/core/version.h` — build 0028
- `version.json` — build 0028

**Rodårsag fikset:** Struct-layout ændring (b0026: eth_config_t voksede) forskydte wifi-felternes offset i RAM. Gammelt NVS-blob med anden layout-version nulstilles nu automatisk til defaults ved boot.

**NVS-note:** Første boot efter flash nulstiller NVS til defaults — rekonfigurér og `save`.

---

## [0.1.0 build 0027] — 2026-05-29 — CLI: configure terminal + kontekst-sensitiv ?-hjælp

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — tilføjet `configure`/`conf` kommando med nested linenoise REPL og skiftende prompt. `?` kommando på alle niveauer. `eth ?` og `wifi ?` subkommando-hjælp. Stack 5120→6144. `<stdlib.h>` og `<ctype.h>` inkluderet.
- `firmware/main/core/version.h` — build 0027
- `version.json` — build 0027

**Nye kommandoer:**
- `configure terminal` / `conf t` — enter config mode
- `?` — vis alle kommandoer (alias for help)
- `eth ?` / `eth type ?` / `eth ip ?` osv. — kontekst-sensitiv hjælp
- `wifi ?` / `wifi ssid ?` / `wifi ip ?` osv. — kontekst-sensitiv hjælp

**Configure mode prompts:**
- `gw(config)#` → `interface eth0|wifi|wifi-ap|modbus<N>`
- `gw(config-eth0)#` → type, enable/disable, ip, mdc, mdio, ...
- `gw(config-wifi)#` → enable/disable, ssid, psk, ip
- `gw(config-wifi-ap)#` → enable/disable, ssid, psk
- `gw(config-modbus0)#` → type, uart, baudrate, format, timeout, tx/rx/de

---

## [0.1.0 build 0026] — 2026-05-29 — ETH config: Enable/Disable, Type, GPIO pins

**Filer ændret:**
- `firmware/main/core/config.h` — `eth_config_t` udvidet: `enabled`, `hw_type` (ETH_HW_LAN8720/W5500), `phy_addr`, `mdc_gpio`, `mdio_gpio`, `phy_rst_gpio`, SPI-pins (cs/mosi/miso/sclk/int)
- `firmware/main/core/config.c` — defaults: LAN8720, enabled=1, mdc=23, mdio=18, phy_addr=0, alle SPI=-1
- `firmware/main/core/serial_cli.c` — `show config` ETH0 blok viser Enable/Disable, Type og alle GPIO-pins. `cmd_eth` udvidet med enable/disable/type/phy-addr/mdc/mdio/phy-rst/cs/mosi/miso/sclk/int subkommandoer
- `firmware/main/core/version.h` — build 0026
- `version.json` — build 0026

**NVS bemærkning:** struct-ændring nulstiller gemt config til defaults ved næste boot (forventet adfærd).

---

## [0.1.0 build 0025] — 2026-05-29 — CLI show config: IOS-stil running config

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `show config` omskrevet til Cisco IOS-stil blokformat. `show_running_config()` udskriver Interface-blokke for ETH0, WIFI, WIFI-AP og alle Modbus-interfaces.
- `firmware/main/core/version.h` — build 0025
- `version.json` — build 0025

---

## [0.1.0 build 0024] — 2026-05-29 — CLI show: komplet konfigurationsvisning

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — `cmd_show()` omskrevet: viser nu alle felter fra `gateway_config_t`
- `firmware/main/core/version.h` — build 0024
- `version.json` — build 0024

**Viser nu:**
- ETHERNET: ip, gateway, netmask
- WIFI STA: aktiv, ssid, password (maskeret), ip, gateway, netmask
- WIFI AP FALLBACK: aktiv, ap-ssid, ap-password (maskeret)
- MODBUS INTERFACES: type, HW/SW, uart_num, baudrate, data_bits+stop_bits+parity, timeout_ms, pins (TX/RX/DE), status

---

## [0.1.0 build 0023] — 2026-05-29 — API index endpoint: /api og /api/v1 returnerer endpoint-liste

**Filer ændret:**
- `firmware/main/api/server.c` — tilføjet `api_index_handler` og `route_api_index` (`/api*` wildcard, registreret sidst). Inkluderer nu `version.h` og `cJSON.h`.
- `firmware/main/core/version.h` — build 0023
- `version.json` — build 0023

**Resultat:**
- `GET /api` og `GET /api/v1/` returnerer JSON med alle 21 endpoints
- `/api*` catch-all matcher kun hvis ingen specifik route matcher (registreret sidst)
- Eksisterende specifikke routes upåvirket

---

## [0.1.0 build 0022] — 2026-05-29 — CLI: wifi status + wifi mode kommandoer

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — tilføjet `wifi status` og `wifi mode` subkommandoer. Inkluderer nu `wifi_manager.h` og `esp_wifi.h`.
- `firmware/main/core/version.h` — build 0022
- `version.json` — build 0022

**`wifi status` viser:**
- Live tilstand (deaktiveret / forbinder / forbundet / AP hotspot / fejl)
- WiFi mode (klient STA / AP / APSTA)
- MAC-adresse (STA)
- Ved forbundet: SSID, IP, RSSI (dBm), kanal, auth-type, BSSID
- Ved AP fallback: AP SSID, AP IP (192.168.4.1), AP MAC

**`wifi mode` viser:**
- Kort status: klient (STA) / AP hotspot / deaktiveret
- Viser forbundet SSID hvis STA er connected
- Viser AP-info hvis APSTA-mode (fallback kører)

---

## [0.1.0 build 0021] — 2026-05-29 — CLI: kommandohistorik + cursor-bevægelse (←→)

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — fjernet `linenoiseSetDumbMode(1)`. ANSI-mode aktiveret: pile-taster virker nu fuldt ud. Historik (op/ned ←→): navigér tidligere kommandoer. Cursor-bevægelse (←→): flyt inden i kommandolinjen. ESC[6n cursor-probe sendes KUN i multi-line mode; vi bruger single-line (default) → ingen probe-spam.
- `firmware/main/core/version.h` — build 0021
- `version.json` — build 0021

**Resultat:**
- ↑/↓ pile: navigér kommandohistorik (op til 20 kommandoer)
- ←/→ pile: flyt cursor inden i aktuel kommando
- Home/End: hop til start/slut af linje
- Ctrl+A / Ctrl+E: start/slut (Emacs-style)
- Ctrl+W: slet ord bagud
- Ctrl+K: slet til linjeslut

---

## [0.1.0 build 0020] — 2026-05-29 — sdkconfig: core dump deaktiveret

**Filer ændret:**
- `sdkconfig.defaults` — tilføjet `CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y`: deaktiverer core dump (ingen coredump-partition i partitionstabellen). Reducerer espcoredump overhead.
- `firmware/main/core/version.h` — build 0020
- `version.json` — build 0020

**Resultater:**
- Flash: 69.0% (1084607 bytes) — uændret fra b0019 (coredump-stubs var allerede små)
- Note: COMPONENTS-filtrering (til at ekskludere mqtt, fatfs, wifi_provisioning mm.) er ikke kompatibel med PlatformIO's ESP-IDF EXTRA_COMPONENT_DIRS-integration. main-komponenten ekskluderes fejlagtigt. Dokumenteret i FEATURES.md som known limitation.

---

## [0.1.0 build 0019] — 2026-05-29 — optimering: build-tid + flash-størrelse

**Filer ændret:**
- `firmware/main/core/version.h` — NY FIL: eneste kilde til GATEWAY_VERSION/GATEWAY_BUILD. Ændringer her recompilerer kun 4 filer i stedet for alle 13 som inkluderer config.h → version-bumps går fra ~4 min til ~30-60 sek
- `firmware/main/core/config.h` — fjernet GATEWAY_VERSION og GATEWAY_BUILD (moved til version.h)
- `firmware/main/main.c` — tilføjet `#include "version.h"`
- `firmware/main/core/serial_cli.c` — tilføjet `#include "version.h"`
- `firmware/main/api/routes/system.c` — tilføjet `#include "version.h"`
- `firmware/main/ota/ota_manager.c` — tilføjet `#include "version.h"`
- `sdkconfig.defaults` — deaktiveret yderligere ubrugte komponenter:
  - mbedTLS: TLS 1.0/1.1 (GitHub kræver 1.2+), SECP192R1/SECP224R1/SECP256K1 kurver, CCM, PKCS12, DHE-PSK
  - lwIP: SLIP protokol
  - SPI Flash: aktiveret Boya chip (fixer boot-advarsel), deaktiveret ISSI/MXIC/TH/XMC
- `version.json` — build 0019

**Resultater:**
- Flash: 77.5% (1219KB) → 69.0% (1085KB) — **134KB sparet**
- RAM: 11.2% (36.8KB) → 10.5% (34.4KB) — 2.4KB sparet
- Boot: `spi_flash: detected chip: boya` — flash-chip korrekt genkendt, ingen advarsel
- Næste version-bump: kun ~30 sek build (4 filer) i stedet for ~4 min (13+ filer)

---

## [0.1.0 build 0018] — 2026-05-29 — fix: dobbelt CLI-prompt ved Enter

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — efter `esp_console_new_repl_uart()`: tilføjet `uart_vfs_dev_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CRLF)`. Standardindstillingen CR-mode oversætter `\r`→`\n`, men Windows-terminaler sender `\r\n` som giver to `\n` — ét afslutter kommandoen, ét printer prompten ekstra. CRLF-mode konsumerer `\r\n` som ét `\n`.
- `platformio.ini` — tilføjet `monitor_eol = CRLF` (gør det eksplicit hvad terminalen sender)
- `firmware/main/core/config.h` — GATEWAY_BUILD "0018"
- `version.json` — build 0018

**Resultat:** Prompten `gw>` vises kun én gang efter hvert Enter. Testet med status, show, help, wifi, eth — alle korrekte.

---

## [0.1.0 build 0017] — 2026-05-28 — fix: config sanitization + Modbus init non-fatal

**Filer ændret:**
- `firmware/main/core/config.c` — tilføjet `config_sanitize()`: retter ugyldige uart_num-værdier (< 0 eller > 2) til 1 ved NVS-load; retter baudrate=0 og timeout_ms=0
- `firmware/main/core/config.h` — tilføjet `config_sanitize()` prototype; GATEWAY_BUILD "0017"
- `firmware/main/storage/config_store.c` — kalder `config_sanitize(cfg)` efter succesfuld NVS-load; fjernet separat interface_count-check (håndteres nu i config_sanitize)
- `firmware/main/modbus/interface.c` — alle 3 `ESP_ERROR_CHECK(mbc_master_*)` erstattet med proper error returns; uart_num-validering før brug; mbc_master_destroy() ved cleanup efter fejl
- `version.json` — build 0017

**Resultat:** Gateway booter stabilt efter save+reboot. Modbus UART1 initialiseres korrekt. Ingen panic ved invalid NVS-data.

---

## [0.1.0 build 0016] — 2026-05-28 — fix: mb_interface_init non-fatal + COM8 port

**Filer ændret:**
- `firmware/main/modbus/interface.c` — alle `ESP_ERROR_CHECK(mbc_master_init/setup/start)` erstattet med error-returns; uart_num valideres mod UART_NUM_MAX; mbc_master_destroy() cleanup ved fejl
- `firmware/main/storage/config_store.c` — `ESP_ERROR_CHECK(nvs_open)` erstattet med graceful fallback til defaults; interface_count bounds check
- `firmware/main/core/config.h` — GATEWAY_BUILD "0016"
- `platformio.ini` — upload_port og monitor_port sat til COM8
- `version.json` — build 0016

**Resultat:** Boot-loop fjernet (ingen panic). Gateway kører videre selv med ugyldig NVS-config.

---

## [0.1.0 build 0015] — 2026-05-28 — fix: boot-loop efter save+reboot

**Filer ændret:**
- `firmware/main/main.c` — nvs_flash_init: eraser NVS ved NO_FREE_PAGES/NEW_VERSION_FOUND i stedet for panic; modbus_manager_init + api_server_start: LOGW/LOGE i stedet for ESP_ERROR_CHECK; version + build i boot-log
- `firmware/main/modbus/interface.c` — uart_set_mode flyttes til EFTER mbc_master_start (UART-driver skal installeres først)
- `firmware/main/core/ethernet.c` — esp_event_loop_create_default: håndterer ESP_ERR_INVALID_STATE (allerede oprettet) gracefully
- `firmware/main/core/config.h` — GATEWAY_BUILD "0015"

**Resultat:** Gateway booter stabilt efter save+reboot. Ingen panic ved Modbus/API/NVS fejl.

---

## [0.1.0 build 0014] — 2026-05-27 — fix: Serial CLI "gw>" prompt spam

**Rod-årsag:** `esp_console_init()` installerer IKKE UART-driveren i ESP-IDF v5.x. stdin kørte i polling-mode: `read()` returnerede 0 bytes straks → `linenoiseDumb()` returnerede tom streng `""` → ingen delay i cli_task → tight loop med hundredvis af "gw>" prompts/sek.

**Filer ændret:**
- `firmware/main/core/serial_cli.c` — erstattet `esp_console_init()` + custom `cli_task` med `esp_console_new_repl_uart()` + `esp_console_start_repl()`. Den nye API installerer UART-driveren og konfigurerer VFS til blokerende læsning. Ingen custom task mere.
- `firmware/main/core/config.h` — GATEWAY_BUILD "0014"
- `version.json` — build 0014

**Resultat:** CLI blokerer korrekt på brugerinput, ingen prompt-spam.

---

## [0.1.0 build 0013] — 2026-05-27 — fix: Ethernet PHY-fejl ikke-fatal + version/build i boot display

**Filer ændret:**
- `firmware/main/core/ethernet.c` — `esp_eth_driver_install` er nu ikke-fatal; ved PHY-fejl logges advarsel og gateway kører videre på WiFi; tilføjet `s_eth_available` flag; `ethernet_wait_for_ip` returnerer straks hvis Ethernet fejlede
- `firmware/main/main.c` — `ESP_ERROR_CHECK(ethernet_init(...))` erstattet med graceful fejlhåndtering
- `firmware/main/core/config.h` — tilføjet `GATEWAY_BUILD "0013"` define
- `firmware/main/core/serial_cli.c` — boot display og `status`-kommando viser nu `v0.1.0 b0013`; `status` viser "ikke tilgængeligt" for Ethernet hvis ingen IP

**Resultat:** Gateway booter og er fuldt funktionel uden Ethernet PHY tilsluttet.

---

## [0.1.0 build 0012] — 2026-05-27 — Build-tid optimeret (Bluetooth + mbedTLS + IPv6 deaktiveret)

**Filer ændret:**
- `sdkconfig.defaults` — deaktiveret: Bluetooth/BLE (`CONFIG_BT_ENABLED=n`), TLS-server (`CONFIG_MBEDTLS_TLS_SERVER=n`), IPv6 (`CONFIG_LWIP_IPV6=n`), ubrugte mbedTLS cipher suites; tilføjet `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`

**Resultat:** Build-tid reduceret fra ~5+ min til ~4.3 min. Flash-forbrug: 77.2% (1213 KB / 1536 KB).

---

## [0.1.0 build 0009] — 2026-05-27 — Kompileringsfejl rettet (ESP-IDF v5.5 + PlatformIO)

**Filer ændret:**
- `firmware/main/core/config.h` — tilføjet `config_set_defaults()` prototype (implicit declaration fejl)
- `firmware/main/core/ethernet.c` — ESP-IDF v5.x API: `eth_esp32_emac_config_t` + `ETH_ESP32_EMAC_DEFAULT_CONFIG()`, tilføjet `esp_eth_mac_esp.h` + `esp_eth_phy.h` includes
- `firmware/main/modbus/interface.c` — `mbcontroller.h` → `esp_modbus_common.h` + `esp_modbus_master.h`; tilføjet lokale `MB_FUNC_*` defines (esp-modbus v1.x eksporterer ikke `mb_functioncode_t`)
- `firmware/main/api/routes/system.c` — tilføjet `#include "esp_timer.h"` (implicit declaration af `esp_timer_get_time`)
- `firmware/main/core/wifi_manager.c` — `lwip/inet.h` → `lwip/ip4_addr.h`; `inet_pton`/`AF_INET` → `ip4addr_aton` (lwIP native API)

**Resultat:** `[SUCCESS]` — rent build uden fejl.

---

## [0.1.0 build 0008] — 2026-05-27 — PlatformIO support

**Filer tilføjet:**
- `platformio.ini` — PlatformIO konfiguration: ESP32, ESP-IDF framework, custom partitions, SPIFFS upload
- `sdkconfig.defaults` — ESP-IDF Kconfig defaults (HTTP WS, TLS bundle, Ethernet RMII, WiFi, logging)
- `firmware/main/idf_component.yml` — komponent-afhængighed: `espressif/esp-modbus ^1.0` (erstatter `freemodbus`)
- `firmware/main/storage/register_cache.h/.c` — stub til register-cache (var i CMakeLists.txt men manglede)
- `firmware/main/service/gateway_service.h/.c` — stub til service-lag (var i CMakeLists.txt men manglede)

**Filer ændret:**
- `firmware/main/CMakeLists.txt` — `freemodbus` → `esp-modbus` (ESP-IDF v5.x navngivning)

**PlatformIO workflow:**
- `pio run` — byg firmware
- `pio run -t upload` — flash firmware
- `pio run -t uploadfs` — upload frontend (SPIFFS)
- `pio device monitor` — serial monitor

---

## [0.1.0 build 0007] — 2026-05-25 — Fix: WiFi statisk IP ignoreret

**Bug fix:**
- `firmware/main/core/wifi_manager.c` — `wifi_manager_init()` brugte altid DHCP, selv når `cfg->ip` var sat til en statisk IP-adresse. Nu stoppes DHCP-klienten og statisk IP/GW/netmask konfigureres via `esp_netif_set_ip_info()` når `ip != "dhcp"`.
- Tilføjet `#include "lwip/inet.h"` for `inet_pton()`.

**Opførsel:**
- `ip = "dhcp"` (eller tomt) → DHCP-klient aktiv (uændret default)
- `ip = "192.168.1.50"` + `gw` + `netmask` → statisk IP, ingen DHCP

---

## [0.1.0 build 0006] — 2026-05-25 — CLI-værktøj (mbgw)

**Filer tilføjet:**
- `cli/mbgw.py` — Python CLI med kommandogrupper: config, status, reboot, wifi, iface, read, write, ota
- `cli/requirements.txt` — click>=8.0, requests>=2.28
- `cli/setup.py` — installérbar som `mbgw` kommando via `pip install -e .`

**Kommandooversigt:**
- `mbgw config set host <IP>` — gem standardgateway IP
- `mbgw status` — version, uptime, heap, IP
- `mbgw reboot` — genstart gateway
- `mbgw wifi status/scan/set/disable` — WiFi-konfiguration og scanning
- `mbgw iface list/show/set` — Modbus interface-konfiguration
- `mbgw read holding/input/coils/discrete` — FC01–FC04 register-læsning
- `mbgw write holding/coil/coils` — FC05/FC06/FC0F/FC10 register-skrivning
- `mbgw ota check/firmware/frontend/status` — OTA opdatering
- `--json` flag på alle kommandoer for maskingenereret output

---

## [0.1.0 build 0005] — 2026-05-25 — WiFi STA/AP support + komplet web frontend

**Filer tilføjet:**
- `firmware/main/core/wifi_manager.h/.c` — WiFi STA med 5-retry logik, AP fallback hotspot (ModbusGW-XXXXXX), WiFi scan, `wifi_manager_reconfigure()`
- `firmware/main/api/routes/wifi.h/.c` — REST endpoints: GET status, PUT config, GET scan
- `frontend/index.html` — Single-page web app med 4 tabs: Status, Trend, Log, Indstillinger
- `frontend/css/style.css` — Mørkt tema, card grid, form grid, badge-system, toast-notifikationer
- `frontend/js/api.js` — Alle REST API-kald samlet i ét API-objekt
- `frontend/js/app.js` — Navigation, header refresh (version/IP/WiFi/uptime), interface loading
- `frontend/js/page-status.js` — System/WiFi status-cards, interface status med HW/SW badge, register-læser
- `frontend/js/page-trend.js` — Enkelt- og multi-register trend via Chart.js (auto-start/stop, min/max/avg)
- `frontend/js/page-log.js` — Log-viewer med niveau-filter, OTA check/trigger med progress bar
- `frontend/js/page-settings.js` — Ethernet, WiFi (scan + AP-fallback), Modbus interface-cards, genstart

**Filer ændret:**
- `firmware/main/core/config.h` — tilføjet `wifi_config_gw_t` struct og `wifi`-felt i `gateway_config_t`
- `firmware/main/api/server.c` — tilføjet wifi.h-include og registrering af 3 WiFi-routes
- `firmware/main/CMakeLists.txt` — tilføjet wifi_manager.c, routes/wifi.c, esp_wifi og esp_netif
- `firmware/main/main.c` — tilføjet `wifi_manager_init()` kald

**API tilføjelser:**
- `GET  /api/v1/system/wifi`      — WiFi-status: state, ssid, ip, rssi, ap_active
- `PUT  /api/v1/system/wifi`      — Gem og anvend WiFi-konfiguration straks (ingen genstart nødvendig)
- `GET  /api/v1/system/wifi/scan` — Scan efter tilgængelige netværk (returnerer array med ssid/rssi/channel/open)

---

## [0.1.0 build 0004] — 2026-05-25 — Software UART til ekstra RS485/RS232 interfaces

**Filer tilføjet:**
- `firmware/main/modbus/sw_uart.h/.c` — GPIO bit-bang UART driver styret af gptimer (max 9600 baud). TX via ISR, RX via GPIO edge-interrupt + gptimer sampling i midten af bit-vinduer. Fuld RS485 DE/RE-styring.
- `firmware/main/modbus/mb_rtu_sw.h/.c` — Modbus RTU framing over SW-UART: CRC-16/IBM beregning, frame-opbygning, silence-detektion (4 ms), FC01–FC10 implementering med FreeRTOS queue-baseret RX.

**Filer ændret:**
- `firmware/main/core/config.h` — tilføjet `iface_uart_mode_t` (HW/SW), `GATEWAY_MAX_IFACES` hævet til 8
- `firmware/main/modbus/interface.h` — tilføjet `sw_uart` felt i `mb_interface_t`
- `firmware/main/modbus/interface.c` — komplet omskrevet med HW/SW branching for alle FC01–FC10
- `firmware/main/modbus/sw_uart.h/.c` — tilføjet `sw_uart_set_userdata()`/`sw_uart_get_userdata()`
- `firmware/main/CMakeLists.txt` — tilføjet sw_uart.c, mb_rtu_sw.c, driver og esp_timer
- `ESP32_REFERENCE.md` — SW-UART sektion med konfigurationseksempel og GPIO-overblik

## [0.1.0 build 0003] — 2026-05-25 — OTA opdatering fra GitHub releases

**Filer tilføjet:**
- `firmware/main/ota/ota_manager.h/.c` — OTA service: GitHub releases API check, firmware-flash via esp_https_ota, frontend-flash til SPIFFS
- `firmware/main/api/routes/ota.h/.c` — REST endpoints: check, trigger firmware/frontend OTA, status

**Filer ændret:**
- `firmware/main/CMakeLists.txt` — tilføjet ota/, esp_https_ota og esp-tls som dependencies
- `firmware/main/api/server.c` — registreret 4 nye OTA-routes
- `FEATURES.md` — OTA markeret som done

**API tilføjelser:**
- `GET  /api/v1/system/ota/check`    — sammenlign nuværende version med seneste GitHub release
- `POST /api/v1/system/ota/firmware` — download og flash firmware.bin fra GitHub release
- `POST /api/v1/system/ota/frontend` — download og flash frontend.bin (SPIFFS-image) fra GitHub release
- `GET  /api/v1/system/ota/status`   — progress og state for igangværende OTA

## [0.1.0 build 0001] — 2026-05-25 — Projektinitialisering

**Filer tilføjet:**
- `CLAUDE.md` — projektdefinition og systemregler
- `version.json` — versionskilde (0.1.0 build 0001)
- `ARCHITECTURE.md` — lagdelt arkitekturbeskrivelse
- `MODBUS_REFERENCE.md` — Modbus RTU protokolreference
- `ESP32_REFERENCE.md` — ESP32/ESP-IDF hardware og API-reference
- `FEATURES.md` — feature-backlog
- `BUGS.md` — bug-register
- `CHANGELOG.md` — denne fil
- `RELEASE_NOTES.md` — release-noter
