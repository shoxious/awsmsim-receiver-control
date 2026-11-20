// SPDX-License-Identifier: MIT
//
// Host-Treiber für tools/host_tests.py. Liest Zeilen von stdin:
//   US <µs>                                   -> "ACHSE <wert>"
//   BERICHT <failsafe 0|1> <je 0|1> <ms> <us1> ... <usN>
//                                             -> "BERICHT <taste1> <x> <y> <z> <rx> <ry> <rz> <s1> <s2>"
#include <cstdio>
#include <cstring>

#include "../lib/achsen/achsen.h"

int main() {
  char wort[16];
  while (std::scanf("%15s", wort) == 1) {
    if (std::strcmp(wort, "US") == 0) {
      unsigned us = 0;
      if (std::scanf("%u", &us) != 1) return 1;
      std::printf("ACHSE %u\n", static_cast<unsigned>(achsen::ausUs(static_cast<uint16_t>(us))));
    } else if (std::strcmp(wort, "BERICHT") == 0) {
      int failsafe = 0, je = 0, n = 0;
      unsigned ms = 0;
      if (std::scanf("%d %d %u %d", &failsafe, &je, &ms, &n) != 4) return 1;
      sumd::Rahmen r;
      r.failsafe = failsafe != 0;
      r.kanaele = static_cast<uint8_t>(n);
      for (int k = 0; k < n; ++k) {
        unsigned us = 0;
        if (std::scanf("%u", &us) != 1) return 1;
        r.us[k] = static_cast<uint16_t>(us);
      }
      const achsen::Bericht b = achsen::abbilden(r, je != 0, ms);
      std::printf("BERICHT %d", b.failsafe ? 1 : 0);
      for (int i = 0; i < achsen::ANZAHL; ++i) std::printf(" %u", static_cast<unsigned>(b.achse[i]));
      std::printf("\n");
    } else {
      return 1;
    }
  }
  return 0;
}
