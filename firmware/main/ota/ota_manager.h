#pragma once
#include "esp_err.h"
#include <stdbool.h>

// GitHub repo — ændres kun her hvis repoet flyttes
#define OTA_GITHUB_OWNER  "Jangreenlarsen"
#define OTA_GITHUB_REPO   "Modbus-API-Gateway"

#define OTA_GITHUB_API_LATEST \
    "https://api.github.com/repos/" OTA_GITHUB_OWNER "/" OTA_GITHUB_REPO "/releases/latest"

// Forventede asset-navne i GitHub release
#define OTA_FIRMWARE_ASSET  "firmware.bin"
#define OTA_FRONTEND_ASSET  "frontend.bin"   // SPIFFS-image

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_FLASHING,
    OTA_STATE_DONE,
    OTA_STATE_ERROR,
} ota_state_t;

typedef struct {
    char     current_version[16];
    char     current_build[8];
    char     latest_version[16];
    bool     firmware_available;
    bool     frontend_available;
    char     firmware_url[256];
    char     frontend_url[256];
    char     release_notes[512];
} ota_info_t;

typedef struct {
    ota_state_t state;
    int         progress_pct;   // 0–100
    char        error[128];
} ota_status_t;

// Forespørg GitHub releases API og udfyld ota_info_t
esp_err_t ota_check(ota_info_t *info);

// Start firmware-opdatering (blokerer indtil færdig eller fejl).
// Opdaterer den globale s_status direkte. Kalder esp_restart() ved succes.
esp_err_t ota_update_firmware(const char *url);

// Opdater frontend-filer på SPIFFS fra SPIFFS-image-URL.
// Opdaterer den globale s_status direkte.
esp_err_t ota_update_frontend(const char *url);

// Hent seneste OTA-status (bruges af REST API)
const ota_status_t *ota_get_status(void);

// Sæt OTA-status til ERROR med en besked (bruges af OTA-task ved fx
// "ingen opdatering tilgængelig", så handleren kan svare uden at blokere).
void ota_report_error(const char *msg);
