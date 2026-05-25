# Features

Format: `[status] vX.X.X — beskrivelse`
Status: `planned` | `in-progress` | `done`

---

## Planlagte features

- [ ] planned — Basis Modbus RTU master på ét RS485-interface (UART1)
- [ ] planned — NVS-baseret konfigurationspersistens (baudrate, slave-adresser, polling-interval)
- [ ] planned — REST API: GET /api/interfaces og GET /api/interfaces/{id}/registers
- [ ] planned — Web frontend: monitoreringsside med live register-visning
- [ ] planned — WebSocket push ved register-ændringer
- [ ] planned — Web frontend: konfigurationsside (interface-parametre, slave-adresser)
- [ ] planned — Multi-interface support (UART1 + UART2)
- [ ] planned — SPIFFS-baseret data-backup (seneste kendte værdier ved strømfald)
- [x] done    — OTA firmware- og frontend-opdatering fra GitHub releases (REST API)
- [ ] planned — Alarm/threshold-logik med notifikation via WebSocket
