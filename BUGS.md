# Bugs

Format: `[status] vX.X.X — beskrivelse`
Status: `open` | `investigating` | `fixed`

---

- [fixed] v0.1.0 b0013 — Ethernet PHY-fejl forårsager reboot-loop: `ESP_ERROR_CHECK(esp_eth_driver_install(...))` panicker hvis PHY-chip ikke er tilsluttet — rettet til non-fatal med graceful fallback til WiFi-only
- [fixed] v0.1.0 b0007 — WiFi statisk IP-konfiguration ignoreret: `wifi_manager_init()` brugte altid DHCP uanset `cfg->ip`
