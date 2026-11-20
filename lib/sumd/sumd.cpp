// SPDX-License-Identifier: MIT
#include "sumd.h"

namespace sumd {

uint16_t crc16(const uint8_t* daten, size_t laenge) {
  uint16_t crc = 0;
  for (size_t i = 0; i < laenge; ++i) {
    crc ^= static_cast<uint16_t>(daten[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

bool Dekoder::einspeisen(uint8_t byte) {
  fertig_ = false;
  verarbeiten(byte);
  // Bytes, die nach einem verworfenen Rahmen noch einmal geprüft werden müssen.
  while (offen_ > 0) {
    const uint8_t b = offenPuffer_[0];
    for (size_t i = 1; i < offen_; ++i) offenPuffer_[i - 1] = offenPuffer_[i];
    --offen_;
    verarbeiten(b);
  }
  return fertig_;
}

void Dekoder::verarbeiten(uint8_t byte) {
  if (laenge_ == 0) {
    if (byte != KOPF) return;  // Müll vor dem Kopf überspringen
    puffer_[laenge_++] = byte;
    return;
  }

  puffer_[laenge_++] = byte;

  if (laenge_ == 2) {
    if (byte != STATUS_GUELTIG && byte != STATUS_FAILSAFE) verwerfen();
    return;
  }

  if (laenge_ == 3) {
    if (byte == 0 || byte > MAX_KANAELE) {
      verwerfen();
      return;
    }
    erwartet_ = 3 + static_cast<size_t>(byte) * 2 + 2;
    return;
  }

  if (laenge_ == erwartet_) abschliessen();
}

void Dekoder::abschliessen() {
  const size_t nutz = erwartet_ - 2;
  const uint16_t gesendet = static_cast<uint16_t>((puffer_[nutz] << 8) | puffer_[nutz + 1]);
  if (crc16(puffer_, nutz) != gesendet) {
    verwerfen();
    return;
  }

  rahmen_.failsafe = puffer_[1] == STATUS_FAILSAFE;
  rahmen_.kanaele = puffer_[2];
  for (uint8_t k = 0; k < MAX_KANAELE; ++k) {
    if (k < rahmen_.kanaele) {
      const uint16_t roh = static_cast<uint16_t>((puffer_[3 + k * 2] << 8) | puffer_[4 + k * 2]);
      rahmen_.us[k] = static_cast<uint16_t>((roh + 4u) / 8u);  // 1/8 µs -> µs, gerundet
    } else {
      rahmen_.us[k] = 0;
    }
  }
  ++angenommen_;
  fertig_ = true;
  laenge_ = 0;
  erwartet_ = 0;
}

void Dekoder::verwerfen() {
  ++verworfen_;
  // Der verworfene "Kopf" kann ein 0xA8 mitten im Müll gewesen sein. Alles nach
  // ihm wird vorn in die offene Schlange gelegt und noch einmal geprüft, damit
  // ein echter Rahmen darin nicht verloren geht. Puffer und Schlange enthalten
  // zusammen nie mehr Bytes als ein Rahmen plus eins, die Schlange läuft nicht über.
  const size_t n = laenge_ - 1;
  for (size_t i = offen_; i > 0; --i) offenPuffer_[i - 1 + n] = offenPuffer_[i - 1];
  for (size_t i = 0; i < n; ++i) offenPuffer_[i] = puffer_[i + 1];
  offen_ += n;
  laenge_ = 0;
  erwartet_ = 0;
}

}  // namespace sumd
