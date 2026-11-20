# SPDX-License-Identifier: MIT
"""
Prüfstand ohne Hardware: übersetzt den SUMD-Dekoder und die Achsabbildung für
diesen Rechner und prüft sie.

Aufruf:  python tools/host_tests.py
Übersetzer: $CXX, sonst c++, g++, clang++, sonst `python -m ziglang c++`
(pip install ziglang).
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
rot = 0


def pruefe(name, ok, info=""):
    global rot
    if not ok:
        rot += 1
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f"  ({info})" if info else ""))


def uebersetzer():
    kandidaten = []
    if os.environ.get("CXX"):
        kandidaten.append(os.environ["CXX"].split())
    kandidaten += [["c++"], ["g++"], ["clang++"], [sys.executable, "-m", "ziglang", "c++"]]
    for k in kandidaten:
        try:
            if subprocess.run(k + ["--version"], capture_output=True).returncode == 0:
                return k
        except OSError:
            pass
    return None


def bauen(cxx, ordner, name, quellen):
    exe = Path(ordner) / (name + (".exe" if os.name == "nt" else ""))
    cmd = cxx + ["-std=c++17", "-O1", "-Wall", "-Werror", "-I", str(WURZEL / "lib" / "sumd"), "-o", str(exe)]
    cmd += [str(WURZEL / q) for q in quellen]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        sys.exit(1)
    return str(exe)


def crc16(daten):
    crc = 0
    for b in daten:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def rahmen(us, status=0x01, crc_falsch=False):
    b = [0xA8, status, len(us)]
    for u in us:
        roh = round(u * 8)
        b += [(roh >> 8) & 0xFF, roh & 0xFF]
    crc = crc16(b) ^ (1 if crc_falsch else 0)
    return b + [(crc >> 8) & 0xFF, crc & 0xFF]


def sumd(exe, daten):
    out = subprocess.run([exe], input=bytes(daten), capture_output=True).stdout.decode().split("\n")
    rahmen_, stand = [], (None, None)
    for z in out:
        t = z.split()
        if t and t[0] == "RAHMEN":
            rahmen_.append((t[1] == "1", [int(x) for x in t[3:]]))
        elif t and t[0] == "STAND":
            stand = (int(t[1]), int(t[2]))
    return rahmen_, stand


def achsen(exe, zeilen):
    out = subprocess.run([exe], input="\n".join(zeilen) + "\n", capture_output=True, text=True).stdout
    return [z.split() for z in out.strip().splitlines()]


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    cxx = uebersetzer()
    if not cxx:
        print("Kein C++-Übersetzer gefunden ($CXX, c++, g++, clang++, python -m ziglang c++).")
        sys.exit(2)
    ordner = tempfile.mkdtemp(prefix="awsmsim-host-")
    try:
        s_exe = bauen(cxx, ordner, "sumd_host", ["lib/sumd/sumd.cpp", "test/sumd_host.cpp"])
        a_exe = bauen(cxx, ordner, "achsen_host", ["lib/sumd/sumd.cpp", "lib/achsen/achsen.cpp", "test/achsen_host.cpp"])
        print(f"SUMD-Dekoder ({' '.join(cxx)})")
        for n in (8, 12, 16):
            w = [988 + round(i * 1024 / (n - 1)) for i in range(n)]
            r, _ = sumd(s_exe, rahmen(w))
            pruefe(f"gültiger Rahmen mit {n} Kanälen, auf 1 µs genau", len(r) == 1 and r[0][1] == w)
        w = [1500] * 8
        r, st = sumd(s_exe, rahmen(w, crc_falsch=True))
        pruefe("falsche Prüfsumme wird verworfen", not r and st[1] >= 1)
        r, _ = sumd(s_exe, rahmen(w)[:10] + rahmen(w))
        pruefe("abgeschnittener Rahmen, danach ein gültiger", len(r) == 1)
        muell = [0x00, 0xFF, 0xA8, 0x13, 0xA8, 0x01, 0x40, 0xA8]
        r, _ = sumd(s_exe, muell + rahmen(w) + muell + rahmen(w))
        pruefe("Müll davor und dazwischen", len(r) == 2)
        r, _ = sumd(s_exe, [0xA8, 0x01, 0x08] + rahmen(w) + rahmen(w))
        pruefe("falscher Kopf verschluckt den Anfang eines echten Rahmens", len(r) == 2)
        r, _ = sumd(s_exe, rahmen(w, status=0x81))
        pruefe("Status 0x81 = Failsafe", len(r) == 1 and r[0][0])

        print("Achsabbildung")
        def us(x):
            return int(achsen(a_exe, [f"US {x}"])[0][1])
        pruefe("988 / 1500 / 2012 µs -> 0 / 16384 / 32767", us(988) == 0 and abs(us(1500) - 16384) <= 1 and us(2012) == 32767)
        pruefe("Kappung bei 900 und 2100 µs", us(900) == 0 and us(2100) == 32767)
        k = [1100, 1200, 1300, 1400, 1600, 1700, 1800, 1900]
        b = [int(x) for x in achsen(a_exe, ["BERICHT 0 1 10 8 " + " ".join(map(str, k))])[0][1:]]
        erwartet = [0] + [us(x) for x in k]  # Taste 1, dann X Y Z RX RY RZ S1 S2 = K1..K8
        pruefe("Kanalfolge K1..K8 -> X, Y, Z, RX, RY, RZ, Slider 1, Slider 2", b == erwartet, str(b))
        fs = [int(x) for x in achsen(a_exe, ["BERICHT 1 1 10 8 " + " ".join(map(str, k))])[0][1:]]
        pruefe("Failsafe: Taste 1, Gas Minimum, Rest Mitte", fs == [1, 0] + [16384] * 7, str(fs))
        alt = [int(x) for x in achsen(a_exe, ["BERICHT 0 1 101 8 " + " ".join(map(str, k))])[0][1:]]
        pruefe("101 ms ohne Rahmen -> Failsafe", alt[0] == 1)
    finally:
        shutil.rmtree(ordner, ignore_errors=True)
    print("\nAlle Prüfungen grün." if rot == 0 else f"\n{rot} Prüfung(en) rot.")
    sys.exit(0 if rot == 0 else 1)


if __name__ == "__main__":
    main()
