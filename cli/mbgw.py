#!/usr/bin/env python3
"""mbgw — Modbus API Gateway CLI

Eksempler:
  mbgw config set host 192.168.1.100
  mbgw status
  mbgw wifi scan
  mbgw wifi set --ssid MitNetværk --password hemmeligt
  mbgw iface list
  mbgw read holding 0 1 0 10
  mbgw write holding 0 1 100 1234
  mbgw ota check
"""

import click
import requests
import json
import sys
import time
from pathlib import Path

CONFIG_FILE = Path.home() / ".mbgw.json"
BASE_TIMEOUT = 8
OTA_TIMEOUT  = 20


# ── Config persistence ─────────────────────────────────────────────────────────

def _load_cfg():
    if CONFIG_FILE.exists():
        try:
            return json.loads(CONFIG_FILE.read_text())
        except Exception:
            return {}
    return {}


def _save_cfg(cfg):
    CONFIG_FILE.write_text(json.dumps(cfg, indent=2))


def _base_url(host, port):
    cfg = _load_cfg()
    h = host or cfg.get("host", "192.168.1.100")
    p = int(port or cfg.get("port", 80))
    return f"http://{h}:{p}/api/v1"


# ── HTTP helpers ───────────────────────────────────────────────────────────────

def _get(base, path, timeout=BASE_TIMEOUT):
    r = requests.get(f"{base}{path}", timeout=timeout)
    r.raise_for_status()
    return r.json()


def _put(base, path, data, timeout=BASE_TIMEOUT):
    r = requests.put(f"{base}{path}", json=data, timeout=timeout)
    r.raise_for_status()
    return r.json() if r.content else {}


def _post(base, path, data=None, timeout=OTA_TIMEOUT):
    r = requests.post(f"{base}{path}", json=data, timeout=timeout)
    r.raise_for_status()
    return r.json() if r.content else {}


def _err(msg):
    click.echo(click.style(f"Fejl: {msg}", fg="red"), err=True)
    sys.exit(1)


def _ok(msg):
    click.echo(click.style(msg, fg="green"))


# ── Root group ─────────────────────────────────────────────────────────────────

@click.group(context_settings={"help_option_names": ["-h", "--help"]})
@click.option("--host", "-H", default=None, metavar="IP",
              help="Gateway IP-adresse (default: gemt eller 192.168.1.100)")
@click.option("--port", "-P", default=None, type=int, metavar="PORT",
              help="Gateway TCP-port (default: gemt eller 80)")
@click.option("--json", "as_json", is_flag=True, default=False,
              help="Rå JSON-output (til scripting)")
@click.pass_context
def cli(ctx, host, port, as_json):
    """mbgw — Modbus API Gateway konfigurationsværktøj."""
    ctx.ensure_object(dict)
    ctx.obj["base"]    = _base_url(host, port)
    ctx.obj["as_json"] = as_json


# ── config ─────────────────────────────────────────────────────────────────────

@cli.group()
def config():
    """Gem standardindstillinger lokalt (~/.mbgw.json)."""


@config.command("set")
@click.argument("key", metavar="KEY")
@click.argument("value", metavar="VALUE")
def config_set(key, value):
    """Gem en nøgle-værdi (host, port).

    \b
    Eksempel:
      mbgw config set host 192.168.1.100
      mbgw config set port 8080
    """
    cfg = _load_cfg()
    cfg[key] = value
    _save_cfg(cfg)
    _ok(f"Gemt: {key} = {value}")


@config.command("show")
def config_show():
    """Vis gemte standardindstillinger."""
    cfg = _load_cfg()
    if not cfg:
        click.echo("Ingen gemte indstillinger. Brug 'mbgw config set host <IP>'.")
        return
    for k, v in cfg.items():
        click.echo(f"  {k:<12}: {v}")


# ── status ─────────────────────────────────────────────────────────────────────

@cli.command()
@click.pass_context
def status(ctx):
    """Vis system-status (version, uptime, heap, IP)."""
    try:
        d = _get(ctx.obj["base"], "/system")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return

    uptime_s = d.get("uptime_s", 0)
    h, rem   = divmod(uptime_s, 3600)
    m, s     = divmod(rem, 60)

    click.echo(f"  Version  : {d.get('version','?')}  build {d.get('build','?')}")
    click.echo(f"  Uptime   : {h:02d}:{m:02d}:{s:02d}  ({uptime_s}s)")
    click.echo(f"  IP       : {d.get('ip','?')}")
    click.echo(f"  Heap fri : {int(d.get('free_heap',0))//1024} KB")
    click.echo(f"  Reset    : {d.get('reset_reason','?')}")
    click.echo(f"  Interfaces: {d.get('interface_count','?')} aktive")


# ── reboot ─────────────────────────────────────────────────────────────────────

@cli.command()
@click.confirmation_option(prompt="Genstart gatewayen?")
@click.pass_context
def reboot(ctx):
    """Genstart gateway."""
    try:
        _post(ctx.obj["base"], "/system/reboot", timeout=4)
    except Exception:
        pass  # connection reset forventes
    click.echo("Gateway genstarter…")


# ── wifi ───────────────────────────────────────────────────────────────────────

@cli.group()
def wifi():
    """WiFi-konfiguration og status."""


@wifi.command("status")
@click.pass_context
def wifi_status(ctx):
    """Vis nuværende WiFi-status."""
    try:
        d = _get(ctx.obj["base"], "/system/wifi")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return

    state = d.get("state", "?")
    color = "green" if state == "connected" else "yellow" if state == "connecting" else "red"
    click.echo(f"  State    : {click.style(state, fg=color)}")
    click.echo(f"  SSID     : {d.get('ssid') or '—'}")
    click.echo(f"  IP       : {d.get('ip') or '—'}")
    click.echo(f"  RSSI     : {d.get('rssi', 0)} dBm")
    ap = d.get("ap_active", False)
    click.echo(f"  AP aktiv : {'Ja — ' + str(d.get('ap_ssid','')) if ap else 'Nej'}")


@wifi.command("scan")
@click.pass_context
def wifi_scan(ctx):
    """Scan efter tilgængelige WiFi-netværk."""
    click.echo("Scanner…", err=True)
    try:
        networks = _get(ctx.obj["base"], "/system/wifi/scan", timeout=15)
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(networks, indent=2))
        return

    if not networks:
        click.echo("Ingen netværk fundet.")
        return

    networks.sort(key=lambda x: x.get("rssi", -100), reverse=True)
    click.echo(f"\n  {'#':>2}  {'SSID':<32}  {'RSSI':>5}  {'CH':>3}  Åben")
    click.echo("  " + "-" * 54)
    for i, n in enumerate(networks, 1):
        open_mark = click.style("Ja", fg="yellow") if n.get("open") else "Nej"
        click.echo(f"  {i:>2}  {n.get('ssid','?'):<32}  {n.get('rssi',0):>4}  {n.get('channel',0):>4}  {open_mark}")


@wifi.command("set")
@click.option("--ssid",        required=True,  help="Netværksnavn (SSID)")
@click.option("--password",    default="",     help="Adgangskode (tom = åbent netværk)")
@click.option("--ip",          default="dhcp", show_default=True,
              help="Statisk IP eller 'dhcp'")
@click.option("--ap-fallback/--no-ap-fallback", default=True, show_default=True,
              help="Aktivér AP-fallback hotspot ved forbindelsesfejl")
@click.option("--ap-ssid",     default="",     help="AP hotspot SSID (blank = auto)")
@click.option("--ap-password", default="",     help="AP hotspot adgangskode")
@click.pass_context
def wifi_set(ctx, ssid, password, ip, ap_fallback, ap_ssid, ap_password):
    """Konfigurer og aktivér WiFi STA-tilstand.

    \b
    Eksempel:
      mbgw wifi set --ssid MitNet --password hemmeligt
      mbgw wifi set --ssid MitNet --password s3cr3t --ip 192.168.1.50
    """
    payload = {
        "enabled":     True,
        "ssid":        ssid,
        "password":    password,
        "ip":          ip,
        "ap_fallback": ap_fallback,
        "ap_ssid":     ap_ssid,
        "ap_password": ap_password,
    }
    try:
        _put(ctx.obj["base"], "/system/wifi", payload)
    except Exception as e:
        _err(e)
    _ok(f"WiFi konfigureret: {ssid}  (AP-fallback: {'ja' if ap_fallback else 'nej'})")


@wifi.command("disable")
@click.confirmation_option(prompt="Deaktiver WiFi?")
@click.pass_context
def wifi_disable(ctx):
    """Deaktiver WiFi (Ethernet-only tilstand)."""
    try:
        _put(ctx.obj["base"], "/system/wifi", {"enabled": False})
    except Exception as e:
        _err(e)
    _ok("WiFi deaktiveret.")


# ── iface ──────────────────────────────────────────────────────────────────────

@cli.group()
def iface():
    """Modbus interface-konfiguration."""


@iface.command("list")
@click.pass_context
def iface_list(ctx):
    """Vis alle konfigurerede Modbus interfaces."""
    try:
        d = _get(ctx.obj["base"], "/interfaces")
    except Exception as e:
        _err(e)

    ifaces = d.get("interfaces", [])

    if ctx.obj["as_json"]:
        click.echo(json.dumps(ifaces, indent=2))
        return

    if not ifaces:
        click.echo("Ingen interfaces konfigureret.")
        return

    click.echo(f"  {'ID':>2}  {'Type':<7} {'Mode':<5} {'Baud':>7}  {'Par':>5}  {'Stop':>4}  Timeout  Status")
    click.echo("  " + "-" * 62)
    for i in ifaces:
        st   = click.style("Aktiv", fg="green") if i.get("enabled") else click.style("Slukket", fg="red")
        mode = i.get("uart_mode", "?")
        click.echo(
            f"  {i['id']:>2}  {i.get('type','?'):<7} {mode:<5} {i.get('baudrate',0):>7}"
            f"  {i.get('parity',0):>5}  {i.get('stop_bits',0):>4}  {i.get('timeout_ms',0):>6}ms  {st}"
        )


@iface.command("show")
@click.argument("id", type=int, metavar="ID")
@click.pass_context
def iface_show(ctx, id):
    """Vis alle detaljer for ét interface."""
    try:
        d = _get(ctx.obj["base"], f"/interfaces/{id}")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return

    labels = {
        "id":         "Interface ID",
        "type":       "Type",
        "uart_mode":  "UART mode",
        "uart_num":   "UART nr.",
        "baudrate":   "Baudrate",
        "parity":     "Paritet",
        "stop_bits":  "Stop bits",
        "timeout_ms": "Timeout (ms)",
        "tx_pin":     "TX pin",
        "rx_pin":     "RX pin",
        "rts_pin":    "DE/RE pin",
        "enabled":    "Aktiveret",
    }
    for k, label in labels.items():
        click.echo(f"  {label:<16}: {d.get(k, '?')}")


@iface.command("set")
@click.argument("id", type=int, metavar="ID")
@click.option("--baud",       type=int,  help="Baudrate")
@click.option("--parity",     type=int,  help="Paritet (0=ingen, 1=ulige, 2=lige)")
@click.option("--stop-bits",  type=int,  help="Stop bits (1 eller 2)")
@click.option("--timeout",    type=int,  help="Timeout i ms (50–5000)")
@click.option("--tx",         type=int,  help="TX GPIO pin")
@click.option("--rx",         type=int,  help="RX GPIO pin")
@click.option("--de",         type=int,  help="DE/RE GPIO pin (-1 = ingen)")
@click.option("--type",  "iface_type", type=int, help="Type (0=RS485, 1=RS232)")
@click.option("--mode",       type=int,  help="UART mode (0=HW ≤115200, 1=SW ≤9600)")
@click.option("--uart-num",   type=int,  help="Hardware UART nr. (1 eller 2, kun HW mode)")
@click.option("--enable/--disable", default=None, help="Aktivér eller deaktivér interface")
@click.pass_context
def iface_set(ctx, id, baud, parity, stop_bits, timeout, tx, rx, de,
              iface_type, mode, uart_num, enable):
    """Konfigurer et Modbus interface.

    \b
    Eksempel:
      mbgw iface set 0 --baud 19200 --parity 0
      mbgw iface set 1 --mode 0 --uart-num 2 --baud 115200 --enable
      mbgw iface set 0 --tx 25 --rx 26 --de 27
    """
    try:
        current = _get(ctx.obj["base"], f"/interfaces/{id}")
    except Exception as e:
        _err(e)

    cfg = dict(current)
    if baud       is not None: cfg["baudrate"]  = baud
    if parity     is not None: cfg["parity"]    = parity
    if stop_bits  is not None: cfg["stop_bits"] = stop_bits
    if timeout    is not None: cfg["timeout_ms"] = timeout
    if tx         is not None: cfg["tx_pin"]    = tx
    if rx         is not None: cfg["rx_pin"]    = rx
    if de         is not None: cfg["rts_pin"]   = de
    if iface_type is not None: cfg["type"]      = iface_type
    if mode       is not None: cfg["uart_mode"] = mode
    if uart_num   is not None: cfg["uart_num"]  = uart_num
    if enable     is not None: cfg["enabled"]   = enable

    try:
        _put(ctx.obj["base"], f"/interfaces/{id}", cfg)
    except Exception as e:
        _err(e)
    _ok(f"Interface {id} opdateret.")


# ── read ───────────────────────────────────────────────────────────────────────

def _print_regs(registers, start_addr):
    click.echo(f"  {'Addr':>6}  {'Dec':>6}  {'Hex':>6}  {'Bin':>18}")
    click.echo("  " + "-" * 42)
    for i, v in enumerate(registers):
        click.echo(f"  {start_addr + i:>6}  {v:>6}  {v:#06x}  {v:016b}")


def _print_bits(bits, start_addr, label="Addr"):
    click.echo(f"  {label:>6}  Værdi")
    click.echo("  " + "-" * 14)
    for i, v in enumerate(bits):
        val = click.style("ON ", fg="green") if v else click.style("OFF", fg="red")
        click.echo(f"  {start_addr + i:>6}  {val}")


@cli.group()
def read():
    """Læs Modbus register-data (FC01–FC04)."""


@read.command("holding")
@click.argument("iface",  type=int, metavar="IFACE")
@click.argument("slave",  type=int, metavar="SLAVE")
@click.argument("addr",   type=int, metavar="ADDR")
@click.argument("count",  type=int, metavar="COUNT", default=1)
@click.pass_context
def read_holding(ctx, iface, slave, addr, count):
    """Læs holding registers FC03.

    \b
    Eksempel:
      mbgw read holding 0 1 0 10    # iface=0 slave=1 addr=0 count=10
    """
    try:
        d = _get(ctx.obj["base"],
                 f"/interfaces/{iface}/holding_registers?slave={slave}&address={addr}&count={count}")
    except Exception as e:
        _err(e)

    regs = d.get("registers", [])
    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return
    _print_regs(regs, addr)


@read.command("input")
@click.argument("iface",  type=int, metavar="IFACE")
@click.argument("slave",  type=int, metavar="SLAVE")
@click.argument("addr",   type=int, metavar="ADDR")
@click.argument("count",  type=int, metavar="COUNT", default=1)
@click.pass_context
def read_input(ctx, iface, slave, addr, count):
    """Læs input registers FC04.

    \b
    Eksempel:
      mbgw read input 0 1 0 4
    """
    try:
        d = _get(ctx.obj["base"],
                 f"/interfaces/{iface}/input_registers?slave={slave}&address={addr}&count={count}")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return
    _print_regs(d.get("registers", []), addr)


@read.command("coils")
@click.argument("iface",  type=int, metavar="IFACE")
@click.argument("slave",  type=int, metavar="SLAVE")
@click.argument("addr",   type=int, metavar="ADDR")
@click.argument("count",  type=int, metavar="COUNT", default=1)
@click.pass_context
def read_coils(ctx, iface, slave, addr, count):
    """Læs coils FC01.

    \b
    Eksempel:
      mbgw read coils 0 1 0 8
    """
    try:
        d = _get(ctx.obj["base"],
                 f"/interfaces/{iface}/coils?slave={slave}&address={addr}&count={count}")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return
    _print_bits(d.get("coils", []), addr)


@read.command("discrete")
@click.argument("iface",  type=int, metavar="IFACE")
@click.argument("slave",  type=int, metavar="SLAVE")
@click.argument("addr",   type=int, metavar="ADDR")
@click.argument("count",  type=int, metavar="COUNT", default=1)
@click.pass_context
def read_discrete(ctx, iface, slave, addr, count):
    """Læs discrete inputs FC02.

    \b
    Eksempel:
      mbgw read discrete 0 1 0 4
    """
    try:
        d = _get(ctx.obj["base"],
                 f"/interfaces/{iface}/discrete_inputs?slave={slave}&address={addr}&count={count}")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return
    _print_bits(d.get("inputs", []), addr)


# ── write ──────────────────────────────────────────────────────────────────────

@cli.group()
def write():
    """Skriv Modbus register-data (FC05, FC06, FC0F, FC10)."""


@write.command("holding")
@click.argument("iface",   type=int, metavar="IFACE")
@click.argument("slave",   type=int, metavar="SLAVE")
@click.argument("addr",    type=int, metavar="ADDR")
@click.argument("values",  type=int, nargs=-1, required=True, metavar="VALUE [VALUE...]")
@click.pass_context
def write_holding(ctx, iface, slave, addr, values):
    """Skriv holding register(s) FC06/FC10.

    \b
    Eksempel:
      mbgw write holding 0 1 100 1234          # enkelt register (FC06)
      mbgw write holding 0 1 100 1234 5678     # multiple (FC10)
    """
    try:
        if len(values) == 1:
            _put(ctx.obj["base"], f"/interfaces/{iface}/holding_registers/{addr}",
                 {"slave": slave, "value": values[0]})
        else:
            _put(ctx.obj["base"], f"/interfaces/{iface}/holding_registers",
                 {"slave": slave, "address": addr, "values": list(values)})
    except Exception as e:
        _err(e)
    _ok(f"Skrevet {len(values)} register(s) @ adresse {addr}.")


@write.command("coil")
@click.argument("iface",  type=int, metavar="IFACE")
@click.argument("slave",  type=int, metavar="SLAVE")
@click.argument("addr",   type=int, metavar="ADDR")
@click.argument("value",  type=int, metavar="VALUE", help="0=OFF, 1=ON")
@click.pass_context
def write_coil(ctx, iface, slave, addr, value):
    """Skriv enkelt coil FC05.  VALUE: 0=OFF  1=ON

    \b
    Eksempel:
      mbgw write coil 0 1 5 1    # sæt coil 5 ON
    """
    try:
        _put(ctx.obj["base"], f"/interfaces/{iface}/coils/{addr}",
             {"slave": slave, "value": bool(value)})
    except Exception as e:
        _err(e)
    _ok(f"Coil {addr} sat til {'ON' if value else 'OFF'}.")


@write.command("coils")
@click.argument("iface",   type=int, metavar="IFACE")
@click.argument("slave",   type=int, metavar="SLAVE")
@click.argument("addr",    type=int, metavar="ADDR")
@click.argument("values",  type=int, nargs=-1, required=True, metavar="VALUE [VALUE...]")
@click.pass_context
def write_coils(ctx, iface, slave, addr, values):
    """Skriv multiple coils FC0F.  VALUES: 0=OFF  1=ON

    \b
    Eksempel:
      mbgw write coils 0 1 0 1 0 1 1    # skriver 5 coils fra adresse 0
    """
    try:
        _put(ctx.obj["base"], f"/interfaces/{iface}/coils",
             {"slave": slave, "address": addr, "values": [bool(v) for v in values]})
    except Exception as e:
        _err(e)
    _ok(f"Skrevet {len(values)} coil(s) @ adresse {addr}.")


# ── ota ────────────────────────────────────────────────────────────────────────

@cli.group()
def ota():
    """OTA firmware/frontend opdatering fra GitHub."""


@ota.command("check")
@click.pass_context
def ota_check(ctx):
    """Tjek GitHub for tilgængelige opdateringer."""
    try:
        d = _get(ctx.obj["base"], "/system/ota/check", timeout=15)
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return

    fw  = d.get("firmware_available", False)
    fe  = d.get("frontend_available", False)
    click.echo(f"  Nuværende version : {d.get('current_version','?')}")
    click.echo(f"  Seneste version   : {d.get('latest_version','?')}")
    click.echo(f"  Firmware          : {click.style('⬆ Tilgængelig', fg='yellow') if fw else '✔ Opdateret'}")
    click.echo(f"  Frontend          : {click.style('⬆ Tilgængelig', fg='yellow') if fe else '✔ Opdateret'}")
    if d.get("release_notes"):
        notes = d["release_notes"][:300]
        if len(d["release_notes"]) > 300:
            notes += "…"
        click.echo(f"\n  Release notes:\n    {notes}")


@ota.command("firmware")
@click.confirmation_option(prompt="Opdater firmware fra GitHub? (gateway genstarter automatisk)")
@click.pass_context
def ota_firmware(ctx):
    """Download og flash ny firmware fra seneste GitHub release."""
    click.echo("Starter firmware OTA…")
    try:
        _post(ctx.obj["base"], "/system/ota/firmware", timeout=6)
    except Exception:
        pass  # connection reset forventes ved firmware OTA
    click.echo("Firmware flashet — gateway genstarter om ca. 10 sek.")


@ota.command("frontend")
@click.confirmation_option(prompt="Opdater frontend fra GitHub?")
@click.pass_context
def ota_frontend(ctx):
    """Download og flash ny frontend (SPIFFS) fra seneste GitHub release."""
    click.echo("Starter frontend OTA…")
    try:
        _post(ctx.obj["base"], "/system/ota/frontend")
    except Exception as e:
        _err(e)

    for _ in range(120):
        time.sleep(1)
        try:
            st    = _get(ctx.obj["base"], "/system/ota/status", timeout=4)
            pct   = st.get("progress_pct", 0)
            state = st.get("state", "?")
            click.echo(f"\r  {state:<12} {pct:>3}%", nl=False)
            if state == "done":
                click.echo()
                _ok("Frontend opdateret!")
                return
            if state == "error":
                click.echo()
                _err(st.get("error", "ukendt fejl"))
        except Exception:
            pass
    click.echo()
    click.echo("Timeout — tjek status manuelt med: mbgw ota status", err=True)


@ota.command("status")
@click.pass_context
def ota_status(ctx):
    """Vis status for igangværende OTA-opdatering."""
    try:
        d = _get(ctx.obj["base"], "/system/ota/status")
    except Exception as e:
        _err(e)

    if ctx.obj["as_json"]:
        click.echo(json.dumps(d, indent=2))
        return

    state = d.get("state", "idle")
    color = "green" if state == "done" else "red" if state == "error" else "yellow"
    click.echo(f"  State    : {click.style(state, fg=color)}")
    click.echo(f"  Fremdrift: {d.get('progress_pct', 0)}%")
    if d.get("error"):
        click.echo(f"  Fejl     : {click.style(d['error'], fg='red')}")


# ── entry point ────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    cli()
