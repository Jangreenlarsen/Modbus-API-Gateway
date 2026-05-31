#pragma once
#include "esp_http_server.h"
#include <stdint.h>

#define API_LOG_CAP  100

typedef struct {
    uint32_t seq;
    uint32_t ts_ms;
    char     method[8];
    char     uri[80];
} api_log_entry_t;

void  api_log_init(void);
void  api_log_append(httpd_req_t *req);
char *api_log_since_json(uint32_t since_seq);  // caller free()s
void  api_log_clear(void);
