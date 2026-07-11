#include "coils.h"
#include "gateway_service.h"
#include "fc_common.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// FC01 — Read Coils
// GET /api/v1/interfaces/{key}/slaves/{slave}/coils?start=N&count=N
esp_err_t api_fc01_read_coils(httpd_req_t *req, int iface, int slave)
{
    char query[64] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = api_query_u16(query, "start", 0);
    uint16_t count = api_query_u16(query, "count", 1);
    if (count > 2000) count = 2000;

    uint8_t bits[250] = {0};
    mb_result_t result = gw_read_coils(iface, slave, start, count, bits);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "function", 1);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    cJSON *arr = cJSON_AddArrayToObject(root, "coils");
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(arr, cJSON_CreateBool((bits[i/8] >> (i%8)) & 1));
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC05 — Write Single Coil
// PUT /api/v1/interfaces/{key}/slaves/{slave}/coils/{addr}    body: {"value":true}
esp_err_t api_fc05_write_coil(httpd_req_t *req, int iface, int slave, int addr)
{
    char body[64] = {0};
    api_recv_body(req, body, sizeof(body));

    cJSON *json = cJSON_Parse(body);
    cJSON *val  = json ? cJSON_GetObjectItem(json, "value") : NULL;
    // L5: afvis tom/ugyldig body i stedet for stille at skrive coil=0.
    if (!val || !(cJSON_IsBool(val) || cJSON_IsNumber(val))) {
        cJSON_Delete(json);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"missing 'value' (bool)\"}");
        return ESP_OK;
    }
    uint8_t on = (cJSON_IsTrue(val) || (cJSON_IsNumber(val) && val->valueint != 0)) ? 1 : 0;
    cJSON_Delete(json);

    mb_result_t result = gw_write_coil(iface, slave, addr, on);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "coil", addr);
    cJSON_AddBoolToObject(root, "value", on);
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// FC0F — Write Multiple Coils
// PUT /api/v1/interfaces/{key}/slaves/{slave}/coils?start=N   body: {"values":[true,false,...]}
esp_err_t api_fc0f_write_coils(httpd_req_t *req, int iface, int slave)
{
    char query[32] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = api_query_u16(query, "start", 0);

    char body[512] = {0};
    api_recv_body(req, body, sizeof(body));

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

    mb_result_t result = gw_write_coils(iface, slave, start, count, bits);
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
