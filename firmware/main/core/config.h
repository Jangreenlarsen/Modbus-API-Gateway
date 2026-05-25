#pragma once
#include <stdint.h>

#define GATEWAY_VERSION     "0.1.0"
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
    uint8_t        interface_count;
    iface_config_t interfaces[GATEWAY_MAX_IFACES];
    eth_config_t   ethernet;
} gateway_config_t;

// Default-værdier
#define DEFAULT_BAUDRATE    9600
#define DEFAULT_TIMEOUT_MS  500
#define DEFAULT_TX_PIN      17
#define DEFAULT_RX_PIN      16
#define DEFAULT_RTS_PIN     4
