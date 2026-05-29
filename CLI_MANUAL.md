# Modbus API Gateway — CLI Manual

Konfiguration via serial terminal (UART0, 115200 8N1).

---

## Kom i gang

Tilslut USB-til-serial til ESP32 UART0 (TX=GPIO1, RX=GPIO3).  
Åbn terminal på den rigtige COM-port:

| Program        | Kommando                          |
|----------------|-----------------------------------|
| PlatformIO     | `pio device monitor`              |
| PuTTY          | Serial, 115200 baud, 8N1, CR+LF   |
| Tera Term      | Serial, 115200, CR+LF receive     |
| minicom        | `minicom -D /dev/ttyUSB0 -b 115200` |

Ved boot vises:
```
================================
 Modbus API Gateway v0.1.0 b0026
 Serial CLI -- skriv 'help'
================================

gw>
```

Skriv `help` for liste over alle kommandoer.

---

## Generelt workflow

```
gw> <konfigurer>       -- foretag ændring
gw> show config        -- verificér ændringen
gw> save               -- gem til NVS (permanent)
gw> reboot             -- genstart og aktivér
```

> **Vigtigt:** Uden `save` + `reboot` er ændringer kun i RAM og tabt ved genstart.

---

## Kommandooversigt

| Kommando       | Beskrivelse                                      |
|----------------|--------------------------------------------------|
| `help`         | Vis alle kommandoer                              |
| `show config`  | Vis komplet gemt konfiguration (IOS-stil)        |
| `status`       | Live status: version, IP, uptime, heap           |
| `eth`          | Konfigurér Ethernet (se nedenfor)                |
| `wifi`         | Konfigurér WiFi STA og AP (se nedenfor)          |
| `save`         | Gem konfiguration til NVS (flash)                |
| `reboot`       | Genstart gateway                                 |

---

## Ethernet (`eth`)

### Vis konfiguration

```
gw> show config
```
```
Interface ETH0
 Enable
 Type LAN8720
 PHY-addr 0
 MDC      GPIO 23
 MDIO     GPIO 18
 IP dhcp
End interface ETH0
```

---

### Enable / Disable

```
gw> eth enable
gw> eth disable
```

---

### Hardware-type

Gateway understøtter to Ethernet-hardware-varianter:

| Type    | Interface | Typisk brug             |
|---------|-----------|-------------------------|
| LAN8720 | RMII      | Indbygget MAC på ESP32  |
| W5500   | SPI       | Ekstern SPI Ethernet    |

```
gw> eth type lan8720
gw> eth type w5500
```

---

### LAN8720 RMII — GPIO-konfiguration

LAN8720 bruger ESP32's interne Ethernet MAC via RMII.  
De fleste pins er faste (RMII-standard), men MDC, MDIO og PHY-reset er konfigurerbare.

| Parameter  | Kommando              | Default | Beskrivelse              |
|------------|-----------------------|---------|--------------------------|
| PHY adresse| `eth phy-addr <0-31>` | 0       | Typisk 0 eller 1         |
| MDC pin    | `eth mdc <gpio>`      | 23      | Management clock         |
| MDIO pin   | `eth mdio <gpio>`     | 18      | Management data          |
| PHY reset  | `eth phy-rst <gpio>`  | -1      | -1 = ikke tilsluttet     |

**Eksempel — LAN8720 standardopsætning:**
```
gw> eth type lan8720
gw> eth phy-addr 0
gw> eth mdc 23
gw> eth mdio 18
gw> eth phy-rst -1
gw> eth dhcp
gw> eth enable
gw> save
gw> reboot
```

---

### W5500 SPI — GPIO-konfiguration

W5500 er en ekstern Ethernet-controller tilsluttet via SPI.

| Parameter  | Kommando              | Beskrivelse                        |
|------------|-----------------------|------------------------------------|
| CS pin     | `eth cs <gpio>`       | SPI Chip Select (aktiv lav)        |
| MOSI pin   | `eth mosi <gpio>`     | SPI Master Out                     |
| MISO pin   | `eth miso <gpio>`     | SPI Master In                      |
| SCLK pin   | `eth sclk <gpio>`     | SPI Clock                          |
| INT pin    | `eth int <gpio>`      | Interrupt (-1 = pollet tilstand)   |

**Eksempel — W5500 på SPI2:**
```
gw> eth type w5500
gw> eth cs 5
gw> eth mosi 23
gw> eth miso 19
gw> eth sclk 18
gw> eth int 26
gw> eth dhcp
gw> eth enable
gw> save
gw> reboot
```

---

### IP-konfiguration

**DHCP (standard):**
```
gw> eth dhcp
```

**Statisk IP:**
```
gw> eth 192.168.1.100 192.168.1.1 255.255.255.0
```
Format: `eth <ip> <gateway> <netmask>`

---

## WiFi (`wifi`)

### Vis konfiguration

```
gw> show config
```
```
Interface WIFI
 Enable
 mode STA
 SSID "MitNetværk"
 PSK "hemmeligt"
 IP dhcp
End interface WIFI
!
Interface WIFI-AP
 Enable
 SSID "ModbusGW-AUTO"
 PSK none
 IP 192.168.4.1
End interface WIFI-AP
```

---

### Enable / Disable WiFi STA

```
gw> wifi on
gw> wifi off
```

---

### Netværk (SSID og adgangskode)

```
gw> wifi ssid MitNetværk
gw> wifi pass HemmeligKode123
```

> Adgangskoden vises som `*** (sat)` i `show config` — aldrig i klartekst.

---

### IP-konfiguration

**DHCP (standard):**
```
gw> wifi ip dhcp
```

**Statisk IP:**
```
gw> wifi ip 192.168.1.50
```

> Gateway og netmask ved statisk WiFi IP konfigureres via REST API:  
> `PUT /api/v1/system/wifi` med `{"ip":"192.168.1.50","gw":"192.168.1.1","netmask":"255.255.255.0"}`

---

### AP fallback hotspot

Hvis STA ikke kan forbinde (forkert kode, netværk utilgængeligt), kan gatewayen automatisk oprette et WiFi hotspot.

```
gw> wifi ap on           -- aktivér AP fallback
gw> wifi ap off          -- deaktivér AP fallback
gw> wifi ap-ssid GatewaySetup    -- AP netværksnavn
gw> wifi pass MinAPKode          -- AP adgangskode (min 8 tegn)
```

Hotspot-IP er altid `192.168.4.1`.  
Standardnavn: `ModbusGW-AUTO` (hvis ap-ssid ikke er sat).

---

### Vis live WiFi status

```
gw> wifi status
```
```
--------------------------------
WiFi live status
  Tilstand  : forbundet
  Mode      : klient (STA)
  MAC (STA) : AA:BB:CC:DD:EE:FF
  SSID      : MitNetværk
  IP        : 192.168.1.55
  RSSI      : -62 dBm
  Kanal     : 6
  Auth      : WPA2-PSK
  BSSID     : 11:22:33:44:55:66
--------------------------------
```

```
gw> wifi mode
```
```
--------------------------------
WiFi mode: klient (STA) — forbundet til 'MitNetværk'
--------------------------------
```

---

### Komplet WiFi STA opsætning — eksempel

```
gw> wifi ssid KontorNet
gw> wifi pass Adgangskode2024
gw> wifi ip dhcp
gw> wifi ap on
gw> wifi ap-ssid GW-Fallback
gw> wifi on
gw> save
gw> reboot
```

---

## Modbus interfaces (`show config`)

Modbus-interfaces konfigureres endnu ikke fuldt via CLI — brug REST API:

```
PUT /api/v1/interfaces/0/config
{
  "baudrate": 9600,
  "data_bits": 8,
  "parity": 0,
  "stop_bits": 1,
  "timeout_ms": 500,
  "enabled": true
}
```

`show config` viser den gemte konfiguration:

```
Interface Modbus0
 Enable
 Type RS485
 UART HW UART1
 com 9600B-8N1
 Timeout 500ms
 Tx GPIO 17
 Rx GPIO 16
 DE GPIO 4
End interface Modbus0
```

---

## System status og info

```
gw> status
```
```
--------------------------------
Version : v0.1.0 b0026
Uptime  : 1234 s
Eth IP  : 192.168.1.100
Heap    : 187 KB fri
--------------------------------
```

---

## REST API

Alle endpoints kan ses direkte fra gatewayen:

```
GET http://<ip>/api/v1/
```

Returnerer JSON med liste over alle 21 endpoints.

**Vigtige endpoints:**

| Endpoint                     | Metode | Beskrivelse                  |
|------------------------------|--------|------------------------------|
| `/api/v1/system`             | GET    | System info                  |
| `/api/v1/system/wifi`        | GET    | WiFi status                  |
| `/api/v1/system/wifi`        | PUT    | Konfigurér WiFi              |
| `/api/v1/system/wifi/scan`   | GET    | Scan WiFi-netværk            |
| `/api/v1/interfaces`         | GET    | List Modbus-interfaces       |
| `/api/v1/interfaces/0/slaves/1/holding-registers?start=0&count=10` | GET | Læs 10 holding registers |
