#include "cache.h"
#include "register_cache.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *fc_name(uint8_t fc)
{
    switch (fc) {
        case CACHE_FC_COIL:     return "coil";
        case CACHE_FC_DISCRETE: return "discrete";
        case CACHE_FC_HOLDING:  return "holding";
        case CACHE_FC_INPUT:    return "input";
        default:                return "?";
    }
}

static const char *status_name(uint8_t s)
{
    switch (s) {
        case CACHE_ENTRY_VALID: return "valid";
        case CACHE_ENTRY_ERROR: return "error";
        default:                return "empty";
    }
}

// GET /api/v1/cache/stats
static esp_err_t get_cache_stats_handler(httpd_req_t *req)
{
    const cache_stats_t *st = cache_get_stats();
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root,   "enabled",        st->enabled != 0);
    cJSON_AddNumberToObject(root, "ttl_ms",         st->ttl_ms);
    cJSON_AddNumberToObject(root, "max_entries",    CACHE_MAX_ENTRIES);
    cJSON_AddNumberToObject(root, "entries_used",   st->entries_used);
    cJSON_AddNumberToObject(root, "hits",           st->hits);
    cJSON_AddNumberToObject(root, "misses",         st->misses);
    cJSON_AddNumberToObject(root, "errors",         st->errors);
    cJSON_AddNumberToObject(root, "evictions",      st->evictions);
    cJSON_AddNumberToObject(root, "total_requests", st->total_requests);
    uint32_t total = st->hits + st->misses;
    double hit_rate = (total > 0) ? (100.0 * st->hits / total) : 0.0;
    cJSON_AddNumberToObject(root, "hit_rate_pct",  hit_rate);
    double util = 100.0 * st->entries_used / (double)CACHE_MAX_ENTRIES;
    cJSON_AddNumberToObject(root, "utilization_pct", util);
    cJSON_AddNumberToObject(root, "since_ms",      st->since_ms);
    cJSON_AddNumberToObject(root, "now_ms",        now);

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(root); cJSON_Delete(root);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// GET /api/v1/cache/entries
static esp_err_t get_cache_entries_handler(httpd_req_t *req)
{
    cache_entry_t *buf = malloc(sizeof(cache_entry_t) * CACHE_MAX_ENTRIES);
    if (!buf) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "{\"error\":\"no memory\"}");
        return ESP_OK;
    }
    int n = cache_get_entries(buf, CACHE_MAX_ENTRIES);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "iface",  buf[i].iface);
        cJSON_AddNumberToObject(e, "slave",  buf[i].slave);
        cJSON_AddStringToObject(e, "fc",     fc_name(buf[i].fc));
        cJSON_AddNumberToObject(e, "addr",   buf[i].addr);
        cJSON_AddNumberToObject(e, "value",  buf[i].value);
        cJSON_AddStringToObject(e, "status", status_name(buf[i].status));
        cJSON_AddNumberToObject(e, "hits",   buf[i].hits);
        cJSON_AddNumberToObject(e, "age_ms", now - buf[i].last_update_ms);
        cJSON_AddItemToArray(arr, e);
    }
    free(buf);

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_PrintUnformatted(arr); cJSON_Delete(arr);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// POST /api/v1/cache/clear
static esp_err_t post_cache_clear_handler(httpd_req_t *req)
{
    cache_clear();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"cleared\":true}");
    return ESP_OK;
}

// POST /api/v1/cache/reset-stats
static esp_err_t post_cache_reset_stats_handler(httpd_req_t *req)
{
    cache_reset_stats();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"reset\":true}");
    return ESP_OK;
}

// PUT /api/v1/cache/config   body: {"enabled":true,"ttl_ms":1000}
static esp_err_t put_cache_config_handler(httpd_req_t *req)
{
    char body[128] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n > 0) body[n] = '\0';

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid JSON\"}");
        return ESP_OK;
    }
    cJSON *v;
    if ((v = cJSON_GetObjectItem(json, "enabled")) && cJSON_IsBool(v))
        cache_set_enabled(cJSON_IsTrue(v));
    if ((v = cJSON_GetObjectItem(json, "ttl_ms")) && cJSON_IsNumber(v) && v->valueint >= 0)
        cache_set_ttl_ms((uint32_t)v->valueint);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"enabled\":%s,\"ttl_ms\":%lu}",
             cache_is_enabled() ? "true" : "false",
             (unsigned long)cache_get_ttl_ms());
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

const httpd_uri_t route_get_cache_stats        = { .uri="/api/v1/cache/stats",        .method=HTTP_GET,  .handler=get_cache_stats_handler };
const httpd_uri_t route_get_cache_entries      = { .uri="/api/v1/cache/entries",      .method=HTTP_GET,  .handler=get_cache_entries_handler };
const httpd_uri_t route_post_cache_clear       = { .uri="/api/v1/cache/clear",        .method=HTTP_POST, .handler=post_cache_clear_handler };
const httpd_uri_t route_post_cache_reset_stats = { .uri="/api/v1/cache/reset-stats",  .method=HTTP_POST, .handler=post_cache_reset_stats_handler };
const httpd_uri_t route_put_cache_config       = { .uri="/api/v1/cache/config",       .method=HTTP_PUT,  .handler=put_cache_config_handler };
