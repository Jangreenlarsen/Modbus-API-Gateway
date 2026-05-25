#pragma once
#include "esp_http_server.h"

extern const httpd_uri_t route_get_ota_check;
extern const httpd_uri_t route_post_ota_firmware;
extern const httpd_uri_t route_post_ota_frontend;
extern const httpd_uri_t route_get_ota_status;
