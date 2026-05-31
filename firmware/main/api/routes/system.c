#include "system.h"
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

const httpd_uri_t route_get_system          = { .uri="/api/v1/system",          .method=HTTP_GET,  .handler=get_system_handler };
const httpd_uri_t route_post_reboot         = { .uri="/api/v1/system/reboot",   .method=HTTP_POST, .handler=post_reboot_handler };
const httpd_uri_t route_get_system_hardware = { .uri="/api/v1/system/hardware", .method=HTTP_GET,  .handler=get_system_hardware_handler };
const httpd_uri_t route_put_system_hardware = { .uri="/api/v1/system/hardware", .method=HTTP_PUT,  .handler=put_system_hardware_handler };
