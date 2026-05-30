#include "serial_cli.h"
#include "config.h"
#include "version.h"
#include "config_store.h"
#include "ethernet.h"
#include "wifi_manager.h"
#include "esp_console.h"
#include "esp_wifi.h"
#include "linenoise/linenoise.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "serial_cli";
static gateway_config_t *s_cfg;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void sep(void) { printf("--------------------------------\r\n"); }

// Fælles Ethernet-hjælp — viser kun GPIO-parametre relevante for valgt type
static void eth_print_help(eth_hw_t hw_type)
{
    printf("  enable / disable              -- aktiver/deaktiver Ethernet\r\n");
    printf("  type lan8720|w5500|none       -- hardware-type\r\n");
    printf("  ip dhcp                       -- DHCP\r\n");
    printf("  ip <ip> <gw> <mask>           -- statisk IP\r\n");
    switch (hw_type) {
        case ETH_HW_LAN8720:
            printf("LAN8720 RMII GPIO:\r\n");
            printf("  phy-addr <0-31>       -- PHY adresse (typisk 0 eller 1)\r\n");
            printf("  mdc <gpio>            -- MDC management clock\r\n");
            printf("  mdio <gpio>           -- MDIO management data\r\n");
            printf("  phy-rst <gpio|-1>     -- PHY reset  (-1=ikke tilsluttet)\r\n");
            break;
        case ETH_HW_W5500:
            printf("W5500 SPI GPIO:\r\n");
            printf("  cs <gpio>             -- SPI CS pin\r\n");
            printf("  mosi <gpio>           -- SPI MOSI pin\r\n");
            printf("  miso <gpio>           -- SPI MISO pin\r\n");
            printf("  sclk <gpio>           -- SPI SCLK pin\r\n");
            printf("  int <gpio|-1>         -- SPI INT pin  (-1=pollet)\r\n");
            break;
        default:
            printf("  (sæt 'type lan8720' eller 'type w5500' for GPIO-parametre)\r\n");
            break;
    }
}

// ── cmd: show ─────────────────────────────────────────────────────────────────

static void show_running_config(void)
{
    static const char par_char[] = { 'N', 'O', 'E' };  // none, odd, even

    printf("!\r\n");

    // ── Interface ETH0 ────────────────────────────────────────────────────────
    printf("Interface ETH0\r\n");
    printf(" %s\r\n", s_cfg->ethernet.enabled ? "Enable" : "Disable");
    switch (s_cfg->ethernet.hw_type) {
        case ETH_HW_LAN8720:
            printf(" Type LAN8720\r\n");
            printf(" PHY-addr %d\r\n",      s_cfg->ethernet.phy_addr);
            printf(" MDC      GPIO %d\r\n", s_cfg->ethernet.mdc_gpio);
            printf(" MDIO     GPIO %d\r\n", s_cfg->ethernet.mdio_gpio);
            if (s_cfg->ethernet.phy_rst_gpio >= 0)
                printf(" PHY-RST  GPIO %d\r\n", s_cfg->ethernet.phy_rst_gpio);
            break;
        case ETH_HW_W5500:
            printf(" Type W5500\r\n");
            printf(" SPI-CS   GPIO %d\r\n", s_cfg->ethernet.spi_cs_gpio);
            printf(" SPI-MOSI GPIO %d\r\n", s_cfg->ethernet.spi_mosi_gpio);
            printf(" SPI-MISO GPIO %d\r\n", s_cfg->ethernet.spi_miso_gpio);
            printf(" SPI-SCLK GPIO %d\r\n", s_cfg->ethernet.spi_sclk_gpio);
            if (s_cfg->ethernet.spi_int_gpio >= 0)
                printf(" SPI-INT  GPIO %d\r\n", s_cfg->ethernet.spi_int_gpio);
            break;
        default:
            printf(" Type none\r\n");
            break;
    }
    if (strcasecmp(s_cfg->ethernet.ip, "dhcp") == 0 || s_cfg->ethernet.ip[0] == '\0') {
        printf(" IP dhcp\r\n");
    } else {
        printf(" IP %s\r\n",      s_cfg->ethernet.ip);
        printf(" Gateway %s\r\n", s_cfg->ethernet.gw[0]      ? s_cfg->ethernet.gw      : "0.0.0.0");
        printf(" Netmask %s\r\n", s_cfg->ethernet.netmask[0] ? s_cfg->ethernet.netmask : "255.255.255.0");
    }
    printf("End interface ETH0\r\n");
    printf("!\r\n");

    // ── Interface WIFI ────────────────────────────────────────────────────────
    printf("Interface WIFI\r\n");
    printf(" %s\r\n", s_cfg->wifi.enabled ? "Enable" : "Disable");
    printf(" mode STA\r\n");
    printf(" SSID \"%s\"\r\n", s_cfg->wifi.ssid[0] ? s_cfg->wifi.ssid : "(ikke sat)");
    printf(" PSK %s\r\n",      s_cfg->wifi.password[0] ? s_cfg->wifi.password : "(ikke sat)");
    if (strcasecmp(s_cfg->wifi.ip, "dhcp") == 0 || s_cfg->wifi.ip[0] == '\0') {
        printf(" IP dhcp\r\n");
    } else {
        printf(" IP %s\r\n",      s_cfg->wifi.ip);
        printf(" Gateway %s\r\n", s_cfg->wifi.gw[0]      ? s_cfg->wifi.gw      : "0.0.0.0");
        printf(" Netmask %s\r\n", s_cfg->wifi.netmask[0] ? s_cfg->wifi.netmask : "255.255.255.0");
    }
    printf("End interface WIFI\r\n");
    printf("!\r\n");

    // ── Interface WIFI-AP ─────────────────────────────────────────────────────
    printf("Interface WIFI-AP\r\n");
    printf(" %s\r\n", s_cfg->wifi.ap_fallback ? "Enable" : "Disable");
    printf(" SSID \"%s\"\r\n", s_cfg->wifi.ap_ssid[0] ? s_cfg->wifi.ap_ssid : "ModbusGW-AUTO");
    printf(" PSK %s\r\n",      s_cfg->wifi.ap_password[0] ? s_cfg->wifi.ap_password : "none (åben)");
    printf(" IP 192.168.4.1\r\n");
    printf("End interface WIFI-AP\r\n");
    printf("!\r\n");

    // ── Modbus interfaces ─────────────────────────────────────────────────────
    for (int i = 0; i < s_cfg->interface_count; i++) {
        iface_config_t *f = &s_cfg->interfaces[i];
        char pch = (f->parity <= 2) ? par_char[f->parity] : 'N';

        printf("Interface Modbus%d\r\n", f->id);
        printf(" %s\r\n", f->enabled ? "Enable" : "Disable");
        printf(" Type %s\r\n",       f->type == IFACE_TYPE_RS485 ? "RS485" : "RS232");
        printf(" UART %s UART%d\r\n", f->uart_mode == IFACE_UART_HW ? "HW" : "SW", f->uart_num);
        printf(" com %luB-%d%c%d\r\n", (unsigned long)f->baudrate,
               f->data_bits, pch, f->stop_bits);
        printf(" Timeout %dms\r\n",   f->timeout_ms);
        printf(" Tx GPIO %d\r\n",     f->tx_pin);
        printf(" Rx GPIO %d\r\n",     f->rx_pin);
        if (f->type == IFACE_TYPE_RS485)
            printf(" DE GPIO %d\r\n", f->rts_pin);
        printf("End interface Modbus%d\r\n", f->id);
        printf("!\r\n");
    }
}

static int cmd_show(int argc, char **argv)
{
    if (argc >= 2 && strcasecmp(argv[1], "config") == 0) {
        show_running_config();
        return 0;
    }

    // uden argument: kort status-oversigt
    sep();
    printf("Brug: show config  -- vis komplet konfiguration\r\n");
    sep();
    return 0;
}

// ── cmd: status ───────────────────────────────────────────────────────────────

static int cmd_status(int argc, char **argv)
{
    char eth_ip[16];
    ethernet_get_ip(eth_ip, sizeof(eth_ip));
    uint64_t uptime_s = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    uint32_t heap_kb  = esp_get_free_heap_size() / 1024;

    sep();
    printf("Version : v%s b%s\r\n", GATEWAY_VERSION, GATEWAY_BUILD);
    printf("Uptime  : %llu s\r\n", uptime_s);
    printf("Eth IP  : %s\r\n", strcmp(eth_ip, "0.0.0.0") == 0 ? "ikke tilgængeligt" : eth_ip);
    printf("Heap    : %lu KB fri\r\n", (unsigned long)heap_kb);
    sep();
    return 0;
}

// ── cmd: eth ──────────────────────────────────────────────────────────────────

static int cmd_eth(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[argc - 1], "?") == 0) {
        if (argc == 2) {
            eth_print_help(s_cfg->ethernet.hw_type); return 0;
        } else if (strcasecmp(argv[1], "type")    == 0) { printf("  lan8720  -- RMII intern MAC\r\n  w5500   -- SPI ekstern MAC\r\n  none    -- ikke valgt\r\n"); }
        else if   (strcasecmp(argv[1], "ip")      == 0) { printf("  dhcp            -- automatisk IP\r\n  <ip> <gw> <mask> -- statisk IP\r\n"); }
        else if   (strcasecmp(argv[1], "phy-addr")== 0) { printf("  <0-31>  -- PHY adresse  (LAN8720)\r\n"); }
        else if   (strcasecmp(argv[1], "mdc")     == 0) { printf("  <gpio>  -- MDC management clock  (LAN8720)\r\n"); }
        else if   (strcasecmp(argv[1], "mdio")    == 0) { printf("  <gpio>  -- MDIO management data  (LAN8720)\r\n"); }
        else if   (strcasecmp(argv[1], "phy-rst") == 0) { printf("  <gpio>  -- PHY reset GPIO  -1=ingen  (LAN8720)\r\n"); }
        else if   (strcasecmp(argv[1], "cs")      == 0) { printf("  <gpio>  -- SPI CS pin  (W5500)\r\n"); }
        else if   (strcasecmp(argv[1], "mosi")    == 0) { printf("  <gpio>  -- SPI MOSI pin  (W5500)\r\n"); }
        else if   (strcasecmp(argv[1], "miso")    == 0) { printf("  <gpio>  -- SPI MISO pin  (W5500)\r\n"); }
        else if   (strcasecmp(argv[1], "sclk")    == 0) { printf("  <gpio>  -- SPI SCLK pin  (W5500)\r\n"); }
        else if   (strcasecmp(argv[1], "int")     == 0) { printf("  <gpio>  -- SPI INT pin  -1=pollet  (W5500)\r\n"); }
        else { eth_print_help(s_cfg->ethernet.hw_type); }
        return 0;
    }
    if (argc < 2) {
        eth_print_help(s_cfg->ethernet.hw_type);
        return 1;
    }

    const char *sub = argv[1];

    if (strcasecmp(sub, "enable") == 0) {
        s_cfg->ethernet.enabled = 1;
        printf("Ethernet aktiveret.\r\n");
    } else if (strcasecmp(sub, "disable") == 0) {
        s_cfg->ethernet.enabled = 0;
        printf("Ethernet deaktiveret.\r\n");
    } else if (strcasecmp(sub, "type") == 0) {
        if (argc < 3) { printf("Fejl: lan8720, w5500 eller none\r\n"); return 1; }
        if (strcasecmp(argv[2], "w5500") == 0) {
            s_cfg->ethernet.hw_type = ETH_HW_W5500;
            printf("Ethernet type: W5500 (SPI)\r\n");
        } else if (strcasecmp(argv[2], "none") == 0) {
            s_cfg->ethernet.hw_type = ETH_HW_NONE;
            printf("Ethernet type: none (ingen GPIO-parametre)\r\n");
        } else {
            s_cfg->ethernet.hw_type = ETH_HW_LAN8720;
            printf("Ethernet type: LAN8720 (RMII)\r\n");
        }
    } else if (strcasecmp(sub, "phy-addr") == 0) {
        if (argc < 3) { printf("Fejl: angiv PHY adresse\r\n"); return 1; }
        s_cfg->ethernet.phy_addr = atoi(argv[2]);
        printf("PHY adresse: %d\r\n", s_cfg->ethernet.phy_addr);
    } else if (strcasecmp(sub, "mdc") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.mdc_gpio = atoi(argv[2]);
        printf("MDC GPIO: %d\r\n", s_cfg->ethernet.mdc_gpio);
    } else if (strcasecmp(sub, "mdio") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.mdio_gpio = atoi(argv[2]);
        printf("MDIO GPIO: %d\r\n", s_cfg->ethernet.mdio_gpio);
    } else if (strcasecmp(sub, "phy-rst") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO eller -1\r\n"); return 1; }
        s_cfg->ethernet.phy_rst_gpio = atoi(argv[2]);
        printf("PHY RST GPIO: %d\r\n", s_cfg->ethernet.phy_rst_gpio);
    } else if (strcasecmp(sub, "cs") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.spi_cs_gpio = atoi(argv[2]);
        printf("SPI CS GPIO: %d\r\n", s_cfg->ethernet.spi_cs_gpio);
    } else if (strcasecmp(sub, "mosi") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.spi_mosi_gpio = atoi(argv[2]);
        printf("SPI MOSI GPIO: %d\r\n", s_cfg->ethernet.spi_mosi_gpio);
    } else if (strcasecmp(sub, "miso") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.spi_miso_gpio = atoi(argv[2]);
        printf("SPI MISO GPIO: %d\r\n", s_cfg->ethernet.spi_miso_gpio);
    } else if (strcasecmp(sub, "sclk") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO\r\n"); return 1; }
        s_cfg->ethernet.spi_sclk_gpio = atoi(argv[2]);
        printf("SPI SCLK GPIO: %d\r\n", s_cfg->ethernet.spi_sclk_gpio);
    } else if (strcasecmp(sub, "int") == 0) {
        if (argc < 3) { printf("Fejl: angiv GPIO eller -1\r\n"); return 1; }
        s_cfg->ethernet.spi_int_gpio = atoi(argv[2]);
        printf("SPI INT GPIO: %d\r\n", s_cfg->ethernet.spi_int_gpio);
    } else if (strcasecmp(sub, "dhcp") == 0) {
        strncpy(s_cfg->ethernet.ip,      "dhcp",    sizeof(s_cfg->ethernet.ip));
        strncpy(s_cfg->ethernet.gw,      "0.0.0.0", sizeof(s_cfg->ethernet.gw));
        strncpy(s_cfg->ethernet.netmask, "0.0.0.0", sizeof(s_cfg->ethernet.netmask));
        printf("Ethernet: DHCP\r\n");
    } else {
        if (argc < 4) {
            printf("Fejl: angiv alle tre: <ip> <gateway> <netmask>\r\n");
            return 1;
        }
        strncpy(s_cfg->ethernet.ip,      argv[1], sizeof(s_cfg->ethernet.ip));
        strncpy(s_cfg->ethernet.gw,      argv[2], sizeof(s_cfg->ethernet.gw));
        strncpy(s_cfg->ethernet.netmask, argv[3], sizeof(s_cfg->ethernet.netmask));
        printf("Ethernet: %s  GW %s  mask %s\r\n",
               s_cfg->ethernet.ip, s_cfg->ethernet.gw, s_cfg->ethernet.netmask);
    }
    printf("Kør 'save' efterfulgt af 'reboot' for at aktivere.\r\n");
    return 0;
}

// ── cmd: wifi ─────────────────────────────────────────────────────────────────

static int cmd_wifi(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[argc - 1], "?") == 0) {
        if      (argc == 2)                                                   goto wifi_full_help;
        else if (strcasecmp(argv[1], "ssid")    == 0) { printf("  <navn>          -- WiFi netværksnavn (maks 32 tegn)\r\n"); }
        else if (strcasecmp(argv[1], "pass")    == 0) { printf("  <kodeord>       -- WiFi adgangskode (maks 64 tegn)\r\n"); }
        else if (strcasecmp(argv[1], "ip")      == 0) { printf("  dhcp            -- automatisk IP\r\n  <ip>            -- statisk IP adresse\r\n"); }
        else if (strcasecmp(argv[1], "ap")      == 0) { printf("  on              -- aktiver AP fallback hotspot\r\n  off             -- deaktiver AP fallback\r\n"); }
        else if (strcasecmp(argv[1], "ap-ssid") == 0) { printf("  <navn>          -- AP hotspot netværksnavn\r\n"); }
        else if (strcasecmp(argv[1], "mode")    == 0) { printf("  (ingen parametre) -- vis live WiFi tilstand\r\n"); }
        else if (strcasecmp(argv[1], "status")  == 0) { printf("  (ingen parametre) -- vis detaljeret live WiFi status\r\n"); }
        else                                          { goto wifi_full_help; }
        return 0;
    }
    if (argc < 2) {
        printf("Brug:\r\n");
        wifi_full_help:
        printf("  wifi status              -- live WiFi status (tilstand, IP, RSSI, MAC)\r\n");
        printf("  wifi mode                -- aktuel tilstand: klient|AP|deaktiveret\r\n");
        printf("  wifi on                  -- aktiver WiFi STA\r\n");
        printf("  wifi off                 -- deaktiver WiFi\r\n");
        printf("  wifi ssid <navn>         -- sæt netværksnavn  (brug \"\" ved mellemrum)\r\n");
        printf("  wifi pass <kodeord>      -- sæt adgangskode   (brug \"\" ved mellemrum)\r\n");
        printf("  wifi ip dhcp             -- DHCP (standard)\r\n");
        printf("  wifi ip <ip>             -- statisk IP\r\n");
        printf("  wifi ap on|off           -- AP fallback hotspot\r\n");
        printf("  wifi ap-ssid <navn>      -- AP hotspot navn\r\n");
        return 1;
    }

    const char *sub = argv[1];

    if (strcasecmp(sub, "status") == 0) {
        wifi_status_t ws = wifi_manager_get_status();
        uint8_t mac[6] = {0};
        esp_wifi_get_mac(WIFI_IF_STA, mac);

        static const char *state_str[] = {
            "deaktiveret", "forbinder...", "forbundet", "AP hotspot", "fejl"
        };
        const char *st = (ws.state <= WIFI_STATE_ERROR) ? state_str[ws.state] : "ukendt";

        sep();
        printf("WiFi live status\r\n");
        printf("  Tilstand  : %s\r\n", st);

        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);
        const char *mode_str = "deaktiveret";
        if      (mode == WIFI_MODE_STA)   mode_str = "klient (STA)";
        else if (mode == WIFI_MODE_AP)    mode_str = "AP hotspot";
        else if (mode == WIFI_MODE_APSTA) mode_str = "klient+AP (APSTA)";
        printf("  Mode      : %s\r\n", mode_str);

        printf("  MAC (STA) : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        if (ws.state == WIFI_STATE_CONNECTED) {
            wifi_ap_record_t ap = {0};
            esp_wifi_sta_get_ap_info(&ap);
            static const char *auth_names[] = {
                "åben", "WEP", "WPA-PSK", "WPA2-PSK", "WPA/WPA2-PSK",
                "WPA2-Enterprise", "WPA3-PSK", "WPA2/WPA3-PSK", "WAPI-PSK"
            };
            const char *auth = (ap.authmode < 9) ? auth_names[ap.authmode] : "ukendt";
            printf("  SSID      : %s\r\n", ws.ssid[0] ? ws.ssid : "(ingen)");
            printf("  IP        : %s\r\n", ws.ip[0]   ? ws.ip   : "0.0.0.0");
            printf("  RSSI      : %d dBm\r\n", (int)ws.rssi);
            printf("  Kanal     : %d\r\n", ap.primary);
            printf("  Auth      : %s\r\n", auth);
            printf("  BSSID     : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   ap.bssid[0], ap.bssid[1], ap.bssid[2],
                   ap.bssid[3], ap.bssid[4], ap.bssid[5]);
        } else if (ws.state == WIFI_STATE_AP_MODE) {
            uint8_t ap_mac[6] = {0};
            esp_wifi_get_mac(WIFI_IF_AP, ap_mac);
            printf("  AP SSID   : %s\r\n", s_cfg->wifi.ap_ssid[0] ? s_cfg->wifi.ap_ssid : "ModbusGW-??????");
            printf("  AP IP     : 192.168.4.1\r\n");
            printf("  MAC (AP)  : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   ap_mac[0], ap_mac[1], ap_mac[2],
                   ap_mac[3], ap_mac[4], ap_mac[5]);
        } else {
            printf("  SSID      : %s\r\n", s_cfg->wifi.ssid[0] ? s_cfg->wifi.ssid : "(ikke konfigureret)");
        }
        sep();
        return 0;
    } else if (strcasecmp(sub, "mode") == 0) {
        wifi_mode_t mode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&mode);
        wifi_status_t ws = wifi_manager_get_status();
        sep();
        if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
            if (ws.state == WIFI_STATE_CONNECTED)
                printf("WiFi mode: klient (STA) — forbundet til '%s'\r\n", ws.ssid);
            else if (ws.state == WIFI_STATE_CONNECTING)
                printf("WiFi mode: klient (STA) — forbinder...\r\n");
            else
                printf("WiFi mode: klient (STA) — ikke forbundet\r\n");
            if (mode == WIFI_MODE_APSTA)
                printf("           + AP hotspot kører (fallback)\r\n");
        } else if (mode == WIFI_MODE_AP) {
            printf("WiFi mode: AP hotspot — '%s'\r\n",
                   s_cfg->wifi.ap_ssid[0] ? s_cfg->wifi.ap_ssid : "ModbusGW-??????");
        } else {
            printf("WiFi mode: deaktiveret\r\n");
        }
        sep();
        return 0;
    } else if (strcasecmp(sub, "on") == 0) {
        s_cfg->wifi.enabled = 1;
        printf("WiFi aktiveret.\r\n");
    } else if (strcasecmp(sub, "off") == 0) {
        s_cfg->wifi.enabled = 0;
        printf("WiFi deaktiveret.\r\n");
    } else if (strcasecmp(sub, "ssid") == 0) {
        if (argc < 3) { printf("Fejl: angiv SSID navn\r\n"); return 1; }
        strncpy(s_cfg->wifi.ssid, argv[2], sizeof(s_cfg->wifi.ssid));
        printf("WiFi SSID: %s\r\n", s_cfg->wifi.ssid);
    } else if (strcasecmp(sub, "pass") == 0) {
        if (argc < 3) { printf("Fejl: angiv adgangskode\r\n"); return 1; }
        strncpy(s_cfg->wifi.password, argv[2], sizeof(s_cfg->wifi.password));
        printf("WiFi adgangskode sat (%d tegn).\r\n", (int)strlen(argv[2]));
    } else if (strcasecmp(sub, "ip") == 0) {
        if (argc < 3) { printf("Fejl: angiv IP eller 'dhcp'\r\n"); return 1; }
        strncpy(s_cfg->wifi.ip, argv[2], sizeof(s_cfg->wifi.ip));
        printf("WiFi IP: %s\r\n", s_cfg->wifi.ip);
    } else if (strcasecmp(sub, "ap") == 0) {
        if (argc < 3) { printf("Fejl: on eller off\r\n"); return 1; }
        s_cfg->wifi.ap_fallback = (strcasecmp(argv[2], "on") == 0) ? 1 : 0;
        printf("AP fallback: %s\r\n", s_cfg->wifi.ap_fallback ? "aktiveret" : "deaktiveret");
    } else if (strcasecmp(sub, "ap-ssid") == 0) {
        if (argc < 3) { printf("Fejl: angiv AP SSID\r\n"); return 1; }
        strncpy(s_cfg->wifi.ap_ssid, argv[2], sizeof(s_cfg->wifi.ap_ssid));
        printf("AP SSID: %s\r\n", s_cfg->wifi.ap_ssid);
    } else {
        printf("Ukendt kommando: '%s' — skriv 'wifi' for hjælp\r\n", sub);
        return 1;
    }
    printf("Kør 'save' efterfulgt af 'reboot' for at aktivere.\r\n");
    return 0;
}

// ── cmd: save ─────────────────────────────────────────────────────────────────

static int cmd_save(int argc, char **argv)
{
    esp_err_t err = config_store_save(s_cfg);
    if (err == ESP_OK) printf("Konfiguration gemt til NVS.\r\n");
    else               printf("Fejl ved gemning: %s\r\n", esp_err_to_name(err));
    return (err == ESP_OK) ? 0 : 1;
}

// ── cmd: reboot ───────────────────────────────────────────────────────────────

static int cmd_reboot(int argc, char **argv)
{
    printf("Genstarter...\r\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return 0;
}

// ── cmd: factory-reset ────────────────────────────────────────────────────────

static int cmd_factory_reset(int argc, char **argv)
{
    printf("ADVARSEL: Sletter al NVS-konfiguration og genstarter!\r\n");
    printf("Genstart med fabriksindstillinger...\r\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200));
    nvs_flash_erase();
    esp_restart();
    return 0;
}

// ── cmd: debug / no debug ────────────────────────────────────────────────────

static int cmd_debug(int argc, char **argv)
{
    // debug              → alt på VERBOSE (så meget som sdkconfig tillader)
    // debug <komponent>  → specifik komponent verbose
    // debug ?            → hjælp
    if (argc >= 2 && strcmp(argv[argc - 1], "?") == 0) {
        printf("  debug              -- alt verbose\r\n");
        printf("  debug wifi         -- WiFi driver + wifi_mgr verbose\r\n");
        printf("  debug <tag>        -- specifik ESP-IDF komponent verbose\r\n");
        printf("  no debug           -- stille (kun WARN + ERROR)\r\n");
        printf("  no debug <tag>     -- stil specifik komponent\r\n");
        return 0;
    }
    if (argc >= 2) {
        const char *tag = argv[1];
        if (strcasecmp(tag, "wifi") == 0) {
            esp_log_level_set("wifi",     ESP_LOG_VERBOSE);
            esp_log_level_set("wifi_mgr", ESP_LOG_VERBOSE);
            printf("Debug: wifi + wifi_mgr → VERBOSE\r\n");
        } else {
            esp_log_level_set(tag, ESP_LOG_VERBOSE);
            printf("Debug: '%s' → VERBOSE\r\n", tag);
        }
    } else {
        esp_log_level_set("*", ESP_LOG_VERBOSE);
        printf("Debug: alt → VERBOSE\r\n");
    }
    return 0;
}

static int cmd_no(int argc, char **argv)
{
    // no debug              → alt på WARN (stille: INFO fra driver skjult)
    // no debug <komponent>  → specifik komponent på WARN
    if (argc < 2 || strcasecmp(argv[1], "debug") != 0) {
        printf("Brug: no debug [<komponent>]\r\n");
        return 1;
    }
    if (argc >= 3) {
        const char *tag = argv[2];
        if (strcasecmp(tag, "wifi") == 0) {
            esp_log_level_set("wifi",     ESP_LOG_WARN);
            esp_log_level_set("wifi_mgr", ESP_LOG_WARN);
            printf("No debug: wifi + wifi_mgr → WARN\r\n");
        } else {
            esp_log_level_set(tag, ESP_LOG_WARN);
            printf("No debug: '%s' → WARN\r\n", tag);
        }
    } else {
        esp_log_level_set("*", ESP_LOG_WARN);
        printf("No debug: alt → WARN (kun WARN + ERROR vises)\r\n");
    }
    return 0;
}

// ── Configure terminal ────────────────────────────────────────────────────────

#define CFG_MAX_ARGC 10

static int cfg_tokenize(char *buf, char **argv)
{
    int argc = 0;
    char *p = buf;
    while (*p && argc < CFG_MAX_ARGC) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            // Quoted token — bevarer mellemrum: ssid "Mit netværk"
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p) *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return argc;
}

typedef enum { CTX_TOP, CTX_ETH, CTX_WIFI, CTX_WIFI_AP, CTX_MODBUS } cfg_ctx_t;

static void cfg_help_top(void) {
    printf("  interface eth0         -- Ethernet\r\n");
    printf("  interface wifi         -- WiFi STA klient\r\n");
    printf("  interface wifi-ap      -- WiFi AP hotspot fallback\r\n");
    printf("  interface modbus<N>    -- Modbus interface N  (eks: modbus0)\r\n");
    printf("  show                   -- vis komplet konfiguration\r\n");
    printf("  save                   -- gem til NVS\r\n");
    printf("  exit / end             -- forlad konfigurationstilstand\r\n");
}

static void cfg_help_eth(void) {
    eth_print_help(s_cfg->ethernet.hw_type);
    printf("  exit                   -- tilbage til config\r\n");
}

static void cfg_help_wifi(void) {
    printf("  enable / disable       -- aktiver/deaktiver WiFi STA\r\n");
    printf("  ssid <navn>            -- netværksnavn\r\n");
    printf("  psk <kodeord>          -- adgangskode\r\n");
    printf("  ip dhcp                -- DHCP\r\n");
    printf("  ip <ip> <gw> <mask>    -- statisk IP\r\n");
    printf("  exit                   -- tilbage til config\r\n");
}

static void cfg_help_wifi_ap(void) {
    printf("  enable / disable       -- aktiver/deaktiver AP fallback\r\n");
    printf("  ssid <navn>            -- AP netværksnavn\r\n");
    printf("  psk <kodeord>          -- AP adgangskode  (tom=åben)\r\n");
    printf("  exit                   -- tilbage til config\r\n");
}

static void cfg_help_modbus(void) {
    printf("  enable / disable       -- aktiver/deaktiver interface\r\n");
    printf("  type rs485|rs232       -- interface-type\r\n");
    printf("  uart hw <num>          -- hardware UART  (eks: uart hw 1)\r\n");
    printf("  uart sw                -- software UART  (GPIO bit-bang)\r\n");
    printf("  baudrate <baud>        -- baud-rate  (eks: 9600)\r\n");
    printf("  format <bits> <n|e|o> <stop>  -- eks: format 8 n 1\r\n");
    printf("  timeout <ms>           -- svar-timeout i ms\r\n");
    printf("  tx <gpio>              -- TX pin\r\n");
    printf("  rx <gpio>              -- RX pin\r\n");
    printf("  de <gpio>              -- DE/RE pin  (RS485)\r\n");
    printf("  exit                   -- tilbage til config\r\n");
}

static int cmd_configure(int argc, char **argv)
{
    if (argc > 1
        && strcasecmp(argv[1], "terminal") != 0
        && strcasecmp(argv[1], "t") != 0) {
        printf("Brug: configure terminal  (eller: conf t)\r\n");
        return 1;
    }

    cfg_ctx_t ctx = CTX_TOP;
    int       modbus_id = 0;
    char      prompt[32];
    char      linebuf[256];
    char     *av[CFG_MAX_ARGC];
    int       ac;

    printf("Konfigurationstilstand aktiv. '?' = hjælp, 'exit'/'end' = afslut.\r\n");

    while (1) {
        switch (ctx) {
            case CTX_TOP:     snprintf(prompt, sizeof(prompt), "gw(config)# ");           break;
            case CTX_ETH:     snprintf(prompt, sizeof(prompt), "gw(config-eth0)# ");      break;
            case CTX_WIFI:    snprintf(prompt, sizeof(prompt), "gw(config-wifi)# ");      break;
            case CTX_WIFI_AP: snprintf(prompt, sizeof(prompt), "gw(config-wifi-ap)# ");   break;
            case CTX_MODBUS:  snprintf(prompt, sizeof(prompt), "gw(config-modbus%d)# ", modbus_id); break;
        }

        char *line = linenoise(prompt);
        if (!line) break;                    // Ctrl+D / EOF
        const char *p = line;
        while (*p == ' ') p++;
        if (!*p) { linenoiseFree(line); continue; }
        linenoiseHistoryAdd(line);
        strncpy(linebuf, line, sizeof(linebuf) - 1);
        linebuf[sizeof(linebuf) - 1] = '\0';
        linenoiseFree(line);

        ac = cfg_tokenize(linebuf, av);
        if (ac == 0) continue;
        const char *cmd = av[0];

        // ── Fælles kommandoer på alle niveauer ────────────────────────────────
        if (strcmp(cmd, "?") == 0) {
            switch (ctx) {
                case CTX_TOP:     cfg_help_top();     break;
                case CTX_ETH:     cfg_help_eth();     break;
                case CTX_WIFI:    cfg_help_wifi();    break;
                case CTX_WIFI_AP: cfg_help_wifi_ap(); break;
                case CTX_MODBUS:  cfg_help_modbus();  break;
            }
            continue;
        }
        if (strcasecmp(cmd, "exit") == 0 || strcasecmp(cmd, "end") == 0) {
            if (ctx == CTX_TOP) break;
            ctx = CTX_TOP; continue;
        }
        if (strcasecmp(cmd, "show") == 0) { show_running_config(); continue; }
        if (strcasecmp(cmd, "save") == 0) {
            esp_err_t e = config_store_save(s_cfg);
            printf(e == ESP_OK ? "Gemt.\r\n" : "Fejl ved gemning.\r\n"); continue;
        }

        // ── CTX_TOP ───────────────────────────────────────────────────────────
        if (ctx == CTX_TOP) {
            if (strcasecmp(cmd, "interface") == 0) {
                if (ac < 2) { printf("Angiv interface navn  (?=hjælp)\r\n"); continue; }
                if      (strcasecmp(av[1], "eth0")    == 0) { ctx = CTX_ETH; }
                else if (strcasecmp(av[1], "wifi")    == 0) { ctx = CTX_WIFI; }
                else if (strcasecmp(av[1], "wifi-ap") == 0) { ctx = CTX_WIFI_AP; }
                else if (strncasecmp(av[1], "modbus", 6) == 0) {
                    int id = atoi(av[1] + 6);
                    if (id < 0 || id >= s_cfg->interface_count)
                        printf("Modbus%d findes ikke (0-%d)\r\n", id, s_cfg->interface_count - 1);
                    else { modbus_id = id; ctx = CTX_MODBUS; }
                } else { printf("Ukendt interface '%s'  (?=hjælp)\r\n", av[1]); }
            } else { printf("Ukendt: '%s'  (?=hjælp)\r\n", cmd); }
            continue;
        }

        // ── CTX_ETH ───────────────────────────────────────────────────────────
        if (ctx == CTX_ETH) {
            if      (strcasecmp(cmd, "enable")   == 0) { s_cfg->ethernet.enabled = 1; printf("Ethernet: aktiveret\r\n"); }
            else if (strcasecmp(cmd, "disable")  == 0) { s_cfg->ethernet.enabled = 0; printf("Ethernet: deaktiveret\r\n"); }
            else if (strcasecmp(cmd, "type")     == 0) {
                if (ac < 2) { printf("Brug: type lan8720|w5500|none\r\n"); continue; }
                if      (strcasecmp(av[1], "w5500")   == 0) { s_cfg->ethernet.hw_type = ETH_HW_W5500;  printf("Type: W5500 — GPIO: cs mosi miso sclk int\r\n"); }
                else if (strcasecmp(av[1], "none")    == 0) { s_cfg->ethernet.hw_type = ETH_HW_NONE;   printf("Type: none  — ingen GPIO-parametre\r\n"); }
                else                                         { s_cfg->ethernet.hw_type = ETH_HW_LAN8720; printf("Type: LAN8720 — GPIO: phy-addr mdc mdio phy-rst\r\n"); }
            }
            // LAN8720-specifikke GPIO
            else if (strcasecmp(cmd, "phy-addr") == 0 ||
                     strcasecmp(cmd, "mdc")      == 0 ||
                     strcasecmp(cmd, "mdio")     == 0 ||
                     strcasecmp(cmd, "phy-rst")  == 0) {
                if (s_cfg->ethernet.hw_type != ETH_HW_LAN8720) {
                    printf("Fejl: '%s' er kun for LAN8720  (brug 'type lan8720' først)\r\n", cmd); continue;
                }
                if (ac < 2) { printf("Brug: %s <gpio>\r\n", cmd); continue; }
                int val = atoi(av[1]);
                if      (strcasecmp(cmd, "phy-addr") == 0) { s_cfg->ethernet.phy_addr    = val; printf("PHY addr: %d\r\n", val); }
                else if (strcasecmp(cmd, "mdc")      == 0) { s_cfg->ethernet.mdc_gpio    = val; printf("MDC GPIO: %d\r\n", val); }
                else if (strcasecmp(cmd, "mdio")     == 0) { s_cfg->ethernet.mdio_gpio   = val; printf("MDIO GPIO: %d\r\n", val); }
                else                                        { s_cfg->ethernet.phy_rst_gpio = val; printf("PHY RST GPIO: %d\r\n", val); }
            }
            // W5500-specifikke GPIO
            else if (strcasecmp(cmd, "cs")   == 0 ||
                     strcasecmp(cmd, "mosi") == 0 ||
                     strcasecmp(cmd, "miso") == 0 ||
                     strcasecmp(cmd, "sclk") == 0 ||
                     strcasecmp(cmd, "int")  == 0) {
                if (s_cfg->ethernet.hw_type != ETH_HW_W5500) {
                    printf("Fejl: '%s' er kun for W5500  (brug 'type w5500' først)\r\n", cmd); continue;
                }
                if (ac < 2) { printf("Brug: %s <gpio>\r\n", cmd); continue; }
                int val = atoi(av[1]);
                if      (strcasecmp(cmd, "cs")   == 0) { s_cfg->ethernet.spi_cs_gpio   = val; printf("SPI CS GPIO: %d\r\n", val); }
                else if (strcasecmp(cmd, "mosi") == 0) { s_cfg->ethernet.spi_mosi_gpio = val; printf("SPI MOSI GPIO: %d\r\n", val); }
                else if (strcasecmp(cmd, "miso") == 0) { s_cfg->ethernet.spi_miso_gpio = val; printf("SPI MISO GPIO: %d\r\n", val); }
                else if (strcasecmp(cmd, "sclk") == 0) { s_cfg->ethernet.spi_sclk_gpio = val; printf("SPI SCLK GPIO: %d\r\n", val); }
                else                                    { s_cfg->ethernet.spi_int_gpio  = val; printf("SPI INT GPIO: %d\r\n", val); }
            }
            else if (strcasecmp(cmd, "ip")       == 0) {
                if (ac < 2) { printf("Brug: ip dhcp  eller  ip <ip> <gw> <mask>\r\n"); continue; }
                if (strcasecmp(av[1], "dhcp") == 0) {
                    strncpy(s_cfg->ethernet.ip, "dhcp", sizeof(s_cfg->ethernet.ip));
                    strncpy(s_cfg->ethernet.gw, "0.0.0.0", sizeof(s_cfg->ethernet.gw));
                    strncpy(s_cfg->ethernet.netmask, "0.0.0.0", sizeof(s_cfg->ethernet.netmask));
                    printf("IP: dhcp\r\n");
                } else if (ac >= 4) {
                    strncpy(s_cfg->ethernet.ip,      av[1], sizeof(s_cfg->ethernet.ip));
                    strncpy(s_cfg->ethernet.gw,      av[2], sizeof(s_cfg->ethernet.gw));
                    strncpy(s_cfg->ethernet.netmask, av[3], sizeof(s_cfg->ethernet.netmask));
                    printf("IP: %s  GW: %s  Mask: %s\r\n", s_cfg->ethernet.ip, s_cfg->ethernet.gw, s_cfg->ethernet.netmask);
                } else { printf("Brug: ip <ip> <gateway> <netmask>\r\n"); }
            }
            else { printf("Ukendt: '%s'  (?=hjælp)\r\n", cmd); }
            continue;
        }

        // ── CTX_WIFI ──────────────────────────────────────────────────────────
        if (ctx == CTX_WIFI) {
            if      (strcasecmp(cmd, "enable")  == 0) { s_cfg->wifi.enabled = 1; printf("WiFi STA: aktiveret\r\n"); }
            else if (strcasecmp(cmd, "disable") == 0) { s_cfg->wifi.enabled = 0; printf("WiFi STA: deaktiveret\r\n"); }
            else if (strcasecmp(cmd, "ssid")    == 0) {
                if (ac < 2) { printf("Brug: ssid <navn>\r\n"); continue; }
                strncpy(s_cfg->wifi.ssid, av[1], sizeof(s_cfg->wifi.ssid)); printf("SSID: %s\r\n", s_cfg->wifi.ssid);
            }
            else if (strcasecmp(cmd, "psk")     == 0) {
                if (ac < 2) { printf("Brug: psk <kodeord>\r\n"); continue; }
                strncpy(s_cfg->wifi.password, av[1], sizeof(s_cfg->wifi.password)); printf("PSK: sat (%d tegn)\r\n", (int)strlen(av[1]));
            }
            else if (strcasecmp(cmd, "ip")      == 0) {
                if (ac < 2) { printf("Brug: ip dhcp  eller  ip <ip> <gw> <mask>\r\n"); continue; }
                if (strcasecmp(av[1], "dhcp") == 0) {
                    strncpy(s_cfg->wifi.ip, "dhcp", sizeof(s_cfg->wifi.ip)); printf("IP: dhcp\r\n");
                } else if (ac >= 4) {
                    strncpy(s_cfg->wifi.ip,      av[1], sizeof(s_cfg->wifi.ip));
                    strncpy(s_cfg->wifi.gw,      av[2], sizeof(s_cfg->wifi.gw));
                    strncpy(s_cfg->wifi.netmask, av[3], sizeof(s_cfg->wifi.netmask));
                    printf("IP: %s  GW: %s  Mask: %s\r\n", s_cfg->wifi.ip, s_cfg->wifi.gw, s_cfg->wifi.netmask);
                } else { printf("Brug: ip <ip> <gateway> <netmask>\r\n"); }
            }
            else { printf("Ukendt: '%s'  (?=hjælp)\r\n", cmd); }
            continue;
        }

        // ── CTX_WIFI_AP ───────────────────────────────────────────────────────
        if (ctx == CTX_WIFI_AP) {
            if      (strcasecmp(cmd, "enable")  == 0) { s_cfg->wifi.ap_fallback = 1; printf("AP fallback: aktiveret\r\n"); }
            else if (strcasecmp(cmd, "disable") == 0) { s_cfg->wifi.ap_fallback = 0; printf("AP fallback: deaktiveret\r\n"); }
            else if (strcasecmp(cmd, "ssid")    == 0) {
                if (ac < 2) { printf("Brug: ssid <navn>\r\n"); continue; }
                strncpy(s_cfg->wifi.ap_ssid, av[1], sizeof(s_cfg->wifi.ap_ssid)); printf("AP SSID: %s\r\n", s_cfg->wifi.ap_ssid);
            }
            else if (strcasecmp(cmd, "psk")     == 0) {
                if (ac < 2) { printf("Brug: psk <kodeord>  (tom streng = åben)\r\n"); continue; }
                strncpy(s_cfg->wifi.ap_password, av[1], sizeof(s_cfg->wifi.ap_password)); printf("AP PSK: sat (%d tegn)\r\n", (int)strlen(av[1]));
            }
            else { printf("Ukendt: '%s'  (?=hjælp)\r\n", cmd); }
            continue;
        }

        // ── CTX_MODBUS ────────────────────────────────────────────────────────
        if (ctx == CTX_MODBUS) {
            iface_config_t *f = &s_cfg->interfaces[modbus_id];
            if      (strcasecmp(cmd, "enable")   == 0) { f->enabled = 1; printf("Modbus%d: aktiveret\r\n", modbus_id); }
            else if (strcasecmp(cmd, "disable")  == 0) { f->enabled = 0; printf("Modbus%d: deaktiveret\r\n", modbus_id); }
            else if (strcasecmp(cmd, "type")     == 0) {
                if (ac < 2) { printf("Brug: type rs485|rs232\r\n"); continue; }
                f->type = (strcasecmp(av[1], "rs232") == 0) ? IFACE_TYPE_RS232 : IFACE_TYPE_RS485;
                printf("Type: %s\r\n", f->type == IFACE_TYPE_RS485 ? "RS485" : "RS232");
            }
            else if (strcasecmp(cmd, "uart")     == 0) {
                if (ac < 2) { printf("Brug: uart hw <num>  eller  uart sw\r\n"); continue; }
                if (strcasecmp(av[1], "hw") == 0) {
                    f->uart_mode = IFACE_UART_HW;
                    f->uart_num  = (ac >= 3) ? atoi(av[2]) : 1;
                    printf("UART: HW UART%d\r\n", f->uart_num);
                } else {
                    f->uart_mode = IFACE_UART_SW; printf("UART: SW (bit-bang)\r\n");
                }
            }
            else if (strcasecmp(cmd, "baudrate") == 0) {
                if (ac < 2) { printf("Brug: baudrate <baud>\r\n"); continue; }
                f->baudrate = (uint32_t)atoi(av[1]); printf("Baudrate: %lu\r\n", (unsigned long)f->baudrate);
            }
            else if (strcasecmp(cmd, "format")   == 0) {
                if (ac < 4) { printf("Brug: format <bits> <n|e|o> <stop>  eks: format 8 n 1\r\n"); continue; }
                f->data_bits = (uint8_t)atoi(av[1]);
                char pc = (char)tolower((unsigned char)av[2][0]);
                f->parity    = (pc == 'e') ? 2 : (pc == 'o') ? 1 : 0;
                f->stop_bits = (uint8_t)atoi(av[3]);
                printf("Format: %d%c%d\r\n", f->data_bits, (f->parity==2)?'E':(f->parity==1)?'O':'N', f->stop_bits);
            }
            else if (strcasecmp(cmd, "timeout")  == 0) {
                if (ac < 2) { printf("Brug: timeout <ms>\r\n"); continue; }
                f->timeout_ms = (uint16_t)atoi(av[1]); printf("Timeout: %d ms\r\n", f->timeout_ms);
            }
            else if (strcasecmp(cmd, "tx")       == 0) {
                if (ac < 2) { printf("Brug: tx <gpio>\r\n"); continue; }
                f->tx_pin = atoi(av[1]); printf("TX GPIO: %d\r\n", f->tx_pin);
            }
            else if (strcasecmp(cmd, "rx")       == 0) {
                if (ac < 2) { printf("Brug: rx <gpio>\r\n"); continue; }
                f->rx_pin = atoi(av[1]); printf("RX GPIO: %d\r\n", f->rx_pin);
            }
            else if (strcasecmp(cmd, "de")       == 0) {
                if (ac < 2) { printf("Brug: de <gpio>\r\n"); continue; }
                f->rts_pin = atoi(av[1]); printf("DE GPIO: %d\r\n", f->rts_pin);
            }
            else { printf("Ukendt: '%s'  (?=hjælp)\r\n", cmd); }
            continue;
        }
    }

    printf("Forlader konfigurationstilstand. Husk: 'save' + 'reboot'.\r\n");
    return 0;
}

static int cmd_question(int argc, char **argv)
{
    int ret = 0;
    esp_console_run("help", &ret);
    return ret;
}

// ── Init ──────────────────────────────────────────────────────────────────────

esp_err_t serial_cli_start(gateway_config_t *cfg)
{
    s_cfg = cfg;

    // esp_console_new_repl_uart installs UART driver + configures VFS for
    // blocking reads — the old esp_console_init() did NOT do this, causing
    // linenoise to spin in a tight loop returning empty strings.
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt             = "gw>";
    repl_config.max_cmdline_length = 256;
    repl_config.task_stack_size    = 6144;
    repl_config.task_priority      = 3;
    repl_config.max_history_len    = 20;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    // esp_console sætter CR-mode (\r->\n) men Windows-terminaler sender \r\n
    // hvilket giver to \n: ét afslutter kommandoen, ét printer prompten ekstra.
    // CRLF-mode konsumerer \r\n som ét \n — løser dobbelt-prompt.
    uart_vfs_dev_port_set_rx_line_endings(0, ESP_LINE_ENDINGS_CRLF);

    // Dumb mode var slået til for at undgå ESC[6n cursor-probe spam.
    // ESC[6n sendes kun i multi-line mode (getColumnPos) — vi bruger single-line
    // (default), så det er sikkert at køre i ANSI-mode og få pile-taster + historik.
    esp_console_register_help_command();

    static const esp_console_cmd_t cmds[] = {
        { .command = "?",          .help = "Vis alle kommandoer",                                    .hint = NULL, .func = cmd_question,   .argtable = NULL },
        { .command = "configure",  .help = "configure terminal — konfigurationstilstand",             .hint = NULL, .func = cmd_configure,  .argtable = NULL },
        { .command = "conf",       .help = "conf t — kort alias for 'configure terminal'",            .hint = NULL, .func = cmd_configure,  .argtable = NULL },
        { .command = "show",       .help = "show config — vis komplet konfiguration (IOS-stil)",      .hint = NULL, .func = cmd_show,        .argtable = NULL },
        { .command = "status",     .help = "System status: version, IP, uptime, heap",               .hint = NULL, .func = cmd_status,      .argtable = NULL },
        { .command = "eth",        .help = "Ethernet config  (eth ? for hjælp)",                     .hint = NULL, .func = cmd_eth,         .argtable = NULL },
        { .command = "wifi",       .help = "WiFi config  (wifi ? for hjælp)",                        .hint = NULL, .func = cmd_wifi,        .argtable = NULL },
        { .command = "save",          .help = "Gem konfiguration til NVS",                           .hint = NULL, .func = cmd_save,          .argtable = NULL },
        { .command = "reboot",        .help = "Genstart gateway",                                    .hint = NULL, .func = cmd_reboot,        .argtable = NULL },
        { .command = "factory-reset", .help = "Slet al NVS-config og genstart med fabriksindst.",   .hint = NULL, .func = cmd_factory_reset, .argtable = NULL },
        { .command = "debug",         .help = "debug [<tag>] — verbose log  (debug ? for hjælp)",   .hint = NULL, .func = cmd_debug,         .argtable = NULL },
        { .command = "no",            .help = "no debug [<tag>] — stilmod (kun WARN+ERROR)",        .hint = NULL, .func = cmd_no,            .argtable = NULL },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }

    printf("\r\n================================\r\n");
    printf(" Modbus API Gateway v%s b%s\r\n", GATEWAY_VERSION, GATEWAY_BUILD);
    printf(" Serial CLI -- skriv 'help'\r\n");
    printf("================================\r\n\r\n");

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "Serial CLI klar pa UART0 (115200 8N1)");
    return ESP_OK;
}
