#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "lwip/ip4_addr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1
#define WIFI_STA_MAX_RETRY   5
#define WIFI_BACKOFF_SEC     30   // sekunder mellem forsøg efter max retries

static EventGroupHandle_t  s_wifi_eg;
static wifi_status_t       s_status = { .state = WIFI_STATE_DISABLED };
static wifi_config_gw_t    s_cfg;
static esp_netif_t        *s_sta_netif   = NULL;
static esp_netif_t        *s_ap_netif    = NULL;
static int                 s_retry       = 0;
static bool                s_initialized = false;
static esp_timer_handle_t  s_retry_timer = NULL;

// ── Backoff-timer ─────────────────────────────────────────────────────────────

static void on_retry_timer(void *arg)
{
    ESP_LOGI(TAG, "WiFi backoff udløbet — forsøger igen");
    esp_wifi_connect();
}

static void retry_schedule(void)
{
    if (!s_retry_timer) return;
    esp_timer_stop(s_retry_timer);   // ignorér fejl hvis ikke i gang
    esp_timer_start_once(s_retry_timer, (uint64_t)WIFI_BACKOFF_SEC * 1000 * 1000);
}

static void retry_cancel(void)
{
    if (s_retry_timer) esp_timer_stop(s_retry_timer);
}

// ── Event handlers ────────────────────────────────────────────────────────────

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        retry_cancel();
        s_retry = 0;
        esp_wifi_connect();
        s_status.state = WIFI_STATE_CONNECTING;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        // Eksplicit DHCP-restart efter 4-way handshake — sikrer DHCP kører
        // selv hvis dhcpc_stop() blev kaldt under en tidligere korrupt IP-config
        if (!s_cfg.ip[0] || strcasecmp(s_cfg.ip, "dhcp") == 0) {
            esp_netif_dhcpc_stop(s_sta_netif);
            esp_netif_dhcpc_start(s_sta_netif);
            ESP_LOGI(TAG, "STA forbundet — DHCP starter");
        } else {
            ESP_LOGI(TAG, "STA forbundet — statisk IP: %s", s_cfg.ip);
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
        // Reason 15=4WAY_HANDSHAKE_TIMEOUT 204=HANDSHAKE_TIMEOUT → sandsynligvis forkert PSK
        // Reason 5=ASSOC_TOOMANY  205=SA_QUERY_TIMEOUT
        s_status.state = WIFI_STATE_CONNECTING;
        s_retry++;
        if (s_retry <= WIFI_STA_MAX_RETRY) {
            ESP_LOGW(TAG, "STA retry %d/%d (reason %d)", s_retry, WIFI_STA_MAX_RETRY, disc->reason);
            esp_wifi_connect();
        } else {
            // Backoff: forsøg hvert WIFI_BACKOFF_SEC sekund — undgår log-spam og
            // giver CLI luft. Reason 15/204 = forkert PSK → konfigurér korrekt password.
            if (s_retry == WIFI_STA_MAX_RETRY + 1) {
                s_status.state = WIFI_STATE_ERROR;
                xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
            }
            if (s_retry == WIFI_STA_MAX_RETRY + 1 || (s_retry % 10) == 1) {
                ESP_LOGE(TAG, "STA utilgængelig (forsøg %d, reason %d) — retry om %ds",
                         s_retry, disc->reason, WIFI_BACKOFF_SEC);
            }
            retry_schedule();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_status.ip, sizeof(s_status.ip), IPSTR, IP2STR(&ev->ip_info.ip));
        s_status.state = WIFI_STATE_CONNECTED;
        s_retry = 0;
        retry_cancel();
        xEventGroupClearBits(s_wifi_eg, WIFI_FAIL_BIT);
        ESP_LOGI(TAG, "WiFi STA forbundet — IP: %s", s_status.ip);
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

// ── Start AP-fallback hotspot ─────────────────────────────────────────────────

static void start_ap_fallback(void)
{
    uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_AP, mac);
    char ap_ssid[33];
    if (s_cfg.ap_ssid[0]) {
        strncpy(ap_ssid, s_cfg.ap_ssid, sizeof(ap_ssid));
    } else {
        snprintf(ap_ssid, sizeof(ap_ssid), "ModbusGW-%02X%02X%02X",
                 mac[3], mac[4], mac[5]);
    }

    wifi_config_t ap_cfg = {};
    strncpy((char*)ap_cfg.ap.ssid, ap_ssid, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen(ap_ssid);
    ap_cfg.ap.max_connection = 4;

    if (s_cfg.ap_password[0] && strlen(s_cfg.ap_password) >= 8) {
        strncpy((char*)ap_cfg.ap.password, s_cfg.ap_password, sizeof(ap_cfg.ap.password));
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    retry_cancel();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    // WIFI_EVENT_STA_START fyres igen → STA genoptager forsøg i baggrunden

    s_status.state = WIFI_STATE_AP_MODE;
    strncpy(s_status.ssid, ap_ssid, sizeof(s_status.ssid));
    strncpy(s_status.ip, "192.168.4.1", sizeof(s_status.ip));

    ESP_LOGI(TAG, "AP fallback aktiv — SSID: %s  IP: 192.168.4.1", ap_ssid);
}

// ── Init ──────────────────────────────────────────────────────────────────────

esp_err_t wifi_manager_init(const wifi_config_gw_t *cfg)
{
    memcpy(&s_cfg, cfg, sizeof(wifi_config_gw_t));

    if (!cfg->enabled) {
        ESP_LOGI(TAG, "WiFi disabled");
        return ESP_OK;
    }

    // Én gang: opret netifs, wifi-driver, event handlers og backoff-timer
    if (!s_initialized) {
        s_wifi_eg   = xEventGroupCreate();
        s_sta_netif = esp_netif_create_default_wifi_sta();
        s_ap_netif  = esp_netif_create_default_wifi_ap();

        wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL);
        esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, on_wifi_event, NULL);

        esp_timer_create_args_t timer_cfg = {
            .callback = on_retry_timer,
            .arg      = NULL,
            .name     = "wifi_retry",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &s_retry_timer));

        s_initialized = true;
    }

    // Statisk IP — eller DHCP. Valider IP-adressen inden DHCP stoppes:
    // korrupt NVS kan give et ikke-tomt ip-felt der ikke er en gyldig IPv4.
    if (cfg->ip[0] && strcasecmp(cfg->ip, "dhcp") != 0) {
        ip4_addr_t test;
        if (ip4addr_aton(cfg->ip, &test) && test.addr != 0) {
            esp_netif_ip_info_t ip_info = {};
            ip4_addr_t a;
            ip4addr_aton(cfg->ip,      &a); ip_info.ip.addr      = a.addr;
            ip4addr_aton(cfg->gw,      &a); ip_info.gw.addr      = a.addr;
            ip4addr_aton(cfg->netmask, &a); ip_info.netmask.addr = a.addr;
            esp_netif_dhcpc_stop(s_sta_netif);
            ESP_ERROR_CHECK(esp_netif_set_ip_info(s_sta_netif, &ip_info));
            ESP_LOGI(TAG, "WiFi STA static IP: %s  gw %s  mask %s",
                     cfg->ip, cfg->gw, cfg->netmask);
        } else {
            ESP_LOGW(TAG, "WiFi IP '%s' ugyldig/korrupt — bruger DHCP", cfg->ip);
            esp_netif_dhcpc_stop(s_sta_netif);
            esp_netif_dhcpc_start(s_sta_netif);
        }
    } else {
        esp_netif_dhcpc_stop(s_sta_netif);
        esp_netif_dhcpc_start(s_sta_netif);
        ESP_LOGI(TAG, "WiFi STA DHCP aktiv");
    }

    wifi_config_t sta_cfg = {};
    strncpy((char*)sta_cfg.sta.ssid,     cfg->ssid,     sizeof(sta_cfg.sta.ssid));
    strncpy((char*)sta_cfg.sta.password, cfg->password, sizeof(sta_cfg.sta.password));
    // WPA2_PSK: ESP32 annoncerer kun WPA2/CCMP — korrekt for enterprise WLC'er
    // pmf_cfg {0,0}: ikke sat — capable=true giver SA_QUERY_TIMEOUT på WLC'er
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    strncpy(s_status.ssid, cfg->ssid, sizeof(s_status.ssid));
    ESP_LOGI(TAG, "WiFi STA forbinder til: %s",
             cfg->ssid[0] ? cfg->ssid : "(ingen SSID konfigureret)");

    // Vent på forbindelse eller fejl (max 10 sek)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_FAIL_BIT) {
        if (cfg->ap_fallback) {
            start_ap_fallback();
        } else {
            s_status.state = WIFI_STATE_ERROR;
            ESP_LOGE(TAG, "WiFi STA fejlede, AP fallback deaktiveret");
        }
    }
    return ESP_OK;
}

wifi_status_t wifi_manager_get_status(void)
{
    if (s_status.state == WIFI_STATE_CONNECTED) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            s_status.rssi = ap.rssi;
    }
    return s_status;
}

char *wifi_manager_scan(void)
{
    wifi_scan_config_t scan_cfg = { .show_hidden = false };
    esp_wifi_scan_start(&scan_cfg, true);

    uint16_t count = 20;
    wifi_ap_record_t records[20];
    esp_wifi_scan_get_ap_records(&count, records);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid",    (char*)records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi",    records[i].rssi);
        cJSON_AddNumberToObject(ap, "channel", records[i].primary);
        cJSON_AddBoolToObject(ap,   "open",    records[i].authmode == WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(arr, ap);
    }
    char *s = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return s;
}

esp_err_t wifi_manager_reconfigure(const wifi_config_gw_t *cfg)
{
    retry_cancel();
    if (s_initialized) esp_wifi_stop();
    s_retry = 0;
    if (s_wifi_eg) xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    return wifi_manager_init(cfg);
}
