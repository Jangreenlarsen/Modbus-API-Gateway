#include "holding_regs.h"
#include "modbus_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "route_hreg";

// GET /api/v1/interfaces/{iface}/slaves/{slave}/holding-registers?start=N&count=N
static esp_err_t get_holding_regs_handler(httpd_req_t *req)
{
    // Udtræk path-parametre fra URI
    // URI form: /api/v1/interfaces/0/slaves/3/holding-registers?start=100&count=10
    int iface = 0, slave = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/holding-registers", &iface, &slave);

    // Query string
    char query[64] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    char param[16];
    uint16_t start = 0, count = 1;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);
    if (httpd_query_key_value(query, "count", param, sizeof(param)) == ESP_OK) count = atoi(param);
    if (count > 125) count = 125;

    uint16_t regs[125];
    mb_result_t result = mb_read_holding_registers(iface, slave, start, count, regs);

    httpd_resp_set_type(req, "application/json");

    if (result.esp_err == ESP_ERR_TIMEOUT) {
        httpd_resp_set_status(req, "504 Gateway Timeout");
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "modbus_timeout");
        cJSON_AddNumberToObject(err, "interface", iface);
        cJSON_AddNumberToObject(err, "slave", slave);
        char *s = cJSON_PrintUnformatted(err); cJSON_Delete(err);
        httpd_resp_sendstr(req, s); free(s);
        return ESP_OK;
    }
    if (result.modbus_exception) {
        httpd_resp_set_status(req, "400 Bad Request");
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "error", "modbus_exception");
        cJSON_AddNumberToObject(err, "exception_code", result.modbus_exception);
        char *s = cJSON_PrintUnformatted(err); cJSON_Delete(err);
        httpd_resp_sendstr(req, s); free(s);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "function", 3);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    cJSON *arr = cJSON_AddArrayToObject(root, "registers");
    for (int i = 0; i < count; i++) cJSON_AddItemToArray(arr, cJSON_CreateNumber(regs[i]));

    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// PUT /api/v1/interfaces/{iface}/slaves/{slave}/holding-registers/{reg}
// Body: {"value": 1234}
static esp_err_t put_holding_reg_single_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0, reg = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/holding-registers/%d", &iface, &slave, &reg);

    char body[64] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body"); return ESP_OK; }

    cJSON *json = cJSON_Parse(body);
    cJSON *val  = json ? cJSON_GetObjectItem(json, "value") : NULL;
    if (!val || !cJSON_IsNumber(val)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'value'");
        return ESP_OK;
    }
    uint16_t value = (uint16_t)val->valueint;
    cJSON_Delete(json);

    mb_result_t result = mb_write_register(iface, slave, reg, value);
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "register", reg);
    cJSON_AddNumberToObject(root, "value", value);
    if (result.esp_err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, result.esp_err == ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "400 Bad Request");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// PUT /api/v1/interfaces/{iface}/slaves/{slave}/holding-registers?start=N
// Body: {"values": [1234, 5678]}
static esp_err_t put_holding_reg_multi_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/holding-registers", &iface, &slave);

    char query[32] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);

    char body[512] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body"); return ESP_OK; }

    cJSON *json  = cJSON_Parse(body);
    cJSON *vals  = json ? cJSON_GetObjectItem(json, "values") : NULL;
    if (!vals || !cJSON_IsArray(vals)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'values' array");
        return ESP_OK;
    }

    int count = cJSON_GetArraySize(vals);
    if (count > 123) count = 123;
    uint16_t regs[123];
    for (int i = 0; i < count; i++) regs[i] = (uint16_t)cJSON_GetArrayItem(vals, i)->valueint;
    cJSON_Delete(json);

    mb_result_t result = mb_write_registers(iface, slave, start, count, regs);
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    if (result.esp_err != ESP_OK)
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

const httpd_uri_t route_get_holding_regs = {
    .uri     = "/api/v1/interfaces/*/slaves/*/holding-registers",
    .method  = HTTP_GET,
    .handler = get_holding_regs_handler,
};
const httpd_uri_t route_put_holding_reg_single = {
    .uri     = "/api/v1/interfaces/*/slaves/*/holding-registers/*",
    .method  = HTTP_PUT,
    .handler = put_holding_reg_single_handler,
};
const httpd_uri_t route_put_holding_reg_multi = {
    .uri     = "/api/v1/interfaces/*/slaves/*/holding-registers",
    .method  = HTTP_PUT,
    .handler = put_holding_reg_multi_handler,
};
