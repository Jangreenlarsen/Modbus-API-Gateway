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
| `GET` | `/interfaces/{iface}/slaves/{addr}/coils?start={n}&count={n}` | FC01 | Læs coils (1-bit R/W) |
| `GET` | `/interfaces/{iface}/slaves/{addr}/discrete-inputs?start={n}&count={n}` | FC02 | Læs discrete inputs (1-bit RO) |
| `GET` | `/interfaces/{iface}/slaves/{addr}/holding-registers?start={n}&count={n}` | FC03 | Læs holding registers (16-bit R/W) |
| `GET` | `/interfaces/{iface}/slaves/{addr}/input-registers?start={n}&count={n}` | FC04 | Læs input registers (16-bit RO) |

### Write operationer

| HTTP Method | Endpoint | Modbus FC | Beskrivelse |
|-------------|----------|-----------|-------------|
| `PUT` | `/interfaces/{iface}/slaves/{addr}/coils/{reg}` | FC05 | Skriv enkelt coil |
| `PUT` | `/interfaces/{iface}/slaves/{addr}/holding-registers/{reg}` | FC06 | Skriv enkelt holding register |
| `PUT` | `/interfaces/{iface}/slaves/{addr}/coils?start={n}` | FC0F (15) | Skriv multiple coils |
| `PUT` | `/interfaces/{iface}/slaves/{addr}/holding-registers?start={n}` | FC10 (16) | Skriv multiple holding registers |

### System

| HTTP Method | Endpoint | Beskrivelse |
|-------------|----------|-------------|
| `GET` | `/interfaces` | Liste over konfigurerede interfaces |
| `GET` | `/interfaces/{iface}` | Interface-status (baudrate, parity, tilsluttede slaves) |
| `PUT` | `/interfaces/{iface}/config` | Opdater interface-konfiguration |
| `GET` | `/system` | Firmware-version, uptime, Ethernet-IP, heap |
| `POST` | `/system/reboot` | Genstart ESP32 |
| `WS` | `/ws` | WebSocket: real-time push ved register-ændringer (polling-cache) |

### URL-parametre

| Parameter | Type | Eksempel | Beskrivelse |
|-----------|------|---------|-------------|
| `{iface}` | int 0..N | `0` | Interface-index (UART-port) |
| `{addr}` | int 1..247 | `3` | Modbus slave-adresse |
| `{reg}` | int | `100` | Register/coil-adresse (0-baseret) |
| `start` | int | `0` | Start-adresse for range-læsning |
| `count` | int | `10` | Antal registers/coils |

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
  "type":        "RS485",
  "uart":        1,
  "baudrate":    9600,
  "data_bits":   8,
  "parity":      "none",
  "stop_bits":   1,
  "timeout_ms":  500,
  "tx_pin":      17,
  "rx_pin":      16,
  "rts_pin":     4
}
```

RS232-interface er identisk men `"type": "RS232"` og ingen `rts_pin` (full-duplex).

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
