# Arkitektur: Modbus API Gateway

## Systemformål

Fuld Modbus RTU master-funktionalitet eksponeret som REST API over Ethernet.
En REST-klient kan gøre præcis det samme som en native Modbus master-enhed.

---

## Arkitektur-oversigt

```
┌──────────────────────────────────────────────────────┐
│              Ethernet-klienter (HTTP)                 │
│   curl / browser / SCADA / PLC / automation tool      │
└────────────────────┬─────────────────────────────────┘
                     │ HTTP REST (TCP/IP over Ethernet)
┌────────────────────▼─────────────────────────────────┐
│              LAG 5 — API-lag (ESP32)                  │
│   esp_http_server  REST-routes  JSON-serialisering    │
│   WebSocket /ws  (real-time register-push)            │
└────────────────────┬─────────────────────────────────┘
                     │
┌────────────────────▼─────────────────────────────────┐
│            LAG 4 — Service-lag                        │
│   request-dispatcher  register-cache  config-mgr     │
└──────────┬──────────────────────────┬────────────────┘
           │                          │
┌──────────▼──────────┐   ┌──────────▼──────────────┐
│   LAG 3 — Modbus    │   │  LAG 3 — Storage         │
│   esp-modbus RTU    │   │  NVS (config)            │
│   master            │   │  SPIFFS (cache/historik) │
└──────────┬──────────┘   └──────────────────────────┘
           │ UART (RS485 / RS232)
┌──────────▼──────────────────────────────────────────┐
│   LAG 1+2 — Hardware (ESP32 UARTs + transceivers)   │
│   UART1+MAX485  UART2+MAX485  (UART1+MAX232 RS232)  │
└─────────────────────────────────────────────────────┘
```

---

## REST API — Endpoint-tabel (1:1 Modbus mapping)

Base URL: `http://{esp32-ip}/api/v1`

### Read operationer

| HTTP Method | Endpoint | Modbus FC | Beskrivelse |
|-------------|----------|-----------|-------------|
| `GET` | `/interfaces/{key}/slaves/{sid}/coils?start={n}&count={n}` | FC01 | Læs coils (1-bit R/W) |
| `GET` | `/interfaces/{key}/slaves/{sid}/discrete-inputs?start={n}&count={n}` | FC02 | Læs discrete inputs (1-bit RO) |
| `GET` | `/interfaces/{key}/slaves/{sid}/holding-registers?start={n}&count={n}` | FC03 | Læs holding registers (16-bit R/W) |
| `GET` | `/interfaces/{key}/slaves/{sid}/input-registers?start={n}&count={n}` | FC04 | Læs input registers (16-bit RO) |

### Write operationer

| HTTP Method | Endpoint | Modbus FC | Beskrivelse |
|-------------|----------|-----------|-------------|
| `PUT` | `/interfaces/{key}/slaves/{sid}/coils/{addr}` | FC05 | Skriv enkelt coil |
| `PUT` | `/interfaces/{key}/slaves/{sid}/holding-registers/{addr}` | FC06 | Skriv enkelt holding register |
| `PUT` | `/interfaces/{key}/slaves/{sid}/coils?start={n}` | FC0F (15) | Skriv multiple coils |
| `PUT` | `/interfaces/{key}/slaves/{sid}/holding-registers?start={n}` | FC10 (16) | Skriv multiple holding registers |

### Interface-administration

| HTTP Method | Endpoint | Beskrivelse |
|-------------|----------|-------------|
| `GET` | `/interfaces` | Liste over konfigurerede interfaces |
| `POST` | `/interfaces` | Opret nyt interface (SW-UART master defaults — body `{}`) |
| `GET` | `/interfaces/{key}` | Hent interface-konfiguration |
| `PUT` | `/interfaces/{key}` | Opdater interface-konfiguration (også bagudkompatibel: `/{key}/config`) |
| `DELETE` | `/interfaces/{key}` | Slet interface og renummerér resterende |

### System

| HTTP Method | Endpoint | Beskrivelse |
|-------------|----------|-------------|
| `GET` | `/system` | Firmware-version, uptime, Ethernet-IP, heap |
| `POST` | `/system/reboot` | Genstart ESP32 |
| `GET` | `/system/wifi` | WiFi status |
| `PUT` | `/system/wifi` | Konfigurér WiFi |
| `GET` | `/system/wifi/scan` | Scan WiFi-netværk |
| `GET` | `/system/ota/check` | Tjek for opdatering på GitHub |
| `POST` | `/system/ota/firmware` | Start firmware OTA |
| `POST` | `/system/ota/frontend` | Start frontend OTA |
| `GET` | `/system/ota/status` | OTA-progress |
| `WS` | `/ws` | WebSocket: real-time push ved register-ændringer (polling-cache) |

### URL-parametre

| Parameter | Type | Eksempel | Beskrivelse |
|-----------|------|---------|-------------|
| `{key}` | int eller string | `0` eller `floor1` | Interface-index ELLER navn-alias (case-insensitive) |
| `{sid}` | int 1..247 | `3` | Modbus slave-adresse |
| `{addr}` | int | `100` | Register/coil-adresse (0-baseret) |
| `start` | int | `0` | Start-adresse for range-læsning |
| `count` | int | `10` | Antal registers/coils |

### Implementeringsnote — master dispatcher
ESP-IDF's `httpd_uri_match_wildcard` accepterer kun `*` ved slutningen af et URI-mønster. Derfor er ALLE `/api/v1/interfaces/*` GET og PUT routes registreret på samme trailing-wildcard og dispatches af `master_get_dispatcher` / `master_put_dispatcher` i [interfaces.c](firmware/main/api/routes/interfaces.c) baseret på URI-suffix (`/slaves/N/<op>[/addr]`).

---

## JSON Response-format

### Read holding registers (FC03)
```json
GET /api/v1/interfaces/0/slaves/3/holding-registers?start=100&count=3

200 OK
{
  "interface": 0,
  "slave":     3,
  "function":  3,
  "start":     100,
  "count":     3,
  "registers": [1234, 5678, 9012],
  "timestamp": 1716638400
}
```

### Write single register (FC06)
```json
PUT /api/v1/interfaces/0/slaves/3/holding-registers/100
{ "value": 1234 }

200 OK
{ "interface": 0, "slave": 3, "register": 100, "value": 1234 }
```

### Modbus exception response
```json
400 Bad Request
{
  "error":          "modbus_exception",
  "exception_code": 2,
  "description":    "Illegal Data Address"
}
```

### Timeout / kommunikationsfejl
```json
504 Gateway Timeout
{
  "error":       "modbus_timeout",
  "interface":   0,
  "slave":       3,
  "description": "No response within 500ms"
}
```

---

## Lag-regler

1. **API-laget** må kun kalde service-laget. Det oversætter HTTP → intern request og intern result → JSON.
2. **Service-laget** dispatcher Modbus-requests og vedligeholder register-cachen. Det kalder Modbus-laget og storage-laget.
3. **Modbus-laget** udfører én RTU-transaktion ad gangen pr. interface (mutex pr. UART). Det kender intet til HTTP eller JSON.
4. **Storage-laget** persisterer konfiguration (NVS) og seneste kendte værdier (SPIFFS). Ingen direkte kobling til Modbus eller API.
5. **Frontend** taler udelukkende med API-laget via `js/api.js`.

---

## Interface-konfiguration (pr. UART-port)

```json
{
  "id":          0,
  "name":        "floor1",
  "type":        "RS485",
  "uart_mode":   "hw",
  "uart":        1,
  "mode":        "master",
  "slave_addr":  1,
  "baudrate":    9600,
  "data_bits":   8,
  "parity":      0,
  "stop_bits":   1,
  "timeout_ms":  500,
  "tx_pin":      17,
  "rx_pin":      16,
  "rts_pin":     4,
  "enabled":     true
}
```

| Felt | Værdier | Beskrivelse |
|------|---------|-------------|
| `name` | string ≤ 23 tegn | Brugervenligt alias — kan bruges i URL i stedet for `id` |
| `type` | `"RS485"` \| `"RS232"` | Elektrisk niveau |
| `uart_mode` | `"hw"` \| `"sw"` | HW UART (UART1/UART2, ≤ 115200 baud) eller SW bit-bang (≤ 9600 baud) |
| `mode` | `"master"` \| `"slave"` | Modbus-rolle — master sender forespørgsler, slave svarer på dem |
| `slave_addr` | 1–247 | Slave-adresse — kun relevant ved `mode=slave` |
| `parity` | 0 \| 1 \| 2 | 0=Ingen, 1=Ulige, 2=Lige |
| `tx_pin`, `rx_pin` | GPIO 0–39 eller -1 | UART pins (-1 = ikke konfigureret) |
| `rts_pin` | GPIO 0–39 eller -1 | DE/RE pin på RS485 transceiver (full-duplex RS232 bruger ikke denne) |

SW-UART slave-mode understøttes ikke (kræver custom bit-bang svar-implementering).

---

## RS485 vs RS232 — hardware-forskel

| Parameter | RS485 | RS232 |
|-----------|-------|-------|
| Duplex | Half-duplex | Full-duplex |
| Transceiver | MAX485 / SN65HVD3082 | MAX232 / SP3232 |
| DE/RE pin | Ja (RTS styrer) | Nej |
| Signalniveau | Differentielt ±1.5V | ±12V single-ended |
| UART mode | `UART_MODE_RS485_HALF_DUPLEX` | `UART_MODE_UART` |
| Max enheder | 247 pr. bus | Point-to-point (1:1) |
| Max afstand | 1200m | 15m |

---

## Projektstruktur

```
.
├── CLAUDE.md
├── version.json
├── ARCHITECTURE.md
├── MODBUS_REFERENCE.md
├── ESP32_REFERENCE.md
├── FEATURES.md
├── BUGS.md
├── CHANGELOG.md
├── RELEASE_NOTES.md
├── .claude/
│   └── settings.local.json
├── firmware/                        # ESP-IDF projekt
│   ├── CMakeLists.txt               # top-level cmake
│   ├── sdkconfig                    # ESP-IDF Kconfig (genereret)
│   ├── partitions.csv               # custom partition table
│   └── main/
│       ├── CMakeLists.txt
│       ├── main.c                   # app_main: init rækkefølge
│       ├── core/
│       │   ├── config.c/.h          # konfigurationsstrukturer + defaults
│       │   └── ethernet.c/.h        # Ethernet init (LAN8720/W5500)
│       ├── modbus/
│       │   ├── modbus_manager.c/.h  # koordinerer alle interfaces, mutex
│       │   └── interface.c/.h       # pr. UART-port: init, read, write
│       ├── storage/
│       │   ├── config_store.c/.h    # NVS: interface-konfiguration
│       │   └── register_cache.c/.h  # SPIFFS: seneste kendte værdier
│       ├── service/
│       │   └── gateway_service.c/.h # dispatcher: REST-request → Modbus-kald
│       └── api/
│           ├── server.c/.h          # httpd init + route-registration
│           ├── routes/
│           │   ├── coils.c/.h       # FC01, FC05, FC0F
│           │   ├── discrete.c/.h    # FC02
│           │   ├── holding_regs.c/.h # FC03, FC06, FC10
│           │   ├── input_regs.c/.h  # FC04
│           │   ├── interfaces.c/.h  # GET/PUT /interfaces
│           │   └── system.c/.h      # GET /system, POST /reboot
│           └── ws_handler.c/.h      # WebSocket /ws
└── frontend/
    ├── index.html
    ├── css/style.css
    └── js/
        ├── api.js                   # al HTTP/WS kommunikation
        ├── monitor.js               # real-time register-visning
        └── config.js                # interface-konfiguration
```
