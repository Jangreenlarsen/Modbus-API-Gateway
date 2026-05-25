# Changelog

Nyeste øverst. Format: `## [version build NNNN] — YYYY-MM-DD — beskrivelse`

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
