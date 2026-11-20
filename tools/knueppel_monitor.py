# SPDX-License-Identifier: MIT
"""
Knüppelanzeige für den awsmSIM-Empfänger im Terminal.

Liest die serielle Meldung des ESP32 (115200 Baud) und zeigt je Kanal einen
Balken. Der Kanal, der sich gerade bewegt, wird hervorgehoben.

Aufruf (aus dem Repo):
    python hardware/esp32-empfaenger/tools/knueppel_monitor.py [COM3]
Ohne Port nimmt das Skript den ersten USB-Seriell-Port.
Beenden mit Strg+C. Vorher jeden anderen seriellen Monitor schließen.

Braucht pyserial (kommt mit PlatformIO mit, sonst: pip install pyserial).
"""
import os
import re
import sys
import time
from collections import deque

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial fehlt: pip install pyserial")
    sys.exit(1)

NAMEN = {1: "Gas", 2: "Quer", 3: "Hoehe", 4: "Seite"}
US_MIN, US_MAX, US_MITTE = 988, 2012, 1500
BREITE = 41  # Balkenbreite in Zeichen, ungerade für eine Mitte
FENSTER_S = 0.4  # über diese Zeit wird "bewegt" bestimmt
BEWEGT_US = 12  # so viel muss sich ein Kanal im Fenster ändern

ZEILE = re.compile(r"^(BLE \w+)\s*\|\s*SUMD (.*?)\|")
KANAL = re.compile(r"K(\d+)=\s*(\d+)")

FETT, GRUEN, GELB, ROT, GRAU, AUS = "\x1b[1m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[90m", "\x1b[0m"


def port_waehlen(arg):
    if arg:
        return arg
    for p in serial.tools.list_ports.comports():
        text = f"{p.description} {p.manufacturer or ''}"
        if any(k in text for k in ("CP210", "CH340", "CH910", "USB", "UART", "Serial")):
            return p.device
    print("Kein serieller Port gefunden. Port als Argument angeben, z. B. COM3.")
    sys.exit(1)


def balken(us):
    anteil = (min(max(us, US_MIN), US_MAX) - US_MIN) / (US_MAX - US_MIN)
    pos = round(anteil * (BREITE - 1))
    mitte = BREITE // 2
    zeichen = []
    for i in range(BREITE):
        if i == pos:
            zeichen.append("█")
        elif i == mitte:
            zeichen.append("┼")
        elif min(pos, mitte) < i < max(pos, mitte):
            zeichen.append("─")
        else:
            zeichen.append("·")
    return "".join(zeichen)


def prozent(us):
    return round((us - US_MITTE) / ((US_MAX - US_MIN) / 2) * 100)


def zeichnen(ble, sumd, werte, verlauf, jetzt):
    zeilen = [f"{FETT}awsmSIM-Empfänger – Knüppelanzeige{AUS}   (Strg+C beendet)", ""]
    if sumd.startswith("ok"):
        status = f"{GRUEN}SUMD ok{AUS}"
    elif "FAILSAFE" in sumd:
        status = f"{ROT}FAILSAFE – Sender aus oder außer Reichweite{AUS}"
    else:
        status = f"{ROT}kein SUMD-Signal{AUS}"
    blau = GRUEN if "verbunden" in ble else GELB
    zeilen.append(f"Empfänger: {status}    Bluetooth: {blau}{ble.replace('BLE ', '')}{AUS}")
    zeilen.append("")

    aktiv = []
    for k in sorted(werte):
        us = werte[k]
        fenster = [w for t, w in verlauf[k] if jetzt - t <= FENSTER_S]
        bewegt = len(fenster) > 1 and max(fenster) - min(fenster) >= BEWEGT_US
        name = f"K{k} {NAMEN.get(k, 'Kanal ' + str(k))}"
        text = f"{name:<12} {us:5d} µs {prozent(us):+5d} %  {balken(us)}"
        if bewegt:
            aktiv.append(name)
            zeilen.append(f"{FETT}{GELB}{text}  ◄ bewegt{AUS}")
        else:
            zeilen.append(f"{text}")

    zeilen.append("")
    zeilen.append(f"Bewegt: {FETT}{', '.join(aktiv)}{AUS}" if aktiv else f"{GRAU}Bewegt: –{AUS}")
    zeilen.append(f"{GRAU}Hinweis: Die Anzeige zeigt K1 bis K8. Welcher Knüppel welcher Kanal ist, legt der Modellspeicher der MZ-18 fest.{AUS}")
    # Cursor nach oben links, jede Zeile bis zum Ende löschen
    sys.stdout.write("\x1b[H" + "\n".join(z + "\x1b[K" for z in zeilen) + "\x1b[J")
    sys.stdout.flush()


def main():
    os.system("")  # schaltet unter Windows die Farbcodes im Terminal ein
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    sekunden = None  # nur zum Prüfen: --sekunden=N beendet nach N s
    for a in sys.argv[1:]:
        if a.startswith("--sekunden="):
            sekunden = float(a.split("=", 1)[1])

    port = port_waehlen(args[0] if args else None)
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.1
    s.dtr = False  # kein Neustart des ESP32 beim Öffnen
    s.rts = False
    try:
        s.open()
    except serial.SerialException as e:
        print(f"Port {port} lässt sich nicht öffnen ({e}). Läuft noch ein anderer Monitor?")
        sys.exit(1)

    werte, verlauf = {}, {}
    ble, sumd = "BLE wartet", "kein Signal"
    rest = b""
    start = letztes = time.time()
    sys.stdout.write("\x1b[2J")
    try:
        while sekunden is None or time.time() - start < sekunden:
            rest += s.read(4096)
            *zeilen, rest = rest.split(b"\n")
            jetzt = time.time()
            for roh in zeilen:
                z = roh.decode("utf-8", "replace").strip()
                m = ZEILE.match(z)
                if not m:
                    continue
                ble, sumd = m.group(1), m.group(2).strip()
                for k, us in KANAL.findall(z):
                    k, us = int(k), int(us)
                    werte[k] = us
                    verlauf.setdefault(k, deque(maxlen=50)).append((jetzt, us))
                if not sumd.startswith("ok"):
                    werte.clear()
            if jetzt - letztes >= 0.1:
                zeichnen(ble, sumd, werte, verlauf, jetzt)
                letztes = jetzt
    except KeyboardInterrupt:
        pass
    finally:
        s.close()
        sys.stdout.write(AUS + "\n")


if __name__ == "__main__":
    main()
