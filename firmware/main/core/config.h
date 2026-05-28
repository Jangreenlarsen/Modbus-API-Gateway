#pragma once
#include <stdint.h>

#define GATEWAY_VERSION     "0.1.0"
#define GATEWAY_BUILD       "0015"
#define GATEWAY_MAX_IFACES  8       // 2 HW UART + op til 6 SW UART

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

typedef struct {
    char ip[16];        // statisk IP, eller "dhcp"
    char gw[16];
    char netmask[16];
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
    uint8_t          interface_count;
    iface_config_t   interfaces[GATEWAY_MAX_IFACES];
    eth_config_t     ethernet;
    wifi_config_gw_t wifi;
} gateway_config_t;

// Default-konfiguration
void config_set_defaults(gateway_config_t *cfg);

// Default-værdier
#define DEFAULT_BAUDRATE    9600
#define DEFAULT_TIMEOUT_MS  500
#define DEFAULT_TX_PIN      17
#define DEFAULT_RX_PIN      16
#define DEFAULT_RTS_PIN     4
