#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

// Software UART via GPIO bit-bang + gptimer
// Designet til Modbus RTU — max 9600 baud
// TX: gptimer ISR skifter bits ud
// RX: GPIO edge-interrupt detekterer start-bit, gptimer sampler resten

#define SW_UART_MAX_BAUD     9600
#define SW_UART_RX_BUF_SIZE  256   // cirkulær buffer
#define SW_UART_FRAME_SILENCE_CHARS  4  // 3.5 afrundet op til 4 for sikkerhed

typedef void (*sw_uart_rx_cb_t)(uint8_t byte, void *ctx);

typedef struct sw_uart_t sw_uart_t;

typedef struct {
    gpio_num_t  tx_pin;
    gpio_num_t  rx_pin;
    gpio_num_t  de_pin;   // RS485 DE/RE — sæt GPIO_NUM_NC (-1) for RS232
    uint32_t    baudrate; // ≤ 9600
    // Kaldes fra ISR-kontekst når en byte er modtaget
    sw_uart_rx_cb_t rx_callback;
    void           *rx_callback_ctx;
} sw_uart_config_t;

// Allokér og initialiser en SW-UART instans
esp_err_t sw_uart_init(sw_uart_t **out, const sw_uart_config_t *cfg);

// Frigør ressourcer
void sw_uart_deinit(sw_uart_t *uart);

// Transmittér en buffer (blokerer til transmission er færdig)
esp_err_t sw_uart_write(sw_uart_t *uart, const uint8_t *data, size_t len);

// Antal ms siden sidste modtagne byte (bruges til Modbus silence-detektion)
uint32_t sw_uart_ms_since_last_rx(sw_uart_t *uart);

// Userdata pointer — bruges til at knytte en FreeRTOS queue til instansen
// så mb_rtu_sw.c kan hente bytes fra rx_callback uden global state
void  sw_uart_set_userdata(sw_uart_t *uart, void *userdata);
void *sw_uart_get_userdata(sw_uart_t *uart);
