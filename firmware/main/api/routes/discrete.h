#pragma once
#include "esp_http_server.h"

// FC02 — kaldes fra master GET dispatcher
esp_err_t api_fc02_read_discrete_inputs(httpd_req_t *req, int iface, int slave);
