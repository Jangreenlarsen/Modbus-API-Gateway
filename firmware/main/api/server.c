#include "server.h"
#include "api_log.h"
#include "routes/interfaces.h"
#include "routes/cache.h"
#include "routes/system.h"
#include "routes/ota.h"
#include "routes/wifi.h"
#include "routes/mgmt.h"
#include "routes/home.h"
#include "routes/manual.h"
#include "ws_handler.h"
#include "version.h"
#include "cJSON.h"
#include "esp_log.h"
#include <assert.h>

static const char *TAG = "api_server";
static httpd_handle_t s_server = NULL;

// ── Logging wrapper ──────────────────────────────────────────────────────────
// Alle routes registreres via reg() som logger hvert kald i api_log.

#define MAX_LOGGED_ROUTES 48
typedef struct { esp_err_t (*orig)(httpd_req_t *req); } orig_ctx_t;
static orig_ctx_t s_orig_ctxs[MAX_LOGGED_ROUTES];
static int        s_nlogged = 0;

static esp_err_t log_wrapper(httpd_req_t *req)
{
    api_log_append(req);
    orig_ctx_t *ctx = (orig_ctx_t *)req->user_ctx;
    return ctx->orig(req);
}

static void reg(httpd_handle_t srv, const httpd_uri_t *r)
{
    assert(s_nlogged < MAX_LOGGED_ROUTES);
    s_orig_ctxs[s_nlogged].orig = r->handler;
    httpd_uri_t wr = *r;
    wr.handler  = log_wrapper;
    wr.user_ctx = &s_orig_ctxs[s_nlogged];
    // F9: httpd_register_uri_handler fejler stille (returkode ignoreret) hvis
    // hcfg.max_uri_handlers er nået — en route kan derved forsvinde uden
    // varsel. Log højlydt så det aldrig sker ubemærket igen.
    esp_err_t err = httpd_register_uri_handler(srv, &wr);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Route-registrering fejlede for %s: %s (hcfg.max_uri_handlers nået?)",
                 r->uri, esp_err_to_name(err));
    s_nlogged++;
}

static bool s_auth_enabled = false;
static char s_api_key[65]  = {0};

bool api_auth_ok(httpd_req_t *req)
{
    if (!s_auth_enabled || s_api_key[0] == '\0') return true;
    char key[80] = {0};
    if (httpd_req_get_hdr_value_str(req, "X-API-Key", key, sizeof(key)) == ESP_OK
        && strcmp(key, s_api_key) == 0) return true;
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"error\":\"unauthorized\",\"hint\":\"X-API-Key header mangler eller forkert\"}");
    return false;
}

// ── API index — svarer på /api og /api/v1 med liste over alle endpoints ───────

static esp_err_t api_index_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "api",     "Modbus API Gateway");
    cJSON_AddStringToObject(root, "version", GATEWAY_VERSION);
    cJSON_AddStringToObject(root, "build",   GATEWAY_BUILD);
    cJSON_AddStringToObject(root, "base",    "/api/v1");

    cJSON *ep = cJSON_AddArrayToObject(root, "endpoints");

    // Helper macro: tilføj endpoint til array
    #define EP(m, p, d) do { \
        cJSON *e = cJSON_CreateObject(); \
        cJSON_AddStringToObject(e, "method",      m); \
        cJSON_AddStringToObject(e, "path",        p); \
        cJSON_AddStringToObject(e, "description", d); \
        cJSON_AddItemToArray(ep, e); \
    } while(0)

    EP("GET",  "/api/v1/system",                                              "System info: version, uptime, IP, heap, board_variant");
    EP("POST", "/api/v1/system/reboot",                                       "Genstart gateway");
    EP("GET",  "/api/v1/system/hardware",                                     "Board variant + GPIO presets for alle interfaces");
    EP("PUT",  "/api/v1/system/hardware",                                     "Gem board variant  {\"board_variant\":\"30pin\"|\"38pin\"}");
    EP("GET",  "/api/v1/system/gpio",                                         "GPIO-tilgængelighed (tx/rx/de, input-only, reserveret, brugt-af) ift. board + ethernet");
    EP("GET",  "/api/v1/system/wifi",                                         "WiFi status");
    EP("PUT",  "/api/v1/system/wifi",                                         "Konfigurér WiFi (enabled, ssid, password, ip, ap_fallback)");
    EP("GET",  "/api/v1/system/wifi/scan",                                    "Scan efter tilgængelige WiFi-netværk");
    EP("GET",  "/api/v1/system/ota/check",                                    "Tjek om OTA-opdatering er tilgængelig");
    EP("POST", "/api/v1/system/ota/firmware",                                 "Start firmware OTA-opdatering");
    EP("POST", "/api/v1/system/ota/frontend",                                 "Start frontend OTA-opdatering");
    EP("GET",  "/api/v1/system/ota/status",                                   "OTA-opdateringsstatus");
    EP("GET",  "/api/v1/interfaces",                                          "List alle Modbus-interfaces");
    EP("POST", "/api/v1/interfaces",                                          "Opret nyt Modbus-interface (SW-UART master, defaults)");
    EP("GET",  "/api/v1/interfaces/:key",                                     "Hent interface-config — :key er id (0,1,..) ELLER navn-alias");
    EP("PUT",  "/api/v1/interfaces/:key",                                     "Opdatér interface (name, mode, slave_addr, baudrate, type, tx_pin, rx_pin, rts_pin, ...)");
    EP("DELETE","/api/v1/interfaces/:key",                                    "Slet Modbus-interface og renummerér");
    EP("POST", "/api/v1/interfaces/:key/selftest",                            "Loopback-selvtest  {\"mode\":\"internal\"|\"external\"}");
    EP("GET",  "/api/v1/interfaces/:key/slaves/:sid/coils?start=N&count=N",    "FC01: læs coils  (:key = id eller navn)");
    EP("GET",  "/api/v1/interfaces/:key/slaves/:sid/discrete-inputs?start=N&count=N", "FC02: læs discrete inputs");
    EP("GET",  "/api/v1/interfaces/:key/slaves/:sid/holding-registers?start=N&count=N", "FC03: læs holding registers");
    EP("GET",  "/api/v1/interfaces/:key/slaves/:sid/input-registers?start=N&count=N", "FC04: læs input registers");
    EP("PUT",  "/api/v1/interfaces/:key/slaves/:sid/coils/:addr",              "FC05: skriv enkelt coil  {\"value\":true}");
    EP("PUT",  "/api/v1/interfaces/:key/slaves/:sid/coils?start=N",            "FC0F: skriv flere coils  {\"values\":[true,false]}");
    EP("PUT",  "/api/v1/interfaces/:key/slaves/:sid/holding-registers/:addr",  "FC06: skriv enkelt register  {\"value\":1234}");
    EP("PUT",  "/api/v1/interfaces/:key/slaves/:sid/holding-registers?start=N","FC10: skriv flere registers  {\"values\":[1234,5678]}");
    EP("GET",  "/api/v1/modbus/log?since=N",                                  "Dekodet Modbus-bus-log (seq/tid/iface/slave/fc/addr/count/status)");
    EP("POST", "/api/v1/modbus/log/clear",                                    "Ryd Modbus-loggen");
    EP("GET",  "/api/v1/cache/stats",                                         "Cache statistik: hits, misses, hit_rate, entries, TTL, refresh-tællere");
    EP("GET",  "/api/v1/cache/entries",                                       "Alle cache-entries med iface/slave/fc/addr/value/age");
    EP("GET",  "/api/v1/cache/history",                                       "Tidsseriedata (60 samples) for hits/miss/err/used/refresh");
    EP("PUT",  "/api/v1/cache/config",                                        "Sæt cache enabled+ttl_ms  {\"enabled\":true,\"ttl_ms\":1000}");
    EP("POST", "/api/v1/cache/clear",                                         "Tøm cache (ikke stats)");
    EP("POST", "/api/v1/cache/reset-stats",                                   "Nulstil hit/miss-tællere");
    EP("GET",  "/ws",                                                         "WebSocket real-time push");
    EP("GET",  "/",                                                           "Forside — status og links til Management/Manual/API");
    EP("GET",  "/manual",                                                     "Komplet manual: installation, GPIO-tildeling, REST API-guide");
    EP("GET",  "/mgmt",                                                       "Management-GUI: konfiguration, cache, OTA, Modbus-log, API-log");

    #undef EP

    httpd_resp_set_type(req, "application/json");
    char *s = cJSON_Print(root);
    cJSON_Delete(root);
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

static const httpd_uri_t route_api_index = {
    .uri     = "/api*",
    .method  = HTTP_GET,
    .handler = api_index_handler,
};

esp_err_t api_server_start(const api_config_t *cfg)
{
    if (!cfg->enabled) {
        ESP_LOGI(TAG, "API server deaktiveret — ikke startet");
        return ESP_OK;
    }

    s_auth_enabled = cfg->auth_enabled && cfg->api_key[0];
    if (s_auth_enabled) {
        strncpy(s_api_key, cfg->api_key, sizeof(s_api_key));
        ESP_LOGI(TAG, "API auth aktiveret (X-API-Key)");
    }

    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    // F9: skal matche/overstige MAX_LOGGED_ROUTES + direkte registreringer
    // (WebSocket). Ellers fejler senere reg()-kald stille — se reg() ovenfor.
    hcfg.max_uri_handlers  = 48;
    hcfg.uri_match_fn      = httpd_uri_match_wildcard;
    hcfg.server_port       = cfg->port;
    hcfg.stack_size        = 16384;
    hcfg.lru_purge_enable  = true;   // frigiv ældste socket automatisk ved pres

    api_log_init();
    s_nlogged = 0;
    ESP_ERROR_CHECK(httpd_start(&s_server, &hcfg));

    // Interface routes
    reg(s_server, &route_get_interfaces);
    reg(s_server, &route_get_interface);
    reg(s_server, &route_put_interface_config);
    reg(s_server, &route_post_interface);
    reg(s_server, &route_post_interface_action);
    reg(s_server, &route_delete_interface);

    // System routes
    reg(s_server, &route_get_system);
    reg(s_server, &route_post_reboot);
    reg(s_server, &route_get_system_hardware);
    reg(s_server, &route_put_system_hardware);
    reg(s_server, &route_get_system_gpio);
    reg(s_server, &route_get_system_log);
    reg(s_server, &route_post_system_log_clear);
    reg(s_server, &route_get_modbus_log);
    reg(s_server, &route_post_modbus_log_clear);

    // OTA routes
    reg(s_server, &route_get_ota_check);
    reg(s_server, &route_post_ota_firmware);
    reg(s_server, &route_post_ota_frontend);
    reg(s_server, &route_get_ota_status);

    // Cache routes
    reg(s_server, &route_get_cache_stats);
    reg(s_server, &route_get_cache_entries);
    reg(s_server, &route_get_cache_history);
    reg(s_server, &route_post_cache_clear);
    reg(s_server, &route_post_cache_reset_stats);
    reg(s_server, &route_put_cache_config);

    // WiFi routes
    reg(s_server, &route_get_wifi_status);
    reg(s_server, &route_put_wifi_config);
    reg(s_server, &route_get_wifi_scan);

    // Management page
    reg(s_server, &route_get_mgmt);

    // Forside + manual
    reg(s_server, &route_get_home);
    reg(s_server, &route_get_manual);

    // WebSocket — ikke wrappes (specielt upgrade-flow)
    esp_err_t ws_err = httpd_register_uri_handler(s_server, &route_ws);
    if (ws_err != ESP_OK)
        ESP_LOGE(TAG, "WebSocket-registrering fejlede: %s", esp_err_to_name(ws_err));

    // API index — catch-all for /api* — registreres SIDST
    reg(s_server, &route_api_index);

    ESP_LOGI(TAG, "REST API server startet på port %d%s",
             cfg->port, s_auth_enabled ? " (auth: X-API-Key)" : "");
    return ESP_OK;
}

void api_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
