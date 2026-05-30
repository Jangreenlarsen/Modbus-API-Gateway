#include "register_cache.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "reg_cache";

static cache_entry_t s_entries[CACHE_MAX_ENTRIES];
static cache_stats_t s_stats;
static SemaphoreHandle_t s_mutex;

#define LOCK()   xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_mutex)

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

esp_err_t register_cache_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    memset(s_entries, 0, sizeof(s_entries));
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.enabled  = 1;
    s_stats.ttl_ms   = 1000;     // default 1s freshness
    s_stats.since_ms = now_ms();
    ESP_LOGI(TAG, "Register cache klar (max %d entries, TTL %lums)",
             CACHE_MAX_ENTRIES, (unsigned long)s_stats.ttl_ms);
    return ESP_OK;
}

// Find entry — returnerer NULL hvis ikke fundet.
static cache_entry_t *find_entry(int iface, int slave, int fc, int addr)
{
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        cache_entry_t *e = &s_entries[i];
        if (e->status == CACHE_ENTRY_EMPTY) continue;
        if (e->iface == iface && e->slave == slave && e->fc == fc && e->addr == addr)
            return e;
    }
    return NULL;
}

// Find ledig slot ELLER evict LRU (ældste).
static cache_entry_t *alloc_entry(void)
{
    cache_entry_t *oldest = NULL;
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        cache_entry_t *e = &s_entries[i];
        if (e->status == CACHE_ENTRY_EMPTY) return e;
        if (!oldest || e->last_update_ms < oldest->last_update_ms)
            oldest = e;
    }
    // Evict LRU
    s_stats.evictions++;
    memset(oldest, 0, sizeof(*oldest));
    return oldest;
}

bool cache_lookup(int iface, int slave, int fc, int addr, uint16_t *out_value)
{
    if (!s_stats.enabled) return false;

    LOCK();
    s_stats.total_requests++;
    cache_entry_t *e = find_entry(iface, slave, fc, addr);
    bool hit = false;
    if (e && e->status == CACHE_ENTRY_VALID) {
        if (s_stats.ttl_ms == 0 || (now_ms() - e->last_update_ms) < s_stats.ttl_ms) {
            if (out_value) *out_value = e->value;
            e->hits++;
            s_stats.hits++;
            hit = true;
        }
    }
    if (!hit) s_stats.misses++;
    UNLOCK();
    return hit;
}

static void store_locked(int iface, int slave, int fc, int addr, uint16_t value)
{
    cache_entry_t *e = find_entry(iface, slave, fc, addr);
    if (!e) {
        e = alloc_entry();
        e->iface = (uint8_t)iface;
        e->slave = (uint8_t)slave;
        e->fc    = (uint8_t)fc;
        e->addr  = (uint16_t)addr;
        e->hits  = 0;
        s_stats.entries_used++;
        if (s_stats.entries_used > CACHE_MAX_ENTRIES) s_stats.entries_used = CACHE_MAX_ENTRIES;
    }
    e->value          = value;
    e->status         = CACHE_ENTRY_VALID;
    e->last_update_ms = now_ms();
}

void cache_store(int iface, int slave, int fc, int addr, uint16_t value)
{
    LOCK();
    store_locked(iface, slave, fc, addr, value);
    UNLOCK();
}

void cache_store_regs(int iface, int slave, int fc, int start, int count, const uint16_t *values)
{
    LOCK();
    for (int i = 0; i < count; i++) store_locked(iface, slave, fc, start + i, values[i]);
    UNLOCK();
}

void cache_store_coils(int iface, int slave, int fc, int start, int count, const uint8_t *bits)
{
    LOCK();
    for (int i = 0; i < count; i++) {
        uint8_t v = (bits[i / 8] >> (i % 8)) & 1;
        store_locked(iface, slave, fc, start + i, v);
    }
    UNLOCK();
}

void cache_mark_error(int iface, int slave, int fc, int addr)
{
    LOCK();
    s_stats.errors++;
    cache_entry_t *e = find_entry(iface, slave, fc, addr);
    if (e) e->status = CACHE_ENTRY_ERROR;
    UNLOCK();
}

void cache_invalidate(int iface, int slave, int fc, int addr)
{
    LOCK();
    cache_entry_t *e = find_entry(iface, slave, fc, addr);
    if (e) {
        memset(e, 0, sizeof(*e));
        if (s_stats.entries_used > 0) s_stats.entries_used--;
    }
    UNLOCK();
}

void cache_invalidate_iface(int iface)
{
    LOCK();
    for (int i = 0; i < CACHE_MAX_ENTRIES; i++) {
        if (s_entries[i].status != CACHE_ENTRY_EMPTY && s_entries[i].iface == iface) {
            memset(&s_entries[i], 0, sizeof(s_entries[i]));
            if (s_stats.entries_used > 0) s_stats.entries_used--;
        }
    }
    UNLOCK();
}

const cache_stats_t *cache_get_stats(void)
{
    return &s_stats;
}

int cache_get_entries(cache_entry_t *out, int max_count)
{
    LOCK();
    int n = 0;
    for (int i = 0; i < CACHE_MAX_ENTRIES && n < max_count; i++) {
        if (s_entries[i].status != CACHE_ENTRY_EMPTY) out[n++] = s_entries[i];
    }
    UNLOCK();
    return n;
}

void cache_clear(void)
{
    LOCK();
    memset(s_entries, 0, sizeof(s_entries));
    s_stats.entries_used = 0;
    UNLOCK();
    ESP_LOGI(TAG, "Cache cleared");
}

void cache_reset_stats(void)
{
    LOCK();
    uint32_t saved_used    = s_stats.entries_used;
    uint32_t saved_enabled = s_stats.enabled;
    uint32_t saved_ttl     = s_stats.ttl_ms;
    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.entries_used = saved_used;
    s_stats.enabled      = saved_enabled;
    s_stats.ttl_ms       = saved_ttl;
    s_stats.since_ms     = now_ms();
    UNLOCK();
    ESP_LOGI(TAG, "Cache stats reset");
}

void cache_set_enabled(bool enabled)
{
    LOCK();
    s_stats.enabled = enabled ? 1 : 0;
    UNLOCK();
}

void cache_set_ttl_ms(uint32_t ttl_ms)
{
    LOCK();
    s_stats.ttl_ms = ttl_ms;
    UNLOCK();
}

bool cache_is_enabled(void) { return s_stats.enabled != 0; }
uint32_t cache_get_ttl_ms(void) { return s_stats.ttl_ms; }
