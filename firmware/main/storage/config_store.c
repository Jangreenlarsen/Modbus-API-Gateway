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
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open fejlede (%s) — using defaults", esp_err_to_name(err));
        config_set_defaults(cfg);
        return ESP_OK;
    }

    size_t sz = sizeof(gateway_config_t);
    err = nvs_get_blob(h, NVS_KEY, cfg, &sz);
    nvs_close(h);

    if (err != ESP_OK || sz != sizeof(gateway_config_t) ||
        cfg->version != CONFIG_STRUCT_VERSION) {
        ESP_LOGW(TAG, "Config blob invalid eller forældet (v%lu != v%d) — using defaults",
                 (unsigned long)(err == ESP_OK ? cfg->version : 0), CONFIG_STRUCT_VERSION);
        config_set_defaults(cfg);
    } else {
        config_sanitize(cfg);
        ESP_LOGI(TAG, "Config loaded v%d (%d interface(s))", cfg->version, cfg->interface_count);
    }
    return ESP_OK;
}

esp_err_t config_store_save(const gateway_config_t *cfg)
{
    nvs_handle_t h;
    // Ikke ESP_ERROR_CHECK: en ekstern PUT-klient må ikke kunne panikke
    // enheden hvis NVS er fuld/korrupt — returnér fejl i stedet (jf. K3).
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open (rw) fejlede: %s — config ikke gemt", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_blob(h, NVS_KEY, cfg, sizeof(gateway_config_t));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) ESP_LOGI(TAG, "Config saved");
    return err;
}
