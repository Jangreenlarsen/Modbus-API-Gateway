#include "modbus_manager.h"
#include "interface.h"
#include "register_cache.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "modbus_mgr";

static mb_interface_t s_interfaces[GATEWAY_MAX_IFACES];
static uint8_t        s_iface_count = 0;
static const gateway_config_t *s_cfg_ref = NULL;
static TaskHandle_t   s_refresh_task = NULL;
static TaskHandle_t   s_history_task = NULL;

// Forward decls
static void refresh_task(void *arg);
static void history_task(void *arg);
static mb_interface_t *get_iface(uint8_t iface);

esp_err_t modbus_manager_init(const gateway_config_t *cfg)
{
    s_iface_count = cfg->interface_count;
    s_cfg_ref     = cfg;
    // K1/N1: esp-modbus v1.x har ÉN global master-controller OG én global
    // slave-controller. Der kan derfor køre højst én HW-UART master og højst én
    // HW-UART slave. En master og en slave kan sameksistere (separate globaler).
    bool hw_master_up = false;
    bool hw_slave_up  = false;
    for (int i = 0; i < s_iface_count; i++) {
        const iface_config_t *ic = &cfg->interfaces[i];
        bool is_hw        = (ic->uart_mode == IFACE_UART_HW);
        bool is_hw_master = is_hw && ic->mode == IFACE_MODE_MASTER;
        bool is_hw_slave  = is_hw && ic->mode == IFACE_MODE_SLAVE;

        // Deaktivér den 2. HW-controller af samme rolle i stedet for at lade den
        // stille kapre den globale controller-peger.
        if ((is_hw_master && hw_master_up) || (is_hw_slave && hw_slave_up)) {
            memcpy(&s_interfaces[i].cfg, ic, sizeof(iface_config_t));
            s_interfaces[i].ready = false;
            s_interfaces[i].mutex = NULL;
            ESP_LOGE(TAG, "Interface %d: yderligere HW-UART %s understøttes ikke "
                          "(esp-modbus global controller) — deaktiveret. Brug SW-UART.",
                     i, is_hw_master ? "master" : "slave");
            continue;
        }

        // Per-interface fejl er ikke fatal: log, marker ikke-ready, fortsæt så
        // resterende interfaces + cache-tasks stadig starter.
        esp_err_t err = mb_interface_init(&s_interfaces[i], ic);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Interface %d init failed: %s — deaktiveret", i, esp_err_to_name(err));
            continue;
        }
        if (is_hw_master) hw_master_up = true;
        if (is_hw_slave)  hw_slave_up  = true;
        ESP_LOGI(TAG, "Interface %d ready (%s, %lu baud)",
                 i,
                 ic->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
                 ic->baudrate);
    }

    // Start baggrundstasks (cache refresh + history snapshot)
    xTaskCreate(refresh_task, "cache_refresh", 4096, NULL, 3, &s_refresh_task);
    xTaskCreate(history_task, "cache_history", 2048, NULL, 2, &s_history_task);
    ESP_LOGI(TAG, "Cache refresh-task og history-task startet");
    return ESP_OK;
}

static mb_interface_t *get_iface(uint8_t iface)
{
    if (iface >= s_iface_count) return NULL;
    if (!s_interfaces[iface].ready) return NULL;   // deaktiveret/fejlet interface
    return &s_interfaces[iface];
}

// ── Cache-integrerede read-funktioner ──────────────────────────────────────
// Strategi: tjek cache først. Hvis hit (fresh), returnér cached værdier uden
// bus-trafik. Hvis miss eller delvist hit, gør Modbus-kald og opdatér cache.
// Multi-register read kræver at ALLE addresser er fresh — ellers kald bus.

static bool all_coils_cached(uint8_t iface, uint8_t slave, uint8_t fc,
                              uint16_t start, uint16_t count, uint8_t *out)
{
    for (int i = 0; i < count; i++) {
        uint16_t v;
        if (!cache_lookup(iface, slave, fc, start + i, &v)) return false;
        if (v) out[i/8] |= (1 << (i%8));
        else   out[i/8] &= ~(1 << (i%8));
    }
    return true;
}

static bool all_regs_cached(uint8_t iface, uint8_t slave, uint8_t fc,
                             uint16_t start, uint16_t count, uint16_t *out)
{
    for (int i = 0; i < count; i++) {
        if (!cache_lookup(iface, slave, fc, start + i, &out[i])) return false;
    }
    return true;
}

mb_result_t mb_read_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };

    memset(out, 0, (count + 7) / 8);
    if (cache_is_enabled() && all_coils_cached(iface, slave, CACHE_FC_COIL, start, count, out))
        return (mb_result_t){ .esp_err = ESP_OK };

    mb_result_t r = mb_interface_read_coils(p, slave, start, count, out);
    if (r.esp_err == ESP_OK)
        cache_store_coils(iface, slave, CACHE_FC_COIL, start, count, out);
    else
        cache_mark_error(iface, slave, CACHE_FC_COIL, start);
    return r;
}

mb_result_t mb_read_discrete_inputs(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint8_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };

    memset(out, 0, (count + 7) / 8);
    if (cache_is_enabled() && all_coils_cached(iface, slave, CACHE_FC_DISCRETE, start, count, out))
        return (mb_result_t){ .esp_err = ESP_OK };

    mb_result_t r = mb_interface_read_discrete_inputs(p, slave, start, count, out);
    if (r.esp_err == ESP_OK)
        cache_store_coils(iface, slave, CACHE_FC_DISCRETE, start, count, out);
    else
        cache_mark_error(iface, slave, CACHE_FC_DISCRETE, start);
    return r;
}

mb_result_t mb_read_holding_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };

    if (cache_is_enabled() && all_regs_cached(iface, slave, CACHE_FC_HOLDING, start, count, out))
        return (mb_result_t){ .esp_err = ESP_OK };

    mb_result_t r = mb_interface_read_holding_regs(p, slave, start, count, out);
    if (r.esp_err == ESP_OK)
        cache_store_regs(iface, slave, CACHE_FC_HOLDING, start, count, out);
    else
        cache_mark_error(iface, slave, CACHE_FC_HOLDING, start);
    return r;
}

mb_result_t mb_read_input_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, uint16_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };

    if (cache_is_enabled() && all_regs_cached(iface, slave, CACHE_FC_INPUT, start, count, out))
        return (mb_result_t){ .esp_err = ESP_OK };

    mb_result_t r = mb_interface_read_input_regs(p, slave, start, count, out);
    if (r.esp_err == ESP_OK)
        cache_store_regs(iface, slave, CACHE_FC_INPUT, start, count, out);
    else
        cache_mark_error(iface, slave, CACHE_FC_INPUT, start);
    return r;
}

// ── Writes — invalidér cache så næste read går til bus ─────────────────────

mb_result_t mb_write_coil(uint8_t iface, uint8_t slave, uint16_t addr, uint8_t value)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    mb_result_t r = mb_interface_write_coil(p, slave, addr, value);
    if (r.esp_err == ESP_OK) {
        // Skrivning var succes — opdatér cache med skrivetværdien
        cache_store(iface, slave, CACHE_FC_COIL, addr, value ? 1 : 0);
    } else {
        cache_invalidate(iface, slave, CACHE_FC_COIL, addr);
    }
    return r;
}

mb_result_t mb_write_register(uint8_t iface, uint8_t slave, uint16_t addr, uint16_t value)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    mb_result_t r = mb_interface_write_register(p, slave, addr, value);
    if (r.esp_err == ESP_OK)
        cache_store(iface, slave, CACHE_FC_HOLDING, addr, value);
    else
        cache_invalidate(iface, slave, CACHE_FC_HOLDING, addr);
    return r;
}

mb_result_t mb_write_coils(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint8_t *bits)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    mb_result_t r = mb_interface_write_coils(p, slave, start, count, bits);
    if (r.esp_err == ESP_OK)
        cache_store_coils(iface, slave, CACHE_FC_COIL, start, count, bits);
    else
        for (int i = 0; i < count; i++) cache_invalidate(iface, slave, CACHE_FC_COIL, start + i);
    return r;
}

mb_result_t mb_write_registers(uint8_t iface, uint8_t slave, uint16_t start, uint16_t count, const uint16_t *regs)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) return (mb_result_t){ .esp_err = ESP_ERR_INVALID_ARG };
    mb_result_t r = mb_interface_write_registers(p, slave, start, count, regs);
    if (r.esp_err == ESP_OK)
        cache_store_regs(iface, slave, CACHE_FC_HOLDING, start, count, regs);
    else
        for (int i = 0; i < count; i++) cache_invalidate(iface, slave, CACHE_FC_HOLDING, start + i);
    return r;
}

// ── Loopback-selvtest ──────────────────────────────────────────────────────

esp_err_t mb_selftest(uint8_t iface, bool external, selftest_result_t *out)
{
    mb_interface_t *p = get_iface(iface);
    if (!p) {
        memset(out, 0, sizeof(*out));
        snprintf(out->detail, sizeof(out->detail), "Interface ikke aktivt");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t e = mb_interface_selftest(p, external, out);
    // Loopback kan have efterladt "svar" i esp-modbus — invalidér cachen for
    // dette interface så efterfølgende reads går til bussen.
    cache_invalidate_iface(iface);
    return e;
}

// ── Cache refresh-task ────────────────────────────────────────────────────
// Scanner cache for VALID entries hvor age > TTL × threshold_pct/100. Henter
// dem fra bussen i baggrunden så hot data forbliver fresh — read-through-hits
// ser stadig stort set 0ms latency selv ved kort TTL.
static void refresh_one(const cache_entry_t *e)
{
    mb_interface_t *p = get_iface(e->iface);
    if (!p) { cache_record_refresh(false); return; }
    mb_result_t r = { .esp_err = ESP_FAIL };
    uint16_t reg;
    uint8_t  bits[1] = {0};
    switch (e->fc) {
        case CACHE_FC_COIL:
            r = mb_interface_read_coils(p, e->slave, e->addr, 1, bits);
            if (r.esp_err == ESP_OK) cache_store_coils(e->iface, e->slave, CACHE_FC_COIL, e->addr, 1, bits);
            break;
        case CACHE_FC_DISCRETE:
            r = mb_interface_read_discrete_inputs(p, e->slave, e->addr, 1, bits);
            if (r.esp_err == ESP_OK) cache_store_coils(e->iface, e->slave, CACHE_FC_DISCRETE, e->addr, 1, bits);
            break;
        case CACHE_FC_HOLDING:
            r = mb_interface_read_holding_regs(p, e->slave, e->addr, 1, &reg);
            if (r.esp_err == ESP_OK) cache_store(e->iface, e->slave, CACHE_FC_HOLDING, e->addr, reg);
            break;
        case CACHE_FC_INPUT:
            r = mb_interface_read_input_regs(p, e->slave, e->addr, 1, &reg);
            if (r.esp_err == ESP_OK) cache_store(e->iface, e->slave, CACHE_FC_INPUT, e->addr, reg);
            break;
    }
    cache_record_refresh(r.esp_err == ESP_OK);
}

static void refresh_task(void *arg)
{
    cache_entry_t stale[8];   // op til 8 entries pr. cycle for ikke at stuffe bussen
    while (1) {
        uint16_t interval = s_cfg_ref ? s_cfg_ref->cache.refresh_interval_ms : 200;
        vTaskDelay(pdMS_TO_TICKS(interval));

        if (!cache_is_enabled()) continue;
        if (!s_cfg_ref || !s_cfg_ref->cache.refresh_enabled) continue;
        uint32_t ttl = cache_get_ttl_ms();
        if (ttl == 0) continue;  // aldrig udløb → ingen grund til refresh

        uint32_t threshold_age = (ttl * s_cfg_ref->cache.refresh_threshold_pct) / 100;
        int n = cache_get_stale_entries(stale, 8, threshold_age);
        for (int i = 0; i < n; i++) {
            refresh_one(&stale[i]);
            // Lille pause mellem refreshes for at give plads til klient-requests
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

// ── Cache history-snapshot task ───────────────────────────────────────────
// Tager periodiske snapshots af cumulative-counters til ringbuffer.
static void history_task(void *arg)
{
    while (1) {
        uint16_t interval = s_cfg_ref ? s_cfg_ref->cache.history_interval_ms : 10000;
        vTaskDelay(pdMS_TO_TICKS(interval));
        cache_history_sample();
    }
}
