#pragma once
#include "esp_http_server.h"
#include "modbus_manager.h"   // mb_result_t

// Fælles fejl-respons for alle FC-routes (H3). Sikrer at FC01–FC10 alle
// bruger PRÆCIS samme JSON-format som dokumenteret i ARCHITECTURE.md:
//   504 → {"error":"modbus_timeout", ...}
//   400 → {"error":"modbus_exception","exception_code":N,"description":...}
//   400 → {"error":"modbus_error","detail":"ESP_ERR_..."}
//
// Returnerer true hvis kaldet lykkedes (fortsæt med success-svar). Ved fejl
// sendes fejl-JSON + status, og funktionen returnerer false.
bool api_mb_ok(httpd_req_t *req, mb_result_t r, int iface, int slave);
