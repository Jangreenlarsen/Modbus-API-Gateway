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
#include "esp_mac.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include <strings.h>   // strcasecmp

static const char *TAG = "ethernet";
static EventGroupHandle_t s_eth_event_group;
#define ETH_CONNECTED_BIT BIT0
static char s_ip[16] = "0.0.0.0";
static bool s_eth_available = false;
static int  s_w5500_int_gpio = -1;
static TaskHandle_t s_w5500_rx_task = NULL;

// IP-konfiguration (statisk eller DHCP) — spejler wifi_manager
static esp_netif_t        *s_eth_netif     = NULL;
static bool                s_use_static_ip = false;
static esp_netif_ip_info_t s_static_ip;

// Parse cfg->ip/gw/netmask → s_use_static_ip + s_static_ip.
// Ugyldig/korrupt IP → fald tilbage til DHCP (som wifi_manager).
static void configure_ip(const eth_config_t *cfg)
{
    s_use_static_ip = false;
    memset(&s_static_ip, 0, sizeof(s_static_ip));
    if (cfg->ip[0] && strcasecmp(cfg->ip, "dhcp") != 0) {
        ip4_addr_t test;
        if (ip4addr_aton(cfg->ip, &test) && test.addr != 0) {
            ip4_addr_t a;
            ip4addr_aton(cfg->ip,      &a); s_static_ip.ip.addr      = a.addr;
            ip4addr_aton(cfg->gw,      &a); s_static_ip.gw.addr      = a.addr;
            ip4addr_aton(cfg->netmask, &a); s_static_ip.netmask.addr = a.addr;
            s_use_static_ip = true;
            ESP_LOGI(TAG, "Ethernet statisk IP konfigureret: %s  gw %s  mask %s",
                     cfg->ip, cfg->gw, cfg->netmask);
        } else {
            ESP_LOGW(TAG, "Ethernet IP '%s' ugyldig/korrupt — bruger DHCP", cfg->ip);
        }
    } else {
        ESP_LOGI(TAG, "Ethernet DHCP aktiv");
    }
}

// Anvend den konfigurerede IP på netif'et (kaldes ved link-up).
static void apply_ip_config(void)
{
    if (!s_eth_netif) return;
    if (s_use_static_ip) {
        esp_netif_dhcpc_stop(s_eth_netif);   // ignorér fejl hvis allerede stoppet
        esp_netif_set_ip_info(s_eth_netif, &s_static_ip);
        ESP_LOGI(TAG, "Ethernet statisk IP anvendt: " IPSTR, IP2STR(&s_static_ip.ip));
    } else {
        // Tving DHCP-genstart (samme robusthed som wifi_manager) — sikrer DHCP
        // kører selv hvis klienten blev stoppet af en tidligere config.
        esp_netif_dhcpc_stop(s_eth_netif);
        esp_netif_dhcpc_start(s_eth_netif);
        ESP_LOGI(TAG, "Ethernet link UP — DHCP starter");
    }
}

// ETH_EVENT-handler — logger link-tilstand og anvender IP-config ved link-up.
static void on_eth_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet link UP");
        apply_ip_config();
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet link DOWN");
        xEventGroupClearBits(s_eth_event_group, ETH_CONNECTED_BIT);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet driver startet");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet driver stoppet");
        break;
    default:
        break;
    }
}

// ── WORKAROUND for ESP-IDF W5500 edge-triggered ISR-miss ─────────────────────
// W5500's RX task ("w5500_tsk") venter på ulTaskNotifyTake(timeout=1000ms).
// Hvis GPIO faldende-flanke-ISR misser (multi-frame queue, INT holdes LOW),
// vågner tasken kun ved 1000ms timeout → 700-1300ms ping-latency.
// Workaround: en lavprioritets-task der hvert 2ms tjekker INT-pin, og hvis
// LOW (W5500 har pending data) sender xTaskNotifyGive() direkte til RX-tasken,
// helt uden om GPIO ISR-laget. Løst i ESP-IDF v5.x men bug-pattern eksisterer
// stadig i visse pakke-burst-scenarier.
static void w5500_int_poll_task(void *arg)
{
    while (1) {
        if (!s_w5500_rx_task) {
            s_w5500_rx_task = xTaskGetHandle("w5500_tsk");
            if (s_w5500_rx_task)
                ESP_LOGI(TAG, "W5500 RX task fundet — INT-polling aktiv (2ms)");
        }
        if (s_w5500_rx_task && s_w5500_int_gpio >= 0) {
            if (gpio_get_level((gpio_num_t)s_w5500_int_gpio) == 0)
                xTaskNotifyGive(s_w5500_rx_task);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

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
        .mosi_io_num     = cfg->spi_mosi_gpio,
        .miso_io_num     = cfg->spi_miso_gpio,
        .sclk_io_num     = cfg->spi_sclk_gpio,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,  // W5500 max frame: 1518+2 = 1520 bytes
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

    // ESP-IDF kræver enten interrupt ELLER polling — ikke begge.
    // INT pin >= 0  →  brug interrupt, sæt poll_period_ms = 0
    // INT pin <  0  →  brug polling,    sæt int_gpio_num   = -1
    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    if (cfg->spi_int_gpio >= 0) {
        w5500_cfg.int_gpio_num   = cfg->spi_int_gpio;
        w5500_cfg.poll_period_ms = 0;
        // F7: driveren kalder gpio_isr_handler_add() for INT-pinnen ved
        // esp_eth_driver_install() — servicen skal være installeret FØR det,
        // ellers fejler tilmeldingen stille og INT virker aldrig (kørte kun
        // via INT-poll-workaround-tasken nedenfor).
        gpio_install_isr_service(0);
        ESP_LOGI(TAG, "W5500: interrupt-mode GPIO%d (kræver ekstern pull-up til 3.3V)",
                 cfg->spi_int_gpio);
    } else {
        w5500_cfg.int_gpio_num   = -1;
        w5500_cfg.poll_period_ms = cfg->spi_poll_ms;
        ESP_LOGW(TAG, "W5500: polling-mode %dms (sæt 'int <gpio>' for lavere latency)",
                 cfg->spi_poll_ms);
    }

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

    // F6: W5500 har ingen fabriks-MAC (modsat ESP32's interne EMAC) — uden
    // dette kører den med 00:00:00:00:00:00 på ALLE devices, hvilket giver
    // MAC-kollisioner på netværket. ESP_MAC_ETH er en dedikeret, unik
    // Ethernet-MAC udledt af chippens eFuse base-MAC (adskilt fra WiFi-MAC).
    uint8_t mac_addr[6];
    esp_err_t mac_err = esp_read_mac(mac_addr, ESP_MAC_ETH);
    if (mac_err == ESP_OK) {
        mac_err = esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr);
    }
    if (mac_err == ESP_OK) {
        ESP_LOGI(TAG, "W5500 MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    } else {
        ESP_LOGE(TAG, "W5500 MAC-tildeling fejlede (%s) — enheden kan få en ikke-unik MAC",
                 esp_err_to_name(mac_err));
    }

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "W5500 SPI Ethernet initialiseret  %dMHz  INT=%s",
             clk_mhz, (cfg->spi_int_gpio >= 0)
                      ? "interrupt-drevet" : "polling (ingen INT pin)");

    // Start ISR-miss workaround task (kun nyttig i interrupt-mode)
    if (cfg->spi_int_gpio >= 0) {
        s_w5500_int_gpio = cfg->spi_int_gpio;
        xTaskCreate(w5500_int_poll_task, "w5500_int_poll", 2048, NULL, 5, NULL);
        ESP_LOGI(TAG, "W5500 INT-poll workaround startet (omgår ESP-IDF ISR-miss)");
    }
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
    s_eth_netif = eth_netif;

    // Parse statisk-IP vs DHCP FØR driveren starter
    configure_ip(cfg);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_got_ip, NULL));

    esp_err_t ret;
    if (cfg->hw_type == ETH_HW_LAN8720)
        ret = init_lan8720(cfg, eth_netif);
    else
        ret = init_w5500(cfg, eth_netif);

    if (ret != ESP_OK)
        return ret;
    s_eth_available = true;

    // Registrér ETH_EVENT-handleren EFTER init (esp_netif_attach har nu
    // registreret glue-handleren) — så vores handler kører sidst og en statisk
    // IP ikke overskrives af glue'ens DHCP-start ved link-up.
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, on_eth_event, NULL));

    // Hvis link allerede er oppe, anvend IP-config med det samme.
    if (s_use_static_ip) apply_ip_config();
    return ESP_OK;
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
