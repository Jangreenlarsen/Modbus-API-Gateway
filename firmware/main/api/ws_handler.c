#include "ws_handler.h"
#include "modbus_log.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ws_handler";
static httpd_handle_t s_server = NULL;

// Skal matche/overstige hcfg.max_open_sockets i server.c.
#define WS_MAX_CLIENTS 10

// ── Broadcast (real-time push af Modbus-bus-aktivitet) ─────────────────────
// httpd_ws_send_frame_async() skriver synkront til socket'en under en
// sessions-lås — ESP-IDF's dokumenterede mønster for kald fra en ANDEN
// kontekst end den http-worker der ejer forbindelsen er at rute det gennem
// httpd_queue_work(), som kører arbejdet i httpd'ens egen task. Hvert
// arbejdselement får sin egen heap-kopi af JSON'en, så broadcast til flere
// klienter ikke deler en buffer hvis levetid ellers ville være uklar.

typedef struct {
    httpd_handle_t hd;
    int             fd;
    char           *json;   // heap-ejet — frigøres af ws_send_work
} ws_send_arg_t;

static void ws_send_work(void *arg)
{
    ws_send_arg_t *a = (ws_send_arg_t *)arg;
    httpd_ws_frame_t pkt = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)a->json,
        .len     = strlen(a->json),
    };
    httpd_ws_send_frame_async(a->hd, a->fd, &pkt);
    free(a->json);
    free(a);
}

// Sendes til modbus_log som broadcast-callback — kaldes hver gang en ny
// bus-transaktion logges (dvs. et rigtigt Modbus-kald, ikke cache-hits).
static void ws_broadcast(const char *json)
{
    if (!s_server) return;

    size_t fds = WS_MAX_CLIENTS;
    int    client_fds[WS_MAX_CLIENTS];
    if (httpd_get_client_list(s_server, &fds, client_fds) != ESP_OK) return;

    for (size_t i = 0; i < fds; i++) {
        if (httpd_ws_get_fd_info(s_server, client_fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET)
            continue;

        ws_send_arg_t *a = malloc(sizeof(*a));
        if (!a) continue;
        a->json = strdup(json);
        if (!a->json) { free(a); continue; }
        a->hd = s_server;
        a->fd = client_fds[i];

        if (httpd_queue_work(s_server, ws_send_work, a) != ESP_OK) {
            free(a->json);
            free(a);
        }
    }
}

void ws_handler_init(httpd_handle_t server)
{
    s_server = server;
    modbus_log_set_broadcast_cb(ws_broadcast);
    ESP_LOGI(TAG, "WebSocket broadcast klar (real-time Modbus-log push)");
}

// ── Handshake + modtagne frames ─────────────────────────────────────────────

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }
    httpd_ws_frame_t pkt;
    uint8_t buf[512] = {0};
    pkt.payload = buf;
    pkt.type    = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &pkt, sizeof(buf));
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "WS recv: %.*s", pkt.len, pkt.payload);
    // Echo for nu — klienter forventer ikke svar (broadcast er envejs push)
    return httpd_ws_send_frame(req, &pkt);
}

const httpd_uri_t route_ws = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = ws_handler,
    .is_websocket = true,
};
