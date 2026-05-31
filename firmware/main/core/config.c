#include "config.h"
#include <string.h>
#include <stdio.h>

// GPIO-preset tabeller: {tx, rx, de}
// Undgår W5500 standard-pins (12,13,14,23,33,34) og UART0 (1,3).
// GPIO 34-39 er input-only — kun brugbare som RX (ikke TX eller DE).
// 30-pin board: GPIO 37+38 er ikke eksponeret.
// 38-pin board: GPIO 37+38 er tilgængelige.

static const int s_presets_30[GATEWAY_MAX_IFACES][3] = {
    //  tx   rx   de
    {   17,  16,   4 },   // iface 0  — UART2 HW
    {   25,  26,  27 },   // iface 1
    {   21,  22,  19 },   // iface 2
    {   32,  35,  15 },   // iface 3  (35=input-only → RX OK)
    {    5,  36,  18 },   // iface 4  (36=input-only → RX OK)
    {    2,  39,   0 },   // iface 5  (39=input-only → RX OK; DE=0 boot-pin, brug med forsigtighed)
    {   15,  34,   4 },   // iface 6  (34=input-only → RX OK; DE=4 deles med iface0 — skift ved konflikt)
    {   18,  38,  19 },   // iface 7  (38 kun eksponeret på visse 30-pin boards; alternativt brug 39)
};

static const int s_presets_38[GATEWAY_MAX_IFACES][3] = {
    //  tx   rx   de
    {   17,  16,   4 },   // iface 0  — UART2 HW
    {   25,  26,  27 },   // iface 1
    {   21,  22,  19 },   // iface 2
    {   32,  35,  15 },   // iface 3
    {    5,  36,  18 },   // iface 4
    {    2,  37,   0 },   // iface 5  (37 eksponeret på 38-pin)
    {   15,  38,   4 },   // iface 6  (38 eksponeret på 38-pin)
    {   18,  39,  19 },   // iface 7
};

void config_get_gpio_preset(int iface_id, iface_type_t type, board_variant_t board,
                             int *tx, int *rx, int *de)
{
    if (iface_id < 0 || iface_id >= GATEWAY_MAX_IFACES) {
        *tx = 17; *rx = 16; *de = (type == IFACE_TYPE_RS485) ? 4 : -1;
        return;
    }
    const int (*tbl)[3] = (board == BOARD_ESP32_38PIN) ? s_presets_38 : s_presets_30;
    *tx = tbl[iface_id][0];
    *rx = tbl[iface_id][1];
    *de = (type == IFACE_TYPE_RS485) ? tbl[iface_id][2] : -1;
}

void config_set_defaults(gateway_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->version        = CONFIG_STRUCT_VERSION;
    cfg->interface_count = 1;
    cfg->board_variant  = BOARD_ESP32_30PIN;

    iface_config_t *iface = &cfg->interfaces[0];
    iface->id         = 0;
    snprintf(iface->name, sizeof(iface->name), "modbus0");
    iface->type       = IFACE_TYPE_RS485;
    iface->uart_mode  = IFACE_UART_HW;
    iface->mode       = IFACE_MODE_MASTER;
    iface->uart_num   = 1;
    iface->baudrate   = DEFAULT_BAUDRATE;
    iface->data_bits  = 8;
    iface->parity     = 0;
    iface->stop_bits  = 1;
    iface->timeout_ms = DEFAULT_TIMEOUT_MS;
    config_get_gpio_preset(0, IFACE_TYPE_RS485, BOARD_ESP32_30PIN,
                           &iface->tx_pin, &iface->rx_pin, &iface->rts_pin);
    iface->slave_addr = 1;
    iface->enabled    = 1;

    cfg->api.enabled      = 1;
    cfg->api.port         = 80;
    cfg->api.auth_enabled = 0;

    cfg->cache.enabled                = 1;
    cfg->cache.ttl_ms                 = 1000;
    cfg->cache.refresh_enabled        = 1;
    cfg->cache.refresh_interval_ms    = 200;
    cfg->cache.refresh_threshold_pct  = 75;
    cfg->cache.history_interval_ms    = 10000;

    cfg->ethernet.enabled       = 1;
    cfg->ethernet.hw_type       = ETH_HW_W5500;
    cfg->ethernet.phy_addr      = 0;
    cfg->ethernet.mdc_gpio      = -1;
    cfg->ethernet.mdio_gpio     = -1;
    cfg->ethernet.phy_rst_gpio  = -1;
    cfg->ethernet.spi_cs_gpio   = 23;
    cfg->ethernet.spi_mosi_gpio = 13;
    cfg->ethernet.spi_miso_gpio = 12;
    cfg->ethernet.spi_sclk_gpio = 14;
    cfg->ethernet.spi_rst_gpio  = 33;
    cfg->ethernet.spi_int_gpio  = 34;
    cfg->ethernet.spi_clock_mhz = 10;
    cfg->ethernet.spi_poll_ms   = 10;
    strncpy(cfg->ethernet.ip,      "dhcp",          sizeof(cfg->ethernet.ip));
    strncpy(cfg->ethernet.gw,      "192.168.1.1",   sizeof(cfg->ethernet.gw));
    strncpy(cfg->ethernet.netmask, "255.255.255.0", sizeof(cfg->ethernet.netmask));
}

void config_sanitize(gateway_config_t *cfg)
{
    if (cfg->board_variant != BOARD_ESP32_30PIN && cfg->board_variant != BOARD_ESP32_38PIN)
        cfg->board_variant = BOARD_ESP32_30PIN;
    if (cfg->api.port == 0) cfg->api.port = 80;
    if (cfg->cache.enabled > 1) cfg->cache.enabled = 1;
    if (cfg->cache.refresh_enabled > 1) cfg->cache.refresh_enabled = 1;
    if (cfg->cache.refresh_interval_ms < 50)    cfg->cache.refresh_interval_ms = 200;
    if (cfg->cache.refresh_interval_ms > 60000) cfg->cache.refresh_interval_ms = 200;
    if (cfg->cache.refresh_threshold_pct < 10)  cfg->cache.refresh_threshold_pct = 75;
    if (cfg->cache.refresh_threshold_pct > 99)  cfg->cache.refresh_threshold_pct = 75;
    if (cfg->cache.history_interval_ms < 1000)  cfg->cache.history_interval_ms = 10000;
    if (cfg->cache.history_interval_ms > 600000) cfg->cache.history_interval_ms = 10000;
    if (cfg->ethernet.spi_clock_mhz < 1 || cfg->ethernet.spi_clock_mhz > 36)
        cfg->ethernet.spi_clock_mhz = 10;
    if (cfg->ethernet.spi_poll_ms < 1 || cfg->ethernet.spi_poll_ms > 100)
        cfg->ethernet.spi_poll_ms = 10;

    if (cfg->interface_count > GATEWAY_MAX_IFACES)
        cfg->interface_count = 0;
    for (uint8_t i = 0; i < cfg->interface_count; i++) {
        iface_config_t *iface = &cfg->interfaces[i];
        if (iface->uart_mode == IFACE_UART_HW &&
            (iface->uart_num < 0 || iface->uart_num > 2)) {
            iface->uart_num = 1;   // UART1 er standard HW Modbus port
        }
        if (iface->baudrate == 0)
            iface->baudrate = DEFAULT_BAUDRATE;
        if (iface->timeout_ms == 0)
            iface->timeout_ms = DEFAULT_TIMEOUT_MS;
        if (iface->slave_addr < 1 || iface->slave_addr > 247)
            iface->slave_addr = 1;
        if (iface->mode != IFACE_MODE_MASTER && iface->mode != IFACE_MODE_SLAVE)
            iface->mode = IFACE_MODE_MASTER;
        iface->name[sizeof(iface->name) - 1] = '\0';
        if (iface->name[0] == '\0')
            snprintf(iface->name, sizeof(iface->name), "modbus%d", i);
    }
}
