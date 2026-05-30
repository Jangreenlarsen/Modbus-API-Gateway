# Release Notes

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
