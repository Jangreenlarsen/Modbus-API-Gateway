#include "config.h"
#include <string.h>

void config_set_defaults(gateway_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->interface_count = 1;

    iface_config_t *iface = &cfg->interfaces[0];
    iface->id         = 0;
    iface->type       = IFACE_TYPE_RS485;
    iface->uart_num   = 1;
    iface->baudrate   = DEFAULT_BAUDRATE;
    iface->data_bits  = 8;
    iface->parity     = 0;
    iface->stop_bits  = 1;
    iface->timeout_ms = DEFAULT_TIMEOUT_MS;
    iface->tx_pin     = DEFAULT_TX_PIN;
    iface->rx_pin     = DEFAULT_RX_PIN;
    iface->rts_pin    = DEFAULT_RTS_PIN;
    iface->enabled    = 1;

    cfg->ethernet.enabled      = 1;
    cfg->ethernet.hw_type      = ETH_HW_LAN8720;
    cfg->ethernet.phy_addr     = 0;
    cfg->ethernet.mdc_gpio     = 23;
    cfg->ethernet.mdio_gpio    = 18;
    cfg->ethernet.phy_rst_gpio = -1;
    cfg->ethernet.spi_cs_gpio   = -1;
    cfg->ethernet.spi_mosi_gpio = -1;
    cfg->ethernet.spi_miso_gpio = -1;
    cfg->ethernet.spi_sclk_gpio = -1;
    cfg->ethernet.spi_int_gpio  = -1;
    strncpy(cfg->ethernet.ip,      "dhcp",          sizeof(cfg->ethernet.ip));
    strncpy(cfg->ethernet.gw,      "192.168.1.1",   sizeof(cfg->ethernet.gw));
    strncpy(cfg->ethernet.netmask, "255.255.255.0", sizeof(cfg->ethernet.netmask));
}

void config_sanitize(gateway_config_t *cfg)
{
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
    }
}
