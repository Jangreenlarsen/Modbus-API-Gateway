#include "holding_regs.h"
#include "gateway_service.h"
#include "fc_common.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "route_hreg";

// FC03 — Read Holding Registers
// GET /api/v1/interfaces/{key}/slaves/{slave}/holding-registers?start=N&count=N
esp_err_t api_fc03_read_holding_regs(httpd_req_t *req, int iface, int slave)
{
    char query[64] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = api_query_u16(query, "start", 0);
    uint16_t count = api_query_u16(query, "count", 1);
    if (count > 125) count = 125;

    uint16_t regs[125];
    mb_result_t result = gw_read_holding_registers(iface, slave, start, count, regs);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
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
    int n = api_recv_body(req, body, sizeof(body));
    if (n <= 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"empty body\"}");
        return ESP_OK;
    }

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

    mb_result_t result = gw_write_register(iface, slave, addr, value);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "register", addr);
    cJSON_AddNumberToObject(root, "value", value);
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC10 — Write Multiple Registers
// PUT /api/v1/interfaces/{key}/slaves/{slave}/holding-registers?start=N   body: {"values":[...]}
esp_err_t api_fc10_write_holding_regs(httpd_req_t *req, int iface, int slave)
{
    char query[32] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = api_query_u16(query, "start", 0);

    char body[512] = {0};
    int n = api_recv_body(req, body, sizeof(body));
    if (n <= 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"empty body\"}");
        return ESP_OK;
    }

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

    mb_result_t result = gw_write_registers(iface, slave, start, count, regs);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}
