#include "serial_cli.h"
#include "config.h"
#include "version.h"
#include "config_store.h"
#include "ethernet.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "driver/uart_vfs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "serial_cli";
static gateway_config_t *s_cfg;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void sep(void) { printf("--------------------------------\r\n"); }

// ── cmd: show ─────────────────────────────────────────────────────────────────

static int cmd_show(int argc, char **argv)
{
    sep();
    printf("Ethernet\r\n");
    printf("  IP:      %s\r\n", s_cfg->ethernet.ip);
    printf("  GW:      %s\r\n", s_cfg->ethernet.gw);
    printf("  Netmask: %s\r\n\r\n", s_cfg->ethernet.netmask);

    printf("WiFi\r\n");
    printf("  Aktiv:      %s\r\n", s_cfg->wifi.enabled ? "ja" : "nej");
    printf("  SSID:       %s\r\n", s_cfg->wifi.ssid[0] ? s_cfg->wifi.ssid : "(ikke sat)");
    printf("  IP:         %s\r\n", s_cfg->wifi.ip[0]   ? s_cfg->wifi.ip   : "dhcp");
    printf("  AP fallback:%s\r\n\r\n", s_cfg->wifi.ap_fallback ? "ja" : "nej");

    printf("Modbus interfaces: %d\r\n", s_cfg->interface_count);
    for (int i = 0; i < s_cfg->interface_count; i++) {
        iface_config_t *f = &s_cfg->interfaces[i];
        printf("  [%d] %-5s %-3s %6lu baud  TX=%d RX=%d DE=%d  %s\r\n",
               f->id,
               f->type == IFACE_TYPE_RS485 ? "RS485" : "RS232",
               f->uart_mode == IFACE_UART_HW ? "HW" : "SW",
               (unsigned long)f->baudrate,
               f->tx_pin, f->rx_pin, f->rts_pin,
               f->enabled ? "aktiv" : "slukket");
    }
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
    if (argc < 2) {
        printf("Brug:\r\n");
        printf("  eth dhcp                              -- DHCP\r\n");
        printf("  eth <ip> <gateway> <netmask>          -- statisk IP\r\n");
        printf("  Eks: eth 192.168.1.100 192.168.1.1 255.255.255.0\r\n");
        return 1;
    }

    if (strcasecmp(argv[1], "dhcp") == 0) {
        strncpy(s_cfg->ethernet.ip,      "dhcp",      sizeof(s_cfg->ethernet.ip));
        strncpy(s_cfg->ethernet.gw,      "0.0.0.0",   sizeof(s_cfg->ethernet.gw));
        strncpy(s_cfg->ethernet.netmask, "0.0.0.0",   sizeof(s_cfg->ethernet.netmask));
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
    if (argc < 2) {
        printf("Brug:\r\n");
        printf("  wifi on                  -- aktiver WiFi STA\r\n");
        printf("  wifi off                 -- deaktiver WiFi\r\n");
        printf("  wifi ssid <navn>         -- sæt netværksnavn\r\n");
        printf("  wifi pass <kodeord>      -- sæt adgangskode\r\n");
        printf("  wifi ip dhcp             -- DHCP (standard)\r\n");
        printf("  wifi ip <ip>             -- statisk IP\r\n");
        printf("  wifi ap on|off           -- AP fallback hotspot\r\n");
        printf("  wifi ap-ssid <navn>      -- AP hotspot navn\r\n");
        return 1;
    }

    const char *sub = argv[1];

    if (strcasecmp(sub, "on") == 0) {
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
    repl_config.task_stack_size    = 5120;
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
        { .command = "show",   .help = "Vis al konfiguration",                           .hint = NULL, .func = cmd_show,        .argtable = NULL },
        { .command = "status", .help = "System status: version, IP, uptime, heap",       .hint = NULL, .func = cmd_status,      .argtable = NULL },
        { .command = "eth",    .help = "Ethernet IP  (eth dhcp | eth <ip> <gw> <mask>)", .hint = NULL, .func = cmd_eth,         .argtable = NULL },
        { .command = "wifi",   .help = "WiFi config  (wifi on/off/ssid/pass/ip/ap)",     .hint = NULL, .func = cmd_wifi,        .argtable = NULL },
        { .command = "save",   .help = "Gem konfiguration til NVS",                      .hint = NULL, .func = cmd_save,        .argtable = NULL },
        { .command = "reboot", .help = "Genstart gateway",                               .hint = NULL, .func = cmd_reboot,      .argtable = NULL },
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
