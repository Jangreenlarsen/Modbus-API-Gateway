#include "ethernet.h"
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

static const char *TAG = "ethernet";
static EventGroupHandle_t s_eth_event_group;
#define ETH_CONNECTED_BIT BIT0
static char s_ip[16] = "0.0.0.0";
static bool s_eth_available = false;

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", s_ip);
    xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
}

static esp_err_t init_lan8720(const eth_config_t *cfg, esp_netif_t *eth_netif)
{
    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t        mac_cfg  = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t        phy_cfg  = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr       = cfg->phy_addr;
    phy_cfg.reset_gpio_num = cfg->phy_rst_gpio;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle;
    esp_err_t ret = esp_eth_driver_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LAN8720 PHY init fejlede (%s) — kører videre uden Ethernet",
                 esp_err_to_name(ret));
        mac->del(mac);
        phy->del(phy);
        return ret;
    }
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "LAN8720 RMII initialiseret  PHY-addr=%d  MDC=%d  MDIO=%d",
             cfg->phy_addr, cfg->mdc_gpio, cfg->mdio_gpio);
    return ESP_OK;
}

static esp_err_t init_w5500(const eth_config_t *cfg, esp_netif_t *eth_netif)
{
    // Hardware reset
    if (cfg->spi_rst_gpio >= 0) {
        gpio_config_t rst_io = {
            .pin_bit_mask = (1ULL << cfg->spi_rst_gpio),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&rst_io);
        gpio_set_level(cfg->spi_rst_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(cfg->spi_rst_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_LOGI(TAG, "W5500 RST puls på GPIO %d", cfg->spi_rst_gpio);
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num   = cfg->spi_mosi_gpio,
        .miso_io_num   = cfg->spi_miso_gpio,
        .sclk_io_num   = cfg->spi_sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI2 bus init fejlede (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI2 bus: MOSI=%d MISO=%d SCLK=%d CS=%d",
             cfg->spi_mosi_gpio, cfg->spi_miso_gpio,
             cfg->spi_sclk_gpio, cfg->spi_cs_gpio);

    uint8_t clk_mhz = (cfg->spi_clock_mhz >= 1 && cfg->spi_clock_mhz <= 36)
                      ? cfg->spi_clock_mhz : 10;

    spi_device_interface_config_t devcfg = {
        .command_bits     = 16,
        .address_bits     = 8,
        .mode             = 0,
        .clock_speed_hz   = clk_mhz * 1000 * 1000,
        .spics_io_num     = cfg->spi_cs_gpio,
        .queue_size       = 20,
        .cs_ena_posttrans = 5,
    };

    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_cfg.int_gpio_num = cfg->spi_int_gpio;

    if (cfg->spi_int_gpio < 0)
        ESP_LOGW(TAG, "W5500: INT pin ikke konfigureret — kører i polling-mode (høj latency). "
                      "Tilslut INT pin og sæt 'int <gpio>' for interrupt-drevet tilstand.");

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num  = -1;   // RST håndteret ovenfor

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);
    if (!mac) {
        ESP_LOGE(TAG, "W5500 MAC oprettelse fejlede");
        spi_bus_free(SPI2_HOST);
        return ESP_FAIL;
    }
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (!phy) {
        ESP_LOGE(TAG, "W5500 PHY oprettelse fejlede");
        mac->del(mac);
        spi_bus_free(SPI2_HOST);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle;
    ret = esp_eth_driver_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "W5500 driver install fejlede (%s)", esp_err_to_name(ret));
        mac->del(mac);
        phy->del(phy);
        spi_bus_free(SPI2_HOST);
        return ret;
    }

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "W5500 SPI Ethernet initialiseret  %dMHz  INT=%s",
             clk_mhz, (cfg->spi_int_gpio >= 0)
                      ? "interrupt-drevet" : "polling (ingen INT pin)");
    return ESP_OK;
}

esp_err_t ethernet_init(const eth_config_t *cfg)
{
    if (!cfg->enabled || cfg->hw_type == ETH_HW_NONE) {
        ESP_LOGI(TAG, "Ethernet deaktiveret — springer over");
        return ESP_OK;
    }

    s_eth_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t evloop_err = esp_event_loop_create_default();
    if (evloop_err != ESP_OK && evloop_err != ESP_ERR_INVALID_STATE)
        return evloop_err;

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_got_ip, NULL));

    esp_err_t ret;
    if (cfg->hw_type == ETH_HW_LAN8720)
        ret = init_lan8720(cfg, eth_netif);
    else
        ret = init_w5500(cfg, eth_netif);

    if (ret == ESP_OK)
        s_eth_available = true;
    return ret;
}

void ethernet_wait_for_ip(uint32_t timeout_ms)
{
    if (!s_eth_available) return;
    xEventGroupWaitBits(s_eth_event_group, ETH_CONNECTED_BIT, false, true,
                        pdMS_TO_TICKS(timeout_ms));
}

void ethernet_get_ip(char *buf, size_t len)
{
    strncpy(buf, s_ip, len);
}

bool ethernet_is_available(void)
{
    return s_eth_available;
}
