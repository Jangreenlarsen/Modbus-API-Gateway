#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "ethernet.h"
#include "wifi_manager.h"
#include "config_store.h"
#include "modbus_manager.h"
#include "server.h"
#include "serial_cli.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Modbus API Gateway v%s starting", GATEWAY_VERSION);

    // 1. NVS init — må ske først (alle lag bruger NVS)
    ESP_ERROR_CHECK(nvs_flash_init());

    // 2. Indlæs konfiguration fra NVS (eller brug defaults)
    gateway_config_t cfg;
    config_store_load(&cfg);

    // 3. Serial CLI — startes tidligt så IP kan konfigureres uden netværk
    serial_cli_start(&cfg);

    // 4. Ethernet init
    ESP_ERROR_CHECK(ethernet_init(&cfg.ethernet));
    ESP_LOGI(TAG, "Ethernet initialised, waiting for IP...");
    ethernet_wait_for_ip(10000);

    // 5. WiFi init (parallel med ethernet — begge kan køre samtidigt)
    wifi_manager_init(&cfg.wifi);

    // 6. Modbus interfaces init
    ESP_ERROR_CHECK(modbus_manager_init(&cfg));

    // 7. HTTP/WebSocket server start
    ESP_ERROR_CHECK(api_server_start());

    ESP_LOGI(TAG, "Gateway ready — %d interface(s) active", cfg.interface_count);
}
