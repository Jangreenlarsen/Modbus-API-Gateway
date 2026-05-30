#pragma once
#include "esp_http_server.h"

// FC01/FC05/FC0F — kaldes fra master GET/PUT dispatcher i interfaces.c.
// iface og slave er allerede resolved fra URI'en.
esp_err_t api_fc01_read_coils  (httpd_req_t *req, int iface, int slave);
esp_err_t api_fc05_write_coil  (httpd_req_t *req, int iface, int slave, int addr);
esp_err_t api_fc0f_write_coils (httpd_req_t *req, int iface, int slave);
