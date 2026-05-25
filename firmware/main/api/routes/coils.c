#include "coils.h"
#include "modbus_manager.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// GET /api/v1/interfaces/{iface}/slaves/{slave}/coils?start=N&count=N  (FC01)
static esp_err_t get_coils_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/coils", &iface, &slave);

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

// PUT /api/v1/interfaces/{iface}/slaves/{slave}/coils/{addr}  (FC05)
// Body: {"value": true}
static esp_err_t put_coil_single_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0, addr = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/coils/%d", &iface, &slave, &addr);

    char body[64] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);
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
    if (result.esp_err != ESP_OK)
        cJSON_AddStringToObject(root, "error", esp_err_to_name(result.esp_err));
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// PUT /api/v1/interfaces/{iface}/slaves/{slave}/coils?start=N  (FC0F)
// Body: {"values": [true, false, true]}
static esp_err_t put_coil_multi_handler(httpd_req_t *req)
{
    int iface = 0, slave = 0;
    sscanf(req->uri, "/api/v1/interfaces/%d/slaves/%d/coils", &iface, &slave);

    char query[32] = {0}; char param[16];
    httpd_req_get_url_query_str(req, query, sizeof(query));
    uint16_t start = 0;
    if (httpd_query_key_value(query, "start", param, sizeof(param)) == ESP_OK) start = atoi(param);

    char body[512] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);
    cJSON *json = cJSON_Parse(body);
    cJSON *vals = json ? cJSON_GetObjectItem(json, "values") : NULL;
    if (!vals || !cJSON_IsArray(vals)) {
        cJSON_Delete(json); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'values'"); return ESP_OK;
    }
    int count = cJSON_GetArraySize(vals);
    uint8_t bits[250] = {0};
    for (int i = 0; i < count && i < 1968; i++)
        if (cJSON_IsTrue(cJSON_GetArrayItem(vals, i))) bits[i/8] |= (1 << (i%8));
    cJSON_Delete(json);

    mb_result_t result = mb_write_coils(iface, slave, start, count, bits);
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

const httpd_uri_t route_get_coils          = { .uri="/api/v1/interfaces/*/slaves/*/coils",   .method=HTTP_GET, .handler=get_coils_handler };
const httpd_uri_t route_put_coil_single    = { .uri="/api/v1/interfaces/*/slaves/*/coils/*", .method=HTTP_PUT, .handler=put_coil_single_handler };
const httpd_uri_t route_put_coil_multi     = { .uri="/api/v1/interfaces/*/slaves/*/coils",   .method=HTTP_PUT, .handler=put_coil_multi_handler };
