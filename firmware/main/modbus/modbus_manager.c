#include "modbus_manager.h"
#include "interface.h"
#include "register_cache.h"
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
