#include "interfaces.h"
#include "config.h"
#include "config_store.h"
#include "cJSON.h"
#include "coils.h"
#include "discrete.h"
#include "holding_regs.h"
#include "input_regs.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── URI-parsing — accepter både ID og navn ──────────────────────────────────
// /api/v1/interfaces/0/...      → id = 0
// /api/v1/interfaces/floor1/... → opslag på navn → id
// Returns -1 hvis ikke fundet.
static int resolve_iface(const gateway_config_t *cfg, const char *key)
{
    if (!key || !*key) return -1;
    // Hvis det starter med ciffer, tolk som ID
    if (isdigit((unsigned char)key[0])) {
        int id = atoi(key);
        if (id >= 0 && id < cfg->interface_count) return id;
        return -1;
    }
    // Ellers navn-opslag (case-insensitive)
    for (int i = 0; i < cfg->interface_count; i++) {
        if (strcasecmp(cfg->interfaces[i].name, key) == 0) return i;
    }
    return -1;
}

// Udtræk segment efter "/api/v1/interfaces/" — fx "0" eller "floor1" eller "floor1/config"
// Skriver kun selve nøglen (uden trailing /config eller andre suffixer) til out_key.
static int parse_iface_key(const char *uri, char *out_key, size_t out_sz)
{
    const char *prefix = "/api/v1/interfaces/";
    const char *p = strstr(uri, prefix);
    if (!p) return -1;
    p += strlen(prefix);
    size_t n = 0;
    while (*p && *p != '/' && *p != '?' && n + 1 < out_sz) {
        out_key[n++] = *p++;
    }
    out_key[n] = '\0';
    return (int)n;
}

// Returnerer true hvis URI'en er en config-request (ingen suffix eller /config).
// FC-routes (/slaves/...) skal ikke håndteres af config-handleren.
static bool is_config_request(const char *uri)
{
    const char *prefix = "/api/v1/interfaces/";
    const char *p = strstr(uri, prefix);
    if (!p) return false;
    p += strlen(prefix);
    // Skip nøgle-segment
    while (*p && *p != '/' && *p != '?') p++;
    // Trim query
    if (*p == '\0' || *p == '?') return true;
    // *p == '/'  → kig på suffix
    if (strncmp(p, "/config", 7) == 0 && (p[7] == '\0' || p[7] == '?')) return true;
    return false;
}

static cJSON *iface_to_json(const iface_config_t *iface)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id",         iface->id);
    cJSON_AddStringToObject(obj, "name",       iface->name);
    cJSON_AddStringToObject(obj, "type",       iface->type == IFACE_TYPE_RS485 ? "RS485" : "RS232");
    cJSON_AddStringToObject(obj, "uart_mode",  iface->uart_mode == IFACE_UART_HW ? "hw" : "sw");
    cJSON_AddStringToObject(obj, "mode",       iface->mode == IFACE_MODE_SLAVE ? "slave" : "master");
    cJSON_AddNumberToObject(obj, "slave_addr", iface->slave_addr);
    cJSON_AddNumberToObject(obj, "uart",       iface->uart_num);
    cJSON_AddNumberToObject(obj, "baudrate",   iface->baudrate);
    cJSON_AddNumberToObject(obj, "data_bits",  iface->data_bits);
    cJSON_AddNumberToObject(obj, "parity",     iface->parity);
    cJSON_AddNumberToObject(obj, "stop_bits",  iface->stop_bits);
    cJSON_AddNumberToObject(obj, "timeout_ms", iface->timeout_ms);
    cJSON_AddNumberToObject(obj, "tx_pin",     iface->tx_pin);
    cJSON_AddNumberToObject(obj, "rx_pin",     iface->rx_pin);
    cJSON_AddNumberToObject(obj, "rts_pin",    iface->rts_pin);
    cJSON_AddBoolToObject(obj,   "enabled",    iface->enabled);
    return obj;
}

static esp_err_t get_interfaces_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < cfg.interface_count; i++)
        cJSON_AddItemToArray(arr, iface_to_json(&cfg.interfaces[i]));
    char *s = cJSON_PrintUnformatted(arr); cJSON_Delete(arr);
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

__attribute__((unused))
static esp_err_t get_interface_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    if (!is_config_request(req->uri)) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"not a config route\"}");
        return ESP_OK;
    }
    char key[32] = {0};
    parse_iface_key(req->uri, key, sizeof(key));
    gateway_config_t cfg; config_store_load(&cfg);
    int id = resolve_iface(&cfg, key);
    if (id < 0) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }
    char *s = cJSON_PrintUnformatted(iface_to_json(&cfg.interfaces[id]));
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// Læs hele bodyen — httpd_req_recv kan returnere partial reads
static int recv_body(httpd_req_t *req, char *buf, int cap)
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

// PUT /api/v1/interfaces/{id|name}      (og /api/v1/interfaces/{id|name}/config — bagudkompatibel)
static esp_err_t put_interface_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    if (!is_config_request(req->uri)) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"not a config route\"}");
        return ESP_OK;
    }

    char body[768] = {0};
    recv_body(req, body, sizeof(body));

    char key[32] = {0};
    parse_iface_key(req->uri, key, sizeof(key));
    gateway_config_t cfg; config_store_load(&cfg);
    int id = resolve_iface(&cfg, key);
    if (id < 0) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"invalid JSON body\"}");
        return ESP_OK;
    }

    iface_config_t *iface = &cfg.interfaces[id];
    cJSON *v;
    if ((v = cJSON_GetObjectItem(json, "name"))       && cJSON_IsString(v) && v->valuestring[0]) {
        strncpy(iface->name, v->valuestring, sizeof(iface->name) - 1);
        iface->name[sizeof(iface->name) - 1] = '\0';
    }
    if ((v = cJSON_GetObjectItem(json, "baudrate"))   && cJSON_IsNumber(v)) iface->baudrate   = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "parity"))     && cJSON_IsNumber(v)) iface->parity     = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "stop_bits"))  && cJSON_IsNumber(v)) iface->stop_bits  = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "timeout_ms")) && cJSON_IsNumber(v)) iface->timeout_ms = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "enabled"))    && cJSON_IsBool(v))   iface->enabled    = cJSON_IsTrue(v);
    if ((v = cJSON_GetObjectItem(json, "slave_addr")) && cJSON_IsNumber(v)
        && v->valueint >= 1 && v->valueint <= 247)                          iface->slave_addr = (uint8_t)v->valueint;
    if ((v = cJSON_GetObjectItem(json, "mode"))       && cJSON_IsString(v)) {
        if      (strcasecmp(v->valuestring, "slave")  == 0) iface->mode = IFACE_MODE_SLAVE;
        else if (strcasecmp(v->valuestring, "master") == 0) iface->mode = IFACE_MODE_MASTER;
    }
    if ((v = cJSON_GetObjectItem(json, "type"))       && cJSON_IsString(v)) {
        if      (strcasecmp(v->valuestring, "RS232") == 0) iface->type = IFACE_TYPE_RS232;
        else if (strcasecmp(v->valuestring, "RS485") == 0) iface->type = IFACE_TYPE_RS485;
    }
    if ((v = cJSON_GetObjectItem(json, "uart_mode")) && cJSON_IsString(v)) {
        if      (strcasecmp(v->valuestring, "hw") == 0) iface->uart_mode = IFACE_UART_HW;
        else if (strcasecmp(v->valuestring, "sw") == 0) iface->uart_mode = IFACE_UART_SW;
    }
    if ((v = cJSON_GetObjectItem(json, "uart"))       && cJSON_IsNumber(v)) iface->uart_num = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "tx_pin"))     && cJSON_IsNumber(v)) iface->tx_pin   = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "rx_pin"))     && cJSON_IsNumber(v)) iface->rx_pin   = v->valueint;
    if ((v = cJSON_GetObjectItem(json, "rts_pin"))    && cJSON_IsNumber(v)) iface->rts_pin  = v->valueint;
    cJSON_Delete(json);

    config_store_save(&cfg);

    cJSON *resp = iface_to_json(iface);
    char *s = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

// POST /api/v1/interfaces  →  opretter nyt SW-UART master interface med defaults
static esp_err_t post_interface_handler(httpd_req_t *req)
{
    gateway_config_t cfg; config_store_load(&cfg);
    httpd_resp_set_type(req, "application/json");
    if (cfg.interface_count >= GATEWAY_MAX_IFACES) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"error\":\"max interfaces reached\"}");
        return ESP_OK;
    }
    int id = cfg.interface_count;
    iface_config_t *nf = &cfg.interfaces[id];
    memset(nf, 0, sizeof(*nf));
    nf->id         = (uint8_t)id;
    snprintf(nf->name, sizeof(nf->name), "modbus%d", id);
    nf->type       = IFACE_TYPE_RS485;
    nf->uart_mode  = IFACE_UART_SW;
    nf->mode       = IFACE_MODE_MASTER;
    nf->baudrate   = DEFAULT_BAUDRATE;
    nf->data_bits  = 8;
    nf->parity     = 0;
    nf->stop_bits  = 1;
    nf->timeout_ms = DEFAULT_TIMEOUT_MS;
    nf->tx_pin     = -1;
    nf->rx_pin     = -1;
    nf->rts_pin    = -1;
    nf->slave_addr = 1;
    nf->enabled    = 1;
    cfg.interface_count++;
    config_store_save(&cfg);

    char *s = cJSON_PrintUnformatted(iface_to_json(nf));
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_sendstr(req, s); free(s);
    return ESP_OK;
}

// DELETE /api/v1/interfaces/{id|name}
static esp_err_t delete_interface_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    if (!is_config_request(req->uri)) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"not a config route\"}");
        return ESP_OK;
    }
    char key[32] = {0};
    parse_iface_key(req->uri, key, sizeof(key));
    gateway_config_t cfg; config_store_load(&cfg);
    int id = resolve_iface(&cfg, key);
    if (id < 0) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }
    if (cfg.interface_count <= 1) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_sendstr(req, "{\"error\":\"at least one interface must remain\"}");
        return ESP_OK;
    }
    for (int k = id; k < cfg.interface_count - 1; k++) {
        cfg.interfaces[k] = cfg.interfaces[k + 1];
        cfg.interfaces[k].id = (uint8_t)k;
    }
    cfg.interface_count--;
    config_store_save(&cfg);
    httpd_resp_sendstr(req, "{\"deleted\":true}");
    return ESP_OK;
}

// ── Master GET/PUT dispatchers ──────────────────────────────────────────────
// ESP-IDF's httpd_uri_match_wildcard behandler kun * AT SLUTNINGEN af
// URI-mønsteret. Vi kan ikke registrere midt-wildcard ruter — så ALLE
// /api/v1/interfaces/* GET og PUT routes går gennem disse dispatchers, der
// parser URI'en og kalder den rette handler-funktion.

// Returner peger til segment efter "/slaves/<N>/" — eller NULL hvis ikke FC-URI.
// Sætter *slave_out til parsed slave-adresse.
static const char *find_fc_op(const char *uri, int *slave_out)
{
    const char *p = strstr(uri, "/slaves/");
    if (!p) return NULL;
    p += 8;
    *slave_out = atoi(p);
    p = strchr(p, '/');
    if (!p) return NULL;
    return p + 1;
}

static esp_err_t master_get_dispatcher(httpd_req_t *req)
{
    char key[32] = {0};
    parse_iface_key(req->uri, key, sizeof(key));
    gateway_config_t cfg; config_store_load(&cfg);
    int iface = resolve_iface(&cfg, key);
    if (iface < 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }

    int slave = 0;
    const char *op = find_fc_op(req->uri, &slave);
    if (!op) {
        // Config GET: /api/v1/interfaces/{key} (eller /{key}/config bagudkompat)
        if (!is_config_request(req->uri)) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_status(req, "404 Not Found");
            httpd_resp_sendstr(req, "{\"error\":\"unknown route\"}");
            return ESP_OK;
        }
        httpd_resp_set_type(req, "application/json");
        char *s = cJSON_PrintUnformatted(iface_to_json(&cfg.interfaces[iface]));
        httpd_resp_sendstr(req, s); free(s);
        return ESP_OK;
    }

    if (strncmp(op, "coils",            5)  == 0 && (op[5]  == '\0' || op[5]  == '?')) return api_fc01_read_coils(req, iface, slave);
    if (strncmp(op, "discrete-inputs",  15) == 0 && (op[15] == '\0' || op[15] == '?')) return api_fc02_read_discrete_inputs(req, iface, slave);
    if (strncmp(op, "holding-registers",17) == 0 && (op[17] == '\0' || op[17] == '?')) return api_fc03_read_holding_regs(req, iface, slave);
    if (strncmp(op, "input-registers",  15) == 0 && (op[15] == '\0' || op[15] == '?')) return api_fc04_read_input_regs(req, iface, slave);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_sendstr(req, "{\"error\":\"unknown FC operation\"}");
    return ESP_OK;
}

static esp_err_t master_put_dispatcher(httpd_req_t *req)
{
    char key[32] = {0};
    parse_iface_key(req->uri, key, sizeof(key));
    gateway_config_t cfg; config_store_load(&cfg);
    int iface = resolve_iface(&cfg, key);
    if (iface < 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "{\"error\":\"interface not found\"}");
        return ESP_OK;
    }

    int slave = 0;
    const char *op = find_fc_op(req->uri, &slave);
    if (!op) {
        // Config PUT (fx /interfaces/0 eller /interfaces/0/config)
        return put_interface_handler(req);
    }

    // FC05: coils/{addr}      FC0F: coils
    if (strncmp(op, "coils", 5) == 0) {
        if (op[5] == '/') {
            int addr = atoi(op + 6);
            return api_fc05_write_coil(req, iface, slave, addr);
        }
        if (op[5] == '\0' || op[5] == '?') return api_fc0f_write_coils(req, iface, slave);
    }
    // FC06: holding-registers/{addr}      FC10: holding-registers
    if (strncmp(op, "holding-registers", 17) == 0) {
        if (op[17] == '/') {
            int addr = atoi(op + 18);
            return api_fc06_write_holding_reg(req, iface, slave, addr);
        }
        if (op[17] == '\0' || op[17] == '?') return api_fc10_write_holding_regs(req, iface, slave);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_sendstr(req, "{\"error\":\"unknown FC write operation\"}");
    return ESP_OK;
}

const httpd_uri_t route_get_interfaces       = { .uri="/api/v1/interfaces",   .method=HTTP_GET,    .handler=get_interfaces_handler };
const httpd_uri_t route_get_interface        = { .uri="/api/v1/interfaces/*", .method=HTTP_GET,    .handler=master_get_dispatcher };
const httpd_uri_t route_put_interface_config = { .uri="/api/v1/interfaces/*", .method=HTTP_PUT,    .handler=master_put_dispatcher };
const httpd_uri_t route_post_interface       = { .uri="/api/v1/interfaces",   .method=HTTP_POST,   .handler=post_interface_handler };
const httpd_uri_t route_delete_interface     = { .uri="/api/v1/interfaces/*", .method=HTTP_DELETE, .handler=delete_interface_handler };
