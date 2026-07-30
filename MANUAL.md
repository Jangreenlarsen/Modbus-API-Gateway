# Modbus API Gateway — Komplet Manual

**Firmware-version ved skrivning:** v0.8.1 build 0094
**Målgruppe:** installatører (hardware/GPIO-opsætning) og udviklere (REST API-integration)

Denne manual er den samlede brugerguide. Den supplerer (og gentager ikke i detalje) de tekniske referencedokumenter i repoet:

| Dokument | Indhold |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Lag-arkitektur, fuld endpoint-tabel, JSON-konventioner |
| [MODBUS_REFERENCE.md](MODBUS_REFERENCE.md) | Modbus RTU-protokolspecifikation |
| [ESP32_REFERENCE.md](ESP32_REFERENCE.md) | ESP-IDF/hardware-detaljer |
| [CLI_MANUAL.md](CLI_MANUAL.md) | Fuld serial-CLI-kommandoreference |
| [CHANGELOG.md](CHANGELOG.md) / [RELEASE_NOTES.md](RELEASE_NOTES.md) | Versionshistorik |

---

## Indholdsfortegnelse

1. [Introduktion](#1-introduktion)
2. [Installation & Hardware-opsætning](#2-installation--hardware-opsætning)
   1. [Hvad du skal bruge](#21-hvad-du-skal-bruge)
   2. [GPIO — grundregler og reserverede pins](#22-gpio--grundregler-og-reserverede-pins)
   3. [Pin-tildeling — RS485](#23-pin-tildeling--rs485)
   4. [Pin-tildeling — RS232](#24-pin-tildeling--rs232)
   5. [Wiring — RS485-transceiver (MAX485)](#25-wiring--rs485-transceiver-max485)
   6. [Wiring — RS232-transceiver (MAX232)](#26-wiring--rs232-transceiver-max232)
   7. [Ethernet-tilslutning](#27-ethernet-tilslutning)
   8. [Vigtig begrænsning: HW-UART master/slave](#28-vigtig-begrænsning-hw-uart-masterslave)
   9. [Første opstart](#29-første-opstart)
   10. [Netværksopsætning](#210-netværksopsætning)
   11. [Opsætning af et Modbus-interface](#211-opsætning-af-et-modbus-interface)
3. [Web Management GUI (/mgmt)](#3-web-management-gui-mgmt)
4. [REST API Programmeringsguide](#4-rest-api-programmeringsguide)
   1. [Grundlæggende](#41-grundlæggende)
   2. [Autentificering](#42-autentificering)
   3. [Fejl-format](#43-fejl-format)
   4. [System & netværk](#44-system--netværk)
   5. [Interface-administration](#45-interface-administration)
   6. [Modbus function codes (FC01–FC10)](#46-modbus-function-codes-fc01fc10)
   7. [Interface loopback-selvtest](#47-interface-loopback-selvtest)
   8. [Register-cache](#48-register-cache)
   9. [Modbus-log (dekodet bus-trafik)](#49-modbus-log-dekodet-bus-trafik)
   10. [API-kald-log](#410-api-kald-log)
   11. [OTA-opdatering](#411-ota-opdatering)
   12. [WebSocket](#412-websocket)
   13. [Programmeringseksempler](#413-programmeringseksempler)
5. [CLI — kort reference](#5-cli--kort-reference)
6. [Fejlfinding](#6-fejlfinding)
7. [Appendix](#7-appendix)

---

## 1. Introduktion

Modbus API Gateway er en ESP32-baseret bro mellem **Modbus RTU** (RS485/RS232, seriel) og **REST/HTTP** (Ethernet/WiFi). Enheden fungerer som Modbus RTU master (og valgfrit slave) på op til 8 seriel-interfaces samtidig, og eksponerer nøjagtig de samme operationer som standard Modbus function codes via et REST API — ingen specialsoftware nødvendig på klientsiden.

**Kernefunktioner:**
- Fuld FC01–FC10 REST-mapping (læs/skriv coils, discrete inputs, holding/input registers)
- Op til 8 seriel-interfaces (2× hardware-UART + op til 6× software-UART bit-bang)
- Lokal register-cache med baggrunds-refresh (reducerer bus-trafik og latency)
- Web-GUI til konfiguration, status, cache-monitorering og dekodet Modbus-log
- Serial CLI (IOS-stil) til konfiguration uden netværk
- OTA-opdatering af både firmware og frontend fra GitHub Releases
- Interface loopback-selvtest til hurtig hardware-diagnose

**Arkitektur (kort):** Frontend → REST API-lag → Service-lag → Modbus-lag → UART/hardware. Se [ARCHITECTURE.md](ARCHITECTURE.md) for det fulde lagdiagram og regler.

---

## 2. Installation & Hardware-opsætning

### 2.1 Hvad du skal bruge

| Komponent | Detaljer |
|---|---|
| ESP32-udviklingsboard | 30-pin (standard, fx NodeMCU-32S) eller 38-pin (bred variant, eksponerer GPIO 37+38) |
| Ethernet | **W5500** (SPI-modul, standard) eller **LAN8720** (RMII, kræver ESP32 med indbygget EMAC-understøttelse) |
| RS485-transceiver | MAX485 / SN65HVD3082 pr. RS485-interface |
| RS232-transceiver | MAX232 / SP3232 pr. RS232-interface |
| Strømforsyning | 5V/USB til ESP32-board; separat 3.3V/5V til transceivere efter datablad |
| USB-til-seriel | Til første konfiguration via CLI (medmindre boardet har indbygget USB) |

Board-variant (30-pin/38-pin) vælges i konfigurationen (`PUT /api/v1/system/hardware` eller web-GUI) — det styrer hvilke GPIO-preset-værdier og pin-tilgængelighed systemet foreslår.

---

### 2.2 GPIO — grundregler og reserverede pins

Før du tildeler pins til RS485/RS232, kend disse **faste begrænsninger** på ESP32:

| Pins | Status | Grund |
|---|---|---|
| GPIO 6–11 | **Aldrig brugbare** | Forbundet til intern SPI-flash |
| GPIO 1, 3 | Reserveret (UART0) | Serial CLI-konsol (TX/RX) — undgå medmindre du opgiver CLI |
| GPIO 34–39 | **Kun input** (RX) | Ingen intern pull-up/pull-down, kan ikke bruges som TX eller DE |
| GPIO 0, 2, 5, 12, 15 | **Forsigtighed** (strapping/boot-pins) | Påvirker boot-mode — undgå som DE-pin hvis muligt, eller sørg for korrekt idle-niveau ved boot |
| GPIO 20, 24, 28–31 | Findes ikke | Ikke til stede på ESP32 |
| GPIO 37, 38 | Kun på 38-pin boards | Ikke eksponeret på standard 30-pin boards |

**Ethernet optager også pins** — de er automatisk reserverede og må ikke genbruges til RS485/RS232:

| Hardware | Reserverede GPIO (default) |
|---|---|
| **W5500** (SPI) | CS=23, MOSI=13, MISO=12, SCLK=14, RST=33, INT=34 |
| **LAN8720** (RMII, faste pins) | 0 (CLK), 19 (TXD0), 21 (TX_EN), 22 (TXD1), 25 (RXD0), 26 (RXD1), 27 (CRS_DV) — samt konfigurerbare MDC/MDIO/PHY-RST |

> **Tip:** `GET /api/v1/system/gpio` (se afsnit 4.4) returnerer live, hvilke GPIO'er der reelt er ledige på din enhed — inklusive de Ethernet-pins der er optaget netop nu. Den samme oversigt vises visuelt i web-GUI'ens "RS485 Config"-fane (farvekodet grid).

---

### 2.3 Pin-tildeling — RS485

RS485 er half-duplex og kræver **tre** GPIO'er pr. interface: TX, RX og **DE** (Driver Enable, styrer transceiverens sende/modtage-retning).

Nedenstående er fabriks-standardpresets (kan altid ændres frit via CLI eller GUI — enhver ledig GPIO kan bruges). "GPIO Preset"-knappen i web-GUI'en udfylder disse automatisk pr. interface-id.

**30-pin board:**

| Interface-id | TX | RX | DE | Bemærkning |
|---|---|---|---|---|
| 0 | 17 | 16 | 4 | HW-UART1 (standard master-port) |
| 1 | 25 | 26 | 27 | |
| 2 | 21 | 22 | 19 | |
| 3 | 32 | 35 | 15 | RX=35 er input-only (OK til RX) |
| 4 | 5 | 36 | 18 | RX=36 er input-only (OK til RX) |
| 5 | 2 | 39 | 0 | RX=39 input-only; **DE=0 er en boot-strap-pin** — brug med forsigtighed |
| 6 | 15 | 34 | 4 | RX=34 input-only; **DE=4 deler pin med interface 0** — skift ved konflikt |
| 7 | 18 | 38 | 19 | GPIO38 er kun tilgængelig på visse 30-pin boards — brug 39 som alternativ hvis ikke eksponeret |

**38-pin board:**

| Interface-id | TX | RX | DE | Bemærkning |
|---|---|---|---|---|
| 0 | 17 | 16 | 4 | HW-UART1 |
| 1 | 25 | 26 | 27 | |
| 2 | 21 | 22 | 19 | |
| 3 | 32 | 35 | 15 | |
| 4 | 5 | 36 | 18 | |
| 5 | 2 | 37 | 0 | GPIO37 eksponeret på 38-pin boards; DE=0 boot-strap-pin |
| 6 | 15 | 38 | 4 | GPIO38 eksponeret på 38-pin boards |
| 7 | 18 | 39 | 19 | |

**Kun interface-id 0 og 1 kan konfigureres som hardware-UART** (UART1/UART2, op til 115200 baud). Interface-id 2–7 er **software-UART** (bit-bang, maks. 9600 baud) — se afsnit 2.8 for hvorfor.

---

### 2.4 Pin-tildeling — RS232

RS232 er full-duplex og kræver kun **TX + RX** — der er ingen DE/RE-styring (bruges ikke, sæt DE til `-1`). Brug samme TX/RX-kolonner som RS485-tabellerne ovenfor; DE-kolonnen er irrelevant.

| Interface-id | TX | RX |
|---|---|---|
| 0 | 17 | 16 |
| 1 | 25 | 26 |
| 2 | 21 | 22 |
| 3 | 32 | 35 |
| 4 | 5 | 36 |
| 5 | 2 | 39 (30-pin) / 37 (38-pin) |
| 6 | 15 | 34 (30-pin) / 38 (38-pin) |
| 7 | 18 | 38 (30-pin) / 39 (38-pin) |

Sæt interface-type til `RS232` via CLI (`type rs232`) eller GUI — DE-feltet skjules/ignoreres automatisk.

---

### 2.5 Wiring — RS485-transceiver (MAX485)

```
ESP32 GPIO          MAX485 / SN65HVD3082        RS485-bus
───────────          ────────────────────        ─────────
TX_PIN  ────────────▶ DI  (Data In)
RX_PIN  ◀──────────── RO  (Receive Out)
DE_PIN  ──────┬─────▶ DE  (Driver Enable)
              └─────▶ RE̅  (Receiver Enable, aktiv lav — tie sammen med DE)
                       A  ───────────────────────▶ Bus A (+)
                       B  ───────────────────────▶ Bus B (−)
3.3V/5V ─────────────▶ VCC   (efter transceiver-datablad)
GND     ─────────────▶ GND
```

- **DE og RE̅ tied sammen** til samme ESP32-GPIO — firmwaren sætter GPIO højt ved TX og lavt ved RX (`UART_MODE_RS485_HALF_DUPLEX` for HW-UART, manuel styring for SW-UART).
- **Terminering:** 120 Ω modstand i **begge** ender af bus-segmentet ved lange kabler eller flere slaver.
- **Op til 247 slaves** pr. bus-segment (standard Modbus-adresserum 1–247).

### 2.6 Wiring — RS232-transceiver (MAX232)

```
ESP32 GPIO          MAX232 / SP3232              RS232 D-SUB9 (DTE)
───────────          ────────────────             ───────────────────
TX_PIN  ────────────▶ T1IN   → T1OUT ────────────▶ Pin 3 (TXD)
RX_PIN  ◀──────────── R1OUT  ← R1IN  ◀──────────── Pin 2 (RXD)
                                                     Pin 5 (GND)
3.3V/5V ─────────────▶ VCC (efter datablad — MAX232 kræver typisk 5V + ladningspumpe-kondensatorer)
GND     ─────────────▶ GND
```

- RS232 er **punkt-til-punkt** (1:1) — ingen bus-topologi, ingen terminering, ingen DE-styring.
- Maks. kabellængde ~15 m ved fuld hastighed.
- Kryds TX/RX korrekt afhængig af om modparten er DTE eller DCE (nul-modem vs. lige kabel).

---

### 2.7 Ethernet-tilslutning

**W5500 (SPI) — anbefalet, standard-konfiguration:**

| ESP32 GPIO | W5500-pin | Konfigurerbar via |
|---|---|---|
| 23 | CS | `eth cs <gpio>` |
| 13 | MOSI | `eth mosi <gpio>` |
| 12 | MISO | `eth miso <gpio>` |
| 14 | SCLK | `eth sclk <gpio>` |
| 33 | RST | `eth rst <gpio\|-1>` |
| 34 | INT | `eth int <gpio\|-1>` (se advarsel nedenfor) |

> **VIGTIGT — INT-pin (GPIO34):** GPIO 34–39 har **ingen intern pull-up**. Ved interrupt-mode kræves en **ekstern pull-up-modstand (4.7–10 kΩ til 3.3V)** på INT-pinnen. Uden den kan Ethernet-linket flappe (se afsnit 6, Fejlfinding). Alternativt: sæt `eth int -1` for polling-mode (lidt højere latency, ingen ekstern modstand nødvendig).

**LAN8720 (RMII) — kræver ESP32 med intern EMAC:**

Faste RMII-pins (kan ikke ændres): GPIO 0 (CLK), 19 (TXD0), 21 (TX_EN), 22 (TXD1), 25 (RXD0), 26 (RXD1), 27 (CRS_DV). MDC/MDIO/PHY-reset er konfigurerbare (`eth mdc`, `eth mdio`, `eth phy-rst`).

Hver enhed får automatisk en **unik MAC-adresse** udledt af sin egen chip ved boot (logges i seriel-loggen ved Ethernet-init).

---

### 2.8 Vigtig begrænsning: HW-UART master/slave

Modbus-biblioteket (esp-modbus) bruger en **global, delt controller** for henholdsvis master- og slave-rollen. Det betyder:

- **Højst ét hardware-UART-interface kan være master.**
- **Højst ét hardware-UART-interface kan være slave.**
- En master og en slave kan sameksistere samtidig (de bruger separate globale controllere).
- Yderligere hardware-interfaces (utover disse to) deaktiveres automatisk ved boot med en tydelig fejlbesked i loggen.

**Har du brug for flere end 2 seriel-porte** (fx 4 RS485-linjer), skal de ekstra interfaces (id 2–7) konfigureres som **software-UART** (`uart sw`), som ikke deler denne begrænsning — til gengæld er SW-UART begrænset til maks. **9600 baud**.

---

### 2.9 Første opstart

1. Flash firmwaren (via PlatformIO/USB, eller efterfølgende via OTA — se afsnit 4.11).
2. Tilslut en USB-seriel-adapter til UART0 (TX=GPIO1, RX=GPIO3) hvis boardet ikke har indbygget USB-CDC.
3. Åbn en terminal: 115200 baud, 8N1 (se [CLI_MANUAL.md](CLI_MANUAL.md) for terminalprogram-specifikke indstillinger).
4. Ved boot vises:
   ```
   ================================
    Modbus API Gateway v0.8.1 b0094
    Serial CLI -- skriv 'help'
   ================================
   gw>
   ```
5. Skriv `help` for kommandooversigt.

---

### 2.10 Netværksopsætning

**Ethernet (DHCP, standard):**
```
gw> eth type w5500
gw> eth dhcp
gw> eth enable
gw> save
gw> reboot
```

**Ethernet (statisk IP):**
```
gw> eth 192.168.1.100 192.168.1.1 255.255.255.0
gw> save
gw> reboot
```

**WiFi (som supplement eller alternativ):**
```
gw> wifi ssid MitNetværk
gw> wifi pass HemmeligKode123
gw> wifi ip dhcp
gw> wifi on
gw> save
gw> reboot
```

Se [CLI_MANUAL.md](CLI_MANUAL.md) for AP-fallback-hotspot og fuld parameterliste. Efter reboot: find IP'en via `status` i CLI'en, eller kig i seriel-loggen (`Got IP: ...`).

---

### 2.11 Opsætning af et Modbus-interface

**Via CLI:**
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

**Via web-GUI:** `http://<ip>/mgmt` → fanen **RS485 Config** → udfyld felter (eller brug "GPIO Preset"-knappen) → **Gem**. Bemærk: konfigurationsændringer via REST/GUI kræver **reboot** for at træde i kraft (svaret indeholder `"reboot_required": true`) — interfaces initialiseres kun ved boot.

**Bekræft opsætningen** med interface loopback-selvtesten (Test-knap i GUI, se afsnit 4.7) inden du sætter slaver på bussen.

---

## 3. Web Management GUI (/mgmt)

Naviger til `http://<enhedens-ip>/mgmt`. Faner:

| Fane | Indhold |
|---|---|
| **Status** | Version, uptime, heap, Ethernet/WiFi-status, interface-oversigt, direkte register-læsning |
| **Cache** | Live cache-statistik, hit-rate-graf, entries-tabel, TTL/refresh-indstillinger |
| **OTA Opdatering** | Tjek/installér firmware- og frontend-opdateringer fra GitHub Releases |
| **RS485 Config** | Interface-CRUD, GPIO-preset, GPIO-tilgængelighedsoversigt, board-variant, loopback-selvtest |
| **Modbus Log** | Live dekodet bus-trafik (FC-navn, slave, adresse, status, værdi) |
| **API Log** | Live log over alle indkomne HTTP-kald |

Siden er indlejret i firmwaren (ingen separat installation nødvendig) og opdateres uafhængigt af frontend-SPIFFS-imaget.

---

## 4. REST API Programmeringsguide

### 4.1 Grundlæggende

- **Base URL:** `http://<enhedens-ip>/api/v1`
- **Format:** JSON for alle request-bodies og responses (`Content-Type: application/json`)
- **Selv-dokumenterende:** `GET /api` eller `GET /api/v1` returnerer en fuld liste over alle endpoints med metode og beskrivelse — nyttigt til at verificere den installerede firmware-versions faktiske API-overflade.
- **Adressering:** Register/coil-adresser er **0-baserede** wire-adresser (ikke de 1-baserede/prefikserede adresser fra udstyrsdatablade — se [MODBUS_REFERENCE.md](MODBUS_REFERENCE.md) for konvertering).
- **Interface-nøgle (`{key}`):** overalt hvor et interface refereres i URL'en, accepteres enten det numeriske id (`0`, `1`, …) **eller** det konfigurerede navn-alias (`floor1`, `pumpestation`, …) — case-insensitive.

### 4.2 Autentificering

Valgfri API-key-autentificering kan aktiveres (`auth_enabled` + `api_key` i systemkonfigurationen). Når aktiveret, skal alle kald inkludere:

```
X-API-Key: <din-nøgle>
```

Manglende/forkert nøgle giver `401 Unauthorized`:
```json
{ "error": "unauthorized", "hint": "X-API-Key header mangler eller forkert" }
```

### 4.3 Fejl-format

Alle Modbus-operationer (FC01–FC10) bruger et **ensartet** fejl-format:

| HTTP-status | `error`-værdi | Betydning |
|---|---|---|
| 504 Gateway Timeout | `modbus_timeout` | Ingen svar fra slave inden for timeout |
| 400 Bad Request | `modbus_exception` | Slave returnerede en Modbus-exception (`exception_code` + `description` medfølger) |
| 400 Bad Request | `modbus_error` | Anden fejl (`detail` = ESP-IDF-fejlnavn) |
| 400 Bad Request | (endpoint-specifik) | Ugyldig/manglende request-body eller parameter |
| 404 Not Found | `interface not found` / `unknown route` | Ukendt interface-nøgle eller sti |
| 409 Conflict | (endpoint-specifik) | Fx forsøg på at slette sidste interface |

> **Bemærk:** `exception_code` er p.t. kun tilgængelig på **software-UART**-interfaces. Hardware-UART-fejl (via esp-modbus-biblioteket) rapporteres som den mere generiske `modbus_error` — biblioteket eksponerer ikke exception-koden for HW-master-kald i den nuværende version.

Eksempel — timeout:
```json
{ "interface": 0, "slave": 3, "error": "modbus_timeout", "description": "No response within timeout" }
```

Eksempel — exception (SW-UART):
```json
{ "interface": 2, "slave": 5, "error": "modbus_exception", "exception_code": 2, "description": "Illegal Data Address" }
```

---

### 4.4 System & netværk

| Metode | Endpoint | Beskrivelse |
|---|---|---|
| GET | `/api/v1/system` | Version, build, uptime, IP, fri heap, reset-årsag, board-variant |
| POST | `/api/v1/system/reboot` | Genstart enheden |
| GET | `/api/v1/system/hardware` | Board-variant + GPIO-presets pr. interface |
| PUT | `/api/v1/system/hardware` | Sæt board-variant: `{"board_variant":"30pin"|"38pin"}` |
| GET | `/api/v1/system/gpio` | **GPIO-tilgængelighed** (se nedenfor) |
| GET | `/api/v1/system/wifi` | WiFi-status (tilstand, SSID, IP, RSSI) |
| PUT | `/api/v1/system/wifi` | Konfigurér WiFi |
| GET | `/api/v1/system/wifi/scan` | Scan tilgængelige WiFi-netværk |
| GET | `/api/v1/system/log?since=N` | API-kald-log siden `seq` N (se afsnit 4.10) |
| POST | `/api/v1/system/log/clear` | Ryd API-kald-loggen |

**Eksempel — `GET /api/v1/system`:**
```json
{
  "version": "0.8.1", "build": "0094", "uptime_s": 3612,
  "ip": "10.1.32.101", "free_heap": 142336,
  "reset_reason": 1, "board_variant": "30pin"
}
```

**Eksempel — `GET /api/v1/system/gpio`** (bruges til at planlægge pin-tildeling programmatisk — se afsnit 2.2/2.3):
```json
{
  "board_variant": "30pin",
  "gpios": [
    { "gpio": 16, "tx": true, "rx": true, "de": true, "input_only": false, "caution": false },
    { "gpio": 17, "tx": true, "rx": true, "de": true, "input_only": false, "caution": false, "used_by": 0, "used_role": "TX" },
    { "gpio": 23, "tx": false, "rx": false, "de": false, "input_only": false, "caution": false, "reserved_by": "ethernet" },
    { "gpio": 35, "tx": false, "rx": true, "de": false, "input_only": true, "caution": false }
  ]
}
```
Felter: `tx`/`rx`/`de` = brugbar til den rolle lige nu; `reserved_by` = `"flash"` | `"uart0"` | `"ethernet"` (kun til stede hvis optaget); `used_by` + `used_role` = interface-id og rolle, hvis pinnen allerede er tildelt et konfigureret interface.

**Eksempel — `PUT /api/v1/system/wifi`:**
```json
{ "enabled": true, "ssid": "MitNetværk", "password": "hemmeligt", "ip": "dhcp", "ap_fallback": true }
```

---

### 4.5 Interface-administration

| Metode | Endpoint | Beskrivelse |
|---|---|---|
| GET | `/api/v1/interfaces` | Liste over alle konfigurerede interfaces |
| POST | `/api/v1/interfaces` | Opret nyt interface (SW-UART master, defaults) |
| GET | `/api/v1/interfaces/{key}` | Hent ét interfaces konfiguration |
| PUT | `/api/v1/interfaces/{key}` | Opdatér interface-konfiguration |
| DELETE | `/api/v1/interfaces/{key}` | Slet interface (renummererer resterende) |

**Interface-objekt (fælles for GET/PUT-body):**
```json
{
  "id": 0, "name": "floor1", "type": "RS485", "uart_mode": "hw",
  "mode": "master", "slave_addr": 1, "uart": 1, "baudrate": 19200,
  "data_bits": 8, "parity": 0, "stop_bits": 1, "timeout_ms": 500,
  "tx_pin": 17, "rx_pin": 16, "rts_pin": 4, "enabled": true
}
```
- `type`: `"RS485"` \| `"RS232"`
- `uart_mode`: `"hw"` \| `"sw"`
- `mode`: `"master"` \| `"slave"`
- `parity`: `0`=ingen, `1`=ulige, `2`=lige
- `rts_pin`: DE/RE-pin (RS485). Sæt `-1` for RS232.

**PUT/POST-svar inkluderer `"reboot_required": true`** — ændringer gemmes til NVS med det samme, men interfaces genindlæses først ved reboot.

**Eksempel:**
```bash
curl -X PUT http://10.1.32.101/api/v1/interfaces/0 \
  -H "Content-Type: application/json" \
  -d '{"baudrate": 19200, "timeout_ms": 500}'
```

---

### 4.6 Modbus function codes (FC01–FC10)

Alle FC-endpoints ligger under:
```
/api/v1/interfaces/{key}/slaves/{sid}/...
```
hvor `{sid}` er Modbus slave-adressen (1–247).

#### FC01 — Read Coils
```
GET /api/v1/interfaces/0/slaves/3/coils?start=0&count=8
```
```json
{ "interface": 0, "slave": 3, "function": 1, "start": 0, "count": 8,
  "coils": [true, false, false, true, false, false, false, false] }
```
Maks. `count`: 2000.

#### FC02 — Read Discrete Inputs
```
GET /api/v1/interfaces/0/slaves/3/discrete-inputs?start=0&count=8
```
```json
{ "interface": 0, "slave": 3, "function": 2, "start": 0, "count": 8,
  "inputs": [true, false, false, false, false, false, false, false] }
```
Maks. `count`: 2000.

#### FC03 — Read Holding Registers
```
GET /api/v1/interfaces/0/slaves/3/holding-registers?start=100&count=3
```
```json
{ "interface": 0, "slave": 3, "function": 3, "start": 100, "count": 3,
  "registers": [1234, 5678, 9012] }
```
Maks. `count`: 125.

#### FC04 — Read Input Registers
```
GET /api/v1/interfaces/0/slaves/3/input-registers?start=0&count=10
```
```json
{ "interface": 0, "slave": 3, "function": 4, "start": 0, "count": 10,
  "registers": [301, 302, 303, 0, 0, 0, 0, 0, 0, 0] }
```
Maks. `count`: 125.

#### FC05 — Write Single Coil
```
PUT /api/v1/interfaces/0/slaves/3/coils/5
Body: {"value": true}
```
```json
{ "interface": 0, "slave": 3, "coil": 5, "value": true }
```
En manglende/ugyldig `value` giver `400 Bad Request` (skriver ikke stille en fejlagtig værdi).

#### FC06 — Write Single Register
```
PUT /api/v1/interfaces/0/slaves/3/holding-registers/100
Body: {"value": 1234}
```
```json
{ "interface": 0, "slave": 3, "register": 100, "value": 1234 }
```

#### FC0F (15) — Write Multiple Coils
```
PUT /api/v1/interfaces/0/slaves/3/coils?start=0
Body: {"values": [true, false, true, true]}
```
```json
{ "interface": 0, "slave": 3, "start": 0, "count": 4 }
```
Maks. antal værdier: 1968.

#### FC10 (16) — Write Multiple Registers
```
PUT /api/v1/interfaces/0/slaves/3/holding-registers?start=100
Body: {"values": [1234, 5678, 9012]}
```
```json
{ "interface": 0, "slave": 3, "start": 100, "count": 3 }
```
Maks. antal værdier: 123.

**Alle otte endpoints** returnerer det [fælles fejl-format](#43-fejl-format) ved timeout/exception/fejl.

---

### 4.7 Interface loopback-selvtest

Bruges til at verificere TX→RX-stien på et interface uden slaver tilsluttet.

```
POST /api/v1/interfaces/{key}/selftest
Body: {"mode": "internal"}     // eller "external"
```

- **`internal`** — bruger ESP32'ens interne UART-loopback (ingen ledninger nødvendige). Kun hardware-UART master-interfaces.
- **`external`** — kræver en fysisk jumper (TX↔RX for RS232; A↔B-loop for RS485). Tester hele stien inkl. transceiveren.
- Software-UART og slave-mode-interfaces understøttes ikke (returnerer en klar fejlbesked).

**Svar:**
```json
{
  "interface": 0, "passed": true, "mode": "internal",
  "tx_bytes": 8, "rx_bytes": 8, "mismatches": 0,
  "duration_ms": 5, "detail": "OK — telegram modtaget retur på RX"
}
```

---

### 4.8 Register-cache

Gatewayen holder en lokal, TTL-baseret read-through cache pr. register/coil for at reducere bus-trafik.

| Metode | Endpoint | Beskrivelse |
|---|---|---|
| GET | `/api/v1/cache/stats` | Hits, misses, hit-rate, entries brugt, refresh-tællere |
| GET | `/api/v1/cache/entries` | Alle cache-entries (iface/slave/fc/addr/value/age) |
| GET | `/api/v1/cache/history` | 60 tidsserie-samples (til graf i GUI) |
| PUT | `/api/v1/cache/config` | `{"enabled":true,"ttl_ms":1000,"refresh_enabled":true,"refresh_interval_ms":200,"refresh_threshold_pct":75}` |
| POST | `/api/v1/cache/clear` | Tøm cachen (stats bevares) |
| POST | `/api/v1/cache/reset-stats` | Nulstil hit/miss-tællere |

**Eksempel — `GET /api/v1/cache/stats`:**
```json
{
  "enabled": true, "ttl_ms": 1000, "max_entries": 256, "entries_used": 42,
  "hits": 1204, "misses": 88, "hit_rate_pct": 93.2, "refresh_done": 310
}
```

---

### 4.9 Modbus-log (dekodet bus-trafik)

Ring-buffer (100 entries) over de faktiske Modbus-transaktioner på bussen — nyttig til fejlfinding.

```
GET /api/v1/modbus/log?since=N
```
```json
{
  "n": 542,
  "entries": [
    { "seq": 541, "t": 88213, "if": 0, "sl": 3, "fc": 3, "ad": 100, "ct": 3, "st": 0, "v": 1234 },
    { "seq": 542, "t": 88950, "if": 0, "sl": 5, "fc": 6, "ad": 10, "ct": 1, "st": 1 }
  ]
}
```
Felter: `if`=interface, `sl`=slave, `fc`=function code, `ad`=start-adresse, `ct`=antal, `st`=status (`0`=OK, `1`=timeout, `2`=exception, `3`=fejl), `ex`=exception-kode (hvis `st=2`), `v`=første register/coil-værdi (kun ved succes).

`POST /api/v1/modbus/log/clear` rydder loggen. Poll `since=<seneste seq>` for kun at hente nye entries (samme mønster som API-loggen).

---

### 4.10 API-kald-log

Ring-buffer (100 entries) over alle indkomne HTTP-kald til API'et.

```
GET /api/v1/system/log?since=N
```
```json
{ "n": 88, "entries": [ { "seq": 88, "t": 88213, "m": "GET", "u": "/api/v1/system" } ] }
```
`POST /api/v1/system/log/clear` rydder loggen.

---

### 4.11 OTA-opdatering

| Metode | Endpoint | Beskrivelse |
|---|---|---|
| GET | `/api/v1/system/ota/check` | Tjek GitHub Releases for nyere version |
| POST | `/api/v1/system/ota/firmware` | Start firmware-OTA (valgfri body: `{"url":"..."}`, ellers GitHub-latest) |
| POST | `/api/v1/system/ota/frontend` | Start frontend-OTA (SPIFFS-image) |
| GET | `/api/v1/system/ota/status` | Poll fremdrift (`state`, `progress_pct`) |

**Eksempel — `GET /api/v1/system/ota/check`:**
```json
{
  "current_version": "0.8.1", "build": "0094", "latest_version": "0.8.1-b0094",
  "firmware_available": false, "frontend_available": false
}
```

`POST`-kaldene svarer øjeblikkeligt (`{"status":"firmware_update_started"}`) — selve download/flash sker i baggrunden; poll `ota/status` for fremdrift. Ved succesfuld firmware-OTA genstarter enheden automatisk.

---

### 4.12 WebSocket

`GET /ws` (upgrade til WebSocket) er registreret, men er p.t. en **stub** (echo af modtagne beskeder) — real-time register-push er endnu ikke implementeret. Brug polling mod REST-endpoints (fx `modbus/log` eller FC-læse-endpoints) indtil videre.

---

### 4.13 Programmeringseksempler

**curl:**
```bash
# Læs 5 holding registers fra slave 3 på interface "floor1"
curl "http://10.1.32.101/api/v1/interfaces/floor1/slaves/3/holding-registers?start=0&count=5"

# Skriv register 10 = 500
curl -X PUT http://10.1.32.101/api/v1/interfaces/floor1/slaves/3/holding-registers/10 \
  -H "Content-Type: application/json" -d '{"value": 500}'
```

**Python (requests):**
```python
import requests

BASE = "http://10.1.32.101/api/v1"

def read_holding(iface, slave, start, count):
    r = requests.get(f"{BASE}/interfaces/{iface}/slaves/{slave}/holding-registers",
                      params={"start": start, "count": count})
    r.raise_for_status()
    return r.json()["registers"]

def write_register(iface, slave, addr, value):
    r = requests.put(f"{BASE}/interfaces/{iface}/slaves/{slave}/holding-registers/{addr}",
                      json={"value": value})
    r.raise_for_status()
    return r.json()

regs = read_holding("floor1", 3, 0, 5)
print(regs)
write_register("floor1", 3, 10, 500)
```

**JavaScript (fetch / Node 18+):**
```javascript
const BASE = "http://10.1.32.101/api/v1";

async function readCoils(iface, slave, start, count) {
  const r = await fetch(`${BASE}/interfaces/${iface}/slaves/${slave}/coils?start=${start}&count=${count}`);
  if (!r.ok) throw new Error((await r.json()).error);
  return (await r.json()).coils;
}

async function writeCoil(iface, slave, addr, value) {
  const r = await fetch(`${BASE}/interfaces/${iface}/slaves/${slave}/coils/${addr}`, {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ value }),
  });
  if (!r.ok) throw new Error((await r.json()).error);
  return r.json();
}
```

**Fejlhåndtering — generisk mønster (alle sprog):** tjek altid HTTP-status. `2xx` = succes, ellers parse JSON-body'ens `error`-felt (se [afsnit 4.3](#43-fejl-format)) for at skelne mellem timeout/exception/valideringsfejl og reagere derefter (fx retry ved timeout, ikke ved exception).

---

## 5. CLI — kort reference

Fuld reference: [CLI_MANUAL.md](CLI_MANUAL.md). Hurtig-oversigt:

| Kommando | Formål |
|---|---|
| `help` | Vis alle kommandoer |
| `show config` | Vis komplet gemt konfiguration |
| `status` | Live systemstatus |
| `show ethernet` / `show wifi` | Live netværksstatus |
| `eth ...` | Ethernet-konfiguration |
| `wifi ...` | WiFi-konfiguration |
| `configure terminal` → `interface modbus<N>` | Modbus-interface-konfiguration |
| `save` | Gem konfiguration til NVS |
| `reboot` | Genstart |

> Husk altid: `<konfigurér>` → `save` → `reboot` — ændringer i RAM er tabt ved genstart uden `save`.

---

## 6. Fejlfinding

| Symptom | Sandsynlig årsag | Handling |
|---|---|---|
| Ethernet-link flapper (`link UP`/`link DOWN` skiftevis) | Manglende ekstern pull-up på W5500 INT-pin (GPIO34–39 har ingen intern pull-up), eller dårlig GND-forbindelse | Tilføj 4.7–10 kΩ pull-up til 3.3V på INT-pin, eller sæt `eth int -1` (polling). Tjek GND mellem ESP32 og W5500-modul |
| Alle enheder har samme MAC-adresse / netværkskollisioner | Rettet i v0.8.1+ (W5500 fik tidligere aldrig tildelt en MAC) | Opdatér til v0.8.1 eller nyere |
| `GPIO_PIN mask error` i loggen ved boot | Et interface er konfigureret med `tx`/`rx` = `-1` (uden pins) | Tildel gyldige GPIO'er til interfacet, eller slet det (`no interface modbusN`) |
| REST-kald rammer forkert/ikke-eksisterende interface efter oprettelse/sletning | Ændringer kræver reboot for at træde i kraft | `save` + `reboot` efter enhver interface-CRUD |
| `modbus_timeout` ved alle kald til et interface | Forkert baudrate/paritet, DE-pin fungerer ikke, eller slave ikke tilsluttet | Kør loopback-selvtest (afsnit 4.7) for at isolere UART-siden fra bus-siden |
| Kun ét af flere HW-UART-interfaces virker | Forventet begrænsning — se [afsnit 2.8](#28-vigtig-begrænsning-hw-uart-masterslave) | Brug software-UART til interfaces udover 1 master + 1 slave |
| OTA "ingen opdatering tilgængelig" trods ny release | Enhedens rapporterede version matcher allerede (eller overstiger) releasen | Tjek `GET /api/v1/system` mod GitHub-tag'et |
| WiFi-status mangler periodisk i web-GUI | Kendt og rettet i v0.5.7+ | Opdatér firmware; statusfanen auto-opdaterer nu hvert 5. sekund |

---

## 7. Appendix

### 7.1 Modbus exception-koder

| Kode | Navn | Betydning |
|---|---|---|
| 1 | Illegal Function | Function code ikke understøttet af slave |
| 2 | Illegal Data Address | Register/coil-adresse findes ikke |
| 3 | Illegal Data Value | Ugyldig værdi i request |
| 4 | Slave Device Failure | Slave fejlede internt under udførelse |
| 5 | Acknowledge | Slave har accepteret men behandler stadig |
| 6 | Slave Device Busy | Slave optaget — prøv igen senere |
| 8 | Memory Parity Error | Parity-fejl i slavens hukommelse |
| 10 | Gateway Path Unavailable | (Modbus-gateway-specifik) |
| 11 | Gateway Target Device Failed To Respond | (Modbus-gateway-specifik) |

### 7.2 HTTP-statuskoder brugt af API'et

| Kode | Betydning i denne API |
|---|---|
| 200 OK | Succes |
| 201 Created | Interface oprettet |
| 400 Bad Request | Ugyldig body/parameter, eller Modbus-fejl |
| 401 Unauthorized | Manglende/forkert API-nøgle |
| 404 Not Found | Ukendt interface/route |
| 409 Conflict | Fx forsøg på at slette sidste interface, eller max interfaces nået |
| 502 Bad Gateway | GitHub utilgængelig (OTA-check) |
| 504 Gateway Timeout | Modbus-slave svarede ikke |

### 7.3 Versionshistorik

Se [CHANGELOG.md](CHANGELOG.md) (teknisk, pr. fil) og [RELEASE_NOTES.md](RELEASE_NOTES.md) (brugervenligt, pr. release) for den fulde historik. Kendte begrænsninger og løste fejl er sporet i [BUGS.md](BUGS.md); planlagte/færdige features i [FEATURES.md](FEATURES.md).
