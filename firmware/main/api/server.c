#include "server.h"
#include "routes/coils.h"
#include "routes/discrete.h"
#include "routes/holding_regs.h"
#include "routes/input_regs.h"
#include "routes/interfaces.h"
#include "routes/system.h"
#include "routes/ota.h"
#include "routes/wifi.h"
#include "ws_handler.h"
#include "version.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "api_server";
static httpd_handle_t s_server = NULL;

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

    EP("GET",  "/api/v1/system",                                              "System info: version, uptime, IP, heap");
    EP("POST", "/api/v1/system/reboot",                                       "Genstart gateway");
    EP("GET",  "/api/v1/system/wifi",                                         "WiFi status");
    EP("PUT",  "/api/v1/system/wifi",                                         "Konfigurér WiFi (enabled, ssid, password, ip, ap_fallback)");
    EP("GET",  "/api/v1/system/wifi/scan",                                    "Scan efter tilgængelige WiFi-netværk");
    EP("GET",  "/api/v1/system/ota/check",                                    "Tjek om OTA-opdatering er tilgængelig");
    EP("POST", "/api/v1/system/ota/firmware",                                 "Start firmware OTA-opdatering");
    EP("POST", "/api/v1/system/ota/frontend",                                 "Start frontend OTA-opdatering");
    EP("GET",  "/api/v1/system/ota/status",                                   "OTA-opdateringsstatus");
    EP("GET",  "/api/v1/interfaces",                                          "List alle Modbus-interfaces");
    EP("GET",  "/api/v1/interfaces/:id",                                      "Hent interface-konfiguration");
    EP("PUT",  "/api/v1/interfaces/:id/config",                               "Opdatér interface-konfiguration");
    EP("GET",  "/api/v1/interfaces/:id/slaves/:sid/coils?start=N&count=N",    "FC01: læs coils (1-bit R/W)");
    EP("GET",  "/api/v1/interfaces/:id/slaves/:sid/discrete-inputs?start=N&count=N", "FC02: læs discrete inputs (1-bit R)");
    EP("GET",  "/api/v1/interfaces/:id/slaves/:sid/holding-registers?start=N&count=N", "FC03: læs holding registers (16-bit R/W)");
    EP("GET",  "/api/v1/interfaces/:id/slaves/:sid/input-registers?start=N&count=N", "FC04: læs input registers (16-bit R)");
    EP("PUT",  "/api/v1/interfaces/:id/slaves/:sid/coils/:addr",              "FC05: skriv enkelt coil  {\"value\":true}");
    EP("PUT",  "/api/v1/interfaces/:id/slaves/:sid/coils?start=N",            "FC0F: skriv flere coils  {\"values\":[true,false]}");
    EP("PUT",  "/api/v1/interfaces/:id/slaves/:sid/holding-registers/:addr",  "FC06: skriv enkelt register  {\"value\":1234}");
    EP("PUT",  "/api/v1/interfaces/:id/slaves/:sid/holding-registers?start=N","FC10: skriv flere registers  {\"values\":[1234,5678]}");
    EP("GET",  "/ws",                                                         "WebSocket real-time push");

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

esp_err_t api_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 32;
    cfg.uri_match_fn     = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&s_server, &cfg));

    // Modbus read routes
    httpd_register_uri_handler(s_server, &route_get_coils);
    httpd_register_uri_handler(s_server, &route_get_discrete_inputs);
    httpd_register_uri_handler(s_server, &route_get_holding_regs);
    httpd_register_uri_handler(s_server, &route_get_input_regs);

    // Modbus write routes
    httpd_register_uri_handler(s_server, &route_put_coil_single);
    httpd_register_uri_handler(s_server, &route_put_coil_multi);
    httpd_register_uri_handler(s_server, &route_put_holding_reg_single);
    httpd_register_uri_handler(s_server, &route_put_holding_reg_multi);

    // Interface config routes
    httpd_register_uri_handler(s_server, &route_get_interfaces);
    httpd_register_uri_handler(s_server, &route_get_interface);
    httpd_register_uri_handler(s_server, &route_put_interface_config);

    // System routes
    httpd_register_uri_handler(s_server, &route_get_system);
    httpd_register_uri_handler(s_server, &route_post_reboot);

    // OTA routes
    httpd_register_uri_handler(s_server, &route_get_ota_check);
    httpd_register_uri_handler(s_server, &route_post_ota_firmware);
    httpd_register_uri_handler(s_server, &route_post_ota_frontend);
    httpd_register_uri_handler(s_server, &route_get_ota_status);

    // WiFi routes
    httpd_register_uri_handler(s_server, &route_get_wifi_status);
    httpd_register_uri_handler(s_server, &route_put_wifi_config);
    httpd_register_uri_handler(s_server, &route_get_wifi_scan);

    // WebSocket
    httpd_register_uri_handler(s_server, &route_ws);

    // API index — catch-all for /api* — registreres SIDST så specifikke routes matcher først
    httpd_register_uri_handler(s_server, &route_api_index);

    ESP_LOGI(TAG, "REST API server started on port 80");
    return ESP_OK;
}

void api_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
