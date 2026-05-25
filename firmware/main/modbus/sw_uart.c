#include "sw_uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "sw_uart";

// ── Intern struktur ─────────────────────────────────────────────────────────

struct sw_uart_t {
    sw_uart_config_t cfg;
    uint32_t         bit_us;     // bit-periode i mikrosekunder (1 000 000 / baudrate)
    uint32_t         half_bit_us;

    // TX state (styret fra task-kontekst)
    SemaphoreHandle_t tx_mutex;

    // RX state (styret fra ISR)
    volatile bool    rx_active;      // er vi midt i at modtage en frame?
    volatile uint8_t rx_bits;        // bit-tæller (0=start, 1-8=data, 9=stop)
    volatile uint8_t rx_byte;        // byte under opbygning
    volatile int64_t rx_last_us;     // esp_timer_get_time() ved seneste byte

    // Userdata — bruges af mb_rtu_sw.c til at gemme rx_queue reference
    void            *userdata;

    // gptimer — deles mellem TX og RX, men aldrig samtidigt
    gptimer_handle_t timer;
    volatile enum { TIMER_IDLE, TIMER_TX, TIMER_RX } timer_mode;

    // TX buffer (midlertidig reference under transmission)
    volatile const uint8_t *tx_buf;
    volatile size_t          tx_len;
    volatile size_t          tx_pos;     // hvilken byte er vi på?
    volatile uint8_t         tx_shift;   // current byte der sendes ud
    volatile uint8_t         tx_bit;     // bit-tæller (0=start, 1-8=data, 9=stop)
    SemaphoreHandle_t        tx_done;
};

// ── Hjælpemakroer ───────────────────────────────────────────────────────────

static inline void de_tx(sw_uart_t *u)
{
    if (u->cfg.de_pin != GPIO_NUM_NC)
        gpio_set_level(u->cfg.de_pin, 1);
}
static inline void de_rx(sw_uart_t *u)
{
    if (u->cfg.de_pin != GPIO_NUM_NC)
        gpio_set_level(u->cfg.de_pin, 0);
}

// ── TX gptimer alarm callback ────────────────────────────────────────────────
// Kaldes hvert bit_us µs under transmission

static bool IRAM_ATTR tx_timer_cb(gptimer_handle_t timer,
                                   const gptimer_alarm_event_data_t *edata,
                                   void *arg)
{
    sw_uart_t *u = (sw_uart_t *)arg;
    BaseType_t woken = pdFALSE;

    if (u->tx_bit == 0) {
        // Start-bit
        gpio_set_level(u->cfg.tx_pin, 0);
        u->tx_shift = u->tx_buf[u->tx_pos];
        u->tx_bit   = 1;
    } else if (u->tx_bit <= 8) {
        // Data-bits LSB first
        gpio_set_level(u->cfg.tx_pin, u->tx_shift & 1);
        u->tx_shift >>= 1;
        u->tx_bit++;
    } else {
        // Stop-bit
        gpio_set_level(u->cfg.tx_pin, 1);
        u->tx_pos++;
        u->tx_bit = 0;

        if (u->tx_pos >= u->tx_len) {
            // Al data sendt — stop timer og frigiv DE
            gptimer_stop(timer);
            u->timer_mode = TIMER_IDLE;
            // DE → RX mode med lille delay (1 stop-bit ekstra)
            esp_rom_delay_us(u->bit_us);
            de_rx(u);
            xSemaphoreGiveFromISR(u->tx_done, &woken);
        }
    }
    return woken == pdTRUE;
}

// ── RX gptimer alarm callback ────────────────────────────────────────────────
// Samples bits i midten af bit-vinduer

static bool IRAM_ATTR rx_timer_cb(gptimer_handle_t timer,
                                   const gptimer_alarm_event_data_t *edata,
                                   void *arg)
{
    sw_uart_t *u = (sw_uart_t *)arg;
    BaseType_t woken = pdFALSE;

    if (u->rx_bits == 0) {
        // Verificér start-bit stadig lav (undgå spike)
        if (gpio_get_level(u->cfg.rx_pin) != 0) {
            gptimer_stop(timer);
            u->timer_mode = TIMER_IDLE;
            u->rx_active  = false;
            gpio_intr_enable(u->cfg.rx_pin);
            return false;
        }
        // Skift alarm til fuld bit-periode for data-bits
        gptimer_alarm_config_t alarm = {
            .alarm_count               = u->bit_us,
            .reload_count              = 0,
            .flags.auto_reload_on_alarm = true,
        };
        gptimer_set_alarm_action(timer, &alarm);
        u->rx_bits = 1;
    } else if (u->rx_bits <= 8) {
        u->rx_byte |= (gpio_get_level(u->cfg.rx_pin) << (u->rx_bits - 1));
        u->rx_bits++;
    } else {
        // Stop-bit — byte komplet
        gptimer_stop(timer);
        u->timer_mode = TIMER_IDLE;
        u->rx_active  = false;
        u->rx_last_us = esp_timer_get_time();

        if (u->cfg.rx_callback)
            u->cfg.rx_callback(u->rx_byte, u->cfg.rx_callback_ctx);

        u->rx_byte = 0;
        u->rx_bits = 0;
        gpio_intr_enable(u->cfg.rx_pin);
    }
    return woken == pdTRUE;
}

// ── GPIO interrupt — start-bit detektion ────────────────────────────────────

static void IRAM_ATTR rx_gpio_isr(void *arg)
{
    sw_uart_t *u = (sw_uart_t *)arg;
    if (u->rx_active || u->timer_mode != TIMER_IDLE) return;

    gpio_intr_disable(u->cfg.rx_pin);
    u->rx_active  = true;
    u->rx_byte    = 0;
    u->rx_bits    = 0;
    u->timer_mode = TIMER_RX;

    // Første alarm ved half_bit_us — sampler midt i start-bit for verifikation
    gptimer_alarm_config_t alarm = {
        .alarm_count               = u->half_bit_us,
        .reload_count              = 0,
        .flags.auto_reload_on_alarm = false,
    };
    gptimer_set_alarm_action(u->timer, &alarm);
    gptimer_set_raw_count(u->timer, 0);
    gptimer_start(u->timer);
}

// ── Initialisering ───────────────────────────────────────────────────────────

esp_err_t sw_uart_init(sw_uart_t **out, const sw_uart_config_t *cfg)
{
    if (cfg->baudrate > SW_UART_MAX_BAUD) {
        ESP_LOGE(TAG, "Baudrate %lu > max %d — brug hardware UART", cfg->baudrate, SW_UART_MAX_BAUD);
        return ESP_ERR_INVALID_ARG;
    }

    sw_uart_t *u = calloc(1, sizeof(sw_uart_t));
    if (!u) return ESP_ERR_NO_MEM;

    memcpy(&u->cfg, cfg, sizeof(sw_uart_config_t));
    u->bit_us      = 1000000 / cfg->baudrate;
    u->half_bit_us = u->bit_us / 2;
    u->tx_mutex    = xSemaphoreCreateMutex();
    u->tx_done     = xSemaphoreCreateBinary();

    // TX pin — idle høj (RS232/RS485 mark)
    gpio_config_t tx_cfg = {
        .pin_bit_mask = (1ULL << cfg->tx_pin),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&tx_cfg);
    gpio_set_level(cfg->tx_pin, 1);

    // RX pin — input med pull-up
    gpio_config_t rx_cfg = {
        .pin_bit_mask = (1ULL << cfg->rx_pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,  // faldende flanke = start-bit
    };
    gpio_config(&rx_cfg);

    // DE pin — start i RX-tilstand
    if (cfg->de_pin != GPIO_NUM_NC) {
        gpio_config_t de_cfg = {
            .pin_bit_mask = (1ULL << cfg->de_pin),
            .mode         = GPIO_MODE_OUTPUT,
        };
        gpio_config(&de_cfg);
        gpio_set_level(cfg->de_pin, 0);
    }

    // gptimer — 1 µs opløsning
    gptimer_config_t timer_cfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,   // 1 µs pr. tick
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &u->timer));

    // Registrér begge callbacks — skifter mellem TX og RX mode
    gptimer_event_callbacks_t cbs = { .on_alarm = rx_timer_cb };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(u->timer, &cbs, u));
    ESP_ERROR_CHECK(gptimer_enable(u->timer));

    // GPIO ISR — installer service hvis ikke allerede gjort
    gpio_install_isr_service(0);
    gpio_isr_handler_add(cfg->rx_pin, rx_gpio_isr, u);

    ESP_LOGI(TAG, "SW-UART init: TX=GPIO%d RX=GPIO%d DE=GPIO%d @ %lu baud (bit=%lu µs)",
             cfg->tx_pin, cfg->rx_pin, cfg->de_pin, cfg->baudrate, u->bit_us);

    *out = u;
    return ESP_OK;
}

void sw_uart_deinit(sw_uart_t *u)
{
    if (!u) return;
    gpio_isr_handler_remove(u->cfg.rx_pin);
    gptimer_disable(u->timer);
    gptimer_del_timer(u->timer);
    vSemaphoreDelete(u->tx_mutex);
    vSemaphoreDelete(u->tx_done);
    free(u);
}

// ── Transmission ─────────────────────────────────────────────────────────────

esp_err_t sw_uart_write(sw_uart_t *u, const uint8_t *data, size_t len)
{
    if (!u || !data || len == 0) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(u->tx_mutex, portMAX_DELAY);

    // Vent til timer er idle (RX kan være aktiv)
    while (u->timer_mode != TIMER_IDLE) {
        xSemaphoreGive(u->tx_mutex);
        vTaskDelay(1);
        xSemaphoreTake(u->tx_mutex, portMAX_DELAY);
    }

    de_tx(u);
    esp_rom_delay_us(2);    // DE setup-tid

    u->tx_buf    = data;
    u->tx_len    = len;
    u->tx_pos    = 0;
    u->tx_bit    = 0;
    u->timer_mode = TIMER_TX;

    // Skift timer-callback til TX
    gptimer_event_callbacks_t cbs = { .on_alarm = tx_timer_cb };
    gptimer_register_event_callbacks(u->timer, &cbs, u);

    gptimer_alarm_config_t alarm = {
        .alarm_count               = u->bit_us,
        .reload_count              = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(u->timer, &alarm);
    gptimer_set_raw_count(u->timer, 0);
    gptimer_start(u->timer);

    // Vent til TX er færdig
    xSemaphoreTake(u->tx_done, pdMS_TO_TICKS(1000));

    // Skift timer-callback tilbage til RX
    gptimer_event_callbacks_t rx_cbs = { .on_alarm = rx_timer_cb };
    gptimer_register_event_callbacks(u->timer, &rx_cbs, u);

    xSemaphoreGive(u->tx_mutex);
    return ESP_OK;
}

uint32_t sw_uart_ms_since_last_rx(sw_uart_t *u)
{
    if (u->rx_last_us == 0) return UINT32_MAX;
    return (uint32_t)((esp_timer_get_time() - u->rx_last_us) / 1000);
}

void sw_uart_set_userdata(sw_uart_t *u, void *userdata)
{
    if (u) u->userdata = userdata;
}

void *sw_uart_get_userdata(sw_uart_t *u)
{
    return u ? u->userdata : NULL;
}
