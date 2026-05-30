#pragma once
#include "config.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t ethernet_init(const eth_config_t *cfg);
void      ethernet_wait_for_ip(uint32_t timeout_ms);
void      ethernet_get_ip(char *buf, size_t len);
bool      ethernet_is_available(void);
