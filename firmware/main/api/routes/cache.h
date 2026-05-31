#pragma once
#include "esp_http_server.h"

extern const httpd_uri_t route_get_cache_stats;
extern const httpd_uri_t route_get_cache_entries;
extern const httpd_uri_t route_post_cache_clear;
extern const httpd_uri_t route_post_cache_reset_stats;
extern const httpd_uri_t route_put_cache_config;
