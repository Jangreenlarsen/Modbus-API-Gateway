#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "version.h"
#include "ethernet.h"
#include "wifi_manager.h"
#include "config_store.h"
#include "modbus_manager.h"
#include "register_cache.h"
#include "server.h"
#include "serial_cli.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Modbus API Gateway v%s b%s starting", GATEWAY_VERSION, GATEWAY_BUILD);

    // 1. NVS init — eraser og geninitaliserer hvis partition er korrupt/fuld
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition ugyldig — sletter og geninitaliserer");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    // 2. Indlæs konfiguration fra NVS (eller brug defaults)
    // static: cfg lever i BSS-segmentet (hele programmets levetid), ikke på
    // app_main's stack. Når app_main returnerer sletter FreeRTOS main-tasken
    // og frigiver stacken — s_cfg i serial_cli.c ville ellers blive dangling.
    static gateway_config_t cfg;
    config_store_load(&cfg);

    // 3. Serial CLI — startes tidligt så IP kan konfigureres uden netværk
    serial_cli_start(&cfg);

    // 4. Ethernet init — fejl er ikke fatal (PHY måske ikke tilsluttet)
    esp_err_t eth_err = ethernet_init(&cfg.ethernet);
    if (eth_err != ESP_OK) {
        ESP_LOGW(TAG, "Ethernet ikke tilgængeligt — kører kun WiFi");
    }
    ethernet_wait_for_ip(5000);

    // 5. WiFi init
    wifi_manager_init(&cfg.wifi);

    // 6. Register cache (skal initialiseres FØR modbus_manager bruger den)
    register_cache_init();

    // 7. Modbus interfaces init — fejl er ikke fatal (interface måske ikke tilsluttet)
    esp_err_t mb_err = modbus_manager_init(&cfg);
    if (mb_err != ESP_OK) {
        ESP_LOGW(TAG, "Modbus init fejl (%s) — kører videre uden Modbus", esp_err_to_name(mb_err));
    }

    // 7. HTTP/WebSocket server start
    esp_err_t api_err = api_server_start(&cfg.api);
    if (api_err != ESP_OK) {
        ESP_LOGE(TAG, "API server fejl: %s", esp_err_to_name(api_err));
    }

    ESP_LOGI(TAG, "Gateway klar — %d interface(s)", cfg.interface_count);
}
