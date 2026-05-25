# ESP32 Hardware og ESP-IDF Reference

Platform: ESP32 (Xtensa LX6 dual-core, 240 MHz)  
Framework: ESP-IDF v5.x  
Modbus-bibliotek: esp-modbus v1.x

---

## UART-konfiguration til RS485

ESP32 har **3 hardware-UARTs**: UART0, UART1, UART2.

| UART  | Default GPIO      | Anbefalet brug              |
|-------|-------------------|-----------------------------|
| UART0 | TX=1, RX=3        | Debug/monitor (reservér)    |
| UART1 | TX=10, RX=9       | RS485 interface 0           |
| UART2 | TX=17, RX=16      | RS485 interface 1           |

> Alle UART-pins kan remappes til næsten alle GPIO via GPIO Matrix.

### Konfiguration (ESP-IDF)
```c
uart_config_t uart_cfg = {
    .baud_rate  = 9600,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
};
uart_driver_install(UART_NUM_1, BUF_SIZE*2, BUF_SIZE*2, 20, &uart_queue, 0);
uart_param_config(UART_NUM_1, &uart_cfg);
uart_set_pin(UART_NUM_1, TX_PIN, RX_PIN, RTS_PIN, UART_PIN_NO_CHANGE);
uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX);  // automatisk DE/RE styring
```

---

## GPIO — MAX485 Transceiver

| ESP32 GPIO | MAX485 Pin | Funktion                      |
|------------|------------|-------------------------------|
| TX_PIN     | DI         | Data In (ESP32 → RS485)       |
| RX_PIN     | RO         | Receive Out (RS485 → ESP32)   |
| RTS_PIN    | DE + RE    | Driver/Receiver Enable (tie)  |

`UART_MODE_RS485_HALF_DUPLEX` styrer RTS-pin automatisk under send/modtagelse.

---

## FreeRTOS Task-model

```c
// Opret polling-task pr. RS485-interface
xTaskCreate(
    modbus_polling_task,   // task-funktion
    "mb_poll_0",           // navn
    4096,                  // stack (bytes)
    (void*)interface_id,   // parameter
    5,                     // prioritet (1=lav, 25=høj)
    &task_handle           // handle (til deletion/notification)
);

// Periodisk polling via timer
esp_timer_create_args_t timer_cfg = {
    .callback = &poll_timer_callback,
    .name     = "poll_timer_0",
};
esp_timer_create(&timer_cfg, &timer_handle);
esp_timer_start_periodic(timer_handle, poll_interval_us);
```

---

## NVS (Non-Volatile Storage) — Konfiguration

NVS bruges til at gemme interface-konfiguration (baudrate, slave-adresser, polling-interval) der overlever strømfald.

```c
#include "nvs_flash.h"
#include "nvs.h"

// Init (kald én gang ved boot)
nvs_flash_init();

// Gem konfiguration
nvs_handle_t handle;
nvs_open("mb_config", NVS_READWRITE, &handle);
nvs_set_u32(handle, "baudrate_0", 9600);
nvs_set_u8(handle, "slave_addr_0", 1);
nvs_commit(handle);
nvs_close(handle);

// Læs konfiguration
nvs_open("mb_config", NVS_READONLY, &handle);
uint32_t baudrate;
nvs_get_u32(handle, "baudrate_0", &baudrate);
nvs_close(handle);
```

---

## SPIFFS — Fil-baseret storage (historik/backup)

SPIFFS bruges til at gemme historiske register-værdier og statiske frontend-filer.

```c
#include "esp_spiffs.h"

esp_vfs_spiffs_conf_t conf = {
    .base_path       = "/spiffs",
    .partition_label = NULL,
    .max_files       = 5,
    .format_if_mount_failed = true,
};
esp_vfs_spiffs_register(&conf);

// Brug standard POSIX fil-API
FILE* f = fopen("/spiffs/data.json", "w");
fprintf(f, "{\"value\": %d}", register_value);
fclose(f);
```

---

## HTTP Server (REST API)

```c
#include "esp_http_server.h"

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    httpd_handle_t server;
    httpd_start(&server, &config);

    httpd_uri_t uri = {
        .uri      = "/api/interfaces",
        .method   = HTTP_GET,
        .handler  = get_interfaces_handler,
    };
    httpd_register_uri_handler(server, &uri);
    return server;
}
```

---

## WebSocket (real-time push)

```c
// ESP-IDF >= 5.0 har WebSocket-support i esp_http_server
httpd_uri_t ws_uri = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = ws_handler,
    .is_websocket = true,
};
httpd_register_uri_handler(server, &ws_uri);

// Send data til alle tilsluttede WS-klienter
httpd_ws_frame_t pkt = {
    .type    = HTTPD_WS_TYPE_TEXT,
    .payload = (uint8_t*)json_str,
    .len     = strlen(json_str),
};
httpd_ws_send_frame_async(server, client_fd, &pkt);
```

---

## Logging

```c
#include "esp_log.h"
static const char* TAG = "modbus_manager";

ESP_LOGI(TAG, "Interface %d: slave %d polling OK, reg[0]=%d", iface_id, slave_addr, value);
ESP_LOGW(TAG, "Interface %d: slave %d timeout", iface_id, slave_addr);
ESP_LOGE(TAG, "Interface %d: CRC error on frame", iface_id);
```

Log-niveauer: `ESP_LOG_NONE` → `ESP_LOG_ERROR` → `ESP_LOG_WARN` → `ESP_LOG_INFO` → `ESP_LOG_DEBUG` → `ESP_LOG_VERBOSE`

---

## OTA (Over-The-Air firmware opdatering)

```c
#include "esp_ota_ops.h"

// Simpel OTA via HTTP
esp_https_ota_config_t ota_config = {
    .http_config = &http_config,
};
esp_err_t ret = esp_https_ota(&ota_config);
```

---

## Vigtige begrænsninger

| Ressource           | ESP32 standard         | Bemærkning                         |
|--------------------|------------------------|------------------------------------|
| Hardware UARTs     | 3 (UART0/1/2)          | UART0 bruges typisk til debug       |
| Max RS485 interfaces | 2 (UART1 + UART2)    | Flere kræver ekstern UART-expander  |
| RAM (SRAM)         | ~320 KB                | Heap ~ 200 KB tilgængeligt         |
| Flash              | 4 MB (typisk modul)    | SPIFFS + OTA + firmware            |
| Max HTTP-klienter  | ~8 samtidige           | Begrænset af RAM og sockets        |
| NVS namespace max  | 15 tegn                |                                    |

---

## Anbefalede komponenter / biblioteker

| Komponent          | Kilde                          | Formål                     |
|--------------------|--------------------------------|----------------------------|
| esp-modbus         | Espressif (ESP-IDF komponent)  | Modbus RTU master/slave    |
| esp_http_server    | ESP-IDF built-in               | REST + WebSocket server    |
| cJSON              | ESP-IDF built-in               | JSON serialisering         |
| esp_timer          | ESP-IDF built-in               | Præcis polling-timer       |
| nvs_flash          | ESP-IDF built-in               | Config-persistens          |
| esp_spiffs         | ESP-IDF built-in               | Fil-storage + frontend     |
