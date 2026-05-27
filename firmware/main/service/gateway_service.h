#pragma once
#include "esp_err.h"

// Service-lag: koordinerer polling, caching og WebSocket-notifikationer
esp_err_t gateway_service_init(void);
void      gateway_service_stop(void);
