# Changelog

Nyeste øverst. Format: `## [version build NNNN] — YYYY-MM-DD — beskrivelse`

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
