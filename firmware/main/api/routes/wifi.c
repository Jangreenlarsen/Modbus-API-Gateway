#include "wifi.h"
#include "wifi_manager.h"
#include "config_store.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static esp_err_t get_wifi_status_handler(httpd_req_t *req)
{
    wifi_status_t st = wifi_manager_get_status();
    const char *state_str[] = { "disabled","connecting","connected","ap_mode","error" };

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state", state_str[st.state]);
    cJSON_AddStringToObject(root, "ssid",  st.ssid);
    cJSON_AddStringToObject(root, "ip",    st.ip);
    cJSON_AddNumberToObject(root, "rssi",  st.rssi);

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

static esp_err_t put_wifi_config_handler(httpd_req_t *req)
{
    char body[512] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);
    cJSON *json = cJSON_Parse(body);
    if (!json) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON"); return ESP_OK; }

    gateway_config_t cfg; config_store_load(&cfg);
    wifi_config_gw_t *w = &cfg.wifi;
    cJSON *v;

    if ((v = cJSON_GetObjectItem(json, "enabled"))     && cJSON_IsBool(v))   w->enabled     = cJSON_IsTrue(v);
    if ((v = cJSON_GetObjectItem(json, "ssid"))        && cJSON_IsString(v)) strncpy(w->ssid,        v->valuestring, sizeof(w->ssid)-1);
    if ((v = cJSON_GetObjectItem(json, "password"))    && cJSON_IsString(v)) strncpy(w->password,    v->valuestring, sizeof(w->password)-1);
    if ((v = cJSON_GetObjectItem(json, "ip"))          && cJSON_IsString(v)) strncpy(w->ip,          v->valuestring, sizeof(w->ip)-1);
    if ((v = cJSON_GetObjectItem(json, "ap_fallback")) && cJSON_IsBool(v))   w->ap_fallback = cJSON_IsTrue(v);
    if ((v = cJSON_GetObjectItem(json, "ap_ssid"))     && cJSON_IsString(v)) strncpy(w->ap_ssid,     v->valuestring, sizeof(w->ap_ssid)-1);
    if ((v = cJSON_GetObjectItem(json, "ap_password")) && cJSON_IsString(v)) strncpy(w->ap_password, v->valuestring, sizeof(w->ap_password)-1);
    cJSON_Delete(json);

    config_store_save(&cfg);
    wifi_manager_reconfigure(w);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"wifi_reconfigured\"}");
    return ESP_OK;
}

static esp_err_t get_wifi_scan_handler(httpd_req_t *req)
{
    char *s = wifi_manager_scan();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

const httpd_uri_t route_get_wifi_status = { .uri="/api/v1/system/wifi",      .method=HTTP_GET, .handler=get_wifi_status_handler };
const httpd_uri_t route_put_wifi_config = { .uri="/api/v1/system/wifi",      .method=HTTP_PUT, .handler=put_wifi_config_handler };
const httpd_uri_t route_get_wifi_scan   = { .uri="/api/v1/system/wifi/scan", .method=HTTP_GET, .handler=get_wifi_scan_handler };
