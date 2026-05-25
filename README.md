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

- **Fuld Modbus RTU master** — understøtter alle standard function codes (FC01–FC10/FC0F)
- **x antal RS485/RS232 interfaces** — alle tilsluttet samme ESP32
- **REST API over Ethernet** — 1:1 mapping af Modbus-operationer til HTTP endpoints
- **Lokal datacache** — seneste kendte registerværdier gemt i flash (overlever strømfald)
- **WebSocket** — real-time push til monitoreringsclients
- **Webgrænseflade** — konfiguration og live monitorering direkte i browser
- **OTA firmware-opdatering** — via webgrænseflade

---

## REST API — hurtig oversigt

Base URL: `http://{gateway-ip}/api/v1`

| Operation | HTTP | Endpoint | Modbus FC |
|-----------|------|----------|-----------|
| Læs coils | `GET` | `/interfaces/0/slaves/3/coils?start=0&count=8` | FC01 |
| Læs digitale indgange | `GET` | `/interfaces/0/slaves/3/discrete-inputs?start=0&count=8` | FC02 |
| Læs holding registers | `GET` | `/interfaces/0/slaves/3/holding-registers?start=100&count=10` | FC03 |
| Læs input registers | `GET` | `/interfaces/0/slaves/3/input-registers?start=0&count=5` | FC04 |
| Skriv coil | `PUT` | `/interfaces/0/slaves/3/coils/0` | FC05 |
| Skriv register | `PUT` | `/interfaces/0/slaves/3/holding-registers/100` | FC06 |

**Eksempel — læs 10 holding registers fra slave 3:**
```bash
curl http://192.168.1.100/api/v1/interfaces/0/slaves/3/holding-registers?start=100&count=10
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

**Eksempel — Node-RED integration:**
En simpel `http request`-node med URL `http://{gateway-ip}/api/v1/interfaces/0/slaves/3/holding-registers?start=0&count=10` — ingen Modbus-node, ingen seriel port, ingen driver.

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
