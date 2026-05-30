#pragma once
#include "esp_http_server.h"

esp_err_t api_fc04_read_input_regs(httpd_req_t *req, int iface, int slave);
