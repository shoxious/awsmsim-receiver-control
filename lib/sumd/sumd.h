// SPDX-License-Identifier: MIT
//
// SUMD-Dekoder für den awsmSIM-Empfänger (Graupner HoTT, z. B. GR-12).
//
// SUMD ist das digitale Summensignal der HoTT-Empfänger: 115200 Baud, 8N1,
// etwa alle 10 ms ein Rahmen:
//
//   0xA8 | Status | N | N x Kanal (16 Bit, big endian) | CRC16 (big endian)
//
//   Status 0x01 = gültige Daten, 0x81 = Failsafe (Empfänger hat kein Signal
//   vom Sender und schickt die Failsafe-Positionen).
//   Kanal in 1/8 µs: 12000 = 1500 µs (Mitte), 7200 = 900 µs, 16800 = 2100 µs.
//   CRC16-CCITT, Polynom 0x1021, Startwert 0, über alles vor der Prüfsumme.
//
// Reines C++ ohne Arduino-Kopf, damit tools/host_tests.py es auf dem
// Rechner übersetzen und prüfen kann.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sumd {

constexpr uint8_t KOPF = 0xA8;
constexpr uint8_t STATUS_GUELTIG = 0x01;
constexpr uint8_t STATUS_FAILSAFE = 0x81;
constexpr uint8_t MAX_KANAELE = 32;
// Kopf + Status + N + 32 x 2 + CRC 2
constexpr size_t MAX_RAHMEN = 3 + MAX_KANAELE * 2 + 2;

struct Rahmen {
  bool failsafe = false;
  uint8_t kanaele = 0;
  // Kanalwerte in µs (gerundet), Index 0 = K1.
  uint16_t us[MAX_KANAELE] = {};
};

uint16_t crc16(const uint8_t* daten, size_t laenge);

class Dekoder {
 public:
  // Ein Byte einspeisen. true, wenn dabei mindestens ein gültiger Rahmen
  // fertig wurde; der letzte steht dann in rahmen().
  bool einspeisen(uint8_t byte);

  const Rahmen& rahmen() const { return rahmen_; }
  uint32_t angenommen() const { return angenommen_; }
  uint32_t verworfen() const { return verworfen_; }

 private:
  void verarbeiten(uint8_t byte);
  void verwerfen();
  void abschliessen();

  uint8_t puffer_[MAX_RAHMEN] = {};
  size_t laenge_ = 0;
  size_t erwartet_ = 0;
  uint8_t offenPuffer_[MAX_RAHMEN + 1] = {};
  size_t offen_ = 0;
  Rahmen rahmen_;
  uint32_t angenommen_ = 0;
  uint32_t verworfen_ = 0;
  bool fertig_ = false;
};

}  // namespace sumd
