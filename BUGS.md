# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

---

## Feltfejl (2026-07-30)

- [fixed] F9 v0.9.1 b0096 — `GET /api` og `/api/v1` gav "Nothing matches the given URI" efter opdatering til b0095. Root cause: `hcfg.max_uri_handlers=32` er httpd-serverens hårde kapacitetsgrænse for antal registrerede routes — adskilt fra `MAX_LOGGED_ROUTES` (kun størrelsen på log-wrapper-arrayet). Da `/` og `/manual` blev tilføjet i b0095, nåede det samlede antal registreringer 33 (>32), og `route_api_index` (`/api*` — sidst registreret) fejlede stille, fordi `httpd_register_uri_handler`s returkode ikke blev tjekket. LØST: `hcfg.max_uri_handlers` hævet til 48 (matcher `MAX_LOGGED_ROUTES`); `reg()` og WebSocket-registreringen logger nu fejl højlydt (`ESP_LOGE`) hvis en route-registrering fejler, så det aldrig sker ubemærket igen.

## Fundet undervejs (2026-07-30)

- [fixed] F8 v0.9.0 b0095 — `MAX_LOGGED_ROUTES` (32) i `server.c` var nået præcis (32 `reg()`-kald) ved tilføjelse af `/` og `/manual` — næste nye route ville have trigget `assert(s_nlogged < MAX_LOGGED_ROUTES)`. LØST: hævet til 48 for at give headroom til fremtidige endpoints.

---

## Feltfejl (2026-07-12)

- [fixed] F6 v0.8.1 b0094 — W5500 kører med MAC `00:00:00:00:00:00` → alle devices har samme (ikke-unikke) MAC-adresse, hvilket giver kollisioner på netværket (DHCP/switch). Root cause: W5500 har ingen fabriks-MAC (modsat ESP32'ens interne EMAC), og `init_w5500()` satte aldrig en. LØST: MAC udledes nu fra chippens eFuse via `esp_read_mac(ESP_MAC_ETH)` og sættes med `esp_eth_ioctl(ETH_CMD_S_MAC_ADDR)` før netif-attach → unik pr. device. MAC logges ved boot.
- [fixed] F7 v0.8.1 b0094 — `gpio_isr_handler_add(553): GPIO isr service is not installed` ved W5500-init: INT-interruptet blev aldrig registreret (Ethernet initialiseres før noget andet kalder `gpio_install_isr_service`). Fungerede kun via INT-poll-workarounden. LØST: GPIO ISR-servicen installeres nu i `init_w5500()` før driver-install når INT-pin er konfigureret.
- [fixed] F5 v0.5.9 b0090 — SW-UART-interface med uconfigurerede pins (-1) blev markeret "ready" og spammede `GPIO_PIN mask error`/`gpio_set_level error` ved boot (fx interfaces oprettet via POST /interfaces uden pins). Root cause: hverken `mb_interface_init` eller `sw_uart_init` validerede pins → `(1ULL << -1)` = ugyldig maske. LØST: begge lag afviser nu tx/rx < 0 (interface markeres ikke-ready i stedet); `ESP_ERROR_CHECK(sw_uart_init)` erstattet med proper fejl-retur (+rx_q ryddes op); `gpio_install_isr_service` kaldes kun én gang (ingen "already installed"-spam ved flere SW-interfaces).
- [fixed] F4 v0.5.8 b0089 — OTA-loop: release-binæren for v0.5.7-b0088 rapporterede sig selv som 0.5.6/0087, så enheden blev ved med at se "opdatering tilgængelig" efter flash. Root cause: `version.h` blev redigeret manuelt, og release-binæren blev bygget FØR versionsbumpet → forkert version indbygget. LØST (proces): (1) genudgivet korrekt versioneret release v0.5.8-b0089 + slettet den fejlbehæftede b0088; (2) `version.h` genereres nu automatisk fra `version.json` ved hvert build via `scripts/gen_version.py` (PlatformIO pre-build) — binæren kan ikke længere komme ud af sync. Ingen versionsbump (build-tooling).
- [fixed] F3 v0.5.8 b0089 — Log-spam: `config_store: Config loaded v12` gentages hvert ~5s. Root cause: `config_store_load()` loggede på INFO ved hvert kald, og F2's auto-refresh (hvert 5s) kalder `/system` + `/interfaces`, som begge loader config → 2 INFO-linjer pr. cyklus. Ufarligt (NVS-læsning, ingen flash-slitage) men støjende. LØST: demoteret til `ESP_LOGD` (DEBUG).
- [fixed] F2 v0.5.7 b0088 — WiFi-status vises ikke i web-GUI (/mgmt) når både WiFi og Ethernet er forbundet (CLI `show wifi` virker fint). Root cause: mgmt-statussiden henter `/system` og `/system/wifi` som to konkurrerende kald der begge skriver samme DOM-felt (`t-net`), og WiFi-rækken vises kun hvis `/system/wifi` har nået at sætte `_wifiData`. Uden auto-refresh — og ved socket-pres (`lru_purge_enable`) kan ét kald droppes — stod WiFi som "—" indtil manuel Opdater. LØST: deterministisk render (`_wifiData` sættes eksplicit, bevares ved fejl) + auto-refresh af status-fanen hvert 5s (self-healing).
- [fixed] F1 v0.5.6 b0087 — Ethernet forbinder ikke selvom switch-link er oppe. Root cause: Ethernet-laget (`ethernet.c`) håndterede IKKE statisk IP (læste aldrig `cfg->ip`/`gw`/`netmask`), tvang ikke DHCP-start, og havde ingen `ETH_EVENT`-handler — i modsætning til WiFi-laget. Statisk IP blev derfor ignoreret, og der var ingen link-up/down-logning til diagnose. LØST: tilføjet `on_eth_event` (link UP/DOWN + driver start/stop-log), statisk-IP-anvendelse med validering (spejler `wifi_manager`), og forceret DHCP-genstart ved link-up.

---

## Re-analyse efter implementering (b0085, 2026-07-12)

- [fixed] N1 v0.5.5 b0086 — K1-guarden var asymmetrisk: den blokerede kun en 2. HW-UART **master**, men esp-modbus' slave-controller er også en global singleton (`slave_interface_ptr`), så to HW-**slaves** kolliderede stadig lydløst. LØST: guarden i `modbus_manager_init` tracker nu både `hw_master_up` og `hw_slave_up` og deaktiverer den anden HW-controller af hver rolle. (Én HW-master + én HW-slave kan sameksistere — separate globaler.)
- [note] N2 v0.5.0 b0080 — Adfærd: interface-omdøbning træder først i kraft ved reboot (FC-routing mod kørende config). Tilsigtet "reboot for at anvende"-model + `reboot_required`-flag. Ingen kodeændring.
- [note] N3 v0.4.5 b0078 — OTA-handlere null-tjekker ikke `calloc` (præeksisterende, lav sandsynlighed). Kan hærdes senere.

---

## Åbne — fundet i kodegennemgang (b0078, 2026-07-11)

**Kritiske**
- [fixed] K1 v0.4.6 b0079 — Flere HW-UART masters kolliderer: esp-modbus v1.x bruger én global controller. LØST: `modbus_manager_init` tillader kun én HW-master; yderligere HW-masters deaktiveres (`ready=false`, log-fejl) i stedet for stille at kapre den globale controller. Per-interface fejl er nu ikke fatal (fortsæt + start cache-tasks). Brug SW-UART til flere master-porte.
- [fixed] K2 v0.4.6 b0079 — Buffer overflow-risiko i SW-UART coil/discrete-læsning. LØST: `mb_sw_read_coils`/`mb_sw_read_discrete` clamper nu kopieret byte-antal til `(count+7)/8` — en slave kan ikke overskride caller-bufferen via et for stort byte-count.
- [fixed] K3 v0.4.6 b0079 — `ESP_ERROR_CHECK(nvs_open(...))` i `config_store_save` kunne panikke enheden. LØST: returnerer nu fejl ved NVS-open-fejl i stedet for at panicke (samme mønster som b0016/b0017).

**Høj**
- [fixed] H1 v0.5.0 b0080 — Config-ændringer desynkroniserede iface-indekser. LØST: FC-routing resolver nu mod KØRENDE config (samme som modbus_manager bruger), så en request aldrig rammer et forkert/ikke-initialiseret interface. POST/PUT/DELETE returnerer `"reboot_required": true` så klienten ved at ændringen først anvendes efter reboot.
- [fixed] H2 v0.5.1 b0081 — Modbus exception-koder på HW-interfaces. DELVIST/DOKUMENTERET: esp-modbus v1.x eksponerer ikke exception-koden via `mbc_master_send_request`, så `exception_code` er kun tilgængelig på SW-UART. HW-UART-fejl rapporteres nu ensartet som `modbus_error` med `detail`. Begrænsningen er noteret i `fc_common.c`. Fuld HW-exception-parsing kræver esp-modbus v2.x.
- [fixed] H3 v0.5.1 b0081 — Inkonsistent exception-håndtering. LØST: ny `api_mb_ok()` i `fc_common.c` giver PRÆCIS samme fejl-JSON (`modbus_timeout`/`modbus_exception`/`modbus_error`) for ALLE FC01–FC10-routes. Duplikeret fejlkode fjernet fra hver route.

**Medium**
- [fixed] M1 v0.5.0 b0080 — Service-laget var en tom stub. LØST: `gateway_service` er nu et reelt lag — alle FC-routes kalder `gw_*`-funktioner (aldrig `modbus_manager` direkte), og service-laget ejer interface-opslag mod kørende config. Overholder ARCHITECTURE.md regel 1+2.
- [fixed] M2 v0.5.0 b0080 — NVS-blob blev læst ved HVER Modbus-request. LØST: FC-routing bruger nu `gw_resolve_iface` mod kørende config i RAM — ingen NVS-læsning på hot-path. Config-CRUD læser stadig NVS.
- [fixed] M3 v0.5.2 b0082 — Partial-read i write-handlers. LØST: fælles `api_recv_body()` (recv-loop med timeout-håndtering) bruges nu i FC05/FC06/FC0F/FC10 samt interfaces.c (duplikeret loop fjernet).
- [fixed] M4 v0.5.3 b0083 — OTA frontend markerede afbrudt download som "done". LØST: efter download valideres `total` mod `content_len`; mismatch → ERROR i stedet for at flashe et ufuldstændigt image.
- [fixed] M5 v0.5.3 b0083 — OTA-check blokerede httpd-worker. LØST: GitHub-URL-opslag sker nu i `ota_task` (baggrund), ikke i handleren. Handleren svarer straks `*_update_started`; "ingen opdatering" rapporteres via OTA-status (`ota_report_error`).

**Lav / oprydning**
- [fixed] L1 v0.5.4 b0084 — Død skeleton-kode `mb_rtu_sw_transaction` (+ `rx_cb`/`rx_ctx_t`). LØST: slettet fra `mb_rtu_sw.c` og `.h` (den rigtige transceiver er `do_transaction`).
- [accepted] L2 v0.4.5 b0078 — Cache-statistik tælles pr. register, ikke pr. request. ACCEPTERET: cachen er per-register, så per-register-tælling er meningsfuld. Dokumenteret; ingen kodeændring.
- [accepted] L3 v0.4.5 b0078 — `frontend_available == firmware_available`. ACCEPTERET: firmware og frontend udgives sammen (samme release-tag), så samme sammenligning er korrekt indtil de versioneres separat.
- [fixed] L4 v0.5.4 b0084 — `cache_lookup` læste `s_stats.enabled` uden lås. LØST: enabled læses nu inde i mutex-låsen.
- [fixed] L5 v0.5.2 b0082 — Tom/ugyldig body ved FC05. LØST: returnerer nu 400 hvis `value` mangler/ikke er bool/tal (skriver ikke stille coil=0).
- [fixed] L6 v0.5.2 b0082 — `start`/`count` validering. LØST: fælles `api_query_u16()` clamper negative → 0 og >65535 → 65535 i alle read/write-routes.
- [accepted] L7 v0.4.5 b0078 — DE-pin default GPIO0 (iface 5, 30-pin preset) er boot-strap pin. ACCEPTERET: allerede kommenteret i `config.c` ("brug med forsigtighed"); DE-pin kan omkonfigureres. Ingen kodeændring.

---

- [fixed] v0.3.0 b0060 — REST FC01-FC10 routes matcher aldrig: URI-mønstre med midt-wildcards virkede ikke i ESP-IDF httpd. Løst med master GET/PUT dispatcher i `interfaces.c` der parser URI'en (`/slaves/N/op[/addr]`) og kalder de eksisterende FC handlers som rene C-funktioner (signatur `api_fcXX(req, iface, slave[, addr])`). FC handlers eksporteret fra coils.c/discrete.c/holding_regs.c/input_regs.c og kaldes nu fra master_get_dispatcher / master_put_dispatcher. Alle FC URLs accepterer både numerisk ID og navn-alias.
- [fixed] v0.2.1 b0056 — W5500 interrupt edge-trigger problem løst via workaround-task der poller INT-pin hvert 2ms og sender `xTaskNotifyGive()` direkte til `w5500_tsk` — porteret fra `Modbus_server_slave_ESP32`.
- [fixed] v0.3.0 b0059 — REST API PUT /api/v1/interfaces/{id}/config returnerede 405 Method Not Allowed: ESP-IDF httpd matchede ikke URI-mønstre med midt-wildcards. Løst ved trailing wildcard `/api/v1/interfaces/*` + handler-dispatcher der både accepterer `/{key}` og `/{key}/config` (bagudkompatibel). Samtidig tilføjet navn-alias så `key` kan være enten ID eller brugervenligt navn.
- [superseded] v0.2.1 b0054 — W5500 interrupt edge-trigger problem (se b0056 fix ovenfor): ESP-IDF W5500 driver bruger GPIO_INTR_NEGEDGE. Når W5500 har multiple frames i RX-buffer holdes INT LOW — ingen ny faldende flanke → ISR misser frames 2..N → 400-900ms latency. Workaround: polling-mode 5ms (`int -1`, `poll-ms 5`). Permanent fix kræver custom W5500 driver component med level-triggered interrupt eller loop over alle frames pr. ISR.
- [fixed] v0.2.1 b0052 — W5500 høj netværkslatency: SPI clock hardkodet til 20 MHz (for højt til prototype-hardware) og INT pin ikke konfigureret (polling-mode ~10ms latency pr. pakke). Løst: default SPI clock sænket til 10 MHz, konfigurerbar via `spi-clock <MHz>` CLI; advarsel i boot-log og `show ethernet` når INT pin ikke er tilsluttet; `lru_purge_enable` aktiveret i httpd for bedre socket-håndtering.
- [fixed] v0.1.0 b0017 — Boot-loop efter save+reboot: root cause var uart_num=-1 i NVS-blob (strukturkorruption fra tidligere build). ESP_ERROR_CHECK i mb_interface_init panikkede på mbc_master_setup(port=-1). Løst i b0016: alle ESP_ERROR_CHECK i mb_interface_init erstattet med proper error returns + uart_num-validering. Løst i b0017: config_sanitize() retter ugyldige uart_num-værdier ved NVS-load.
- [fixed] v0.1.0 b0014 — Serial CLI "gw>" prompt spam: `esp_console_init()` installerer IKKE UART-driveren, så stdin kørte i polling-mode (non-blocking `read()` returnerer 0 bytes straks → linenoise returnerer tom streng → tight loop). Løst ved at bruge `esp_console_new_repl_uart()` som installerer UART-driveren og konfigurerer VFS til blokerende læsning
- [fixed] v0.1.0 b0013 — Ethernet PHY-fejl forårsager reboot-loop: `ESP_ERROR_CHECK(esp_eth_driver_install(...))` panicker hvis PHY-chip ikke er tilsluttet — rettet til non-fatal med graceful fallback til WiFi-only
- [fixed] v0.1.0 b0007 — WiFi statisk IP-konfiguration ignoreret: `wifi_manager_init()` brugte altid DHCP uanset `cfg->ip`
