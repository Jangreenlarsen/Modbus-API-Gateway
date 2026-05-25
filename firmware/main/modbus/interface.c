#include "interface.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "mbcontroller.h"
#include <string.h>

static const char *TAG = "mb_iface";

esp_err_t mb_interface_init(mb_interface_t *iface, const iface_config_t *cfg)
{
    memcpy(&iface->cfg, cfg, sizeof(iface_config_t));
    iface->mutex = xSemaphoreCreateMutex();

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

    if (cfg->type == IFACE_TYPE_RS485) {
        uart_set_mode(cfg->uart_num, UART_MODE_RS485_HALF_DUPLEX);
    }

    ESP_ERROR_CHECK(mbc_master_start());
    return ESP_OK;
}

// Hjælpefunktion: tag mutex, udfør kald, frigiv mutex
#define MB_LOCK(iface)   xSemaphoreTake((iface)->mutex, pdMS_TO_TICKS(1000))
#define MB_UNLOCK(iface) xSemaphoreGive((iface)->mutex)

mb_result_t mb_interface_read_holding_regs(mb_interface_t *iface, uint8_t slave,
                                            uint16_t start, uint16_t count, uint16_t *out)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr    = slave,
        .command       = MB_FUNC_READ_HOLDING_REGISTER,
        .reg_start     = start,
        .reg_size      = count,
    };
    esp_err_t err = mbc_master_send_request(&req, (void*)out);
    result.esp_err = err;
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "iface%d slave%d FC03 start=%d err=%s",
                 iface->cfg.id, slave, start, esp_err_to_name(err));
    }
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_read_input_regs(mb_interface_t *iface, uint8_t slave,
                                          uint16_t start, uint16_t count, uint16_t *out)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_READ_INPUT_REGISTER,
        .reg_start  = start,
        .reg_size   = count,
    };
    result.esp_err = mbc_master_send_request(&req, (void*)out);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_read_coils(mb_interface_t *iface, uint8_t slave,
                                     uint16_t start, uint16_t count, uint8_t *out)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_READ_COILS,
        .reg_start  = start,
        .reg_size   = count,
    };
    result.esp_err = mbc_master_send_request(&req, (void*)out);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_read_discrete_inputs(mb_interface_t *iface, uint8_t slave,
                                               uint16_t start, uint16_t count, uint8_t *out)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_READ_DISCRETE_INPUTS,
        .reg_start  = start,
        .reg_size   = count,
    };
    result.esp_err = mbc_master_send_request(&req, (void*)out);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_write_coil(mb_interface_t *iface, uint8_t slave,
                                     uint16_t addr, uint8_t value)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    uint16_t val = value ? 0xFF00 : 0x0000;
    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_WRITE_SINGLE_COIL,
        .reg_start  = addr,
        .reg_size   = 1,
    };
    result.esp_err = mbc_master_send_request(&req, &val);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_write_register(mb_interface_t *iface, uint8_t slave,
                                         uint16_t addr, uint16_t value)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_WRITE_REGISTER,
        .reg_start  = addr,
        .reg_size   = 1,
    };
    result.esp_err = mbc_master_send_request(&req, &value);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_write_coils(mb_interface_t *iface, uint8_t slave,
                                      uint16_t start, uint16_t count, const uint8_t *bits)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_WRITE_MULTIPLE_COILS,
        .reg_start  = start,
        .reg_size   = count,
    };
    result.esp_err = mbc_master_send_request(&req, (void*)bits);
    MB_UNLOCK(iface);
    return result;
}

mb_result_t mb_interface_write_registers(mb_interface_t *iface, uint8_t slave,
                                          uint16_t start, uint16_t count, const uint16_t *regs)
{
    mb_result_t result = {0};
    if (!MB_LOCK(iface)) { result.esp_err = ESP_ERR_TIMEOUT; return result; }

    mb_param_request_t req = {
        .slave_addr = slave,
        .command    = MB_FUNC_WRITE_MULTIPLE_REGISTERS,
        .reg_start  = start,
        .reg_size   = count,
    };
    result.esp_err = mbc_master_send_request(&req, (void*)regs);
    MB_UNLOCK(iface);
    return result;
}
