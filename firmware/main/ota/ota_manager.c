#include "ota_manager.h"
#include "config.h"
#include "version.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_crt_bundle.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ota";
static ota_status_t s_status = { .state = OTA_STATE_IDLE };

const ota_status_t *ota_get_status(void) { return &s_status; }

// ── HTTP response buffer ────────────────────────────────────────────────────

#define HTTP_BUF_SIZE 16384

typedef struct { char *buf; int len; int cap; } http_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buf_t *b = (http_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && b) {
        if (b->len + evt->data_len < b->cap - 1) {
            memcpy(b->buf + b->len, evt->data, evt->data_len);
            b->len += evt->data_len;
        }
    }
    return ESP_OK;
}

static esp_err_t http_get(const char *url, char *out_buf, int buf_size)
{
    http_buf_t b = { .buf = out_buf, .len = 0, .cap = buf_size };
    memset(out_buf, 0, buf_size);

    esp_http_client_config_t cfg = {
        .url            = url,
        .crt_bundle_attach = esp_crt_bundle_attach,   // validér GitHub TLS-cert
        .event_handler  = http_event_handler,
        .user_data      = &b,
        .timeout_ms     = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "User-Agent", "ESP32-OTA/" GATEWAY_VERSION);
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    esp_err_t err = esp_http_client_perform(client);
    int code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) { ESP_LOGE(TAG, "HTTP GET %s failed: %s", url, esp_err_to_name(err)); return err; }
    if (code != 200)   { ESP_LOGE(TAG, "HTTP GET %s → %d", url, code); return ESP_FAIL; }
    return ESP_OK;
}

// ── Version-sammenligning ───────────────────────────────────────────────────

// Returnerer true hvis remote > local (simpel string-sammenligning er ok for MAJOR.MINOR.PATCH)
static bool version_newer(const char *local, const char *remote)
{
    // Understøtter både MAJOR.MINOR.PATCH og MAJOR.MINOR.PATCH.D (debug-format)
    int lM=0,lm=0,lp=0,ld=0, rM=0,rm=0,rp=0,rd=0;
    sscanf(local,  "%d.%d.%d.%d", &lM, &lm, &lp, &ld);
    sscanf(remote, "%d.%d.%d.%d", &rM, &rm, &rp, &rd);
    if (rM != lM) return rM > lM;
    if (rm != lm) return rm > lm;
    if (rp != lp) return rp > lp;
    return rd > ld;
}

// ── GitHub releases API ─────────────────────────────────────────────────────

esp_err_t ota_check(ota_info_t *info)
{
    s_status.state = OTA_STATE_CHECKING;
    memset(info, 0, sizeof(*info));
    strncpy(info->current_version, GATEWAY_VERSION, sizeof(info->current_version));
    strncpy(info->current_build,   GATEWAY_BUILD,   sizeof(info->current_build));

    char *buf = malloc(HTTP_BUF_SIZE);
    if (!buf) { s_status.state = OTA_STATE_ERROR; return ESP_ERR_NO_MEM; }
    esp_err_t err = http_get(OTA_GITHUB_API_LATEST, buf, HTTP_BUF_SIZE);
    if (err != ESP_OK) { free(buf); s_status.state = OTA_STATE_ERROR; return err; }

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        ESP_LOGE(TAG, "JSON parse fejlede — svar for stort? (buf=%d bytes)", HTTP_BUF_SIZE);
        s_status.state = OTA_STATE_ERROR;
        return ESP_FAIL;
    }

    // tag_name er typisk "v1.2.3" — strip 'v' prefix
    const char *tag = cJSON_GetStringValue(cJSON_GetObjectItem(json, "tag_name"));
    if (tag) {
        const char *ver = (tag[0] == 'v') ? tag + 1 : tag;
        strncpy(info->latest_version, ver, sizeof(info->latest_version));
        info->firmware_available = version_newer(info->current_version, ver);
        info->frontend_available = version_newer(info->current_version, ver);
    }

    // Release notes
    const char *body = cJSON_GetStringValue(cJSON_GetObjectItem(json, "body"));
    if (body) strncpy(info->release_notes, body, sizeof(info->release_notes) - 1);

    // Find asset-URLs
    cJSON *assets = cJSON_GetObjectItem(json, "assets");
    cJSON *asset;
    cJSON_ArrayForEach(asset, assets) {
        const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "name"));
        const char *url  = cJSON_GetStringValue(cJSON_GetObjectItem(asset, "browser_download_url"));
        if (!name || !url) continue;
        if (strcmp(name, OTA_FIRMWARE_ASSET) == 0)
            strncpy(info->firmware_url, url, sizeof(info->firmware_url) - 1);
        if (strcmp(name, OTA_FRONTEND_ASSET) == 0)
            strncpy(info->frontend_url, url, sizeof(info->frontend_url) - 1);
    }

    cJSON_Delete(json);
    s_status.state = OTA_STATE_IDLE;
    ESP_LOGI(TAG, "Current: %s  Latest: %s  FW update: %s  FE update: %s",
             info->current_version, info->latest_version,
             info->firmware_available ? "yes" : "no",
             info->frontend_available ? "yes" : "no");
    return ESP_OK;
}

// ── Firmware OTA ────────────────────────────────────────────────────────────

static int s_fw_progress = 0;

static esp_err_t fw_ota_event_handler(esp_http_client_event_t *evt)
{
    // esp_https_ota bruger denne til intern brug — vi kan ikke få progress herfra direkte.
    // Progress beregnes i task via esp_ota_get_running_partition.
    return ESP_OK;
}

esp_err_t ota_update_firmware(const char *url, ota_status_t *status)
{
    ESP_LOGI(TAG, "Starting firmware OTA from: %s", url);
    status->state = OTA_STATE_DOWNLOADING;
    status->progress_pct = 0;

    esp_http_client_config_t http_cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 60000,
        .keep_alive_enable = true,
        .buffer_size       = 4096,   // GitHub redirect-headers er ~2-3KB
        .buffer_size_tx    = 1024,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t ota_handle;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (err != ESP_OK) {
        snprintf(status->error, sizeof(status->error), "OTA begin failed: %s", esp_err_to_name(err));
        status->state = OTA_STATE_ERROR;
        return err;
    }

    status->state = OTA_STATE_FLASHING;
    int image_size = esp_https_ota_get_image_size(ota_handle);

    while (1) {
        err = esp_https_ota_perform(ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
        if (image_size > 0) {
            int written = esp_https_ota_get_image_len_read(ota_handle);
            status->progress_pct = (written * 100) / image_size;
        }
    }

    if (err != ESP_OK) {
        snprintf(status->error, sizeof(status->error), "OTA perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(ota_handle);
        status->state = OTA_STATE_ERROR;
        return err;
    }

    err = esp_https_ota_finish(ota_handle);
    if (err != ESP_OK) {
        snprintf(status->error, sizeof(status->error), "OTA finish failed: %s", esp_err_to_name(err));
        status->state = OTA_STATE_ERROR;
        return err;
    }

    status->state = OTA_STATE_DONE;
    status->progress_pct = 100;
    ESP_LOGI(TAG, "Firmware OTA complete — rebooting in 2s");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK; // nås ikke
}

// ── Frontend OTA (SPIFFS-image) ─────────────────────────────────────────────

#define FRONTEND_CHUNK  4096

esp_err_t ota_update_frontend(const char *url, ota_status_t *status)
{
    ESP_LOGI(TAG, "Starting frontend OTA from: %s", url);
    status->state    = OTA_STATE_DOWNLOADING;
    status->progress_pct = 0;

    // Find SPIFFS-partitionen
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!part) {
        snprintf(status->error, sizeof(status->error), "SPIFFS partition not found");
        status->state = OTA_STATE_ERROR;
        return ESP_FAIL;
    }

    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 30000,
        .buffer_size       = FRONTEND_CHUNK,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        snprintf(status->error, sizeof(status->error), "HTTP open failed: %s", esp_err_to_name(err));
        status->state = OTA_STATE_ERROR;
        esp_http_client_cleanup(client);
        return err;
    }

    int content_len = esp_http_client_fetch_headers(client);
    int code = esp_http_client_get_status_code(client);
    if (code != 200) {
        snprintf(status->error, sizeof(status->error), "HTTP %d", code);
        status->state = OTA_STATE_ERROR;
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Slet eksisterende SPIFFS-data
    ESP_LOGI(TAG, "Erasing SPIFFS partition (%lu bytes)...", part->size);
    esp_partition_erase_range(part, 0, part->size);

    status->state = OTA_STATE_FLASHING;
    uint8_t  buf[FRONTEND_CHUNK];
    int total = 0, offset = 0;

    while (1) {
        int read = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (read < 0)  { err = ESP_FAIL; break; }
        if (read == 0) { err = ESP_OK;   break; }

        if (offset + read > (int)part->size) {
            snprintf(status->error, sizeof(status->error), "Frontend image too large for SPIFFS");
            err = ESP_FAIL; break;
        }
        esp_partition_write(part, offset, buf, read);
        offset += read;
        total  += read;
        if (content_len > 0)
            status->progress_pct = (total * 100) / content_len;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        status->state = OTA_STATE_ERROR;
        return err;
    }

    status->state        = OTA_STATE_DONE;
    status->progress_pct = 100;
    ESP_LOGI(TAG, "Frontend OTA complete — %d bytes written to SPIFFS", total);
    return ESP_OK;
}
