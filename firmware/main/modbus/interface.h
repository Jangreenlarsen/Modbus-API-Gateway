#pragma once
#include "config.h"
#include "modbus_manager.h"
#include "sw_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    iface_config_t    cfg;
    SemaphoreHandle_t mutex;      // én transaktion ad gangen pr. interface

    // HW UART: esp-modbus handle
    void             *mb_handle;

    // SW UART: custom Modbus RTU over bit-bang
    sw_uart_t        *sw_uart;
} mb_interface_t;

esp_err_t   mb_interface_init(mb_interface_t *iface, const iface_config_t *cfg);

mb_result_t mb_interface_read_coils(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t mb_interface_read_discrete_inputs(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t mb_interface_read_holding_regs(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t mb_interface_read_input_regs(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t mb_interface_write_coil(mb_interface_t *iface, uint8_t slave, uint16_t addr, uint8_t value);
mb_result_t mb_interface_write_register(mb_interface_t *iface, uint8_t slave, uint16_t addr, uint16_t value);
mb_result_t mb_interface_write_coils(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits);
mb_result_t mb_interface_write_registers(mb_interface_t *iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs);
