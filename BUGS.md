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
- [open] H1 v0.4.5 b0078 — Config-ændringer anvendes ikke live og desynkroniserer iface-indekser: `modbus_manager_init` kaldes kun ved boot. Efter POST/DELETE resolver FC-dispatcher iface-id ud fra frisk NVS-load, mens `get_iface()` bruger boot-snapshot → forespørgsler rammer forkert/manglende interface indtil reboot. → FC-routing mod kørende config + `reboot_required`-flag i CRUD-svar.
- [open] H2 v0.4.5 b0078 — Modbus exception-koder surfaces aldrig for HW-interfaces: `mb_result_t.modbus_exception` sættes kun i SW-UART-stien. Dokumenteret `{"error":"modbus_exception","exception_code":N}` virker aldrig på HW-UART. → dokumentér esp-modbus-begrænsning + ensret fejl-JSON.
- [open] H3 v0.4.5 b0078 — Inkonsistent exception-håndtering: kun `holding_regs.c` tjekker `result.modbus_exception`; coils/discrete/input_regs gør ikke → FC01/02/04 returnerer aldrig det dokumenterede exception-format. → fælles fejl-respons-helper på tværs af alle FC-routes.

**Medium**
- [open] M1 v0.4.5 b0078 — Service-laget er en tom stub (`gateway_service.c`). Routes kalder `modbus_manager` direkte i strid med ARCHITECTURE.md (API→service→modbus). → arkitektur-gæld; vurderes separat (fuld refactor er stor/risikabel).
- [open] M2 v0.4.5 b0078 — NVS-blob læses ved HVER Modbus-request (`config_store_load` i FC-dispatcher) → unødig latency under polling. → brug kørende config i RAM (løses sammen med H1).
- [open] M3 v0.4.5 b0078 — Partial-read i write-handlers: FC05/FC06/FC0F/FC10 bruger enkelt `httpd_req_recv` der kan returnere delvise reads; kun `interfaces.c` har korrekt loop. Store `values`-arrays kan afkortes. → fælles `recv_body`-helper.
- [open] M4 v0.4.5 b0078 — OTA frontend markerer afbrudt download som "done": `read==0` behandles altid som success uden kontrol af `total` mod `content_len`. Netværksdrop → delvist frontend-image flashet. → valider modtaget størrelse.
- [open] M5 v0.4.5 b0078 — OTA-check blokerer httpd-worker: `ota_check()` (op til 10s HTTP) kaldes synkront i handleren når ingen URL angives → kan stalle API. → flyt URL-opslag ind i ota_task.

**Lav / oprydning**
- [open] L1 v0.4.5 b0078 — Død skeleton-kode `mb_rtu_sw_transaction` returnerer altid `ESP_ERR_NOT_FINISHED`, kaldes aldrig, og lækker en queue (`xQueueCreate` uden delete). → slet.
- [open] L2 v0.4.5 b0078 — Cache-statistik forvrænget ved range-reads: `total_requests`/`hits`/`misses` tælles pr. register, ikke pr. request. → dokumentér eller separat api_requests-tæller.
- [open] L3 v0.4.5 b0078 — `frontend_available == firmware_available` (samme sammenligning); frontend versioneres ikke separat. → dokumentér/accepter.
- [open] L4 v0.4.5 b0078 — `cache_lookup` læser `s_stats.enabled` uden lås (benignt). → læs under lås for konsistens.
- [open] L5 v0.4.5 b0078 — Tom/ugyldig body ved FC05 skriver stille coil=0 i stedet for 400. → returnér 400 ved manglende `value`.
- [open] L6 v0.4.5 b0078 — `start`/`count` parses med `atoi` uden validering (negativ → wrap). → input-hærdning.
- [open] L7 v0.4.5 b0078 — DE-pin default GPIO0 (iface 5, 30-pin preset) er boot-strap pin. → dokumentér risiko.

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
