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
    cJSON_AddStringToObject(obj, "uart_mode",  iface->uart_mode == IFACE_UART_HW ? "hw" : "sw");
    cJSON_AddStringToObject(obj, "mode",       iface->mode == IFACE_MODE_SLAVE ? "slave" : "master");
    cJSON_AddNumberToObject(obj, "slave_addr", iface->slave_addr);
    cJSON_AddNumberToObject(obj, "uart",       iface->uart_num);
    cJSON_AddNumberToObject(obj, "baudrate",   iface->baudrate);
    cJSON_AddNumberToObject(obj, "data_bits",  iface->data_bits);
    cJSON_AddNumberToObject(obj, "parity",     iface->parity);
    cJSON_AddNumberToObject(obj, "stop_bits",  iface->stop_bits);
    cJSON_AddNumberToObject(obj, "timeout_ms", iface->timeout_ms);
    cJSON_AddNumberToObject(obj, "tx_pin",     iface->tx_pin);
    cJSON_AddNumberToObject(obj, "rx_pin",     iface->rx_pin);
    cJSON_AddNumberToObject(obj, "rts_pin",    iface->rts_pin);
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
    char body[512] = {0};
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
    if ((v = cJSON_GetObjectItem(json, "slave_addr")) && cJSON_IsNumber(v)
        && v->valueint >= 1 && v->valueint <= 247)                          iface->slave_addr = (uint8_t)v->valueint;
    if ((v = cJSON_GetObjectItem(json, "mode"))       && cJSON_IsString(v)) {
        if      (strcasecmp(v->valuestring, "slave")  == 0) iface->mode = IFACE_MODE_SLAVE;
        else if (strcasecmp(v->valuestring, "master") == 0) iface->mode = IFACE_MODE_MASTER;
    }
    if ((v = cJSON_GetObjectItem(json, "type"))       && cJSON_IsString(v)) {
        if      (strcasecmp(v->valuestring, "RS232") == 0) iface->type = IFACE_TYPE_RS232;
        else if (strcasecmp(v->valuestring, "RS485") == 0) iface->type = IFACE_TYPE_RS485;
    }
    if ((v = cJSON_GetObjectItem(json, "tx_pin"))     && cJSON_IsNumber(v)) iface->tx_pin  = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "rx_pin"))     && cJSON_IsNumber(v)) iface->rx_pin  = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "rts_pin"))    && cJSON_IsNumber(v)) iface->rts_pin = v->valueint;
    cJSON_Delete(json);

    config_store_save(&cfg);
    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(iface_to_json(iface));
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// POST /api/v1/interfaces  →  opretter nyt SW-UART master interface med defaults
static esp_err_t post_interface_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    if (cfg.interface_count >= GATEWAY_MAX_IFACES) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"error\":\"max interfaces reached\"}");
        return ESP_OK;
    }
    int id = cfg.interface_count;
    iface_config_t *nf = &cfg.interfaces[id];
    memset(nf, 0, sizeof(*nf));
    nf->id         = (uint8_t)id;
    nf->type       = IFACE_TYPE_RS485;
    nf->uart_mode  = IFACE_UART_SW;
    nf->mode       = IFACE_MODE_MASTER;
    nf->baudrate   = DEFAULT_BAUDRATE;
    nf->data_bits  = 8;
    nf->parity     = 0;
    nf->stop_bits  = 1;
    nf->timeout_ms = DEFAULT_TIMEOUT_MS;
    nf->tx_pin     = -1;
    nf->rx_pin     = -1;
    nf->rts_pin    = -1;
    nf->slave_addr = 1;
    nf->enabled    = 1;
    cfg.interface_count++;
    config_store_save(&cfg);

    char *s = cJSON_PrintUnformatted(iface_to_json(nf));
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// DELETE /api/v1/interfaces/{id}  →  sletter og renummererer remaining
static esp_err_t delete_interface_handler(httpd_req_t *req)
{
    int id = 0; sscanf(req->uri, "/api/v1/interfaces/%d", &id);
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    if (id < 0 || id >= cfg.interface_count) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }
    if (cfg.interface_count <= 1) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"error\":\"at least one interface must remain\"}");
        return ESP_OK;
    }
    for (int k = id; k < cfg.interface_count - 1; k++) {
        cfg.interfaces[k] = cfg.interfaces[k + 1];
        cfg.interfaces[k].id = (uint8_t)k;
    }
    cfg.interface_count--;
    config_store_save(&cfg);
    httpd_resp_sendstr(req, "{\"deleted\":true}");
    return ESP_OK;
}

const httpd_uri_t route_get_interfaces       = { .uri="/api/v1/interfaces",          .method=HTTP_GET,    .handler=get_interfaces_handler };
const httpd_uri_t route_get_interface        = { .uri="/api/v1/interfaces/*",        .method=HTTP_GET,    .handler=get_interface_handler };
const httpd_uri_t route_put_interface_config = { .uri="/api/v1/interfaces/*/config", .method=HTTP_PUT,    .handler=put_interface_config_handler };
const httpd_uri_t route_post_interface       = { .uri="/api/v1/interfaces",          .method=HTTP_POST,   .handler=post_interface_handler };
const httpd_uri_t route_delete_interface     = { .uri="/api/v1/interfaces/*",        .method=HTTP_DELETE, .handler=delete_interface_handler };
