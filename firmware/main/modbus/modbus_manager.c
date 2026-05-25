#include "modbus_manager.h"
#include "interface.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "modbus_mgr";

static mb_interface_t s_interfaces[GATEWAY_MAX_IFACES];
static uint8_t        s_iface_count = 0;

esp_err_t modbus_manager_init(const gateway_config_t *cfg)
{
    s_iface_count = cfg->interface_count;
    for (int i = 0; i < s_iface_count; i++) {
        esp_err_t err = mb_interface_init(&s_interfaces[i], &cfg->interfaces[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Interface %d init failed: %s", i, esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Interface %d ready (%s, %lu baud)",
                 i,
                 cfg->interfaces[i].type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
                 cfg->interfaces[i].baudrate);
    }
    return ESP_OK;
}

static mb_interface_t *get_iface(uint8_t iface)
{
    if (iface >= s_iface_count) return NULL;
    return &s_interfaces[iface];
}

mb_result_t mb_read_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_read_coils(p, slave, start, count, out);
}

mb_result_t mb_read_discrete_inputs(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_read_discrete_inputs(p, slave, start, count, out);
}

mb_result_t mb_read_holding_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_read_holding_regs(p, slave, start, count, out);
}

mb_result_t mb_read_input_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_read_input_regs(p, slave, start, count, out);
}

mb_result_t mb_write_coil(uint8_t iface, uint8_t slave, uint16_t addr, uint8_t value)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_write_coil(p, slave, addr, value);
}

mb_result_t mb_write_register(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t value)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_write_register(p, slave, addr, value);
}

mb_result_t mb_write_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_write_coils(p, slave, start, count, bits);
}

mb_result_t mb_write_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    return mb_interface_write_registers(p, slave, start, count, regs);
}
