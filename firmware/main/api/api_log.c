#include "api_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static api_log_entry_t s_buf[API_LOG_CAP];
static int             s_head  = 0;
static int             s_count = 0;
static uint32_t        s_seq   = 0;

static const char *method_name(httpd_method_t m)
{
    switch (m) {
        case HTTP_GET:    return "GET";
        case HTTP_POST:   return "POST";
        case HTTP_PUT:    return "PUT";
        case HTTP_DELETE: return "DELETE";
        default:          return "???";
    }
}

void api_log_init(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    s_head = 0; s_count = 0; s_seq = 0;
}

void api_log_append(httpd_req_t *req)
{
    // Log-endpoint selv logges ikke — undgår støj
    if (strncmp(req->uri, "/api/v1/system/log", 18) == 0) return;

    api_log_entry_t *e = &s_buf[s_head];
    e->seq   = ++s_seq;
    e->ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    strncpy(e->method, method_name(req->method), sizeof(e->method) - 1);
    e->method[sizeof(e->method) - 1] = '\0';
    strncpy(e->uri, req->uri, sizeof(e->uri) - 1);
    e->uri[sizeof(e->uri) - 1] = '\0';
    s_head = (s_head + 1) % API_LOG_CAP;
    if (s_count < API_LOG_CAP) s_count++;
}

// Returnerer JSON: {"n":<total_ever>, "entries":[{seq,t,m,u}, ...]}
// Kun entries med seq > since_seq inkluderes.
char *api_log_since_json(uint32_t since_seq)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "n", (double)s_seq);
    cJSON *arr  = cJSON_AddArrayToObject(root, "entries");

    int start = (s_count < API_LOG_CAP) ? 0 : s_head;
    for (int i = 0; i < s_count; i++) {
        api_log_entry_t *e = &s_buf[(start + i) % API_LOG_CAP];
        if (e->seq <= since_seq) continue;
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "seq", (double)e->seq);
        cJSON_AddNumberToObject(obj, "t",   (double)e->ts_ms);
        cJSON_AddStringToObject(obj, "m",   e->method);
        cJSON_AddStringToObject(obj, "u",   e->uri);
        cJSON_AddItemToArray(arr, obj);
    }
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

void api_log_clear(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    s_head = 0; s_count = 0;
    // s_seq nulstilles ikke — klienten kan bibeholde sin since-pointer
}
