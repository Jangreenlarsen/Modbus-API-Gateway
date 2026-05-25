#include "ws_handler.h"
#include "esp_log.h"

static const char *TAG = "ws_handler";

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
    // Echo for nu — udvid med subscribe-kommandoer
    return httpd_ws_send_frame(req, &pkt);
}

const httpd_uri_t route_ws = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = ws_handler,
    .is_websocket = true,
};
