# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

---

- [fixed] v0.2.1 b0052 — W5500 høj netværkslatency: SPI clock hardkodet til 20 MHz (for højt til prototype-hardware) og INT pin ikke konfigureret (polling-mode ~10ms latency pr. pakke). Løst: default SPI clock sænket til 10 MHz, konfigurerbar via `spi-clock <MHz>` CLI; advarsel i boot-log og `show ethernet` når INT pin ikke er tilsluttet; `lru_purge_enable` aktiveret i httpd for bedre socket-håndtering.
- [fixed] v0.1.0 b0017 — Boot-loop efter save+reboot: root cause var uart_num=-1 i NVS-blob (strukturkorruption fra tidligere build). ESP_ERROR_CHECK i mb_interface_init panikkede på mbc_master_setup(port=-1). Løst i b0016: alle ESP_ERROR_CHECK i mb_interface_init erstattet med proper error returns + uart_num-validering. Løst i b0017: config_sanitize() retter ugyldige uart_num-værdier ved NVS-load.
- [fixed] v0.1.0 b0014 — Serial CLI "gw>" prompt spam: `esp_console_init()` installerer IKKE UART-driveren, så stdin kørte i polling-mode (non-blocking `read()` returnerer 0 bytes straks → linenoise returnerer tom streng → tight loop). Løst ved at bruge `esp_console_new_repl_uart()` som installerer UART-driveren og konfigurerer VFS til blokerende læsning
- [fixed] v0.1.0 b0013 — Ethernet PHY-fejl forårsager reboot-loop: `ESP_ERROR_CHECK(esp_eth_driver_install(...))` panicker hvis PHY-chip ikke er tilsluttet — rettet til non-fatal med graceful fallback til WiFi-only
- [fixed] v0.1.0 b0007 — WiFi statisk IP-konfiguration ignoreret: `wifi_manager_init()` brugte altid DHCP uanset `cfg->ip`
