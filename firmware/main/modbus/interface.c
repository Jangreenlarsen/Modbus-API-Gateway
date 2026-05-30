#include "interface.h"
#include "mb_rtu_sw.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_modbus_common.h"
#include "esp_modbus_master.h"
#include "esp_modbus_slave.h"
#include "freertos/queue.h"
#include <string.h>

// Modbus function codes — mb_functioncode_t er ikke eksporteret i esp-modbus v1.x
#define MB_FUNC_READ_COILS                0x01
#define MB_FUNC_READ_DISCRETE_INPUTS      0x02
#define MB_FUNC_READ_HOLDING_REGISTER     0x03
#define MB_FUNC_READ_INPUT_REGISTER       0x04
#define MB_FUNC_WRITE_SINGLE_COIL         0x05
#define MB_FUNC_WRITE_REGISTER            0x06
#define MB_FUNC_WRITE_MULTIPLE_COILS      0x0F
#define MB_FUNC_WRITE_MULTIPLE_REGISTERS  0x10

static const char *TAG = "mb_iface";

// ── RX callback fra SW-UART ISR → FreeRTOS queue ───────────────────────────

static void sw_rx_callback(uint8_t byte, void *ctx)
{
    QueueHandle_t q = (QueueHandle_t)ctx;
    xQueueSendFromISR(q, &byte, NULL);
}

// ── Init ─────────────────────────────────────────────────────────────────────

static esp_err_t init_hw_master(mb_interface_t *iface, const iface_config_t *cfg)
{
    if (cfg->uart_num < 0 || cfg->uart_num >= UART_NUM_MAX) {
        ESP_LOGE(TAG, "Interface %d: uart_num=%d ugyldig (0..%d)",
                 cfg->id, cfg->uart_num, UART_NUM_MAX - 1);
        return ESP_ERR_INVALID_ARG;
    }
    mb_communication_info_t comm = {
        .port     = cfg->uart_num,
        .mode     = MB_MODE_RTU,
        .baudrate = cfg->baudrate,
        .parity   = cfg->parity,
    };
    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &iface->mb_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_master_init: %s", cfg->id, esp_err_to_name(err)); return err; }
    err = mbc_master_setup((void*)&comm);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_master_setup: %s", cfg->id, esp_err_to_name(err)); mbc_master_destroy(); return err; }
    uart_set_pin(cfg->uart_num, cfg->tx_pin, cfg->rx_pin,
                 cfg->rts_pin >= 0 ? cfg->rts_pin : UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    err = mbc_master_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_master_start: %s", cfg->id, esp_err_to_name(err)); mbc_master_destroy(); return err; }
    if (cfg->type == IFACE_TYPE_RS485)
        uart_set_mode(cfg->uart_num, UART_MODE_RS485_HALF_DUPLEX);
    ESP_LOGI(TAG, "HW-UART MASTER interface %d: %s UART%d @ %lu baud",
             cfg->id, cfg->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
             cfg->uart_num, cfg->baudrate);
    return ESP_OK;
}

static esp_err_t init_hw_slave(mb_interface_t *iface, const iface_config_t *cfg)
{
    if (cfg->uart_num < 0 || cfg->uart_num >= UART_NUM_MAX) {
        ESP_LOGE(TAG, "Interface %d: uart_num=%d ugyldig (0..%d)",
                 cfg->id, cfg->uart_num, UART_NUM_MAX - 1);
        return ESP_ERR_INVALID_ARG;
    }
    mb_communication_info_t comm = {
        .port       = cfg->uart_num,
        .mode       = MB_MODE_RTU,
        .baudrate   = cfg->baudrate,
        .parity     = cfg->parity,
        .slave_addr = cfg->slave_addr,
    };
    esp_err_t err = mbc_slave_init(MB_PORT_SERIAL_SLAVE, &iface->mb_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_slave_init: %s", cfg->id, esp_err_to_name(err)); return err; }
    err = mbc_slave_setup((void*)&comm);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_slave_setup: %s", cfg->id, esp_err_to_name(err)); mbc_slave_destroy(); return err; }
    uart_set_pin(cfg->uart_num, cfg->tx_pin, cfg->rx_pin,
                 cfg->rts_pin >= 0 ? cfg->rts_pin : UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Nulstil register-lager
    memset(iface->slave_holding,  0, sizeof(iface->slave_holding));
    memset(iface->slave_input,    0, sizeof(iface->slave_input));
    memset(iface->slave_coils,    0, sizeof(iface->slave_coils));
    memset(iface->slave_discrete, 0, sizeof(iface->slave_discrete));

    // Registrér register-områder i esp-modbus slave
    mb_register_area_descriptor_t areas[] = {
        { .type = MB_PARAM_HOLDING,  .start_offset = 0, .address = iface->slave_holding,  .size = sizeof(iface->slave_holding) },
        { .type = MB_PARAM_INPUT,    .start_offset = 0, .address = iface->slave_input,    .size = sizeof(iface->slave_input) },
        { .type = MB_PARAM_COIL,     .start_offset = 0, .address = iface->slave_coils,    .size = sizeof(iface->slave_coils) },
        { .type = MB_PARAM_DISCRETE, .start_offset = 0, .address = iface->slave_discrete, .size = sizeof(iface->slave_discrete) },
    };
    for (int i = 0; i < 4; i++) {
        err = mbc_slave_set_descriptor(areas[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Interface %d: mbc_slave_set_descriptor[%d]: %s", cfg->id, i, esp_err_to_name(err));
            mbc_slave_destroy();
            return err;
        }
    }
    err = mbc_slave_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "Interface %d: mbc_slave_start: %s", cfg->id, esp_err_to_name(err)); mbc_slave_destroy(); return err; }
    if (cfg->type == IFACE_TYPE_RS485)
        uart_set_mode(cfg->uart_num, UART_MODE_RS485_HALF_DUPLEX);
    ESP_LOGI(TAG, "HW-UART SLAVE interface %d: %s UART%d addr=%d @ %lu baud",
             cfg->id, cfg->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
             cfg->uart_num, cfg->slave_addr, cfg->baudrate);
    return ESP_OK;
}

esp_err_t mb_interface_init(mb_interface_t *iface, const iface_config_t *cfg)
{
    memcpy(&iface->cfg, cfg, sizeof(iface_config_t));
    iface->mutex = xSemaphoreCreateMutex();

    if (cfg->uart_mode == IFACE_UART_HW) {
        return (cfg->mode == IFACE_MODE_SLAVE)
               ? init_hw_slave(iface, cfg)
               : init_hw_master(iface, cfg);
    }

    // ── Software UART — kun master understøttes ───────────────────────────
    if (cfg->mode == IFACE_MODE_SLAVE) {
        ESP_LOGE(TAG, "Interface %d: SW-UART slave mode understøttes ikke", cfg->id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (cfg->baudrate > SW_UART_MAX_BAUD) {
        ESP_LOGE(TAG, "Interface %d: baudrate %lu > SW-UART max %d",
                 cfg->id, cfg->baudrate, SW_UART_MAX_BAUD);
        return ESP_ERR_INVALID_ARG;
    }
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
    sw_uart_set_userdata(iface->sw_uart, rx_q);
    ESP_LOGI(TAG, "SW-UART MASTER interface %d: %s TX=GPIO%d RX=GPIO%d DE=GPIO%d @ %lu baud",
             cfg->id, cfg->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
             cfg->tx_pin, cfg->rx_pin, cfg->rts_pin, cfg->baudrate);
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
