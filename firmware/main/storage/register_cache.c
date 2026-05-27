#include "register_cache.h"
#include "esp_log.h"

static const char *TAG = "reg_cache";

esp_err_t register_cache_init(void)
{
    ESP_LOGI(TAG, "Register cache initialiseret (stub)");
    return ESP_OK;
}

esp_err_t register_cache_save(uint8_t iface, uint8_t slave, uint16_t addr,
                               const uint16_t *data, uint8_t count)
{
    return ESP_OK;
}

esp_err_t register_cache_load(uint8_t iface, uint8_t slave, uint16_t addr,
                               uint16_t *data, uint8_t count)
{
    return ESP_ERR_NOT_FOUND;
}
