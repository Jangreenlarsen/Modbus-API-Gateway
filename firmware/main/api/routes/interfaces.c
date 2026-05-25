#include "interfaces.h"
#include "config.h"
#include "config_store.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static gateway_config_t *s_cfg = NULL;

static cJSON *iface_to_json(const iface_config_t *iface)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id",         iface->id);
    cJSON_AddStringToObject(obj, "type",       iface->type == IFACE_TYPE_RS485 ? "RS485" : "RS232");
    cJSON_AddNumberToObject(obj, "uart",       iface->uart_num);
    cJSON_AddNumberToObject(obj, "baudrate",   iface->baudrate);
    cJSON_AddNumberToObject(obj, "data_bits",  iface->data_bits);
    cJSON_AddNumberToObject(obj, "parity",     iface->parity);
    cJSON_AddNumberToObject(obj, "stop_bits",  iface->stop_bits);
    cJSON_AddNumberToObject(obj, "timeout_ms", iface->timeout_ms);
    cJSON_AddBoolToObject(obj,   "enabled",    iface->enabled);
    return obj;
}

static esp_err_t get_interfaces_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < cfg.interface_count; i++)
        cJSON_AddItemToArray(arr, iface_to_json(&cfg.interfaces[i]));
    char *s = cJSON_PrintUnformatted(arr); cJSON_Delete(arr);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

static esp_err_t get_interface_handler(httpd_req_t *req)
{
    int id = 0; sscanf(req->uri, "/api/v1/interfaces/%d", &id);
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    if (id >= cfg.interface_count) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }
    char *s = cJSON_PrintUnformatted(iface_to_json(&cfg.interfaces[id]));
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

static esp_err_t put_interface_config_handler(httpd_req_t *req)
{
    int id = 0; sscanf(req->uri, "/api/v1/interfaces/%d/config", &id);
    char body[256] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);
    cJSON *json = cJSON_Parse(body);

    gateway_config_t cfg; config_store_load(&cfg);
    if (id >= cfg.interface_count) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Interface not found"); cJSON_Delete(json); return ESP_OK;
    }
    iface_config_t *iface = &cfg.interfaces[id];
    cJSON *v;
    if ((v = cJSON_GetObjectItem(json, "baudrate"))   && cJSON_IsNumber(v)) iface->baudrate   = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "parity"))     && cJSON_IsNumber(v)) iface->parity     = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "stop_bits"))  && cJSON_IsNumber(v)) iface->stop_bits  = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "timeout_ms")) && cJSON_IsNumber(v)) iface->timeout_ms = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "enabled"))    && cJSON_IsBool(v))   iface->enabled    = cJSON_IsTrue(v);
    cJSON_Delete(json);

    config_store_save(&cfg);
    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(iface_to_json(iface));
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

const httpd_uri_t route_get_interfaces      = { .uri="/api/v1/interfaces",          .method=HTTP_GET, .handler=get_interfaces_handler };
const httpd_uri_t route_get_interface       = { .uri="/api/v1/interfaces/*",         .method=HTTP_GET, .handler=get_interface_handler };
const httpd_uri_t route_put_interface_config= { .uri="/api/v1/interfaces/*/config",  .method=HTTP_PUT, .handler=put_interface_config_handler };
