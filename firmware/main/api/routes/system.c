#include "system.h"
#include "config.h"
#include "version.h"
#include "ethernet.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <stdlib.h>

static esp_err_t get_system_handler(httpd_req_t *req)
{
    char ip[16]; ethernet_get_ip(ip, sizeof(ip));
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version",    GATEWAY_VERSION);
    cJSON_AddNumberToObject(root, "uptime_s",   esp_timer_get_time() / 1000000);
    cJSON_AddStringToObject(root, "ip",         ip);
    cJSON_AddNumberToObject(root, "free_heap",  esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "reset_reason", esp_reset_reason());
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

const httpd_uri_t route_get_system  = { .uri="/api/v1/system",        .method=HTTP_GET,  .handler=get_system_handler };
const httpd_uri_t route_post_reboot = { .uri="/api/v1/system/reboot", .method=HTTP_POST, .handler=post_reboot_handler };
