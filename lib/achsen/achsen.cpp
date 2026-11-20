// SPDX-License-Identifier: MIT
#include "achsen.h"

namespace achsen {

uint16_t ausUs(uint16_t us) {
  if (us <= US_MIN) return ACHSE_MIN;
  if (us >= US_MAX) return ACHSE_MAX;
  const uint32_t weg = static_cast<uint32_t>(us - US_MIN);
  const uint32_t spanne = static_cast<uint32_t>(US_MAX - US_MIN);
  return static_cast<uint16_t>((weg * ACHSE_MAX + spanne / 2) / spanne);
}

Bericht abbilden(const sumd::Rahmen& rahmen, bool jeEinRahmen, uint32_t msSeitRahmen) {
  Bericht b;  // Vorgabe = Failsafe-Lage
  if (!jeEinRahmen || msSeitRahmen > KEIN_SIGNAL_MS || rahmen.failsafe) return b;

  b.failsafe = false;
  for (uint8_t k = 0; k < ANZAHL; ++k) {
    if (k < rahmen.kanaele) b.achse[ACHSE_FUER_KANAL[k]] = ausUs(rahmen.us[k]);
  }
  return b;
}

bool gleich(const Bericht& a, const Bericht& b) {
  if (a.failsafe != b.failsafe) return false;
  for (uint8_t i = 0; i < ANZAHL; ++i) {
    if (a.achse[i] != b.achse[i]) return false;
  }
  return true;
}

}  // namespace achsen
