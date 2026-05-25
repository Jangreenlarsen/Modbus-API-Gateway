#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t api_server_start(void);
void      api_server_stop(void);

// Bruges af ws_handler til at sende push til klienter
void api_ws_broadcast(const char *json);
