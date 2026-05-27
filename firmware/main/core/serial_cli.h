#pragma once
#include "config.h"
#include "esp_err.h"

// Serial CLI over UART0 — konfiguration uden netværk
// Kommandoer: help, show, eth, wifi, save, reboot, status
esp_err_t serial_cli_start(gateway_config_t *cfg);
