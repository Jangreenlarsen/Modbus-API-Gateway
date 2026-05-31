#pragma once
#include "esp_http_server.h"
#include "config.h"

// Skal kaldes før api_server_start så PUT /cache/config kan opdatere refresh-felter
void cache_routes_set_cfg(gateway_config_t *cfg);

extern const httpd_uri_t route_get_cache_stats;
extern const httpd_uri_t route_get_cache_entries;
extern const httpd_uri_t route_get_cache_history;
extern const httpd_uri_t route_post_cache_clear;
extern const httpd_uri_t route_post_cache_reset_stats;
extern const httpd_uri_t route_put_cache_config;
