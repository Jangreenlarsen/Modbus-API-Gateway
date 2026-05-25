# Modbus RTU Protokol-Reference

Kilde: Modbus Application Protocol Specification V1.1b3 (Modbus.org)  
Implementering: esp-modbus (Espressif) over RS485

---

## Frame-struktur (RTU)

```
┌──────────────┬───────────────┬──────────────────┬───────────┐
│ Slave Addr   │ Function Code │ Data             │ CRC-16    │
│ 1 byte       │ 1 byte        │ 0–252 bytes      │ 2 bytes   │
│ (1–247)      │               │                  │ (LE)      │
└──────────────┴───────────────┴──────────────────┴───────────┘
      ↑                                                  ↑
3.5 char silence before                      3.5 char silence after
```

- **Broadcast**: slave-adresse 0 (ingen svar forventes)
- **Max frame-størrelse**: 256 bytes
- **CRC**: CRC-16/IBM (polynom 0x8005, initial value 0xFFFF, little-endian i frame)

---

## Timing-krav

| Parameter                  | Krav                              |
|----------------------------|-----------------------------------|
| Frame-start silence        | ≥ 3.5 character times             |
| Max inter-character gap    | < 1.5 character times             |
| Response timeout           | Typisk 100–500 ms (konfigurerbar) |
| Turnaround delay (RS485)   | ≥ 3.5 character times             |

**Character time** ved 9600 bps = 1/960 s ≈ 1.042 ms (10 bits pr. char, 1 start + 8 data + 1 stop)

---

## Function Codes

### FC 0x01 — Read Coils
```
Request:  [addr] [0x01] [start_addr Hi] [start_addr Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Response: [addr] [0x01] [byte_count] [coil_data...] [CRC Lo] [CRC Hi]
```

### FC 0x02 — Read Discrete Inputs
```
Request:  [addr] [0x02] [start_addr Hi] [start_addr Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Response: [addr] [0x02] [byte_count] [input_data...] [CRC Lo] [CRC Hi]
```

### FC 0x03 — Read Holding Registers
```
Request:  [addr] [0x03] [start_addr Hi] [start_addr Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Response: [addr] [0x03] [byte_count] [reg_hi] [reg_lo] ... [CRC Lo] [CRC Hi]
Max qty: 125 registre pr. request
```

### FC 0x04 — Read Input Registers
```
Request:  [addr] [0x04] [start_addr Hi] [start_addr Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Response: [addr] [0x04] [byte_count] [reg_hi] [reg_lo] ... [CRC Lo] [CRC Hi]
Max qty: 125 registre pr. request
```

### FC 0x05 — Write Single Coil
```
Request:  [addr] [0x05] [coil_addr Hi] [coil_addr Lo] [0xFF/0x00] [0x00] [CRC Lo] [CRC Hi]
Response: Echo af request
ON = 0xFF00, OFF = 0x0000
```

### FC 0x06 — Write Single Register
```
Request:  [addr] [0x06] [reg_addr Hi] [reg_addr Lo] [value Hi] [value Lo] [CRC Lo] [CRC Hi]
Response: Echo af request
```

### FC 0x0F (15) — Write Multiple Coils
```
Request:  [addr] [0x0F] [start Hi] [start Lo] [qty Hi] [qty Lo] [byte_count] [data...] [CRC]
Response: [addr] [0x0F] [start Hi] [start Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Max qty: 1968 coils
```

### FC 0x10 (16) — Write Multiple Registers
```
Request:  [addr] [0x10] [start Hi] [start Lo] [qty Hi] [qty Lo] [byte_count] [data...] [CRC]
Response: [addr] [0x10] [start Hi] [start Lo] [qty Hi] [qty Lo] [CRC Lo] [CRC Hi]
Max qty: 123 registre
```

---

## Registertyper

| Type             | Prefix | Adresse-range | Størrelse | R/W        | Typisk brug                   |
|------------------|--------|---------------|-----------|------------|-------------------------------|
| Coil             | 0x     | 00001–09999   | 1 bit     | Read/Write | Digitale udgange (relæer)     |
| Discrete Input   | 1x     | 10001–19999   | 1 bit     | Read-only  | Digitale indgange             |
| Input Register   | 3x     | 30001–39999   | 16 bit    | Read-only  | Analoge målinger              |
| Holding Register | 4x     | 40001–49999   | 16 bit    | Read/Write | Konfiguration, setpunkter     |

> **Bemærk**: Protokolens wire-adresser er 0-baserede (0x0000–0xFFFF).  
> Bruger-adresser (vist i datablad) er typisk 1-baserede og med prefix.  
> Holding register 40001 = wire-adresse 0x0000.

---

## Exception Response

Hvis slave ikke kan udføre en request, returnerer den:
```
[addr] [FC | 0x80] [exception_code] [CRC Lo] [CRC Hi]
```

| Exception Code | Navn                          | Beskrivelse                              |
|---------------|-------------------------------|------------------------------------------|
| 0x01          | Illegal Function              | FC ikke understøttet af slave            |
| 0x02          | Illegal Data Address          | Registeradresse eksisterer ikke          |
| 0x03          | Illegal Data Value            | Ugyldig dataværdi i request              |
| 0x04          | Slave Device Failure          | Slave fejlede under udførelse            |

---

## RS485 Elektrisk Specifikation

| Parameter            | Værdi                              |
|---------------------|------------------------------------|
| Topologi            | Half-duplex, multi-drop, differentialt |
| Max enheder pr. bus | 247 slaves (standard)              |
| Max kabellængde     | 1200 meter (uden repeater)         |
| Baud-rates          | 1200 / 2400 / 4800 / 9600 / 19200 / 38400 / 57600 / 115200 bps |
| Terminering         | 120 Ω i begge ender af bus         |
| Signalniveauer      | Logic 1: A > B + 200 mV; Logic 0: B > A + 200 mV |
| Common-mode range   | -7V til +12V                       |

---

## esp-modbus Nøgle-API (Master RTU)

```c
// Initialiser Modbus master
mbc_master_init(MB_PORT_SERIAL_MASTER, &handler);
mb_communication_info_t comm = {
    .port     = UART_NUM_1,
    .mode     = MB_MODE_RTU,
    .baudrate = 9600,
    .parity   = MB_PARITY_NONE,
};
mbc_master_setup((void*)&comm);
mbc_master_set_descriptor(&device_parameters, num_devices);
mbc_master_start();

// Læs holding registers (FC 0x03)
mbc_master_get_parameter(param_index, param_key, value_buf, &type);

// Skriv holding register (FC 0x06/0x10)
mbc_master_set_parameter(param_index, param_key, value_buf, &type);
```

---

## Kendte gotchas

- **DE/RE pin timing**: MAX485 kræver DE=HIGH under send og DE=LOW under modtagelse. esp-modbus håndterer dette via konfigureret RTS-pin.
- **Silent interval**: Ved høje baud-rates (115200) er 3.5 character-time kun ~0.3 ms — sørg for at software-timers har tilstrækkelig opløsning.
- **Big-endian registre**: Modbus registre er big-endian (MSB først). ESP32 er little-endian — brug `__bswap16()` eller esp-modbus' type-system.
- **Broadcast skriv**: FC 0x10 kan sendes til adresse 0 — ingen response forventes; timeout opstår ikke (konfigurér `MB_SERIAL_RX_TOUT_TICKS`).
