# Changelog

Nyeste øverst. Format: `## [version build NNNN] — YYYY-MM-DD — beskrivelse`

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
