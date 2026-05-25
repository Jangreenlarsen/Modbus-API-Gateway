#include "ethernet.h"
#include <string.h>
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "ethernet";
static EventGroupHandle_t s_eth_event_group;
#define ETH_CONNECTED_BIT BIT0
static char s_ip[16] = "0.0.0.0";

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", s_ip);
    xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
}

esp_err_t ethernet_init(const eth_config_t *cfg)
{
    s_eth_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    // LAN8720 via RMII — tilpas pins til din hardware
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = 0;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, on_got_ip, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    return ESP_OK;
}

void ethernet_wait_for_ip(uint32_t timeout_ms)
{
    xEventGroupWaitBits(s_eth_event_group, ETH_CONNECTED_BIT, false, true,
                        pdMS_TO_TICKS(timeout_ms));
}

void ethernet_get_ip(char *buf, size_t len)
{
    strncpy(buf, s_ip, len);
}
