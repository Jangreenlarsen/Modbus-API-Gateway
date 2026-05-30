#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ── Modbus register cache — synchronous read-through ────────────────────────
//
// Inspireret af Modbus_server_slave_ESP32's async cache. Denne version er
// synchronous (ingen baggrundstask, ingen priority queue) for at holde det
// simpelt. Phase 2 kan tilføje async + priority queue.
//
// Brug: modbus_manager kalder cache_lookup_reg/cache_store_* omkring hvert
// Modbus-kald. Hits → cache returnerer værdien uden bus-trafik. Misses →
// gennem til esp-modbus, gem resultat.

#define CACHE_MAX_ENTRIES 256

typedef enum {
    CACHE_ENTRY_EMPTY  = 0,
    CACHE_ENTRY_VALID  = 1,
    CACHE_ENTRY_ERROR  = 2,
} cache_status_t;

typedef enum {
    CACHE_FC_COIL          = 1, // FC01
    CACHE_FC_DISCRETE      = 2, // FC02
    CACHE_FC_HOLDING       = 3, // FC03
    CACHE_FC_INPUT         = 4, // FC04
} cache_fc_t;

typedef struct {
    uint8_t  iface;          // 0-7
    uint8_t  slave;          // 1-247
    uint8_t  fc;             // cache_fc_t
    uint8_t  status;         // cache_status_t
    uint16_t addr;
    uint16_t value;          // 0/1 for coil/discrete, 16-bit for register
    uint32_t last_update_ms;
    uint32_t hits;
} cache_entry_t;             // 16 bytes

typedef struct {
    uint32_t hits;
    uint32_t misses;
    uint32_t errors;
    uint32_t total_requests;
    uint32_t evictions;
    uint32_t entries_used;
    uint32_t enabled;
    uint32_t ttl_ms;         // 0 = never expire
    uint32_t since_ms;       // millis() at last stats reset
} cache_stats_t;

esp_err_t register_cache_init(void);

// Lookup: returner true ved HIT med fresh data → *out_value sat.
// Fresh = entry status VALID OG (ttl_ms == 0 ELLER alder < ttl_ms).
bool cache_lookup(int iface, int slave, int fc, int addr, uint16_t *out_value);

// Gem værdi efter succesfuldt Modbus-read.
void cache_store(int iface, int slave, int fc, int addr, uint16_t value);

// Bulk-gem multi-register read (FC03/FC04 med count > 1).
void cache_store_regs(int iface, int slave, int fc, int start, int count, const uint16_t *values);

// Bulk-gem multi-coil read (FC01/FC02).
void cache_store_coils(int iface, int slave, int fc, int start, int count, const uint8_t *bits);

// Markér fejl for entry (timeout/exception).
void cache_mark_error(int iface, int slave, int fc, int addr);

// Invalider entry (efter write).
void cache_invalidate(int iface, int slave, int fc, int addr);

// Invalider alle entries for et interface (efter config-ændring).
void cache_invalidate_iface(int iface);

// Stats og admin
const cache_stats_t *cache_get_stats(void);
int  cache_get_entries(cache_entry_t *out, int max_count);
void cache_clear(void);
void cache_reset_stats(void);

void cache_set_enabled(bool enabled);
void cache_set_ttl_ms(uint32_t ttl_ms);
bool cache_is_enabled(void);
uint32_t cache_get_ttl_ms(void);
