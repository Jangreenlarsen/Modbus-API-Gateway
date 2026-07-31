#include "modbus_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static modbus_log_entry_t s_buf[MODBUS_LOG_CAP];
static int                s_head  = 0;
static int                s_count = 0;
static uint32_t           s_seq   = 0;
static SemaphoreHandle_t  s_mtx   = NULL;   // kaldes fra httpd-task + refresh-task
static modbus_log_broadcast_cb_t s_broadcast_cb = NULL;

void modbus_log_set_broadcast_cb(modbus_log_broadcast_cb_t cb) { s_broadcast_cb = cb; }

// Fælles felt-serialisering — bruges både af modbus_log_since_json (array af
// entries) og broadcast-hooket (ét enkelt element pr. kald).
static cJSON *entry_to_cjson(const modbus_log_entry_t *e)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "seq", (double)e->seq);
    cJSON_AddNumberToObject(o, "t",   (double)e->ts_ms);
    cJSON_AddNumberToObject(o, "if",  e->iface);
    cJSON_AddNumberToObject(o, "sl",  e->slave);
    cJSON_AddNumberToObject(o, "fc",  e->fc);
    cJSON_AddNumberToObject(o, "ad",  e->addr);
    cJSON_AddNumberToObject(o, "ct",  e->count);
    cJSON_AddNumberToObject(o, "st",  e->status);
    if (e->exception) cJSON_AddNumberToObject(o, "ex", e->exception);
    if (e->status == 0) cJSON_AddNumberToObject(o, "v", e->first_val);
    return o;
}

void modbus_log_init(void)
{
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    memset(s_buf, 0, sizeof(s_buf));
    s_head = 0; s_count = 0; s_seq = 0;
}

static uint8_t status_of(mb_result_t r)
{
    if (r.esp_err == ESP_OK)          return 0;
    if (r.esp_err == ESP_ERR_TIMEOUT) return 1;
    if (r.modbus_exception)           return 2;
    return 3;
}

void modbus_log_add(uint8_t iface, uint8_t slave, uint8_t fc,
                    uint16_t addr, uint16_t count, mb_result_t r, uint16_t first_val)
{
    if (!s_mtx) return;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    modbus_log_entry_t *e = &s_buf[s_head];
    e->seq       = ++s_seq;
    e->ts_ms     = (uint32_t)(esp_timer_get_time() / 1000ULL);
    e->iface     = iface;
    e->slave     = slave;
    e->fc        = fc;
    e->addr      = addr;
    e->count     = count;
    e->status    = status_of(r);
    e->exception = r.modbus_exception;
    e->first_val = first_val;
    modbus_log_entry_t snapshot = *e;   // kopi til brug efter unlock (broadcast)
    s_head = (s_head + 1) % MODBUS_LOG_CAP;
    if (s_count < MODBUS_LOG_CAP) s_count++;
    xSemaphoreGive(s_mtx);

    // Broadcast UDENFOR kritisk sektion — JSON-serialisering + WS-afsendelse
    // skal ikke blokere andre tråde der venter på cache-/log-mutex'en.
    if (s_broadcast_cb) {
        cJSON *o = entry_to_cjson(&snapshot);
        char *json = cJSON_PrintUnformatted(o);
        cJSON_Delete(o);
        if (json) {
            s_broadcast_cb(json);
            free(json);
        }
    }
}

char *modbus_log_since_json(uint32_t since_seq)
{
    cJSON *root = cJSON_CreateObject();
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
    cJSON_AddNumberToObject(root, "n", (double)s_seq);
    cJSON *arr = cJSON_AddArrayToObject(root, "entries");

    int start = (s_count < MODBUS_LOG_CAP) ? 0 : s_head;
    for (int i = 0; i < s_count; i++) {
        modbus_log_entry_t *e = &s_buf[(start + i) % MODBUS_LOG_CAP];
        if (e->seq <= since_seq) continue;
        cJSON_AddItemToArray(arr, entry_to_cjson(e));
    }
    if (s_mtx) xSemaphoreGive(s_mtx);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

void modbus_log_clear(void)
{
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
    memset(s_buf, 0, sizeof(s_buf));
    s_head = 0; s_count = 0;
    if (s_mtx) xSemaphoreGive(s_mtx);
}
