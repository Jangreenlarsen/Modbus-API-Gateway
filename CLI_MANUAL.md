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

| Parameter      | Kommando                | Default | Beskrivelse                                      |
|----------------|-------------------------|---------|--------------------------------------------------|
| CS pin         | `eth cs <gpio>`         | 23      | SPI Chip Select (aktiv lav)                      |
| MOSI pin       | `eth mosi <gpio>`       | 13      | SPI Master Out                                   |
| MISO pin       | `eth miso <gpio>`       | 12      | SPI Master In                                    |
| SCLK pin       | `eth sclk <gpio>`       | 14      | SPI Clock                                        |
| RST pin        | `eth rst <gpio\|-1>`    | 33      | Hardware reset (-1 = ikke tilsluttet)            |
| INT pin        | `eth int <gpio\|-1>`    | 34      | Interrupt-pin (-1 = polling-mode, høj latency!)  |
| SPI clock      | `eth spi-clock <1-36>`  | 10      | SPI clock i MHz (10=safe, 20=fast, max 36)       |
| Poll interval  | `eth poll-ms <1-100>`   | 10      | Polling interval når `int=-1` (lavere = mindre latency) |

> **VIGTIGT om INT-pin:** GPIO 34-39 har INGEN intern pull-up på ESP32 — brug ekstern 4.7-10 kΩ modstand til 3.3V. Uden korrekt pull-up får du sporadisk pakke-tab.

**Eksempel — W5500 standardopsætning (default):**
```
gw> eth type w5500
gw> eth cs 23
gw> eth mosi 13
gw> eth miso 12
gw> eth sclk 14
gw> eth rst 33
gw> eth int 34
gw> eth spi-clock 10
gw> eth dhcp
gw> eth enable
gw> save
gw> reboot
```

**Konservativ konfiguration (lange dupont-ledninger):**
```
gw> eth spi-clock 5
gw> save
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

## Modbus interfaces (`interface modbus<N>`)

Modbus-interfaces konfigureres nu fuldt via CLI under `configure terminal`-mode.

### Gå ind i et interface

```
gw> configure terminal
gw(config)# interface modbus0          -- konfigurér eksisterende interface
gw(config)# interface modbus1          -- N == antal interfaces → opretter nyt (SW-UART master)
gw(config)# no interface modbus1       -- slet interface (renummererer resten)
```

Op til 8 interfaces total (2 HW UART + 6 SW UART).

### Kommandoer i `gw(config-modbusN)#`

| Kommando | Beskrivelse |
|----------|-------------|
| `enable` / `disable` | Aktivér/deaktivér interface |
| `name <navn>` | Brugervenligt navn (max 23 tegn). Kan bruges som alias i REST API URLs |
| `mode master\|slave` | Modbus-rolle (slave kun på HW-UART) |
| `addr <1-247>` | Slave-adresse (kun ved `mode slave`) |
| `type rs485\|rs232` | Elektrisk niveau |
| `uart hw <num>` / `uart sw` | HW UART (1 eller 2) eller SW bit-bang |
| `baudrate <baud>` | 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 (SW: ≤ 9600) |
| `format <bits> <n\|e\|o> <stop>` | Fx `format 8 n 1` (8N1) |
| `timeout <ms>` | Modbus master request timeout |
| `tx <gpio>` | TX GPIO pin |
| `rx <gpio>` | RX GPIO pin |
| `de <gpio>` | DE/RE GPIO pin (RS485 transceiver) |

### Eksempel — komplet RS485 master opsætning

```
gw> configure terminal
gw(config)# interface modbus0
gw(config-modbus0)# name floor1
gw(config-modbus0)# mode master
gw(config-modbus0)# type rs485
gw(config-modbus0)# uart hw 1
gw(config-modbus0)# baudrate 19200
gw(config-modbus0)# format 8 n 1
gw(config-modbus0)# timeout 500
gw(config-modbus0)# tx 17
gw(config-modbus0)# rx 16
gw(config-modbus0)# de 4
gw(config-modbus0)# enable
gw(config-modbus0)# exit
gw(config)# exit
gw> save
gw> reboot
```

### Eksempel — slave på UART2

```
gw(config)# interface modbus1
gw(config-modbus1)# name plc-slave
gw(config-modbus1)# mode slave
gw(config-modbus1)# addr 5
gw(config-modbus1)# uart hw 2
gw(config-modbus1)# baudrate 9600
gw(config-modbus1)# tx 22
gw(config-modbus1)# rx 21
gw(config-modbus1)# de 25
gw(config-modbus1)# enable
```

### `show config` output

```
Interface Modbus0
 Enable
 Name floor1
 Mode Master
 Type RS485
 UART HW UART1
 com 19200B-8N1
 Timeout 500ms
 Tx GPIO 17
 Rx GPIO 16
 DE GPIO 4
End interface Modbus0
!
```

### Web GUI alternativ

Den nemmeste vej er ofte via `http://<ip>/mgmt` → RS485 Config-fanen, som understøtter alle samme felter inkl. interface-tilføj/slet og GPIO-pins.

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

| Endpoint | Metode | Beskrivelse |
|----------|--------|-------------|
| `/api/v1/system` | GET | System info |
| `/api/v1/system/reboot` | POST | Genstart |
| `/api/v1/system/wifi` | GET / PUT | WiFi status og konfiguration |
| `/api/v1/system/wifi/scan` | GET | Scan WiFi-netværk |
| `/api/v1/system/ota/check` | GET | Tjek for opdatering på GitHub |
| `/api/v1/system/ota/firmware` | POST | Start firmware-OTA |
| `/api/v1/interfaces` | GET | List alle Modbus-interfaces |
| `/api/v1/interfaces` | POST | Opret nyt interface |
| `/api/v1/interfaces/{key}` | GET / PUT / DELETE | Konfiguration — `{key}` = ID eller navn-alias |
| `/api/v1/interfaces/{key}/slaves/{sid}/coils?start=0&count=8` | GET | FC01 — læs coils |
| `/api/v1/interfaces/{key}/slaves/{sid}/discrete-inputs?start=0&count=8` | GET | FC02 — læs discrete inputs |
| `/api/v1/interfaces/{key}/slaves/{sid}/holding-registers?start=0&count=10` | GET | FC03 — læs holding registers |
| `/api/v1/interfaces/{key}/slaves/{sid}/input-registers?start=0&count=10` | GET | FC04 — læs input registers |
| `/api/v1/interfaces/{key}/slaves/{sid}/coils/{addr}` | PUT | FC05 — skriv enkelt coil |
| `/api/v1/interfaces/{key}/slaves/{sid}/holding-registers/{addr}` | PUT | FC06 — skriv enkelt register |
| `/api/v1/interfaces/{key}/slaves/{sid}/coils?start=N` | PUT | FC0F — skriv flere coils |
| `/api/v1/interfaces/{key}/slaves/{sid}/holding-registers?start=N` | PUT | FC10 — skriv flere registers |

> `{key}` accepterer både numerisk ID (`0`, `1`, ...) og navn-alias (`floor1`, `pumpestation`, ...) — case-insensitive.
