# Release Notes

---

## v0.5.0 build 0080 — 2026-07-11 — refactor: reelt service-lag

Arkitekturen følger nu ARCHITECTURE.md korrekt: API-laget kalder udelukkende service-laget, som dispatcher Modbus-operationer videre. Tidligere kaldte REST-routes Modbus-laget direkte, og service-laget var en tom stub.

Samtidig er interface-routing gjort konsistent:

- **REST-kald rammer altid det rigtige interface.** Tidligere kunne indeksering blive ude af sync efter oprettelse/sletning af interfaces indtil reboot. Routing sker nu mod den kørende konfiguration.
- **`reboot_required` i svar.** Når du opretter, ændrer eller sletter et interface, svarer API'et nu med `"reboot_required": true` — ændringen gemmes, men træder først i kraft efter genstart.
- **Hurtigere polling.** Hvert Modbus-REST-kald læste tidligere hele konfigurationen fra flash (NVS). Det sker ikke længere på læse/skrive-stien.

---

## v0.4.6 build 0079 — 2026-07-11 — fix: kritiske robusthedsfejl

Tre kritiske fejl fundet i kodegennemgang er rettet:

1. **Enheden kunne panikke ved konfigurations-gem** — hvis NVS-flashen var fuld eller korrupt, kunne et enkelt `PUT`-kald fra en klient trigge en reboot-loop. Nu returneres en fejl i stedet.
2. **Buffer-overflow-risiko på SW-UART** — en fejlbehæftet Modbus-slave kunne overskride en intern buffer ved coil/discrete-læsning. Antallet af kopierede bytes begrænses nu korrekt.
3. **Kun én HW-UART master understøttes** — esp-modbus deler en global controller, så flere HW-master-porte kolliderede lydløst. Ekstra HW-masters deaktiveres nu med en tydelig log-fejl; brug SW-UART til flere master-porte. Samtidig er interface-init gjort robust: én fejlende port stopper ikke resten.

---

## v0.4.5 build 0078 — 2026-05-31 — feat: live API log

Ny "API Log" tab i management-siden viser alle indgående HTTP-kald i realtid:

- **Live opdatering** hvert 1,5 sekund via `?since=N` polling — kun nye entries sendes
- **Metode-badges**: GET (blå), POST (grøn), PUT (gul), DELETE (rød)
- **Auto-scroll** til nyeste entry, stopper hvis du scroller op
- **Pause-knap** fryser visningen uden at stoppe logningen
- **Ryd-knap** nulstiller loggen på enheden

Implementeret via en `log_wrapper` i `server.c` der intercepter alle routes automatisk — ingen kode-ændringer i de individuelle handlers.

---

## v0.4.5 build 0077 — 2026-05-31 — fix: OTA Installer-knap virker nu

To bugs forhindrede OTA-installation fra at starte:

1. **Asset-navn matchede aldrig**: Firmwaren ledte efter `firmware.bin` men releases uploadede filer som `modbus-gateway-v0.4.5-b0076.bin` — så `firmware_url` var altid tom og knappen reagerede ikke.

2. **Progress-polling viste aldrig fremskridt**: OTA-tasken oprettede en lokal `status`-variabel og opdaterede den under flashing — men browseren pollede den globale `s_status` som aldrig blev rørt. Siden frøs i "Starter..." uanset hvad der foregik.

Begge bugs er nu fixet. OTA skulle køre end-to-end.

---

## v0.4.5 build 0076 — 2026-05-31 — Bootstrap-release: OTA version-check fix når frem

Enheder på ≤0.4.4-b0074 kan ikke detektere b0075 via OTA fordi den gamle `version_newer()` ignorerer `-bNNNN` suffix. Denne release bumper PATCH (0.4.4 → 0.4.5) så selv den gamle kode detekterer opdateringen.

Efter OTA til 0.4.5-b0076 fungerer build-nummer-sammenligning korrekt fremadrettet.

---

## v0.4.4 build 0075 — 2026-05-31 — fix: OTA version-check ignorerede build-nummer

OTA-tjekket viste altid "Firmware opdateret" selvom en nyere build fandtes på GitHub.

**Årsag**: GitHub release-tags bruger format `v0.4.4-b0074`. `sscanf` stoppede ved `-` og ignorerede build-nummeret — begge sider parsede til `0.4.4.0` → ingen forskel.

**Fix**: `version_newer()` parser nu `-bNNNN` suffix. Lokalt sammenlignes med `VERSION-bBUILD` så build-numre tæller med.

**Forventet adfærd nu**:
- v0.4.4-b0074 → v0.4.4-b0075: "Opdatering tilgængelig" ✓
- v0.4.4-b0075 → v0.4.4-b0075: "Firmware opdateret" ✓

---

## v0.4.4 build 0074 — 2026-05-31 — OTA-side med live fremskridt og auto-reconnect

### Hvad er nyt
- **OTA trin-indikatorer**: Fire synlige trin — Download → Flash → Genstart → Online — viser præcis hvor i processen firmware-opdateringen er
- **Live progress**: Procent-bjælke opdateres under download og flash
- **Auto-reconnect**: Siden venter automatisk på at enheden kommer online efter reboot — ingen manuel genindlæsning nødvendig
- **Ny version vises**: Når enheden er online igen, vises den nye version direkte på OTA-siden og header opdateres

### Tidligere oplevelse vs. nu
Tidligere: siden frøs i "Venter"-tilstand og kom aldrig tilbage — man måtte genindlæse manuelt og gætte om OTA lykkedes.
Nu: hele forløbet er synligt og siden vender selv tilbage med bekræftelse.

---

## v0.4.1 build 0067 — 2026-05-31 — Release: GPIO presets, board variant, OTA, cache fase 2

### Highlights
- **GPIO presets**: RS485 Config vælger automatisk korrekte GPIO-pins ved type-skift
- **Board variant**: Vælg 30-pin eller 38-pin ESP32 — gemmes i NVS
- **OTA version-fix**: Understøtter nu 4-komponent versioner (MAJOR.MINOR.PATCH.D)
- **Cache fase 2**: Async refresh-task holder hot data varmt + historisk metrics med SVG sparkline
- **W5500 latency-fix**: ISR-miss workaround eliminerer 700-1300ms pings

### OTA fra v0.4.0.x
Den installerede v0.4.0.x firmware genkender v0.4.1 som nyere og tilbyder OTA-opdatering via GitHub.

---

## v0.4.0.3 build 0066 — 2026-05-31 — GPIO presets + ESP32 board variant

RS485 Config-siden kender nu ESP32-boardet og foreslår korrekte GPIO-pins automatisk:

- **Board variant** (30-pin / 38-pin) vælges øverst i RS485 Config — gemmes i NVS og overlever reboot
- **Auto-preset**: Skifter man type (RS485 ↔ RS232), fyldes TX/RX/DE ud automatisk med sensible pins baseret på interface-nummer og board
- **GPIO Preset-knap**: Nulstiller GPIO-felter til standard for det aktuelle board og interface
- **Input-only validering**: Gem blokerer og advarer hvis GPIO 34-39 forsøges brugt som TX eller DE
- **TX max=33, DE max=33**: Input-felter begrænser nu synligt at kun output-capable GPIO (0-33) vælges til TX/DE
- **Status-side**: Viser board variant (30-pin / 38-pin)
- **API**: `GET/PUT /api/v1/system/hardware` returnerer board variant + komplet preset-tabel

**Preset-tabel 30-pin** (undgår W5500 standard-pins 12,13,14,23,33,34):
| Iface | TX | RX | DE (RS485) |
|-------|----|----|-----------|
| 0     | 17 | 16 | 4         |
| 1     | 25 | 26 | 27        |
| 2     | 21 | 22 | 19        |
| 3     | 32 | 35 | 15        |
| 4     |  5 | 36 | 18        |
| 5     |  2 | 39 |  0        |
| 6     | 15 | 34 |  4        |
| 7     | 18 | 38 | 19        |

**38-pin**: iface 5-7 bruger GPIO 37/38 (eksponeret på bred board).

---

## v0.4.0.2 build 0065 — 2026-05-31 — OTA-side: korrekt version-visning

OTA-siden viste gammel GitHub release-version som "Tilgængelig" selv når installeret firmware er nyere. "Tilgængelig" viser nu "—" når ingen opdatering er tilgængelig. GitHub release v0.4.0.1-b0064 oprettet.

---

## v0.4.0 build 0063 — 2026-05-31 — Cache fase 2: async refresh-task + historisk metrics

Den synkron cache fra v0.4.0 b0061 fungerer som read-through — første læsning rammer bus, efterfølgende læsninger inden for TTL serveres fra cache. **Men** når TTL udløber skal næste klient stadig vente på bus-svar.

Denne version løser det med en **baggrunds-refresh task** der holder hot data varmt:
- Scanner cache hver 200ms for entries hvor `age > TTL × 75%`
- Henter dem fra bus i baggrunden (op til 8 pr. cycle)
- Klienter ser konsistent ~0ms latency selv på data der ellers ville være udløbet

### Historisk metrics

`/mgmt` → Cache tab har nu et **Historisk performance** kort med:
- **SVG sparkline-graf** der viser de seneste 10 minutter (60 samples × 10s)
  - Blå linje: hit rate (0-100%)
  - Rød linje: requests/sekund
  - Grøn linje: refreshes/sekund
- Stats-tabel under grafen: senest målt hit rate, requests/s, refresh/s, gennemsnit, tidsvindue

REST endpoint:
```
GET /api/v1/cache/history    → array af 60 samples med {t, hits, miss, err, used, rfr}
```

Cumulative-counter format — klient beregner delta mellem samples for periode-rater.

### Refresh-konfiguration

| Parameter | Default | Range | CLI |
|-----------|---------|-------|-----|
| Refresh enabled | on | on/off | `cache refresh on\|off` |
| Scan interval | 200 ms | 50-60000 | `cache refresh interval <ms>` |
| Age threshold | 75% af TTL | 10-99% | `cache refresh threshold <pct>` |
| History sample interval | 10000 ms | 1000-600000 | (kun via REST) |

### Effekt
Med TTL=1000ms, threshold=75% og interval=200ms: en hot register-adresse refreshes ca. 1× per sekund. Klienter der poller hvert 100ms ser **alle** reads som cache-hits (~0ms latency) — bussen håndterer kun den ene refresh per sekund.

Refresh hjælper kun entries der bliver brugt — ubrugte entries bliver naturligt udløbet/evictet.

---

## v0.4.0 build 0061 — 2026-05-30 — Modbus register cache + monitoring

**Cache engine** inspireret af `Modbus_server_slave_ESP32`-projektet — eliminerer redundant bus-trafik når flere klienter spørger om samme register inden for TTL-vinduet.

### Funktioner
- Read-through synchronous cache: 256 entries × 16 bytes = 4 KB RAM
- TTL-baseret freshness (default 1000ms, 0 = aldrig udløb)
- LRU eviction når cache er fuld
- Per-entry: iface, slave, FC, addr, value, status, hits, age
- Mutex-beskyttet (thread-safe mellem httpd og modbus-tasks)
- Write-through på succes; invalidering ved fejl

### Cache tab i `/mgmt`
Ny tab med:
- **Stats-tabel**: enabled, TTL, entries / max, hits, misses, hit rate %, errors, evictions
- **Live entries-tabel**: alle aktive cache-entries med kolonner iface/slave/FC/addr/value/status/hits/age
- **Controls**: enable-toggle, TTL-input, Opdater / Nulstil stats / Tøm cache knapper

### REST endpoints
```
GET    /api/v1/cache/stats         → JSON med hits/misses/hit_rate/...
GET    /api/v1/cache/entries       → array af alle aktive entries
PUT    /api/v1/cache/config        → {"enabled":true,"ttl_ms":1000}
POST   /api/v1/cache/clear         → tøm cache
POST   /api/v1/cache/reset-stats   → nulstil tællere
```

### CLI
```
gw> show cache                     -- statistik
gw> cache enable / disable
gw> cache ttl <ms>
gw> cache clear
gw> cache reset-stats
gw> cache entries                  -- list alle entries
```

### Typisk effekt
Hvis 3 SCADA-klienter alle poller samme 10 holding registers hvert sekund, var det 30 bus-transaktioner/sek tidligere. Med TTL=1000ms bliver det 10/sek (kun 1 sæt friske reads, resten serveres fra cache). Hit rate på 90%+ er forventeligt ved typisk polling-mønster.

---

## v0.3.0 build 0060 — 2026-05-30 — REST API fuldt funktionel + navn-alias + GUI-config

**Stor milepæl:** Alle Modbus FC01-FC10 REST endpoints virker nu, alle interface-felter kan konfigureres fra web GUI, og hvert interface kan navngives og refereres til via navn i URL.

### Nye features

**Navn-alias pr. interface**  
Sæt et brugervenligt navn og brug det som URL-segment:
```bash
curl http://10.1.32.101/api/v1/interfaces/floor1/slaves/3/holding-registers?start=0&count=10
```
Konfigureres i:
- CLI: `gw(config-modbus0)# name floor1`
- Web GUI: `/mgmt` → RS485 Config → Navn-felt
- REST: `PUT /api/v1/interfaces/0 -d '{"name":"floor1"}'`

**Komplet interface-config i web GUI**  
`/mgmt` → RS485 Config tabben understøtter nu:
- Navn (API alias)
- Rolle (Master/Slave) — slave-adresse vises kun ved slave-rolle
- Type (RS485/RS232) — DE GPIO vises kun ved RS485
- Baudrate, Paritet, Stop bits, Timeout
- TX GPIO, RX GPIO, DE GPIO
- **+ Tilføj** og **− Slet** knapper (op til 8 interfaces)

**REST API: opret og slet interfaces**
```bash
# Opret nyt interface (defaults: SW-UART master)
curl -X POST http://10.1.32.101/api/v1/interfaces

# Slet interface og renummerér resterende
curl -X DELETE http://10.1.32.101/api/v1/interfaces/floor1
```

### Bug fixes

| Bug | Effekt |
|-----|--------|
| FC01-FC10 routes matchede aldrig (ESP-IDF httpd midt-wildcard) | **ALLE** Modbus REST-operationer var i praksis ubrugelige før — virker nu |
| PUT /interfaces/N/config returnerede 405 Method Not Allowed | "Gem" knappen i GUI virkede ikke |
| W5500 ~700-1300ms ping latency (ISR-miss bug) | Konsistent ~5ms ping efter workaround-task |
| W5500 init crash når både INT-pin og poll_period_ms blev sat | Boot-fejl ved interrupt-mode |
| mgmt-siden viste statiske "Indlæser..." labels | JS SyntaxError forhindrede al API-kald fra siden |

### Implementeringsdetalje
ESP-IDF's `httpd_uri_match_wildcard` behandler kun `*` ved slutningen af et URI-mønster — midt-stjerner er bogstavelige tegn. Alle `/api/v1/interfaces/*` GET og PUT registreres derfor på samme trailing-wildcard og dispatches af `master_get_dispatcher` / `master_put_dispatcher` i [interfaces.c](firmware/main/api/routes/interfaces.c) baseret på URI-suffix.

---

## v0.2.1 build 0053 — 2026-05-30 — W5500 som standard hardware-profil

W5500 SPI Ethernet er nu default-konfiguration ved `erase_nvs` / factory reset:

| Pin  | GPIO |
|------|------|
| CS   | 23   |
| MOSI | 13   |
| MISO | 12   |
| SCLK | 14   |
| RST  | 33   |
| INT  | 34   |
| Clock| 10 MHz |

---

## v0.2.1 build 0052 — 2026-05-30 — W5500 ydeevne-fix

**Problem:** Netværkslatency på 100–800ms (normalt < 5ms) på W5500 SPI Ethernet.

**Årsager og løsninger:**

1. **SPI clock 20 MHz → 10 MHz default**  
   Dupont-ledninger og prototypekabling kan ikke pålideligt drive 20 MHz SPI. Stille bitfejl forårsager TCP-retransmissions og framedrops. Ny default er 10 MHz. Kan justeres via CLI:
   ```
   configure terminal
   interface eth0
   spi-clock 8     ← forsigtig (langt kabel)
   spi-clock 10    ← standard default
   spi-clock 20    ← kun ved kort, skærmet kabel
   save
   reboot
   ```

2. **INT pin advarsel**  
   Uden INT pin kører W5500-driveren i polling-mode (~10ms per pakke-detektion). `show ethernet` viser nu tydelig ADVARSEL. Tilslut INT pin og konfigurer med `int <gpio>` for interrupt-drevet tilstand (< 1ms latency).

3. **HTTP socket-håndtering**  
   `lru_purge_enable` aktiveret — serveren frigiver automatisk den ældste forbindelse under pres i stedet for at afvise nye klienter.

---

## v0.2.0 build 0051 — 2026-05-30 — Modbus slave mode + dynamiske interfaces

**Modbus slave mode**  
Hvert Modbus interface kan nu konfigureres som enten **master** (sender forespørgsler) eller **slave** (besvarer forespørgsler fra en ekstern Modbus master). Slave mode kræver HW-UART (SW-UART understøttes endnu ikke).

CLI (i `configure terminal` → `interface modbus0`):
```
mode slave      — skift til slave rolle
mode master     — skift til master rolle
addr 5          — sæt slave-adresse (1–247)
```

Slaven eksponerer 128 holding-registre, 128 input-registre, 128 coils og 128 discrete inputs (alle initialiseret til 0).

**Dynamisk interface-håndtering**  
Tilføj og slet interfaces fra CLI uden at genkompilere:

```
interface modbus1   — opret nyt interface (SW-UART master, GPIO ikke sat)
no interface modbus1 — slet interface
```
Op til 8 interfaces i alt (2 HW-UART + 6 SW-UART). Nye interfaces henter TX/RX/DE GPIO-pins fra konfiguration.

---

## v0.1.0 build 0047 — 2026-05-30 — Web management side: http://ip/mgmt

Åbn `http://<gateway-ip>/mgmt` i en browser for at se:

**Status**  
System (version, uptime, heap), netværk (Ethernet + WiFi), Modbus interfaces overblik.

**OTA Opdatering**  
Tjek GitHub for ny firmware → installer med ét klik + progress bar.

**RS485 Config**  
Konfigurér baudrate, paritet, stop bits, timeout og aktiver/deaktiver pr. interface direkte fra browseren — gemmes i NVS.

Siden er embedded i firmware (ingen SPIFFS nødvendig) og er altid tilgængelig så længe API-serveren kører.

---

## v0.1.0 build 0046 — 2026-05-30 — W5500 SPI Ethernet driver

W5500 Ethernet virker nu. `ethernet.c` er omskrevet til at bruge konfigureret hardware-type:

- **LAN8720**: RMII intern MAC (uændret)
- **W5500**: SPI2 bus, 20 MHz, hardware RST-puls på konfigureret GPIO, INT-pin eller polling

`sdkconfig.defaults` opdateret med `CONFIG_ETH_SPI_ETHERNET_W5500=y` så det medfølger ved nye builds.

Uart-log ved W5500 init:
```
I ethernet: W5500 RST puls på GPIO 33
I ethernet: SPI2 bus: MOSI=13 MISO=12 SCLK=14 CS=23
I ethernet: W5500 SPI Ethernet initialiseret  INT=34  poll=nej
I ethernet: Got IP: 192.168.x.x
```

---

## v0.1.0 build 0045 — 2026-05-30 — fix: build-fejl + show config paste-kompatibilitet

- **Build-fejl:** `ethernet.h` manglede `#include <stdbool.h>` → `unknown type 'bool'`. Rettet.
- **show config:** `mode STA` fjernet (ingen tilsvarende CLI-kommando). AP PSK vises kun hvis kodeord er sat — ellers `! PSK (ingen — åbent netværk)` som kommentarlinje.

---

## v0.1.0 build 0044 — 2026-05-30 — show ethernet

Ny kommando `show ethernet` (alias `show eth`) viser detaljeret Ethernet-status:

```
gw> show ethernet
--------------------------------
Ethernet status
  Tilstand  : forbundet
  IP        : 192.168.1.100
  IP-mode   : dhcp
  Hardware  : LAN8720 (RMII)

LAN8720 RMII GPIO
  PHY addr  : 0
  MDC       : GPIO 23
  MDIO      : GPIO 18
  PHY RST   : ikke tilsluttet
--------------------------------
```

Viser PHY addr / MDC / MDIO / PHY-RST for LAN8720, eller CS / MOSI / MISO / SCLK / RST / INT for W5500.

---

## v0.1.0 build 0043 — 2026-05-30 — W5500 RST GPIO pin

W5500 hardware-reset pin kan nu konfigureres:

```
gw(config-eth)# rst 5        -- sæt RST GPIO til 5
gw(config-eth)# rst -1       -- deaktivér (-1 = ikke tilsluttet)
```

`show config` viser nu `rst <gpio>` som del af W5500-sektionen. Default er -1.

**OBS:** CONFIG_STRUCT_VERSION er bumped til 5 — gemt NVS-config nulstilles ved første boot efter flash.

---

## v0.1.0 build 0041 — 2026-05-30 — show status / version / wifi

Tre nye `show`-kommandoer:

```
gw> show status
--------------------------------
System
  Version   : v0.1.0 b0041
  Uptime    : 0d 00h 12m 34s
  Heap      : 234 KB fri

Netværk
  Ethernet  : ikke tilgængeligt
  WiFi      : forbundet  192.168.10.45  (MitSSID  -62 dBm)

API server
  Status    : kører  port 80
  Auth      : deaktiveret

Modbus
  Modbus0   : aktiv   RS485  9600B-8N1  UART1
--------------------------------

gw> show version
--------------------------------
Modbus API Gateway
  Version   : v0.1.0
  Build     : 0041
  ESP-IDF   : v5.5.0
  Chip      : ESP32  rev3  2 cores
  Flash     : ekstern SPI
  WiFi+BT   : WiFi
--------------------------------

gw> show wifi
--------------------------------
WiFi status
  Tilstand  : forbundet
  Mode      : klient (STA)
  MAC (STA) : AA:BB:CC:DD:EE:FF
  SSID      : MitNetværk
  IP        : 192.168.10.45
  RSSI      : -62 dBm
  Kanal     : 11
  Auth      : WPA2-PSK
  BSSID     : 00:FE:C8:73:7E:50
--------------------------------
```

`status` og `wifi status` er stadig gyldige som aliaser.

---

## v0.1.0 build 0040 — 2026-05-30 — API server config i CLI

Ny konfigurationssektion i `configure terminal`:

```
gw(config)# interface api
gw(config-api)# port 8080        ← skift HTTP port fra 80
gw(config-api)# auth on          ← kræv API nøgle
gw(config-api)# key MinNøgle     ← sæt nøglen
gw(config-api)# exit
gw(config)# save
```

Klienter sender `X-API-Key: MinNøgle` header. Auth er slået fra som standard.

**OBS:** Config-struct-version bumped (3→4) — NVS-config nulstilles ved første boot efter flash.

---

## v0.1.0 build 0039 — 2026-05-30 — status viser WiFi IP

`status`-kommandoen viser nu WiFi-tilstand og IP-adresse:
```
WiFi    : forbundet  192.168.10.45  (MitNetværk)
```

---

## v0.1.0 build 0038 — 2026-05-30 — fix: WiFi forbinder men får aldrig IP-adresse

**Problem:** Gateway associerede korrekt med AP (PSK OK, WLC viste klient tilkoblet), men modtog aldrig en IP-adresse via DHCP.

**Årsag:** Tidligere NVS-korruption (b0030-buggen) kunne efterlade `wifi.ip`-feltet med garbage-data. Init-koden tolkede det som en statisk IP-konfiguration og stoppede DHCP-klienten. WiFi-laget fungerede, men DHCP-laget var slukket.

**Fix:**
- IP-feltet valideres nu med `ip4addr_aton()` inden DHCP stoppes. Korrupt/ugyldig IP giver advarsel og falder automatisk til DHCP.
- `WIFI_EVENT_STA_CONNECTED` genstarter eksplicit DHCP-klienten efter 4-way handshake — DHCP kører garanteret uanset tidligere tilstand.

---

## v0.1.0 build 0037 — 2026-05-30 — show config viser PSK i clear text

`show config` viser nu WiFi-passwords i klartekst i stedet for `*** (sat)`. Gør det muligt at verificere at korrekt password er gemt i NVS.

---

## v0.1.0 build 0036 — 2026-05-30 — debug/no debug — runtime log-niveau styring

Nye CLI-kommandoer til at styre log-output uden genstart:

```
gw> debug           ← alt verbose (se alt)
gw> debug wifi      ← kun WiFi verbose
gw> debug <tag>     ← specifik komponent verbose
gw> no debug        ← alt stille (kun WARN + ERROR)
gw> no debug wifi   ← WiFi-komponenter stille
gw> debug ?         ← vis hjælp
```

Nyttigt ved fejlsøgning: tænd verbose på præcis det du undersøger, sluk igen når du er færdig — ingen genstart nødvendig.

---

## v0.1.0 build 0035 — 2026-05-29 — WiFi 30s backoff — CLI brugbar under reconnect

Efter 5 fejlede WiFi-forsøg venter gateway nu 30 sekunder inden næste forsøg. Terminalen viser én loglinje hvert 30. sekund i stedet for én hvert 3. sekund. CLI er fuldt brugbar mens WiFi reconnect foregår i baggrunden.

---

## v0.1.0 build 0034 — 2026-05-29 — fix: WiFi threshold WPA2_PSK for WLC-kompatibilitet

`WIFI_AUTH_WPA_PSK` threshold (b0031) inkluderede WPA1/TKIP-kapabiliteter i association request. Enterprise WLC'er med CCMP-only policy afviser dette under 4-way handshake → `4WAY_HANDSHAKE_TIMEOUT (reason 15)`. Threshold ændret tilbage til `WPA2_PSK` — ESP32 annoncerer kun WPA2/CCMP.

---

## v0.1.0 build 0033 — 2026-05-29 — fix: WiFi PMF — SA_QUERY_TIMEOUT på enterprise WLC

**Problem:** WiFi forbandt (auth → assoc → run) men fik aldrig en IP-adresse. WLC viste enheden som associated. Disconnect reason 205 = `SA_QUERY_TIMEOUT`.

**Årsag:** `pmf_cfg.capable = true` (tilføjet i b0031) signalerede til AP'en at ESP32 understøtter PMF (Protected Management Frames). Enterprise WLC'er starter herefter SA-query procedurer mod klienten. ESP32 timeouder på disse forespørgsler → forbindelsen droppes før WPA2-handshaken og DHCP kan afsluttes.

**Fix:** PMF-konfiguration fjernet. ESP32 annoncerer ikke PMF-støtte → WLC udfører ingen SA queries → normal WPA2-forbindelse.

---

## v0.1.0 build 0032 — 2026-05-29 — WiFi disconnect reason + factory-reset

**Ny kommando:** `factory-reset` — sletter al NVS-konfiguration og genstarter med fabriksindstillinger. Nyttigt ved korrupt config eller ved skift til ny opsætning.

**Bedre WiFi fejldiagnose:** Disconnect-loggen viser nu reason-koden:
```
W wifi_mgr: STA retry 1/5 (reason 15)
```
Nøgle reason-koder: **15 / 204 = forkert password**, 201 = AP ikke fundet, 202 = auth fejl, 200 = beacon timeout.

---

## v0.1.0 build 0031 — 2026-05-29 — fix: WiFi STA forbindelsesproblemer

**5 rettelser til WiFi STA:**

**1. Auth-mode threshold** — Gateway afviste WPA-only AP'er og nogle WPA2/WPA3 transition-mode AP'er fordi threshold var sat til `WPA2_PSK`. Ændret til `WPA_PSK` som accepterer WPA og stærkere.

**2. Gateway giver op aldrig** — Tidligere stoppede WiFi forsøg permanent efter 5 fejlede retries. På en gateway der booter FØR routeren er klar, betød det ingen WiFi uden reboot. Nu forsøger WiFi igen uendeligt — FAIL_BIT sættes stadig (for AP-fallback trigger), men forbindelsesforsøg fortsætter i baggrunden.

**3. Double-init crash** — `wifi_manager_reconfigure()` kaldte `wifi_manager_init()` som opretter WiFi-stack strukturer på ny. Andet opkald resulterede i fejl/crash fordi netifs og drivers allerede eksisterede. Rettet med `s_initialized` guard.

**4. State ved reconnect** — `WIFI_FAIL_BIT` blev sat men aldrig ryddet ved succesfuld forbindelse. Rettet så state korrekt vises som "forbundet" igen efter AP-fallback → router kommer online.

**5. AP-fallback mode-skift** — I ESP-IDF v5.x kræver APSTA-mode aktivering stop + start. Uden dette startede AP'en ikke korrekt. Rettet i `start_ap_fallback()`.

**Quoted SSID/password** — `configure terminal` → `interface wifi` → `ssid "Mit netværk"` understøtter nu anførselstegn så SSID og PSK med mellemrum gemmes korrekt.

---

## v0.1.0 build 0030 — 2026-05-29 — fix: gem-rutine korrupterede data

**Problem:** `save`-kommandoen gemte tilfældigt indhold i stedet for den aktuelle konfiguration.

**Årsag:** `gateway_config_t cfg` var stack-allokeret i `app_main()`. Når `app_main` returnerer (den har ingen `while(1)`-løkke — alle subsystemer kører som FreeRTOS-tasks), sletter ESP-IDF main-tasken og frigiver dens stack. CLI'ens interne pointer `s_cfg` pegede stadig på denne frigjorte stack-hukommelse. Enhver `show config`, `save`, `wifi ssid`, osv. efterfølgende læste/skrev korrupt/tilfældig hukommelse.

**Fix:** `cfg` i `main.c` er nu `static` — placeret i BSS-segmentet med levetid lig hele programmets kørsel.

---

## v0.1.0 build 0029 — 2026-05-29 — ETH GPIO: kun relevante pins for valgt type

`show config` og `?`-hjælp viser nu kun de GPIO-pins der er relevante for den valgte Ethernet-controller:

**LAN8720 valgt:**
```
Interface ETH0
 Enable
 Type LAN8720
 PHY-addr 0
 MDC      GPIO 23
 MDIO     GPIO 18
End interface ETH0
```

**W5500 valgt:**
```
Interface ETH0
 Enable
 Type W5500
 SPI-CS   GPIO 5
 SPI-MOSI GPIO 23
 SPI-MISO GPIO 19
 SPI-SCLK GPIO 18
End interface ETH0
```

**Ingen type valgt (`eth type none`):**
```
Interface ETH0
 Disable
 Type none
 IP dhcp
End interface ETH0
```

I configure mode (`conf t` → `interface eth0`) giver forkert type en klar fejl:
```
gw(config-eth0)# cs 5
Fejl: 'cs' er kun for W5500  (brug 'type w5500' først)
```

---

## v0.1.0 build 0028 — 2026-05-29 — fix: WiFi SSID altid vist + NVS struct-version

**Rettede problemer:**
- WiFi STA SSID vises nu altid i `show config` — tidligere skjult hvis feltet var tomt
- PSK vises som `*** (sat)` eller `(ikke sat)` — aldrig i klartekst, aldrig som garbage-tegn
- NVS-blob valideres nu mod `CONFIG_STRUCT_VERSION` — forældet config fra tidligere builds nulstilles automatisk til defaults i stedet for at indlæse med forkert feltoffset

**`show config` WIFI-blok ser nu sådan ud:**
```
Interface WIFI
 Enable
 mode STA
 SSID "(ikke sat)"
 PSK (ikke sat)
 IP dhcp
End interface WIFI
!
Interface WIFI-AP
 Disable
 SSID "ModbusGW-AUTO"
 PSK none (åben)
 IP 192.168.4.1
End interface WIFI-AP
```

> **Efter flash:** NVS nulstilles automatisk — konfigurér med `conf t` og `save`.

---

## v0.1.0 build 0027 — 2026-05-29 — Configure terminal + kontekst-sensitiv ?-hjælp

**`configure terminal`** (eller `conf t`) giver Cisco IOS-stil konfigurationstilstand med skiftende prompt:

```
gw> conf t
Konfigurationstilstand aktiv. '?' = hjælp, 'exit'/'end' = afslut.

gw(config)# ?
  interface eth0         -- Ethernet
  interface wifi         -- WiFi STA klient
  interface wifi-ap      -- WiFi AP hotspot fallback
  interface modbus<N>    -- Modbus interface N
  show                   -- vis komplet konfiguration
  save                   -- gem til NVS
  exit / end             -- forlad konfigurationstilstand

gw(config)# interface eth0

gw(config-eth0)# ?
  enable / disable       -- aktiver/deaktiver Ethernet
  type lan8720|w5500     -- hardware-type
  ip dhcp                -- DHCP
  ...

gw(config-eth0)# type lan8720
Type: LAN8720

gw(config-eth0)# exit

gw(config)# interface wifi

gw(config-wifi)# ssid MitNetværk
SSID: MitNetværk

gw(config-wifi)# psk HemmeligKode
PSK: sat (11 tegn)

gw(config-wifi)# exit

gw(config)# save
Gemt.

gw(config)# end
Forlader konfigurationstilstand. Husk: 'save' + 'reboot'.
gw>
```

**Kontekst-sensitiv `?`-hjælp** på alle niveauer:
- `eth ?` → alle eth subkommandoer
- `eth type ?` → `lan8720 | w5500`
- `wifi ssid ?` → `<SSID navn> (maks 32 tegn)`
- `?` i gw> prompten → vis alle kommandoer

---

## v0.1.0 build 0026 — 2026-05-29 — ETH0 komplet config: Enable, Type, GPIO pins

`show config` viser nu al Ethernet-konfiguration, og `eth` kommandoen understøtter alle parametre:

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

Eller ved W5500 SPI:
```
Interface ETH0
 Enable
 Type W5500
 SPI-CS   GPIO 5
 SPI-MOSI GPIO 23
 SPI-MISO GPIO 19
 SPI-SCLK GPIO 18
 SPI-INT  GPIO 26
 IP dhcp
End interface ETH0
```

**`eth` subkommandoer:** `enable`, `disable`, `type lan8720|w5500`, `dhcp`, `<ip> <gw> <mask>`, `phy-addr`, `mdc`, `mdio`, `phy-rst`, `cs`, `mosi`, `miso`, `sclk`, `int`

---

## v0.1.0 build 0025 — 2026-05-29 — CLI show config: IOS-stil running config

`show config` viser nu al konfiguration i Cisco IOS-stil blokformat:

```
!
Interface ETH0
 IP 192.168.1.100
 Gateway 192.168.1.1
 Netmask 255.255.255.0
End interface ETH0
!
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
!
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
!
```

---

## v0.1.0 build 0024 — 2026-05-29 — CLI show: komplet konfigurationsvisning

`show` kommandoen viser nu **al gemt konfiguration** opdelt i tre sektioner:

```
--------------------------------
ETHERNET
  IP      : 192.168.1.100
  Gateway : 192.168.1.1
  Netmask : 255.255.255.0

WIFI STA
  Aktiv   : ja
  SSID    : MitNetværk
  Password: *** (sat)
  IP      : dhcp
  Gateway : (dhcp)
  Netmask : (dhcp)

WIFI AP FALLBACK
  Aktiv   : ja
  SSID    : ModbusGW-XXXXXX (auto)
  Password: (åben)

MODBUS INTERFACES  (1 konfigureret)
  [0] RS485  HW  UART1
       Baud    : 9600
       Format  : 8N1  paritet=ingen
       Timeout : 500 ms
       Pins    : TX=17  RX=16  DE/RTS=4
       Status  : aktiv
--------------------------------
```

---

## v0.1.0 build 0023 — 2026-05-29 — API endpoint-oversigt på /api og /api/v1

`GET http://ip/api` eller `GET http://ip/api/v1/` returnerer nu en komplet liste over alle tilgængelige endpoints:

```json
{
  "api": "Modbus API Gateway",
  "version": "0.1.0",
  "build": "0023",
  "base": "/api/v1",
  "endpoints": [
    {"method": "GET",  "path": "/api/v1/system", "description": "System info..."},
    ...
  ]
}
```

Alle 21 endpoints er beskrevet med metode, sti og dansk beskrivelse. Fungerer som API-dokumentation direkte fra gatewayen.

---

## v0.1.0 build 0022 — 2026-05-29 — CLI: wifi status og wifi mode

To nye subkommandoer under `wifi`:

**`wifi status`** — live WiFi-status:
- Tilstand: deaktiveret / forbinder / forbundet / AP hotspot / fejl
- WiFi mode: klient (STA) / AP / klient+AP (APSTA)
- MAC-adresse (STA-interface)
- Ved forbundet: SSID, IP-adresse, signalstyrke (RSSI i dBm), kanal, kryptering (WPA2 etc.), BSSID
- Ved AP fallback: AP-netværksnavn, IP (192.168.4.1), AP MAC-adresse

**`wifi mode`** — kort tilstandsvisning:
- Viser om gatewayen kører som WiFi-klient, AP hotspot, eller begge (APSTA fallback)
- Viser SSID ved aktiv STA-forbindelse

---

## v0.1.0 build 0021 — 2026-05-29 — CLI: kommandohistorik og cursor-bevægelse

Serial CLI understøtter nu pile-taster fuldt ud:
- **↑/↓**: navigér de seneste 20 kommandoer
- **←/→**: flyt cursor inden i aktuel kommando
- **Home/End** og **Ctrl+A/E**: hop til linjens start/slut
- **Ctrl+W**: slet ord bagud
- **Ctrl+K**: slet til linjeslut

---

## v0.1.0 build 0020 — 2026-05-29 — Core dump deaktiveret

Core dump er deaktiveret (ingen coredump-partition i partitionstabellen). Reducerer espcoredump-komponentens overhead.

---

## v0.1.0 build 0019 — 2026-05-29 — Build-optimering: 134 KB flash sparet + hurtigere version-bumps

Firmware-image reduceret fra 77,5% (1219 KB) til 69,0% (1085 KB) — 134 KB frigjort. Ubrugte komponenter fjernet fra build: Bluetooth (Bluedroid + NimBLE), TLS 1.0/1.1, sjældne elliptiske kurver, AES-CCM, PKCS12, DHE-PSK, SLIP, PPP og overflødige SPI flash-drivere. Boya flash-chip nu korrekt identificeret (ingen boot-advarsel).

Version og build-nummer er flyttet til `version.h` (inkluderes kun af 4 filer mod tidligere 13). Fremtidige version-bumps recompilerer nu kun ~4 filer i stedet for alle 13 — byggetid for version-only ændringer: ca. 6 sekunder.

---

## v0.1.0 build 0018 — 2026-05-29 — CLI-prompt vises kun én gang

CLI-prompten `gw>` vises nu kun én gang efter hvert Enter — ikke to gange. Rod-årsag: ESP-IDF's standard UART RX-mode (`CR`) oversætter `\r`→`\n`, men Windows-terminaler sender `\r\n` (to tegn) som begge tolkes som Enter. Løst ved at skifte til `CRLF`-mode der konsumerer `\r\n` som ét enkelt `\n`.

---

## v0.1.0 build 0017 — 2026-05-28 — Stabil boot + Modbus starter korrekt

Gateway booter nu stabilt og Modbus RS485 starter korrekt ved hvert boot — også efter save+reboot. Rod-årsag til boot-loop var en ugyldig `uart_num=-1` i NVS-config fra en tidligere build. Ny `config_sanitize()` retter automatisk ugyldige værdier ved load. `mb_interface_init` er nu fuldt non-fatal — ingen `ESP_ERROR_CHECK` der kan forårsage panic.

CLI testet og verificeret: `help`, `status`, `show`, `eth`, `wifi`, `save`, `reboot` — alle fungerer korrekt.

---

## v0.1.0 build 0015 — 2026-05-28 — Stabil boot efter save+reboot

Gateway booter nu stabilt selv efter save+reboot. Alle tidligere årsager til boot-loop er rettet: NVS-fejl håndteres gracefully, Modbus/API init-fejl forårsager ikke længere panic, og UART RS485-mode sættes i korrekt rækkefølge.

---

## v0.1.0 build 0014 — 2026-05-27 — Serial CLI fungerer nu korrekt

Serial CLI blokkerer nu korrekt på brugerinput — ingen "gw>" prompt-spam mere. Rod-årsag: `esp_console_init()` installerer ikke UART-driveren i ESP-IDF v5.x, så stdin kørte non-blocking. Løst ved at bruge `esp_console_new_repl_uart()` som er den korrekte v5.x API.

Du kan nu bruge CLI normalt: `help`, `show`, `status`, `wifi`, `eth`, `save`, `reboot`.

---

## v0.1.0 build 0013 — 2026-05-27 — Boot uden Ethernet + version i CLI

Gateway booter nu stabilt selv uden Ethernet PHY tilsluttet. Ved manglende PHY logges en advarsel og systemet kører videre på WiFi alene — ingen reboot-loop.

Serial CLI boot-display og `status`-kommando viser nu fuld version: `v0.1.0 b0013`.

---

## v0.1.0 build 0012 — 2026-05-27 — Hurtigere build

Bluetooth, TLS-server og IPv6 er nu deaktiveret i `sdkconfig.defaults`. Build-tid falder fra over 5 minutter til ~4.3 minutter. Flash-footprint reduceret.

---

## v0.1.0 build 0009 — 2026-05-27 — Kompilerer rent med PlatformIO

Alle kompileringsfejl under ESP-IDF v5.5 + PlatformIO er rettet. `pio run` giver nu `[SUCCESS]`.

---

## v0.1.0 build 0008 — 2026-05-27 — PlatformIO support

Projektet bygger nu med PlatformIO IDE i VS Code.

**Kom i gang:**
```bash
# Installer PlatformIO IDE extension i VS Code
# Åbn projektet — PlatformIO genkender automatisk platformio.ini

# Byg
pio run

# Flash firmware
pio run -t upload

# Upload web frontend til SPIFFS
pio run -t uploadfs

# Serial monitor (Ctrl+C for at afslutte)
pio device monitor
```

**Første gang:**
PlatformIO downloader automatisk ESP-IDF toolchain og `espressif/esp-modbus` komponenten. Det tager nogle minutter første gang.

---

## v0.1.0 build 0007 — 2026-05-25 — Fix: WiFi statisk IP

WiFi statisk IP-konfiguration virkede ikke — gatewayen brugte altid DHCP uanset hvad der var konfigureret. Rettet nu.

**WiFi IP-konfiguration:**
- **DHCP** (standard): sæt `ip` til `"dhcp"` eller lad feltet være tomt
- **Statisk IP**: udfyld `ip`, `gw` og `netmask` — DHCP-klienten deaktiveres automatisk

Via CLI:
```bash
# DHCP (standard)
mbgw wifi set --ssid MitNet --password s3cr3t

# Statisk IP
mbgw wifi set --ssid MitNet --password s3cr3t --ip 192.168.1.50
# (gw og netmask konfigureres via web-frontend eller direkte via PUT /api/v1/system/wifi)
```

---

## v0.1.0 build 0006 — 2026-05-25 — CLI-værktøj (mbgw)

Terminal-CLI til direkte konfiguration og testning af gatewayen — ingen browser nødvendig.

**Installation:**
```bash
cd cli
pip install -r requirements.txt
# Kør direkte:
python mbgw.py --help
# Eller installér globalt som 'mbgw':
pip install -e .
```

**Hurtig start:**
```bash
# Gem gateway-IP én gang
mbgw config set host 192.168.1.100

# Tjek system-status
mbgw status

# Konfigurer WiFi
mbgw wifi scan
mbgw wifi set --ssid MitNetværk --password hemmeligt

# Læs 10 holding registers fra slave 1
mbgw read holding 0 1 0 10

# Skriv værdi 1234 til register 100
mbgw write holding 0 1 100 1234

# Tjek og opdater firmware
mbgw ota check
mbgw ota firmware
```

**JSON-output til scripting:**
```bash
mbgw --json status | python -c "import json,sys; d=json.load(sys.stdin); print(d['ip'])"
mbgw --json read holding 0 1 0 5 | jq '.registers[]'
```

---

## v0.1.0 build 0005 — 2026-05-25 — WiFi STA/AP support + komplet web frontend

Gatewayen har nu fuldt WiFi-understøttelse og en komplet web-brugergrænseflade tilgængelig direkte fra browseren.

**WiFi:**
- STA-tilstand med automatisk AP-fallback hotspot (`ModbusGW-XXXXXX`) hvis STA ikke kan oprette forbindelse efter 5 forsøg
- Konfigureres og genanvendes via REST API — ingen genstart nødvendig
- WiFi scan returnerer netværksliste med RSSI, kanal og åben/lukket status

**Web frontend (SPIFFS):**
- **Status-side**: System-info (version, uptime, heap, IP), WiFi-status, Modbus interface-kort med HW/SW badge, on-demand register-læser
- **Trend-side**: Enkelt-register trend med Chart.js (min/max/avg-statistik) + multi-register sammenligning
- **Log-side**: Live log-viewer med niveau-filter (I/W/E), OTA check og opdateringstrigger med progress bar
- **Indstillinger-side**: Ethernet, WiFi (scan-knap, netværksvælger, AP-fallback), Modbus interface-cards (baudrate, paritet, GPIO pins, HW/SW mode), tilføj/fjern interface, genstart

**Eksempel — WiFi-konfiguration:**
```bash
# Tjek WiFi-status
curl http://192.168.1.100/api/v1/system/wifi

# Konfigurer og aktivér WiFi
curl -X PUT http://192.168.1.100/api/v1/system/wifi \
  -H 'Content-Type: application/json' \
  -d '{"enabled":true,"ssid":"MyNetwork","password":"secret","ap_fallback":true}'

# Scan efter netværk
curl http://192.168.1.100/api/v1/system/wifi/scan
```

---

## v0.1.0 build 0003 — 2026-05-25 — OTA opdatering fra GitHub releases

Firmware og frontend kan nu opdateres direkte fra GitHub releases over Ethernet — ingen USB-kabel nødvendig.

**Sådan virker det:**
1. Tag en ny GitHub release med `firmware.bin` og `frontend.bin` som assets
2. Kald `GET /api/v1/system/ota/check` — gatewayen sammenligner nuværende version med seneste release
3. Kald `POST /api/v1/system/ota/firmware` for at starte firmware-opdatering (genstarter automatisk)
4. Kald `POST /api/v1/system/ota/frontend` for at opdatere webgrænsefladen på SPIFFS
5. Følg fremdrift via `GET /api/v1/system/ota/status`

**Eksempel:**
```bash
# Tjek om opdatering er tilgængelig
curl http://192.168.1.100/api/v1/system/ota/check

# Start firmware-opdatering
curl -X POST http://192.168.1.100/api/v1/system/ota/firmware

# Følg status
curl http://192.168.1.100/api/v1/system/ota/status
```

---

## v0.1.0 — 2026-05-25 — Projektinitialisering

Projektstruktur og dokumentation oprettet.

- Komplet projektdefinition i `CLAUDE.md` med versioneringsregler, workflow og arkitekturprincipper
- Lagdelt arkitektur defineret: Hardware → Modbus RTU → Storage/Service → API → Frontend
- Modbus RTU protokolreference inkl. alle function codes, register-typer og RS485-specifikation
- ESP32/ESP-IDF reference inkl. UART-konfiguration, NVS, SPIFFS, HTTP-server og WebSocket
- Feature-backlog oprettet med 10 planlagte features

**Næste skridt**: Opret ESP-IDF projekt i `firmware/` og implementer basis Modbus RTU master.
