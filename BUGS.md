# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

---

- [fixed] v0.1.0 b0015 — Boot-loop efter save+reboot: ESP_ERROR_CHECK på modbus_manager_init/api_server_start panicker ved fejl; nvs_flash_init håndterede ikke NVS_NO_FREE_PAGES/NEW_VERSION; uart_set_mode kaldt før mbc_master_start (UART-driver ikke installeret endnu); esp_event_loop_create_default håndterede ikke INVALID_STATE
- [fixed] v0.1.0 b0014 — Serial CLI "gw>" prompt spam: `esp_console_init()` installerer IKKE UART-driveren, så stdin kørte i polling-mode (non-blocking `read()` returnerer 0 bytes straks → linenoise returnerer tom streng → tight loop). Løst ved at bruge `esp_console_new_repl_uart()` som installerer UART-driveren og konfigurerer VFS til blokerende læsning
- [fixed] v0.1.0 b0013 — Ethernet PHY-fejl forårsager reboot-loop: `ESP_ERROR_CHECK(esp_eth_driver_install(...))` panicker hvis PHY-chip ikke er tilsluttet — rettet til non-fatal med graceful fallback til WiFi-only
- [fixed] v0.1.0 b0007 — WiFi statisk IP-konfiguration ignoreret: `wifi_manager_init()` brugte altid DHCP uanset `cfg->ip`
