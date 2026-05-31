#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Forward decl — undgår cirkulær include
struct gateway_config_t;

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
    uint32_t ttl_ms;             // 0 = never expire
    uint32_t since_ms;           // millis() at last stats reset
    uint32_t refresh_done;       // antal baggrunds-refreshes udført
    uint32_t refresh_failed;     // antal baggrunds-refreshes der fejlede
} cache_stats_t;

// ── History — ringbuffer af cumulative-counter samples ─────────────────────
// Klienten beregner delta = sample[N] - sample[N-1] for periode-rater.
#define CACHE_HISTORY_SAMPLES 60      // 60 samples
#define CACHE_HISTORY_DEFAULT_INTERVAL_MS 10000  // 10 s = 10 min historik total

typedef struct {
    uint32_t timestamp_ms;
    uint32_t hits;
    uint32_t misses;
    uint32_t errors;
    uint16_t entries_used;
    uint16_t refresh_done;
} cache_history_sample_t;          // 20 bytes × 60 = 1200 bytes

// Init cache. Hvis cfg er ikke-NULL, læses enabled+ttl_ms fra cfg.cache og
// register_cache holder en peger så cache_set_*() kan opdatere cfg → næste
// 'save' persisterer ændringer i NVS.
esp_err_t register_cache_init(struct gateway_config_t *cfg);

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

// History API — kaldes af baggrundstask hvert sample-interval.
void cache_history_sample(void);
// Returnerer antal samples kopieret (op til CACHE_HISTORY_SAMPLES), nyeste først.
int  cache_history_get(cache_history_sample_t *out, int max);

// Find entries der trænger til refresh.
// Returnerer kun entries med status==VALID og age > min_age_ms.
// Op til 'max' entries, sorteret efter age descending (mest stale først).
int  cache_get_stale_entries(cache_entry_t *out, int max, uint32_t min_age_ms);

// Stats-opdatering fra refresh-task.
void cache_record_refresh(bool success);
