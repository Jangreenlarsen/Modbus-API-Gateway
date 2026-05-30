#include "mgmt.h"

extern const char mgmt_page_html_start[] asm("_binary_mgmt_page_html_start");
extern const char mgmt_page_html_end[]   asm("_binary_mgmt_page_html_end");

static esp_err_t get_mgmt_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, mgmt_page_html_start,
                    mgmt_page_html_end - mgmt_page_html_start);
    return ESP_OK;
}

const httpd_uri_t route_get_mgmt = {
    .uri     = "/mgmt",
    .method  = HTTP_GET,
    .handler = get_mgmt_handler,
};
