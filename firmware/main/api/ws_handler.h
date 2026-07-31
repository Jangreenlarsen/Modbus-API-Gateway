#pragma once
#include "esp_http_server.h"
extern const httpd_uri_t route_ws;

// Kaldes fra api_server_start() EFTER httpd_start() — gemmer server-handle og
// registrerer sig som modbus_log's broadcast-callback (real-time WS push af
// nye bus-transaktioner til alle tilsluttede /ws-klienter).
void ws_handler_init(httpd_handle_t server);
