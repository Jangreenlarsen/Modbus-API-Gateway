#pragma once
#include "esp_http_server.h"
#include "config.h"

extern const httpd_uri_t route_get_system;
extern const httpd_uri_t route_post_reboot;
extern const httpd_uri_t route_get_system_hardware;
extern const httpd_uri_t route_put_system_hardware;
extern const httpd_uri_t route_get_system_gpio;
extern const httpd_uri_t route_get_system_log;
extern const httpd_uri_t route_post_system_log_clear;
