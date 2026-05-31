#include "holding_regs.h"
#include "modbus_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "route_hreg";

// FC03 — Read Holding Registers
// GET /api/v1/interfaces/{key}/slaves/{slave}/holding-registers?start=N&count=N
esp_err_t api_fc03_read_holding_regs(httpd_req_t *req, int iface, int slave)
{
    char query[64] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
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
    (void)TAG;
    return ESP_OK;
}

// FC06 — Write Single Register
// PUT /api/v1/interfaces/{key}/slaves/{slave}/holding-registers/{addr}    body: {"value":1234}
esp_err_t api_fc06_write_holding_reg(httpd_req_t *req, int iface, int slave, int addr)
{
    char body[64] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"empty body\"}");
        return ESP_OK;
    }
    body[n] = '\0';

    cJSON *json = cJSON_Parse(body);
    cJSON *val  = json ? cJSON_GetObjectItem(json, "value") : NULL;
    if (!val || !cJSON_IsNumber(val)) {
        cJSON_Delete(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"missing 'value'\"}");
        return ESP_OK;
    }
    uint16_t value = (uint16_t)val->valueint;
    cJSON_Delete(json);

    mb_result_t result = mb_write_register(iface, slave, addr, value);
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "register", addr);
    cJSON_AddNumberToObject(root, "value", value);
    if (result.esp_err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, result.esp_err == ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "400 Bad Request");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC10 — Write Multiple Registers
// PUT /api/v1/interfaces/{key}/slaves/{slave}/holding-registers?start=N   body: {"values":[...]}
esp_err_t api_fc10_write_holding_regs(httpd_req_t *req, int iface, int slave)
{
    char query[32] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);

    char body[512] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"empty body\"}");
        return ESP_OK;
    }
    body[n] = '\0';

    cJSON *json  = cJSON_Parse(body);
    cJSON *vals  = json ? cJSON_GetObjectItem(json, "values") : NULL;
    if (!vals || !cJSON_IsArray(vals)) {
        cJSON_Delete(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"missing 'values' array\"}");
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
    if (result.esp_err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, result.esp_err == ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "400 Bad Request");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}
