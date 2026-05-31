#include "coils.h"
#include "modbus_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// FC01 — Read Coils
// GET /api/v1/interfaces/{key}/slaves/{slave}/coils?start=N&count=N
esp_err_t api_fc01_read_coils(httpd_req_t *req, int iface, int slave)
{
    char query[64] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0, count = 1;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);
    if (httpd_query_key_value(query, "count", param, sizeof(param)) == ESP_OK) count = atoi(param);
    if (count > 2000) count = 2000;

    uint8_t bits[250] = {0};
    mb_result_t result = mb_read_coils(iface, slave, start, count, bits);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "function", 1);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    if (result.esp_err == ESP_OK) {
        cJSON *arr = cJSON_AddArrayToObject(root, "coils");
        for (int i = 0; i < count; i++)
            cJSON_AddItemToArray(arr, cJSON_CreateBool((bits[i/8] >> (i%8)) & 1));
    } else {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, result.esp_err == ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "400 Bad Request");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC05 — Write Single Coil
// PUT /api/v1/interfaces/{key}/slaves/{slave}/coils/{addr}    body: {"value":true}
esp_err_t api_fc05_write_coil(httpd_req_t *req, int iface, int slave, int addr)
{
    char body[64] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n > 0) body[n] = '\0';

    cJSON *json = cJSON_Parse(body);
    cJSON *val  = json ? cJSON_GetObjectItem(json, "value") : NULL;
    uint8_t on  = (val && cJSON_IsTrue(val)) ? 1 : 0;
    cJSON_Delete(json);

    mb_result_t result = mb_write_coil(iface, slave, addr, on);
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "coil", addr);
    cJSON_AddBoolToObject(root, "value", on);
    if (result.esp_err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, result.esp_err == ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "400 Bad Request");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC0F — Write Multiple Coils
// PUT /api/v1/interfaces/{key}/slaves/{slave}/coils?start=N   body: {"values":[true,false,...]}
esp_err_t api_fc0f_write_coils(httpd_req_t *req, int iface, int slave)
{
    char query[32] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);

    char body[512] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n > 0) body[n] = '\0';

    cJSON *json = cJSON_Parse(body);
    cJSON *vals = json ? cJSON_GetObjectItem(json, "values") : NULL;
    if (!vals || !cJSON_IsArray(vals)) {
        cJSON_Delete(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"missing 'values' array\"}");
        return ESP_OK;
    }
    int count = cJSON_GetArraySize(vals);
    if (count > 1968) count = 1968;
    uint8_t bits[250] = {0};
    for (int i = 0; i < count; i++)
        if (cJSON_IsTrue(cJSON_GetArrayItem(vals, i))) bits[i/8] |= (1 << (i%8));
    cJSON_Delete(json);

    mb_result_t result = mb_write_coils(iface, slave, start, count, bits);
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
