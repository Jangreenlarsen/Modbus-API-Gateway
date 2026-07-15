#pragma once
#include "config.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Returtype for alle Modbus-operationer
typedef struct {
    esp_err_t  esp_err;         // ESP_OK, ESP_ERR_TIMEOUT, ESP_FAIL
    uint8_t    modbus_exception; // 0 = ingen exception; 1-4 = Modbus exception code
} mb_result_t;

// Resultat af loopback-selvtest pr. interface
typedef struct {
    bool     passed;
    char     mode[12];       // "internal" | "external"
    uint16_t tx_bytes;
    uint16_t rx_bytes;
    uint16_t mismatches;
    uint32_t duration_ms;
    char     detail[80];
} selftest_result_t;

esp_err_t  modbus_manager_init(const gateway_config_t *cfg);

// FC01 — Read Coils
mb_result_t mb_read_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out_bits);

// FC02 — Read Discrete Inputs
mb_result_t mb_read_discrete_inputs(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out_bits);

// FC03 — Read Holding Registers
mb_result_t mb_read_holding_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out_regs);

// FC04 — Read Input Registers
mb_result_t mb_read_input_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out_regs);

// FC05 — Write Single Coil
mb_result_t mb_write_coil(uint8_t iface, uint8_t slave, uint16_t addr, uint8_t value);

// FC06 — Write Single Register
mb_result_t mb_write_register(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t value);

// FC0F — Write Multiple Coils
mb_result_t mb_write_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits);

// FC10 — Write Multiple Registers
mb_result_t mb_write_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs);

// Loopback-selvtest: send et telegram og verificér at det modtages på RX.
// external=false → intern UART-loopback (HW); external=true → fysisk TX↔RX jumper.
esp_err_t mb_selftest(uint8_t iface, bool external, selftest_result_t *out);
