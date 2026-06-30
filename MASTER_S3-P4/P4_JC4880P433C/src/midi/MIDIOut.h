// midi/MIDIOut.h — ExPressif MIDI send layer  (AITEC 2026-06-29)
#pragma once
#include <Arduino.h>

void sendCC(uint8_t ch, uint8_t cc, uint8_t val);
void sendNote(uint8_t ch, uint8_t note, uint8_t vel, bool on);
void sendAllNotesOff(uint8_t ch);
// Selección de banco + programa: CC0(MSB) → CC32(LSB) → PC  (JV-2080 p.184)
void sendBankPC(uint8_t ch, uint8_t msb, uint8_t lsb, uint8_t pc);
