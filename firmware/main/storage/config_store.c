#include "config_store.h"
#include "config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG      = "config_store";
static const char *NVS_NS   = "gw_config";
static const char *NVS_KEY  = "cfg_blob";

esp_err_t config_store_load(gateway_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved config — using defaults");
        config_set_defaults(cfg);
        return ESP_OK;
    }
    ESP_ERROR_CHECK(err);

    size_t sz = sizeof(gateway_config_t);
    err = nvs_get_blob(h, NVS_KEY, cfg, &sz);
    nvs_close(h);

    if (err != ESP_OK || sz != sizeof(gateway_config_t)) {
        ESP_LOGW(TAG, "Config blob invalid — using defaults");
        config_set_defaults(cfg);
    } else {
        ESP_LOGI(TAG, "Config loaded (%d interface(s))", cfg->interface_count);
    }
    return ESP_OK;
}

esp_err_t config_store_save(const gateway_config_t *cfg)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS, NVS_READWRITE, &h));
    esp_err_t err = nvs_set_blob(h, NVS_KEY, cfg, sizeof(gateway_config_t));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) ESP_LOGI(TAG, "Config saved");
    return err;
}
