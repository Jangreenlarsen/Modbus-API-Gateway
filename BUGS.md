# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

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
