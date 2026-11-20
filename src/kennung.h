// SPDX-License-Identifier: MIT
//
// Name, unter dem sich der Empfänger per Bluetooth meldet. Der Sim erkennt ihn
// an diesem Namen (keine eigene VID/PID).
//
// Bewusst ohne Umlaut: Der Name läuft über BLE, das Betriebssystem und die
// Gamepad-Schnittstelle des Browsers, und nicht jede Stufe reicht UTF-8 sauber
// durch.
#pragma once

#define AWSM_EMPFAENGER_NAME "awsmSIM Empfaenger"
#define AWSM_EMPFAENGER_HERSTELLER "awsmSIM"
