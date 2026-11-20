// SPDX-License-Identifier: MIT
//
// awsmSIM-Empfänger: SUMD vom HoTT-Empfänger lesen und als Bluetooth-Gamepad
// weitergeben. Zusätzlich meldet die serielle Konsole (115200 Baud) alle 100 ms
// die Kanäle, damit Verkabelung, Pegel und Empfängermenü prüfbar sind.
//
// Verkabelung (GR-12):
//   GR-12 Plus  -> VIN/5V des ESP32 (nicht 3V3)
//   GR-12 Minus -> GND
//   GR-12 Servobuchse 6, Signal (SUMD) -> 1 kOhm -> GPIO16 (UART2 RX)
//   SUMD muss im Telemetriemenü des Senders für K6 eingeschaltet sein.
// Vorher den Pegel der SUMD-Leitung messen: höchstens 3,3 V.
//
// LED (GPIO2): blinkt langsam = wartet auf Koppeln, an = verbunden,
//              blinkt schnell = verbunden, aber kein Signal / Failsafe.
#include <Arduino.h>
#include <BleGamepad.h>

#include "achsen.h"
#include "kennung.h"
#include "sumd.h"

namespace {

constexpr int PIN_SUMD_RX = 16;
constexpr int PIN_LED = 2;
constexpr uint32_t SUMD_BAUD = 115200;
constexpr uint32_t BERICHT_SPAETESTENS_MS = 50;
constexpr uint32_t MELDEN_MS = 100;

sumd::Dekoder dekoder;
BleGamepad gamepad(AWSM_EMPFAENGER_NAME, AWSM_EMPFAENGER_HERSTELLER, 100);

uint32_t letzterRahmenMs = 0;
bool jeEinRahmen = false;
achsen::Bericht gesendet;
bool jeGesendet = false;
uint32_t letzterBerichtMs = 0;
uint32_t letzteMeldungMs = 0;

void senden(const achsen::Bericht& b) {
  gamepad.setX(b.achse[achsen::ACHSE_X]);
  gamepad.setY(b.achse[achsen::ACHSE_Y]);
  gamepad.setZ(b.achse[achsen::ACHSE_Z]);
  gamepad.setRX(b.achse[achsen::ACHSE_RX]);
  gamepad.setRY(b.achse[achsen::ACHSE_RY]);
  gamepad.setRZ(b.achse[achsen::ACHSE_RZ]);
  gamepad.setSlider1(b.achse[achsen::SCHIEBER1]);
  gamepad.setSlider2(b.achse[achsen::SCHIEBER2]);
  if (b.failsafe) {
    gamepad.press(BUTTON_1);
  } else {
    gamepad.release(BUTTON_1);
  }
  gamepad.sendReport();
}

void melden(const achsen::Bericht& b) {
  const sumd::Rahmen& r = dekoder.rahmen();
  Serial.printf("%s | ", gamepad.isConnected() ? "BLE verbunden" : "BLE wartet   ");
  if (b.failsafe) {
    Serial.printf("SUMD %s", jeEinRahmen && r.failsafe ? "FAILSAFE   " : "kein Signal");
  } else {
    Serial.printf("SUMD ok %2u Kanaele", r.kanaele);
    const uint8_t zeigen = r.kanaele < 8 ? r.kanaele : 8;
    for (uint8_t k = 0; k < zeigen; ++k) Serial.printf(" K%u=%4u", k + 1, r.us[k]);
  }
  Serial.printf(" | ok %lu, verworfen %lu\n", static_cast<unsigned long>(dekoder.angenommen()),
                static_cast<unsigned long>(dekoder.verworfen()));
}

}  // namespace

void setup() {
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(115200);
  Serial2.begin(SUMD_BAUD, SERIAL_8N1, PIN_SUMD_RX, -1);

  BleGamepadConfiguration konfig;
  konfig.setAutoReport(false);
  konfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  konfig.setButtonCount(1);
  konfig.setHatSwitchCount(0);
  konfig.setIncludeStart(false);
  konfig.setIncludeSelect(false);
  konfig.setWhichAxes(true, true, true, true, true, true, true, true);
  konfig.setAxesMin(achsen::ACHSE_MIN);
  konfig.setAxesMax(achsen::ACHSE_MAX);
  gamepad.begin(&konfig);

  Serial.println();
  Serial.println("awsmSIM-Empfaenger: SUMD -> Bluetooth-Gamepad \"" AWSM_EMPFAENGER_NAME "\"");
}

void loop() {
  while (Serial2.available() > 0) {
    if (dekoder.einspeisen(static_cast<uint8_t>(Serial2.read()))) {
      letzterRahmenMs = millis();
      jeEinRahmen = true;
    }
  }

  const uint32_t jetzt = millis();
  const achsen::Bericht b = achsen::abbilden(dekoder.rahmen(), jeEinRahmen, jetzt - letzterRahmenMs);
  const bool verbunden = gamepad.isConnected();

  if (verbunden) {
    const bool neu = !jeGesendet || !achsen::gleich(b, gesendet);
    if (neu || jetzt - letzterBerichtMs >= BERICHT_SPAETESTENS_MS) {
      senden(b);
      gesendet = b;
      jeGesendet = true;
      letzterBerichtMs = jetzt;
    }
  } else {
    jeGesendet = false;
  }

  if (!verbunden) {
    digitalWrite(PIN_LED, ((jetzt / 500) % 2) ? HIGH : LOW);
  } else if (b.failsafe) {
    digitalWrite(PIN_LED, ((jetzt / 100) % 2) ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED, HIGH);
  }

  if (jetzt - letzteMeldungMs >= MELDEN_MS) {
    letzteMeldungMs = jetzt;
    melden(b);
  }

  delay(1);
}
