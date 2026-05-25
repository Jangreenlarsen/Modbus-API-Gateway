#pragma once
// Modbus RTU frame-kodning/-dekodning og transceiver over SW-UART
// Bruges af interface.c når uart_mode == IFACE_UART_SW

#include "modbus_manager.h"
#include "sw_uart.h"
#include <stdint.h>

// Max data bytes i en RTU frame: 252 + 4 header/CRC = 256
#define MB_RTU_MAX_FRAME  256
#define MB_RTU_SILENCE_MS 4    // 3.5 char-tider ved 9600 baud ≈ 3.65 ms → 4 ms

uint16_t mb_crc16(const uint8_t *buf, uint16_t len);

// Send en komplet RTU request og modtag svar
// request_len: antal bytes i request (inkl. adresse og FC, UDEN CRC — tilføjes automatisk)
// response: buffer til svar (inkl. CRC)
// response_len: [in] max buffer-størrelse, [out] faktisk antal modtagne bytes
mb_result_t mb_rtu_sw_transaction(sw_uart_t        *uart,
                                   uint16_t          timeout_ms,
                                   const uint8_t    *request,
                                   uint16_t          request_len,
                                   uint8_t          *response,
                                   uint16_t         *response_len);

// Hjælpere der bygger request og parser svar for hvert FC
mb_result_t mb_sw_read_coils        (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t mb_sw_read_discrete     (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out);
mb_result_t mb_sw_read_holding_regs (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t mb_sw_read_input_regs   (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out);
mb_result_t mb_sw_write_coil        (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t addr, uint8_t value);
mb_result_t mb_sw_write_register    (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t addr, uint16_t value);
mb_result_t mb_sw_write_coils       (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits);
mb_result_t mb_sw_write_registers   (sw_uart_t *u, uint16_t tmo, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs);
