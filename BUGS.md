# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

---

- [open] v0.3.0 b0058 — REST FC01-FC10 routes matcher aldrig: URI-mønstre med midt-wildcards (fx `/api/v1/interfaces/*/slaves/*/holding-registers`) virker IKKE i ESP-IDF httpd. `httpd_uri_match_wildcard` behandler kun `*` ved slutningen af URI'en som wildcard — midt-stjerner er bogstavelige tegn. Samme problem fandtes for `/interfaces/*/config` (PUT save), fixet i b0059 ved trailing wildcard + handler-dispatcher. FC-routes kræver tilsvarende refactor: master GET/PUT dispatcher på `/api/v1/interfaces/*` der ud fra URI-suffix kalder coils/discrete/holding-regs/input-regs handlers.
- [fixed] v0.3.0 b0059 — REST API PUT /api/v1/interfaces/{id}/config returnerede 405 Method Not Allowed: ESP-IDF httpd matchede ikke URI-mønstre med midt-wildcards. Løst ved trailing wildcard `/api/v1/interfaces/*` + handler-dispatcher der både accepterer `/{key}` og `/{key}/config` (bagudkompatibel). Samtidig tilføjet navn-alias så `key` kan være enten ID eller brugervenligt navn.
- [open] v0.2.1 b0054 — W5500 interrupt edge-trigger problem: ESP-IDF W5500 driver bruger GPIO_INTR_NEGEDGE. Når W5500 har multiple frames i RX-buffer holdes INT LOW — ingen ny faldende flanke → ISR misser frames 2..N → 400-900ms latency. Workaround: polling-mode 5ms (`int -1`, `poll-ms 5`). Permanent fix kræver custom W5500 driver component med level-triggered interrupt eller loop over alle frames pr. ISR.
- [fixed] v0.2.1 b0052 — W5500 høj netværkslatency: SPI clock hardkodet til 20 MHz (for højt til prototype-hardware) og INT pin ikke konfigureret (polling-mode ~10ms latency pr. pakke). Løst: default SPI clock sænket til 10 MHz, konfigurerbar via `spi-clock <MHz>` CLI; advarsel i boot-log og `show ethernet` når INT pin ikke er tilsluttet; `lru_purge_enable` aktiveret i httpd for bedre socket-håndtering.
- [fixed] v0.1.0 b0017 — Boot-loop efter save+reboot: root cause var uart_num=-1 i NVS-blob (strukturkorruption fra tidligere build). ESP_ERROR_CHECK i mb_interface_init panikkede på mbc_master_setup(port=-1). Løst i b0016: alle ESP_ERROR_CHECK i mb_interface_init erstattet med proper error returns + uart_num-validering. Løst i b0017: config_sanitize() retter ugyldige uart_num-værdier ved NVS-load.
- [fixed] v0.1.0 b0014 — Serial CLI "gw>" prompt spam: `esp_console_init()` installerer IKKE UART-driveren, så stdin kørte i polling-mode (non-blocking `read()` returnerer 0 bytes straks → linenoise returnerer tom streng → tight loop). Løst ved at bruge `esp_console_new_repl_uart()` som installerer UART-driveren og konfigurerer VFS til blokerende læsning
- [fixed] v0.1.0 b0013 — Ethernet PHY-fejl forårsager reboot-loop: `ESP_ERROR_CHECK(esp_eth_driver_install(...))` panicker hvis PHY-chip ikke er tilsluttet — rettet til non-fatal med graceful fallback til WiFi-only
- [fixed] v0.1.0 b0007 — WiFi statisk IP-konfiguration ignoreret: `wifi_manager_init()` brugte altid DHCP uanset `cfg->ip`
