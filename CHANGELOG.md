# Changelog

Nyeste øverst. Format: `## [version build NNNN] — YYYY-MM-DD — beskrivelse`

---

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
