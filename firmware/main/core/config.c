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

    strncpy(cfg->ethernet.ip,      "dhcp", sizeof(cfg->ethernet.ip));
    strncpy(cfg->ethernet.gw,      "192.168.1.1", sizeof(cfg->ethernet.gw));
    strncpy(cfg->ethernet.netmask, "255.255.255.0", sizeof(cfg->ethernet.netmask));
}
