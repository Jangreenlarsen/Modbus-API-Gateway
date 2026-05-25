#include "interface.h"
#include "mb_rtu_sw.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "mbcontroller.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "mb_iface";

// ── RX callback fra SW-UART ISR → FreeRTOS queue ───────────────────────────

static void sw_rx_callback(uint8_t byte, void *ctx)
{
    QueueHandle_t q = (QueueHandle_t)ctx;
    xQueueSendFromISR(q, &byte, NULL);
}

// ── Init ─────────────────────────────────────────────────────────────────────

esp_err_t mb_interface_init(mb_interface_t *iface, const iface_config_t *cfg)
{
    memcpy(&iface->cfg, cfg, sizeof(iface_config_t));
    iface->mutex = xSemaphoreCreateMutex();

    if (cfg->uart_mode == IFACE_UART_HW) {
        // ── Hardware UART via esp-modbus ──────────────────────────────────
        mb_communication_info_t comm = {
            .port     = cfg->uart_num,
            .mode     = MB_MODE_RTU,
            .baudrate = cfg->baudrate,
            .parity   = cfg->parity,
        };
        ESP_ERROR_CHECK(mbc_master_init(MB_PORT_SERIAL_MASTER, &iface->mb_handle));
        ESP_ERROR_CHECK(mbc_master_setup((void*)&comm));
        uart_set_pin(cfg->uart_num, cfg->tx_pin, cfg->rx_pin,
                     cfg->rts_pin >= 0 ? cfg->rts_pin : UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
        if (cfg->type == IFACE_TYPE_RS485)
            uart_set_mode(cfg->uart_num, UART_MODE_RS485_HALF_DUPLEX);
        ESP_ERROR_CHECK(mbc_master_start());
        ESP_LOGI(TAG, "HW-UART interface %d: %s UART%d @ %lu baud",
                 cfg->id,
                 cfg->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
                 cfg->uart_num, cfg->baudrate);

    } else {
        // ── Software UART via GPIO bit-bang ───────────────────────────────
        if (cfg->baudrate > SW_UART_MAX_BAUD) {
            ESP_LOGE(TAG, "Interface %d: baudrate %lu > SW-UART max %d",
                     cfg->id, cfg->baudrate, SW_UART_MAX_BAUD);
            return ESP_ERR_INVALID_ARG;
        }

        // Opret RX queue — bytes ankommer fra ISR via sw_rx_callback
        QueueHandle_t rx_q = xQueueCreate(256, sizeof(uint8_t));

        sw_uart_config_t sw_cfg = {
            .tx_pin          = cfg->tx_pin,
            .rx_pin          = cfg->rx_pin,
            .de_pin          = cfg->rts_pin >= 0 ? cfg->rts_pin : GPIO_NUM_NC,
            .baudrate        = cfg->baudrate,
            .rx_callback     = sw_rx_callback,
            .rx_callback_ctx = rx_q,
        };
        ESP_ERROR_CHECK(sw_uart_init(&iface->sw_uart, &sw_cfg));

        // Gem queue på sw_uart så mb_rtu_sw.c kan hente den via sw_uart_get_userdata()
        sw_uart_set_userdata(iface->sw_uart, rx_q);

        ESP_LOGI(TAG, "SW-UART interface %d: %s TX=GPIO%d RX=GPIO%d DE=GPIO%d @ %lu baud",
                 cfg->id,
                 cfg->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
                 cfg->tx_pin, cfg->rx_pin, cfg->rts_pin, cfg->baudrate);
    }
    return ESP_OK;
}

// ── Hjælpemakroer ───────────────────────────────────────────────────────────

#define MB_LOCK(iface)   xSemaphoreTake((iface)->mutex, pdMS_TO_TICKS(2000))
#define MB_UNLOCK(iface) xSemaphoreGive((iface)->mutex)
#define SW(iface)        ((iface)->sw_uart)
#define TMO(iface)       ((iface)->cfg.timeout_ms)
#define IS_SW(iface)     ((iface)->cfg.uart_mode == IFACE_UART_SW)

// ── FC01 — Read Coils ────────────────────────────────────────────────────────

mb_result_t mb_interface_read_coils(mb_interface_t *iface, uint8_t slave,
                                     uint16_t start, uint16_t count, uint8_t *out)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_read_coils(SW(iface), TMO(iface), slave, start, count, out);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_READ_COILS, start, count };
        r.esp_err = mbc_master_send_request(&req, out);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC02 — Read Discrete Inputs ──────────────────────────────────────────────

mb_result_t mb_interface_read_discrete_inputs(mb_interface_t *iface, uint8_t slave,
                                               uint16_t start, uint16_t count, uint8_t *out)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_read_discrete(SW(iface), TMO(iface), slave, start, count, out);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_READ_DISCRETE_INPUTS, start, count };
        r.esp_err = mbc_master_send_request(&req, out);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC03 — Read Holding Registers ────────────────────────────────────────────

mb_result_t mb_interface_read_holding_regs(mb_interface_t *iface, uint8_t slave,
                                            uint16_t start, uint16_t count, uint16_t *out)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_read_holding_regs(SW(iface), TMO(iface), slave, start, count, out);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_READ_HOLDING_REGISTER, start, count };
        r.esp_err = mbc_master_send_request(&req, out);
        if (r.esp_err != ESP_OK)
            ESP_LOGW(TAG, "iface%d slave%d FC03 start=%d: %s",
                     iface->cfg.id, slave, start, esp_err_to_name(r.esp_err));
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC04 — Read Input Registers ──────────────────────────────────────────────

mb_result_t mb_interface_read_input_regs(mb_interface_t *iface, uint8_t slave,
                                          uint16_t start, uint16_t count, uint16_t *out)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_read_input_regs(SW(iface), TMO(iface), slave, start, count, out);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_READ_INPUT_REGISTER, start, count };
        r.esp_err = mbc_master_send_request(&req, out);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC05 — Write Single Coil ─────────────────────────────────────────────────

mb_result_t mb_interface_write_coil(mb_interface_t *iface, uint8_t slave,
                                     uint16_t addr, uint8_t value)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_write_coil(SW(iface), TMO(iface), slave, addr, value);
    } else {
        uint16_t v = value ? 0xFF00 : 0x0000;
        mb_param_request_t req = { slave, MB_FUNC_WRITE_SINGLE_COIL, addr, 1 };
        r.esp_err = mbc_master_send_request(&req, &v);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC06 — Write Single Register ─────────────────────────────────────────────

mb_result_t mb_interface_write_register(mb_interface_t *iface, uint8_t slave,
                                         uint16_t addr, uint16_t value)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_write_register(SW(iface), TMO(iface), slave, addr, value);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_WRITE_REGISTER, addr, 1 };
        r.esp_err = mbc_master_send_request(&req, &value);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC0F — Write Multiple Coils ──────────────────────────────────────────────

mb_result_t mb_interface_write_coils(mb_interface_t *iface, uint8_t slave,
                                      uint16_t start, uint16_t count, const uint8_t *bits)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_write_coils(SW(iface), TMO(iface), slave, start, count, bits);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_WRITE_MULTIPLE_COILS, start, count };
        r.esp_err = mbc_master_send_request(&req, (void*)bits);
    }
    MB_UNLOCK(iface);
    return r;
}

// ── FC10 — Write Multiple Registers ─────────────────────────────────────────

mb_result_t mb_interface_write_registers(mb_interface_t *iface, uint8_t slave,
                                          uint16_t start, uint16_t count, const uint16_t *regs)
{
    mb_result_t r = {0};
    if (!MB_LOCK(iface)) { r.esp_err = ESP_ERR_TIMEOUT; return r; }
    if (IS_SW(iface)) {
        r = mb_sw_write_registers(SW(iface), TMO(iface), slave, start, count, regs);
    } else {
        mb_param_request_t req = { slave, MB_FUNC_WRITE_MULTIPLE_REGISTERS, start, count };
        r.esp_err = mbc_master_send_request(&req, (void*)regs);
    }
    MB_UNLOCK(iface);
    return r;
}
