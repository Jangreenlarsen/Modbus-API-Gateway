#include "discrete.h"
#include "modbus_manager.h"
#include "cJSON.h"
#include <stdlib.h>

// GET /api/v1/interfaces/{iface}/slaves/{slave}/discrete-inputs?start=N&count=N  (FC02)
static esp_err_t get_discrete_inputs_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/discrete-inputs", &iface, &slave);

    char query[64] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0, count = 1;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);
    if (httpd_query_key_value(query, "count", param, sizeof(param)) == ESP_OK) count = atoi(param);
    if (count > 2000) count = 2000;

    uint8_t bits[250] = {0};
    mb_result_t result = mb_read_discrete_inputs(iface, slave, start, count, bits);

    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "interface", iface);
    cJSON_AddNumberToObject(root, "slave", slave);
    cJSON_AddNumberToObject(root, "function", 2);
    cJSON_AddNumberToObject(root, "start", start);
    cJSON_AddNumberToObject(root, "count", count);
    if (result.esp_err == ESP_OK) {
        cJSON *arr = cJSON_AddArrayToObject(root, "inputs");
        for (int i = 0; i < count; i++)
            cJSON_AddItemToArray(arr, cJSON_CreateBool((bits[i/8] >> (i%8)) & 1));
    } else {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
        httpd_resp_set_status(req, "504 Gateway Timeout");
    }
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

const httpd_uri_t route_get_discrete_inputs = {
    .uri     = "/api/v1/interfaces/*/slaves/*/discrete-inputs",
    .method  = HTTP_GET,
    .handler = get_discrete_inputs_handler,
};
