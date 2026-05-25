#include "server.h"
#include "routes/coils.h"
#include "routes/discrete.h"
#include "routes/holding_regs.h"
#include "routes/input_regs.h"
#include "routes/interfaces.h"
#include "routes/system.h"
#include "ws_handler.h"
#include "esp_log.h"

static const char *TAG = "api_server";
static httpd_handle_t s_server = NULL;

esp_err_t api_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 24;
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

    // WebSocket
    httpd_register_uri_handler(s_server, &route_ws);

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
