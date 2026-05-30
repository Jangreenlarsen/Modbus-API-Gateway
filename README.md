# Modbus ↔ API Gateway

ESP32-baseret kommunikationsgateway der samler al RS485/RS232 Modbus RTU kommunikation ét sted og eksponerer den som et simpelt REST API over Ethernet.

---

## Problemet

Modbus RTU er en seriel protokol — kun én enhed kan kommunikere på bussen ad gangen, og hver forespørgsel tager tid (timeout, svar, pause). Når flere systemer skal have adgang til de samme Modbus-enheder opstår der problemer:

- **Node-RED**, **SCADA-servere**, **HMI-panels** og **dataloggers** konkurrerer om den samme serielle bus
- Hvert system skal selv håndtere timing, CRC, timeouts og fejlhåndtering
- Direkte Modbus-adgang fra mange klienter overbelaster bussen og giver kollisioner
- RS485-hardware skal fysisk tilsluttes på hvert system der skal have adgang

```
UDEN gateway — hvert system taler direkte med bussen:

  [Node-RED]──┐
  [SCADA]─────┼──► RS485 bus ──► [Slave 1] [Slave 2] [Slave 3]
  [HMI]───────┘
       ↑ konkurrence, kollisioner, duplikeret logik
```

---

## Løsningen

Modbus API Gateway sidder som den **eneste** Modbus-master på RS485/RS232 bussen. Den håndterer al seriel kommunikation og stiller dataene til rådighed for alle klienter via et standard HTTP REST API over Ethernet.

```
MED gateway — ét enkelt kontaktpunkt:

  [Node-RED]──┐
  [SCADA]─────┼──► REST over Ethernet ──► [ESP32 Gateway] ──► RS485 bus ──► [Slave 1]
  [HMI]───────┤                                          └──► RS485 bus ──► [Slave 2]
  [curl]──────┘                                          └──► RS232     ──► [Slave 3]
```

**Fordele:**
- Klienter bruger standard HTTP — ingen Modbus-biblioteker eller seriel hardware nødvendig
- Bussen beskyttes mod kollisioner — kun gatewayen sender på RS485
- Nem integration i Node-RED (`http request`-node), Python, JavaScript, Excel, SCADA osv.
- Konfiguration og monitorering via indbygget webgrænseflade

---

## Funktioner

- **Fuld Modbus RTU master OG slave** — alle standard function codes (FC01–FC10/FC0F). Hvert interface kan konfigureres som enten master (sender forespørgsler) eller slave (besvarer forespørgsler fra ekstern master)
- **Op til 8 RS485/RS232 interfaces** — 2 HW UART + 6 SW UART bit-bang (≤ 9600 baud)
- **Dynamisk tilføjelse/sletning af interfaces** — direkte fra web-GUI eller CLI
- **Navn-alias pr. interface** — referér til `floor1` eller `pumpestation` i URL i stedet for numerisk ID
- **REST API over Ethernet** — 1:1 mapping af Modbus-operationer til HTTP endpoints, fuld konfiguration via PUT
- **WebSocket** — real-time push til monitoreringsclients
- **Webgrænseflade** (`/mgmt`) — komplet konfiguration: navn, master/slave, slave-adresse, baudrate, paritet, stop bits, timeout, TX/RX/DE GPIO pins, RS485/RS232 type
- **CLI over UART0** — IOS-stil konfiguration (`configure terminal` → `interface modbus0`)
- **OTA firmware-opdatering** — via webgrænseflade og REST API (henter fra GitHub releases)

---

## REST API — hurtig oversigt

Base URL: `http://{gateway-ip}/api/v1`

`{key}` kan være enten numerisk ID (`0`, `1`, ...) eller navn-alias (`floor1`, `pumpestation`, ...) — case-insensitive.

### Modbus operationer
| Operation | HTTP | Endpoint | Modbus FC |
|-----------|------|----------|-----------|
| Læs coils | `GET` | `/interfaces/{key}/slaves/{sid}/coils?start=0&count=8` | FC01 |
| Læs digitale indgange | `GET` | `/interfaces/{key}/slaves/{sid}/discrete-inputs?start=0&count=8` | FC02 |
| Læs holding registers | `GET` | `/interfaces/{key}/slaves/{sid}/holding-registers?start=100&count=10` | FC03 |
| Læs input registers | `GET` | `/interfaces/{key}/slaves/{sid}/input-registers?start=0&count=5` | FC04 |
| Skriv enkelt coil | `PUT` | `/interfaces/{key}/slaves/{sid}/coils/{addr}` | FC05 |
| Skriv enkelt register | `PUT` | `/interfaces/{key}/slaves/{sid}/holding-registers/{addr}` | FC06 |
| Skriv flere coils | `PUT` | `/interfaces/{key}/slaves/{sid}/coils?start=0` | FC0F |
| Skriv flere registers | `PUT` | `/interfaces/{key}/slaves/{sid}/holding-registers?start=0` | FC10 |

### Interface-administration
| Operation | HTTP | Endpoint |
|-----------|------|----------|
| List alle interfaces | `GET` | `/interfaces` |
| Opret nyt interface | `POST` | `/interfaces` |
| Hent interface-config | `GET` | `/interfaces/{key}` |
| Opdater interface-config | `PUT` | `/interfaces/{key}` |
| Slet interface | `DELETE` | `/interfaces/{key}` |

**Eksempel — læs 10 holding registers fra slave 3 (med navn-alias):**
```bash
curl "http://192.168.1.100/api/v1/interfaces/floor1/slaves/3/holding-registers?start=100&count=10"
```
```json
{
  "interface": 0,
  "slave": 3,
  "function": 3,
  "start": 100,
  "count": 10,
  "registers": [1234, 5678, 0, 0, 9012, 0, 0, 0, 0, 42]
}
```

**Eksempel — opdater interface-konfiguration:**
```bash
curl -X PUT http://192.168.1.100/api/v1/interfaces/floor1 \
  -H "Content-Type: application/json" \
  -d '{"name":"basement","baudrate":19200,"mode":"slave","slave_addr":5}'
```

**Eksempel — Node-RED integration:**
En simpel `http request`-node med URL `http://{gateway-ip}/api/v1/interfaces/floor1/slaves/3/holding-registers?start=0&count=10` — ingen Modbus-node, ingen seriel port, ingen driver.

---

## Hardware

- **MCU**: ESP32 (Xtensa LX6 dual-core, 240 MHz)
- **Ethernet**: LAN8720 (RMII) eller W5500 (SPI)
- **RS485**: MAX485 / SN65HVD3082 transceiver pr. interface
- **RS232**: MAX232 / SP3232 transceiver pr. interface
- **Framework**: ESP-IDF v5.x

---

## Projekt-status

Under aktiv udvikling — se [FEATURES.md](FEATURES.md) for backlog og [CHANGELOG.md](CHANGELOG.md) for ændringer.

---

## Licens

[GNU Affero General Public License v3.0](LICENSE)
