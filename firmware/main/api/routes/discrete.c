#include "discrete.h"
#include "gateway_service.h"
#include "fc_common.h"
#include "cJSON.h"
#include <stdlib.h>

// FC02 — Read Discrete Inputs
// GET /api/v1/interfaces/{key}/slaves/{slave}/discrete-inputs?start=N&count=N
esp_err_t api_fc02_read_discrete_inputs(httpd_req_t *req, int iface, int slave)
{
    char query[64] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0, count = 1;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);
    if (httpd_query_key_value(query, "count", param, sizeof(param)) == ESP_OK) count = atoi(param);
    if (count > 2000) count = 2000;

    uint8_t bits[250] = {0};
    mb_result_t result = gw_read_discrete_inputs(iface, slave, start, count, bits);
    if (!api_mb_ok(req, result, iface, slave)) return ESP_OK;

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "function", 2);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    cJSON *arr = cJSON_AddArrayToObject(root, "inputs");
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(arr, cJSON_CreateBool((bits[i/8] >> (i%8)) & 1));
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}
