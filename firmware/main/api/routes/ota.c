#include "ota.h"
#include "ota_manager.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "route_ota";

// Hjælper: serialiser ota_info_t til JSON
static char *ota_info_to_json(const ota_info_t *info)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "current_version",   info->current_version);
    cJSON_AddStringToObject(root, "build",             info->current_build);
    cJSON_AddStringToObject(root, "latest_version",    info->latest_version);
    cJSON_AddBoolToObject(root,   "firmware_available", info->firmware_available);
    cJSON_AddBoolToObject(root,   "frontend_available", info->frontend_available);
    if (info->firmware_url[0]) cJSON_AddStringToObject(root, "firmware_url", info->firmware_url);
    if (info->frontend_url[0]) cJSON_AddStringToObject(root, "frontend_url", info->frontend_url);
    if (info->release_notes[0]) cJSON_AddStringToObject(root, "release_notes", info->release_notes);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

// Hjælper: serialiser ota_status_t til JSON
static char *ota_status_to_json(const ota_status_t *st)
{
    const char *state_str[] = { "idle", "checking", "downloading", "flashing", "done", "error" };
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "state",        state_str[st->state]);
    cJSON_AddNumberToObject(root, "progress_pct", st->progress_pct);
    if (st->error[0]) cJSON_AddStringToObject(root, "error", st->error);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

// ── GET /api/v1/system/ota/check ────────────────────────────────────────────
// Forespørger GitHub releases API og returnerer versionsstatus

static esp_err_t get_ota_check_handler(httpd_req_t *req)
{
    ota_info_t info;
    esp_err_t err = ota_check(&info);

    httpd_resp_set_type(req, "application/json");
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "502 Bad Gateway");
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "error", "github_unreachable");
        char *s = cJSON_PrintUnformatted(e); cJSON_Delete(e);
        httpd_resp_sendstr(req, s); free(s);
        return ESP_OK;
    }

    char *s = ota_info_to_json(&info);
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

// ── FreeRTOS OTA task ────────────────────────────────────────────────────────
// OTA kører i en separat task — ESP32 kan ikke serve HTTP mens den flasher

typedef struct {
    char url[256];      // tom → opslag på GitHub latest release i tasken
    bool is_firmware;
} ota_task_args_t;

static void ota_task(void *arg)
{
    ota_task_args_t *args = (ota_task_args_t *)arg;
    char url[256];
    strncpy(url, args->url, sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';
    bool is_fw = args->is_firmware;
    free(args);

    // M5: URL-opslag (blokerende GitHub-HTTP) sker HER i tasken — ikke i
    // httpd-handleren — så API'et ikke stalles mens vi kontakter GitHub.
    if (url[0] == '\0') {
        ota_info_t info;
        if (ota_check(&info) != ESP_OK) {
            ota_report_error("GitHub unreachable");
            vTaskDelete(NULL);
            return;
        }
        const char *u    = is_fw ? info.firmware_url       : info.frontend_url;
        bool        avail = is_fw ? info.firmware_available : info.frontend_available;
        if (!avail || !u[0]) {
            ota_report_error("no_update_available");
            vTaskDelete(NULL);
            return;
        }
        strncpy(url, u, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
    }

    if (is_fw) ota_update_firmware(url);
    else       ota_update_frontend(url);
    vTaskDelete(NULL);
}

// ── POST /api/v1/system/ota/firmware ────────────────────────────────────────
// Body (valgfri): {"url": "https://..."} — ellers bruges GitHub latest release

static esp_err_t post_ota_firmware_handler(httpd_req_t *req)
{
    char body[300] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);

    ota_task_args_t *args = calloc(1, sizeof(ota_task_args_t));
    args->is_firmware = true;

    // Brug URL fra body hvis angivet — ellers lader vi tasken slå op på GitHub
    // (M5: ingen blokerende HTTP her i handleren).
    cJSON *json = body[0] ? cJSON_Parse(body) : NULL;
    cJSON *url_item = json ? cJSON_GetObjectItem(json, "url") : NULL;
    if (url_item && cJSON_IsString(url_item)) {
        strncpy(args->url, url_item->valuestring, sizeof(args->url) - 1);
    }
    cJSON_Delete(json);

    ESP_LOGI(TAG, "Queuing firmware OTA: %s", args->url[0] ? args->url : "(GitHub latest)");
    xTaskCreate(ota_task, "ota_fw", 8192, args, 5, NULL);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"firmware_update_started\"}");
    return ESP_OK;
}

// ── POST /api/v1/system/ota/frontend ────────────────────────────────────────

static esp_err_t post_ota_frontend_handler(httpd_req_t *req)
{
    char body[300] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);

    ota_task_args_t *args = calloc(1, sizeof(ota_task_args_t));
    args->is_firmware = false;

    // URL fra body hvis angivet — ellers slår tasken op på GitHub (M5).
    cJSON *json = body[0] ? cJSON_Parse(body) : NULL;
    cJSON *url_item = json ? cJSON_GetObjectItem(json, "url") : NULL;
    if (url_item && cJSON_IsString(url_item)) {
        strncpy(args->url, url_item->valuestring, sizeof(args->url) - 1);
    }
    cJSON_Delete(json);

    ESP_LOGI(TAG, "Queuing frontend OTA: %s", args->url[0] ? args->url : "(GitHub latest)");
    xTaskCreate(ota_task, "ota_fe", 8192, args, 5, NULL);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"frontend_update_started\"}");
    return ESP_OK;
}

// ── GET /api/v1/system/ota/status ───────────────────────────────────────────

static esp_err_t get_ota_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    char *s = ota_status_to_json(ota_get_status());
    httpd_resp_sendstr(req, s);
    free(s);
    return ESP_OK;
}

// ── Route-definitioner ───────────────────────────────────────────────────────

const httpd_uri_t route_get_ota_check    = { .uri="/api/v1/system/ota/check",    .method=HTTP_GET,  .handler=get_ota_check_handler };
const httpd_uri_t route_post_ota_firmware= { .uri="/api/v1/system/ota/firmware", .method=HTTP_POST, .handler=post_ota_firmware_handler };
const httpd_uri_t route_post_ota_frontend= { .uri="/api/v1/system/ota/frontend", .method=HTTP_POST, .handler=post_ota_frontend_handler };
const httpd_uri_t route_get_ota_status   = { .uri="/api/v1/system/ota/status",   .method=HTTP_GET,  .handler=get_ota_status_handler };
