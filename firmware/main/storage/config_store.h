#pragma once
#include "config.h"
#include "esp_err.h"

esp_err_t config_store_load(gateway_config_t *cfg);
esp_err_t config_store_save(const gateway_config_t *cfg);
