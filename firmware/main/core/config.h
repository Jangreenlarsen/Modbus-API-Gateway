#pragma once
#include <stdint.h>

#define GATEWAY_MAX_IFACES  8   // 2 HW UART + op til 6 SW UART

typedef enum {
    IFACE_TYPE_RS485,
    IFACE_TYPE_RS232,
} iface_type_t;

typedef enum {
    IFACE_UART_HW,   // Hardware UART (UART1/UART2) — op til 115200 baud
    IFACE_UART_SW,   // Software UART (GPIO bit-bang + gptimer) — max 9600 baud
} iface_uart_mode_t;

typedef struct {
    uint8_t           id;
    iface_type_t      type;
    iface_uart_mode_t uart_mode;
    int               uart_num;     // HW: UART_NUM_1/2  SW: ignoreret
    uint32_t          baudrate;
    uint8_t           data_bits;
    uint8_t           parity;       // 0=none, 1=odd, 2=even
    uint8_t           stop_bits;
    uint16_t          timeout_ms;
    int               tx_pin;
    int               rx_pin;
    int               rts_pin;      // RS485 DE/RE — GPIO_NUM_NC for RS232
    uint8_t           enabled;
} iface_config_t;

typedef enum {
    ETH_HW_LAN8720,   // RMII intern MAC (LAN8720/LAN8742)
    ETH_HW_W5500,     // SPI ekstern MAC (W5500)
    ETH_HW_NONE,      // Ikke valgt — ingen GPIO pins vises
} eth_hw_t;

typedef struct {
    uint8_t  enabled;
    eth_hw_t hw_type;
    char     ip[16];        // statisk IP, eller "dhcp"
    char     gw[16];
    char     netmask[16];
    // LAN8720 RMII
    int      phy_addr;      // PHY adresse (typisk 0 eller 1)
    int      mdc_gpio;      // MDC management clock
    int      mdio_gpio;     // MDIO management data
    int      phy_rst_gpio;  // PHY reset, -1 = ikke tilsluttet
    // W5500 SPI
    int      spi_cs_gpio;
    int      spi_mosi_gpio;
    int      spi_miso_gpio;
    int      spi_sclk_gpio;
    int      spi_int_gpio;  // interrupt pin, -1 = pollet
} eth_config_t;

typedef struct {
    uint8_t enabled;
    char    ssid[33];       // max 32 tegn + null
    char    password[65];   // max 64 tegn + null
    char    ip[16];         // "dhcp" eller statisk IP
    char    gw[16];
    char    netmask[16];
    // AP-fallback — startes hvis STA ikke kan forbinde inden timeout
    uint8_t ap_fallback;    // 1 = opret hotspot hvis STA fejler
    char    ap_ssid[33];    // AP navn — default "ModbusGW-XXXXXX"
    char    ap_password[65];// AP kodeord — min 8 tegn, eller "" for åben
} wifi_config_gw_t;

typedef struct {
    uint8_t  enabled;
    uint16_t port;           // HTTP port — standard 80
    uint8_t  auth_enabled;   // API key autentificering
    char     api_key[65];    // API nøgle — max 64 tegn
} api_config_t;

// Bump CONFIG_STRUCT_VERSION ved ENHVER ændring af gateway_config_t eller sub-structs.
// NVS-load afviser blob hvis version ikke matcher → defaults indlæses.
#define CONFIG_STRUCT_VERSION  4

typedef struct {
    uint32_t         version;          // skal matche CONFIG_STRUCT_VERSION
    uint8_t          interface_count;
    iface_config_t   interfaces[GATEWAY_MAX_IFACES];
    eth_config_t     ethernet;
    wifi_config_gw_t wifi;
    api_config_t     api;
} gateway_config_t;

// Default-konfiguration
void config_set_defaults(gateway_config_t *cfg);

// Sanitér loaded config — ret ugyldige felter til safe defaults
void config_sanitize(gateway_config_t *cfg);

// Default-værdier
#define DEFAULT_BAUDRATE    9600
#define DEFAULT_TIMEOUT_MS  500
#define DEFAULT_TX_PIN      17
#define DEFAULT_RX_PIN      16
#define DEFAULT_RTS_PIN     4
