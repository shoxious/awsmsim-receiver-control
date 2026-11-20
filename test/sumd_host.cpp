// SPDX-License-Identifier: MIT
//
// Host-Treiber für tools/host_tests.py: liest einen Bytestrom von
// stdin, speist ihn in den SUMD-Dekoder und schreibt je fertigem Rahmen eine
// Zeile "RAHMEN <failsafe 0|1> <kanaele> <us1> <us2> ...", am Ende
// "STAND <angenommen> <verworfen>".
#include <cstdio>

#include "../lib/sumd/sumd.h"

int main() {
  sumd::Dekoder dekoder;
  int c;
  while ((c = std::getchar()) != EOF) {
    if (dekoder.einspeisen(static_cast<uint8_t>(c))) {
      const sumd::Rahmen& r = dekoder.rahmen();
      std::printf("RAHMEN %d %u", r.failsafe ? 1 : 0, static_cast<unsigned>(r.kanaele));
      for (uint8_t k = 0; k < r.kanaele; ++k) std::printf(" %u", static_cast<unsigned>(r.us[k]));
      std::printf("\n");
    }
  }
  std::printf("STAND %u %u\n", static_cast<unsigned>(dekoder.angenommen()), static_cast<unsigned>(dekoder.verworfen()));
  return 0;
}
