#pragma once
#include "config.h"
#include "esp_err.h"

typedef enum {
    WIFI_STATE_DISABLED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,      // STA fejlede, kører som hotspot
    WIFI_STATE_ERROR,
} wifi_state_t;

typedef struct {
    wifi_state_t state;
    char         ip[16];
    char         ssid[33];
    int8_t       rssi;       // dBm — kun gyldig i CONNECTED state
} wifi_status_t;

// Init WiFi — forsøger STA, falder tilbage til AP hvis ap_fallback=1
esp_err_t     wifi_manager_init(const wifi_config_gw_t *cfg);

// Hent aktuel status
wifi_status_t wifi_manager_get_status(void);

// Scan efter tilgængelige netværk — returnerer JSON-array som string (caller free()'er)
char *        wifi_manager_scan(void);

// Opdater konfiguration og genopret forbindelse
esp_err_t     wifi_manager_reconfigure(const wifi_config_gw_t *cfg);
