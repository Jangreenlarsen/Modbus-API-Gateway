#pragma once
#include "esp_err.h"
#include "esp_http_server.h"
#include "config.h"

esp_err_t api_server_start(const api_config_t *cfg);
void      api_server_stop(void);

// Bruges af ws_handler til at sende push til klienter
void api_ws_broadcast(const char *json);

// Auth-check: kalder httpd_resp_send_err og returnerer false ved fejl
bool api_auth_ok(httpd_req_t *req);
