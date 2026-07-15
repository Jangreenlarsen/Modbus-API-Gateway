#include "system.h"
#include "api_log.h"
#include "modbus_log.h"
#include "config.h"
#include "config_store.h"
#include "version.h"
#include "ethernet.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static esp_err_t get_system_handler(httpd_req_t *req)
{
    char ip[16]; ethernet_get_ip(ip, sizeof(ip));
    gateway_config_t cfg; config_store_load(&cfg);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version",      GATEWAY_VERSION);
    cJSON_AddStringToObject(root, "build",        GATEWAY_BUILD);
    cJSON_AddNumberToObject(root, "uptime_s",     esp_timer_get_time() / 1000000);
    cJSON_AddStringToObject(root, "ip",           ip);
    cJSON_AddNumberToObject(root, "free_heap",    esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "reset_reason", esp_reset_reason());
    cJSON_AddStringToObject(root, "board_variant",
        cfg.board_variant == BOARD_ESP32_38PIN ? "38pin" : "30pin");
    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

static esp_err_t post_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// GET /api/v1/system/hardware — board variant + GPIO presets for alle interfaces
static esp_err_t get_system_hardware_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);
    const char *bv = (cfg.board_variant == BOARD_ESP32_38PIN) ? "38pin" : "30pin";

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "board_variant", bv);

    cJSON *p485 = cJSON_AddArrayToObject(root, "presets_rs485");
    cJSON *p232 = cJSON_AddArrayToObject(root, "presets_rs232");
    for (int i = 0; i < GATEWAY_MAX_IFACES; i++) {
        int tx, rx, de;
        config_get_gpio_preset(i, IFACE_TYPE_RS485, cfg.board_variant, &tx, &rx, &de);
        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "tx", tx);
        cJSON_AddNumberToObject(e, "rx", rx);
        cJSON_AddNumberToObject(e, "de", de);
        cJSON_AddItemToArray(p485, e);

        config_get_gpio_preset(i, IFACE_TYPE_RS232, cfg.board_variant, &tx, &rx, &de);
        e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "tx", tx);
        cJSON_AddNumberToObject(e, "rx", rx);
        cJSON_AddNumberToObject(e, "de", de);
        cJSON_AddItemToArray(p232, e);
    }

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// PUT /api/v1/system/hardware  body: {"board_variant":"30pin"|"38pin"}
static esp_err_t put_system_hardware_handler(httpd_req_t *req)
{
    char body[64] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n > 0) body[n] = '\0';

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"error\":\"invalid JSON\"}");
        return ESP_OK;
    }

    gateway_config_t cfg; config_store_load(&cfg);
    cJSON *v = cJSON_GetObjectItem(json, "board_variant");
    if (v && cJSON_IsString(v)) {
        cfg.board_variant = (strcmp(v->valuestring, "38pin") == 0)
                            ? BOARD_ESP32_38PIN : BOARD_ESP32_30PIN;
    }
    cJSON_Delete(json);
    config_store_save(&cfg);

    const char *bv = (cfg.board_variant == BOARD_ESP32_38PIN) ? "38pin" : "30pin";
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"board_variant\":\"%s\"}", bv);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ── GET /api/v1/system/gpio — GPIO-tilgængelighed til interface-pins ─────────
// Beregner pr. GPIO om den kan bruges som TX/RX/DE, ift. board-variant og den
// aktive Ethernet-config (pins optaget af W5500/LAN8720 markeres reserveret).

static bool gpio_exists(int g, board_variant_t board)
{
    if (g < 0 || g > 39) return false;
    if (g == 20 || g == 24 || (g >= 28 && g <= 31)) return false;   // findes ikke på ESP32
    if (board == BOARD_ESP32_30PIN && (g == 37 || g == 38)) return false;  // ikke eksponeret
    return true;
}
static bool gpio_input_only(int g)  { return g >= 34 && g <= 39; }
static bool gpio_is_flash(int g)    { return g >= 6 && g <= 11; }
static bool gpio_is_strapping(int g){ return g==0 || g==2 || g==5 || g==12 || g==15; }

static const char *gpio_reserved_by(int g, const eth_config_t *e)
{
    if (gpio_is_flash(g)) return "flash";
    if (g == 1 || g == 3) return "uart0";
    if (!e->enabled) return NULL;
    if (e->hw_type == ETH_HW_W5500) {
        if (g==e->spi_cs_gpio || g==e->spi_mosi_gpio || g==e->spi_miso_gpio || g==e->spi_sclk_gpio) return "ethernet";
        if (e->spi_rst_gpio >= 0 && g==e->spi_rst_gpio) return "ethernet";
        if (e->spi_int_gpio >= 0 && g==e->spi_int_gpio) return "ethernet";
    } else if (e->hw_type == ETH_HW_LAN8720) {
        static const int rmii[] = { 0, 19, 21, 22, 25, 26, 27 };  // faste RMII-pins
        for (unsigned i = 0; i < sizeof(rmii)/sizeof(rmii[0]); i++) if (g==rmii[i]) return "ethernet";
        if (e->mdc_gpio  >= 0 && g==e->mdc_gpio)  return "ethernet";
        if (e->mdio_gpio >= 0 && g==e->mdio_gpio) return "ethernet";
        if (e->phy_rst_gpio >= 0 && g==e->phy_rst_gpio) return "ethernet";
    }
    return NULL;
}

// Hvilket interface (id) bruger pin'en, og som hvilken rolle. -1 = fri.
static int gpio_used_by(int g, const gateway_config_t *cfg, const char **role)
{
    for (int i = 0; i < cfg->interface_count; i++) {
        const iface_config_t *f = &cfg->interfaces[i];
        if (f->tx_pin  == g) { *role = "TX"; return f->id; }
        if (f->rx_pin  == g) { *role = "RX"; return f->id; }
        if (f->rts_pin == g) { *role = "DE"; return f->id; }
    }
    return -1;
}

static esp_err_t get_system_gpio_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "board_variant",
        cfg.board_variant == BOARD_ESP32_38PIN ? "38pin" : "30pin");
    cJSON *arr = cJSON_AddArrayToObject(root, "gpios");

    for (int g = 0; g <= 39; g++) {
        if (!gpio_exists(g, cfg.board_variant)) continue;
        const char *res    = gpio_reserved_by(g, &cfg.ethernet);
        bool        inonly = gpio_input_only(g);
        bool        freep  = (res == NULL);

        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "gpio",       g);
        cJSON_AddBoolToObject(e,   "tx",         freep && !inonly);
        cJSON_AddBoolToObject(e,   "rx",         freep);
        cJSON_AddBoolToObject(e,   "de",         freep && !inonly);
        cJSON_AddBoolToObject(e,   "input_only", inonly);
        cJSON_AddBoolToObject(e,   "caution",    gpio_is_strapping(g));
        if (res) cJSON_AddStringToObject(e, "reserved_by", res);
        const char *role = NULL;
        int uid = gpio_used_by(g, &cfg, &role);
        if (uid >= 0) {
            cJSON_AddNumberToObject(e, "used_by",   uid);
            cJSON_AddStringToObject(e, "used_role", role);
        }
        cJSON_AddItemToArray(arr, e);
    }

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// ── GET /api/v1/system/log?since=N ──────────────────────────────────────────

static esp_err_t get_system_log_handler(httpd_req_t *req)
{
    uint32_t since = 0;
    char qs[32] = {0};
    if (httpd_req_get_url_query_str(req, qs, sizeof(qs)) == ESP_OK) {
        char val[16] = {0};
        if (httpd_query_key_value(qs, "since", val, sizeof(val)) == ESP_OK)
            since = (uint32_t)strtoul(val, NULL, 10);
    }
    httpd_resp_set_type(req, "application/json");
    char *s = api_log_since_json(since);
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

// ── POST /api/v1/system/log/clear ───────────────────────────────────────────

static esp_err_t post_system_log_clear_handler(httpd_req_t *req)
{
    api_log_clear();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"cleared\"}");
    return ESP_OK;
}

// ── Modbus-log (dekodede bus-transaktioner) ─────────────────────────────────

static esp_err_t get_modbus_log_handler(httpd_req_t *req)
{
    uint32_t since = 0;
    char qs[32] = {0};
    if (httpd_req_get_url_query_str(req, qs, sizeof(qs)) == ESP_OK) {
        char val[16] = {0};
        if (httpd_query_key_value(qs, "since", val, sizeof(val)) == ESP_OK)
            since = (uint32_t)strtoul(val, NULL, 10);
    }
    httpd_resp_set_type(req, "application/json");
    char *s = modbus_log_since_json(since);
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

static esp_err_t post_modbus_log_clear_handler(httpd_req_t *req)
{
    modbus_log_clear();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"cleared\"}");
    return ESP_OK;
}

const httpd_uri_t route_get_modbus_log       = { .uri="/api/v1/modbus/log",          .method=HTTP_GET,  .handler=get_modbus_log_handler };
const httpd_uri_t route_post_modbus_log_clear= { .uri="/api/v1/modbus/log/clear",    .method=HTTP_POST, .handler=post_modbus_log_clear_handler };

const httpd_uri_t route_get_system          = { .uri="/api/v1/system",              .method=HTTP_GET,  .handler=get_system_handler };
const httpd_uri_t route_post_reboot         = { .uri="/api/v1/system/reboot",       .method=HTTP_POST, .handler=post_reboot_handler };
const httpd_uri_t route_get_system_hardware = { .uri="/api/v1/system/hardware",     .method=HTTP_GET,  .handler=get_system_hardware_handler };
const httpd_uri_t route_put_system_hardware = { .uri="/api/v1/system/hardware",     .method=HTTP_PUT,  .handler=put_system_hardware_handler };
const httpd_uri_t route_get_system_gpio     = { .uri="/api/v1/system/gpio",         .method=HTTP_GET,  .handler=get_system_gpio_handler };
const httpd_uri_t route_get_system_log      = { .uri="/api/v1/system/log",          .method=HTTP_GET,  .handler=get_system_log_handler };
const httpd_uri_t route_post_system_log_clear = { .uri="/api/v1/system/log/clear",  .method=HTTP_POST, .handler=post_system_log_clear_handler };
