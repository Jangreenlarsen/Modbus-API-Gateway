#pragma once
#include <stdint.h>
#include "esp_err.h"

// Gemmer seneste kendte register-værdier i SPIFFS
// Giver adgang til data selv under kortvarige Modbus-fejl
esp_err_t register_cache_init(void);
esp_err_t register_cache_save(uint8_t iface, uint8_t slave, uint16_t addr, const uint16_t *data, uint8_t count);
esp_err_t register_cache_load(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t *data, uint8_t count);
