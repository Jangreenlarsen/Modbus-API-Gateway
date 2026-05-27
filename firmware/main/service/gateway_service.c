#include "gateway_service.h"
#include "esp_log.h"

static const char *TAG = "gw_service";

esp_err_t gateway_service_init(void)
{
    ESP_LOGI(TAG, "Gateway service initialiseret (stub)");
    return ESP_OK;
}

void gateway_service_stop(void) {}
