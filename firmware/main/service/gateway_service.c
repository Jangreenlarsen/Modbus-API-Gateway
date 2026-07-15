#include "gateway_service.h"
#include "esp_log.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "gw_service";

static gateway_config_t *s_cfg = NULL;   // kørende config (boot-snapshot)

esp_err_t gateway_service_init(gateway_config_t *cfg)
{
    s_cfg = cfg;
    ESP_LOGI(TAG, "Gateway service initialiseret (%d interface(s))",
             cfg ? cfg->interface_count : 0);
    return ESP_OK;
}

void gateway_service_stop(void) { s_cfg = NULL; }

const gateway_config_t *gw_running_config(void) { return s_cfg; }

// Opslag mod den KØRENDE config — spejler routernes historiske resolve_iface,
// men uden NVS-læsning (fixer M2 hot-path og H1 indeks-desync). FC-routing
// følger dermed altid det der faktisk er initialiseret ved boot.
int gw_resolve_iface(const char *key)
{
    if (!s_cfg || !key || !*key) return -1;
    if (isdigit((unsigned char)key[0])) {
        int id = atoi(key);
        if (id >= 0 && id < s_cfg->interface_count) return id;
        return -1;
    }
    for (int i = 0; i < s_cfg->interface_count; i++) {
        if (strcasecmp(s_cfg->interfaces[i].name, key) == 0) return i;
    }
    return -1;
}

// ── Modbus-operationer — pass-through til modbus-laget ──────────────────────
mb_result_t gw_read_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{ return mb_read_coils(iface, slave, start, count, out); }

mb_result_t gw_read_discrete_inputs(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{ return mb_read_discrete_inputs(iface, slave, start, count, out); }

mb_result_t gw_read_holding_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{ return mb_read_holding_registers(iface, slave, start, count, out); }

mb_result_t gw_read_input_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{ return mb_read_input_registers(iface, slave, start, count, out); }

mb_result_t gw_write_coil(uint8_t iface, uint8_t slave, uint16_t addr, uint8_t value)
{ return mb_write_coil(iface, slave, addr, value); }

mb_result_t gw_write_register(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t value)
{ return mb_write_register(iface, slave, addr, value); }

mb_result_t gw_write_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits)
{ return mb_write_coils(iface, slave, start, count, bits); }

mb_result_t gw_write_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs)
{ return mb_write_registers(iface, slave, start, count, regs); }

esp_err_t gw_selftest_iface(uint8_t iface, bool external, selftest_result_t *out)
{ return mb_selftest(iface, external, out); }
