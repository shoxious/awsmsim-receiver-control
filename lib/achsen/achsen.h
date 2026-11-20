// SPDX-License-Identifier: MIT
//
// Abbildung SUMD-Kanal -> Gamepad-Achse für den awsmSIM-Empfänger.
//
//   K1 Gas  -> X        K5 -> RY
//   K2 Quer -> Y        K6 -> RZ
//   K3 Höhe -> Z        K7 -> Slider 1
//   K4 Seite-> RX       K8 -> Slider 2
//
// Der Browser nummeriert die Achsen X, Y, Z, RX, ... als 0, 1, 2, 3, ... Damit
// kommen die Kanäle in der Graupner-Folge an, und das ist genau die Vorgabe des
// Sims awsmSIM (Gas 0, Quer 1, Höhe 2, Seite 3).
//
// 988..2012 µs gehen linear auf 0..32767, darüber und darunter wird gekappt.
// Die Firmware kehrt keine Richtung um; das macht die Kalibrierung im Sim.
//
// Failsafe (Empfänger meldet 0x81) oder kein gültiger Rahmen seit
// KEIN_SIGNAL_MS: Steuerachsen und Hilfskanäle auf Mitte, Gas auf Minimum,
// Taste 1 gedrückt, damit der Sim „kein Signal“ anzeigen kann.
//
// Reines C++ ohne Arduino-Kopf (Prüfstand: python tools/host_tests.py).
#pragma once

#include <stdint.h>

#include "sumd.h"

namespace achsen {

constexpr uint16_t US_MIN = 988;
constexpr uint16_t US_MAX = 2012;
constexpr uint16_t ACHSE_MIN = 0;
constexpr uint16_t ACHSE_MAX = 32767;
constexpr uint16_t ACHSE_MITTE = 16384;
constexpr uint32_t KEIN_SIGNAL_MS = 100;

enum Achse : uint8_t { ACHSE_X = 0, ACHSE_Y, ACHSE_Z, ACHSE_RX, ACHSE_RY, ACHSE_RZ, SCHIEBER1, SCHIEBER2, ANZAHL };

struct Bericht {
  uint16_t achse[ANZAHL] = {ACHSE_MIN, ACHSE_MITTE, ACHSE_MITTE, ACHSE_MITTE,
                            ACHSE_MITTE, ACHSE_MITTE, ACHSE_MITTE, ACHSE_MITTE};
  bool failsafe = true;  // Taste 1
};

// µs -> Achswert, gekappt.
uint16_t ausUs(uint16_t us);

// Kanal (0-basiert, K1 = 0) -> Achse.
constexpr Achse ACHSE_FUER_KANAL[ANZAHL] = {ACHSE_X, ACHSE_Y, ACHSE_Z, ACHSE_RX, ACHSE_RY, ACHSE_RZ, SCHIEBER1, SCHIEBER2};

// Baut den Bericht. `msSeitRahmen` ist die Zeit seit dem letzten gültigen
// Rahmen; `jeEinRahmen` false heißt: noch nie einer angekommen.
Bericht abbilden(const sumd::Rahmen& rahmen, bool jeEinRahmen, uint32_t msSeitRahmen);

bool gleich(const Bericht& a, const Bericht& b);

}  // namespace achsen
