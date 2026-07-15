#pragma once
#include <stdint.h>
#include "modbus_manager.h"   // mb_result_t

// Ring-buffer over dekodede Modbus-bus-transaktioner (til web-GUI live-log).
#define MODBUS_LOG_CAP  100

typedef struct {
    uint32_t seq;
    uint32_t ts_ms;
    uint8_t  iface;
    uint8_t  slave;
    uint8_t  fc;         // Modbus function code (1,2,3,4,5,6,15,16)
    uint16_t addr;
    uint16_t count;
    uint8_t  status;     // 0=OK, 1=timeout, 2=exception, 3=error
    uint8_t  exception;  // modbus exception-kode (hvis status=2)
    uint16_t first_val;  // første register/coil-værdi (hurtig visning)
} modbus_log_entry_t;

void  modbus_log_init(void);
void  modbus_log_add(uint8_t iface, uint8_t slave, uint8_t fc,
                     uint16_t addr, uint16_t count, mb_result_t r, uint16_t first_val);
char *modbus_log_since_json(uint32_t since_seq);   // caller free()s
void  modbus_log_clear(void);
