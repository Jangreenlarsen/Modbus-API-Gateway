#pragma once
#include "esp_http_server.h"

esp_err_t api_fc03_read_holding_regs    (httpd_req_t *req, int iface, int slave);
esp_err_t api_fc06_write_holding_reg    (httpd_req_t *req, int iface, int slave, int addr);
esp_err_t api_fc10_write_holding_regs   (httpd_req_t *req, int iface, int slave);
