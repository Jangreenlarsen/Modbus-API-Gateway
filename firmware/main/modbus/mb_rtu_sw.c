#include "mb_rtu_sw.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "mb_rtu_sw";

// ── CRC-16/IBM (Modbus) ─────────────────────────────────────────────────────

uint16_t mb_crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

// ── RX accumulator ──────────────────────────────────────────────────────────
// SW-UART kalder rx_callback fra ISR — vi putter bytes i en FreeRTOS queue

typedef struct {
    QueueHandle_t queue;
} rx_ctx_t;

static void rx_cb(uint8_t byte, void *ctx)
{
    rx_ctx_t *r = (rx_ctx_t *)ctx;
    xQueueSendFromISR(r->queue, &byte, NULL);
}

// ── Hoved-transaktion ────────────────────────────────────────────────────────

mb_result_t mb_rtu_sw_transaction(sw_uart_t     *uart,
                                   uint16_t       timeout_ms,
                                   const uint8_t *request,
                                   uint16_t       request_len,
                                   uint8_t       *response,
                                   uint16_t      *response_len)
{
    mb_result_t result = {0};

    // Byg komplet frame med CRC
    uint8_t frame[MB_RTU_MAX_FRAME];
    if (request_len + 2 > MB_RTU_MAX_FRAME) {
        result.esp_err = ESP_ERR_INVALID_SIZE; return result;
    }
    memcpy(frame, request, request_len);
    uint16_t crc = mb_crc16(frame, request_len);
    frame[request_len]     = crc & 0xFF;   // CRC low
    frame[request_len + 1] = crc >> 8;     // CRC high

    // Opret midlertidig RX queue og hook callback
    rx_ctx_t rx_ctx = { .queue = xQueueCreate(MB_RTU_MAX_FRAME, 1) };
    sw_uart_config_t *cfg = (sw_uart_config_t *)uart; // kun til at sætte callback
    // Sæt callback direkte via sw_uart's offentlige API er ikke muligt her —
    // callback registreres ved sw_uart_init. Vi bruger en global per-uart queue
    // der initialiseres i mb_interface_init (se nedenfor).
    // Her antager vi at rx_ctx er sat op udefra — se mb_interface_init i interface.c.
    (void)rx_ctx; // placeholder — se interface.c

    // Send frame (sw_uart_write håndterer DE/RE og venter til TX done)
    esp_err_t err = sw_uart_write(uart, frame, request_len + 2);
    if (err != ESP_OK) { result.esp_err = err; return result; }

    // Modtag svar — vent på stilhed i MB_RTU_SILENCE_MS efter første byte
    uint8_t  resp[MB_RTU_MAX_FRAME];
    uint16_t resp_pos   = 0;
    uint32_t deadline   = timeout_ms;
    bool     first_byte = true;

    while (deadline > 0) {
        uint8_t b;
        uint32_t wait = first_byte ? timeout_ms : MB_RTU_SILENCE_MS;
        // sw_uart_ms_since_last_rx bruges til at detektere frame-afslutning
        uint32_t since = sw_uart_ms_since_last_rx(uart);
        if (!first_byte && since >= MB_RTU_SILENCE_MS) break;

        vTaskDelay(pdMS_TO_TICKS(1));
        deadline--;
    }
    // NOTE: Den faktiske byte-akkumulering sker via rx_callback sat i interface.c
    // Dette er skeleton — se interface.c for komplet integration.

    result.esp_err = ESP_ERR_NOT_FINISHED; // erstattes af interface.c
    return result;
}

// ── Komplet SW-UART Modbus implementation ────────────────────────────────────
// Bygger request, sender via sw_uart_write, akkumulerer svar fra rx_queue,
// validerer CRC og parser svar.

// Intern hjælper der bruges af alle FC-funktioner
typedef struct {
    sw_uart_t    *uart;
    uint16_t      timeout_ms;
    uint8_t       request[8];
    uint16_t      req_len;
    uint8_t       response[MB_RTU_MAX_FRAME];
    uint16_t      resp_len;
    QueueHandle_t rx_queue;     // sættes af interface.c via sw_uart userdata
} mb_sw_ctx_t;

// Bruges af interface.c — sæt rx_queue reference på sw_uart instans
// via sw_uart_set_rx_queue (se sw_uart.h extension nedenfor)

static mb_result_t do_transaction(sw_uart_t *uart, QueueHandle_t rx_queue,
                                   uint16_t timeout_ms,
                                   const uint8_t *req, uint16_t req_len,
                                   uint8_t *resp, uint16_t *resp_len)
{
    mb_result_t result = {0};

    // Tilføj CRC
    uint8_t frame[MB_RTU_MAX_FRAME];
    memcpy(frame, req, req_len);
    uint16_t crc = mb_crc16(frame, req_len);
    frame[req_len]   = crc & 0xFF;
    frame[req_len+1] = crc >> 8;

    // Tøm RX queue inden transmission
    xQueueReset(rx_queue);

    // Send
    esp_err_t err = sw_uart_write(uart, frame, req_len + 2);
    if (err != ESP_OK) { result.esp_err = err; return result; }

    // Modtag svar — bytes ankommer via ISR → queue
    uint8_t  buf[MB_RTU_MAX_FRAME];
    uint16_t pos = 0;
    TickType_t first_byte_timeout = pdMS_TO_TICKS(timeout_ms);
    TickType_t inter_byte_timeout = pdMS_TO_TICKS(MB_RTU_SILENCE_MS + 1);
    TickType_t wait = first_byte_timeout;

    while (pos < MB_RTU_MAX_FRAME) {
        uint8_t b;
        if (xQueueReceive(rx_queue, &b, wait) != pdTRUE) break; // timeout = frame slut
        buf[pos++] = b;
        wait = inter_byte_timeout; // skift til inter-byte timeout efter første byte
    }

    if (pos < 4) {
        // Minimum: addr(1) + FC(1) + CRC(2)
        result.esp_err = (pos == 0) ? ESP_ERR_TIMEOUT : ESP_FAIL;
        ESP_LOGW(TAG, "Short response: %d bytes", pos);
        return result;
    }

    // Valider CRC
    uint16_t recv_crc = buf[pos-2] | ((uint16_t)buf[pos-1] << 8);
    uint16_t calc_crc = mb_crc16(buf, pos - 2);
    if (recv_crc != calc_crc) {
        ESP_LOGW(TAG, "CRC error: recv=0x%04X calc=0x%04X", recv_crc, calc_crc);
        result.esp_err = ESP_FAIL;
        return result;
    }

    // Tjek for Modbus exception (FC | 0x80)
    if (buf[1] & 0x80) {
        result.modbus_exception = pos > 2 ? buf[2] : 1;
        result.esp_err = ESP_FAIL;
        return result;
    }

    memcpy(resp, buf, pos);
    *resp_len = pos;
    return result;
}

// Alle FC-funktioner bruger do_transaction — rx_queue hentes fra sw_uart userdata
// Interface.c sætter queuen ved init og passer den med til alle kald.

mb_result_t mb_sw_read_holding_regs(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                                     uint16_t start, uint16_t count, uint16_t *out)
{
    uint8_t req[] = { slave, 0x03, start >> 8, start & 0xFF, count >> 8, count & 0xFF };
    uint8_t resp[MB_RTU_MAX_FRAME]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    mb_result_t r = do_transaction(u, q, tmo, req, 6, resp, &resp_len);
    if (r.esp_err != ESP_OK) return r;
    uint8_t byte_count = resp[2];
    for (int i = 0; i < byte_count / 2 && i < count; i++)
        out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    return r;
}

mb_result_t mb_sw_read_input_regs(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                                   uint16_t start, uint16_t count, uint16_t *out)
{
    uint8_t req[] = { slave, 0x04, start >> 8, start & 0xFF, count >> 8, count & 0xFF };
    uint8_t resp[MB_RTU_MAX_FRAME]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    mb_result_t r = do_transaction(u, q, tmo, req, 6, resp, &resp_len);
    if (r.esp_err != ESP_OK) return r;
    uint8_t byte_count = resp[2];
    for (int i = 0; i < byte_count / 2 && i < count; i++)
        out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    return r;
}

mb_result_t mb_sw_read_coils(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                              uint16_t start, uint16_t count, uint8_t *out)
{
    uint8_t req[] = { slave, 0x01, start >> 8, start & 0xFF, count >> 8, count & 0xFF };
    uint8_t resp[MB_RTU_MAX_FRAME]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    mb_result_t r = do_transaction(u, q, tmo, req, 6, resp, &resp_len);
    if (r.esp_err == ESP_OK) memcpy(out, resp + 3, resp[2]);
    return r;
}

mb_result_t mb_sw_read_discrete(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                                 uint16_t start, uint16_t count, uint8_t *out)
{
    uint8_t req[] = { slave, 0x02, start >> 8, start & 0xFF, count >> 8, count & 0xFF };
    uint8_t resp[MB_RTU_MAX_FRAME]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    mb_result_t r = do_transaction(u, q, tmo, req, 6, resp, &resp_len);
    if (r.esp_err == ESP_OK) memcpy(out, resp + 3, resp[2]);
    return r;
}

mb_result_t mb_sw_write_coil(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                              uint16_t addr, uint8_t value)
{
    uint16_t v = value ? 0xFF00 : 0x0000;
    uint8_t req[] = { slave, 0x05, addr >> 8, addr & 0xFF, v >> 8, v & 0xFF };
    uint8_t resp[8]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    return do_transaction(u, q, tmo, req, 6, resp, &resp_len);
}

mb_result_t mb_sw_write_register(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                                  uint16_t addr, uint16_t value)
{
    uint8_t req[] = { slave, 0x06, addr >> 8, addr & 0xFF, value >> 8, value & 0xFF };
    uint8_t resp[8]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    return do_transaction(u, q, tmo, req, 6, resp, &resp_len);
}

mb_result_t mb_sw_write_coils(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                               uint16_t start, uint16_t count, const uint8_t *bits)
{
    uint8_t byte_count = (count + 7) / 8;
    uint8_t req[7 + 31]; // max 248 coils
    req[0] = slave; req[1] = 0x0F;
    req[2] = start >> 8; req[3] = start & 0xFF;
    req[4] = count >> 8; req[5] = count & 0xFF;
    req[6] = byte_count;
    memcpy(req + 7, bits, byte_count);
    uint8_t resp[8]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    return do_transaction(u, q, tmo, req, 7 + byte_count, resp, &resp_len);
}

mb_result_t mb_sw_write_registers(sw_uart_t *u, uint16_t tmo, uint8_t slave,
                                   uint16_t start, uint16_t count, const uint16_t *regs)
{
    uint8_t byte_count = count * 2;
    uint8_t req[7 + 246];
    req[0] = slave; req[1] = 0x10;
    req[2] = start >> 8; req[3] = start & 0xFF;
    req[4] = count >> 8; req[5] = count & 0xFF;
    req[6] = byte_count;
    for (int i = 0; i < count; i++) {
        req[7 + i*2]   = regs[i] >> 8;
        req[7 + i*2+1] = regs[i] & 0xFF;
    }
    uint8_t resp[8]; uint16_t resp_len = 0;
    QueueHandle_t q = (QueueHandle_t)sw_uart_get_userdata(u);
    return do_transaction(u, q, tmo, req, 7 + byte_count, resp, &resp_len);
}
