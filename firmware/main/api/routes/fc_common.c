#include "fc_common.h"
#include "cJSON.h"
#include "esp_err.h"
#include <stdlib.h>

static const char *exception_desc(uint8_t code)
{
    switch (code) {
        case 1:  return "Illegal Function";
        case 2:  return "Illegal Data Address";
        case 3:  return "Illegal Data Value";
        case 4:  return "Slave Device Failure";
        case 5:  return "Acknowledge";
        case 6:  return "Slave Device Busy";
        case 8:  return "Memory Parity Error";
        case 10: return "Gateway Path Unavailable";
        case 11: return "Gateway Target Device Failed To Respond";
        default: return "Modbus Exception";
    }
}

bool api_mb_ok(httpd_req_t *req, mb_result_t r, int iface, int slave)
{
    if (r.esp_err == ESP_OK && r.modbus_exception == 0) return true;

    httpd_resp_set_type(req, "application/json");
    cJSON *e = cJSON_CreateObject();
    cJSON_AddNumberToObject(e, "interface", iface);
    cJSON_AddNumberToObject(e, "slave", slave);

    if (r.esp_err == ESP_ERR_TIMEOUT) {
        httpd_resp_set_status(req, "504 Gateway Timeout");
        cJSON_AddStringToObject(e, "error", "modbus_timeout");
        cJSON_AddStringToObject(e, "description", "No response within timeout");
    } else if (r.modbus_exception != 0) {
        // Bemærk (H2): esp-modbus v1.x eksponerer ikke exception-koden via
        // mbc_master_send_request — derfor er exception_code p.t. kun tilgængelig
        // på SW-UART-interfaces. HW-UART-fejl rapporteres som modbus_error nedenfor.
        httpd_resp_set_status(req, "400 Bad Request");
        cJSON_AddStringToObject(e, "error", "modbus_exception");
        cJSON_AddNumberToObject(e, "exception_code", r.modbus_exception);
        cJSON_AddStringToObject(e, "description", exception_desc(r.modbus_exception));
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        cJSON_AddStringToObject(e, "error", "modbus_error");
        cJSON_AddStringToObject(e, "detail", esp_err_to_name(r.esp_err));
    }

    char *s = cJSON_PrintUnformatted(e);
    cJSON_Delete(e);
    httpd_resp_sendstr(req, s);
    free(s);
    return false;
}

int api_recv_body(httpd_req_t *req, char *buf, int cap)
{
    int remaining = req->content_len;
    if (remaining < 0) remaining = 0;
    if (remaining > cap - 1) remaining = cap - 1;
    int total = 0;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf + total, remaining);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) break;
        total += n; remaining -= n;
    }
    buf[total] = '\0';
    return total;
}

uint16_t api_query_u16(const char *query, const char *key, uint16_t def)
{
    char param[16];
    if (httpd_query_key_value(query, key, param, sizeof(param)) != ESP_OK) return def;
    long v = strtol(param, NULL, 10);
    if (v < 0) v = 0;
    if (v > 65535) v = 65535;
    return (uint16_t)v;
}
