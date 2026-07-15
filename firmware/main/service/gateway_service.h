#pragma once
#include "esp_err.h"
#include "config.h"
#include "modbus_manager.h"   // mb_result_t + backend FC-operationer

// ── Service-lag ──────────────────────────────────────────────────────────────
// API-laget kalder KUN dette lag (jf. ARCHITECTURE.md regel 1+2). Service-laget
// oversætter interface-nøgler til indeks mod den KØRENDE config og dispatcher
// Modbus-operationer til modbus-laget. Det holder ingen bus-tilstand selv.

esp_err_t gateway_service_init(gateway_config_t *cfg);
void      gateway_service_stop(void);

// Interface-opslag mod den KØRENDE config (ikke NVS). Accepterer numerisk id
// eller navn-alias (case-insensitive). Returnerer -1 hvis ikke fundet/aktiv.
int  gw_resolve_iface(const char *key);

// Pointer til den kørende config (boot-snapshot). NULL før init.
const gateway_config_t *gw_running_config(void);

// ── Modbus-operationer (FC01–FC10) ──────────────────────────────────────────
mb_result_t gw_read_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t gw_read_discrete_inputs(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t gw_read_holding_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t gw_read_input_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t gw_write_coil(uint8_t iface, uint8_t slave, uint16_t addr, uint8_t value);
mb_result_t gw_write_register(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t value);
mb_result_t gw_write_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits);
mb_result_t gw_write_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs);

// Loopback-selvtest på et interface (opslag mod kørende config).
esp_err_t gw_selftest_iface(uint8_t iface, bool external, selftest_result_t *out);
