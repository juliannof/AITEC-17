// midi/MIDIOut.cpp — ExPressif MIDI send layer  (AITEC 2026-06-29)
#include "MIDIOut.h"
#include <USBMIDI.h>

extern USBMIDI MIDI;

void sendCC(uint8_t ch, uint8_t cc, uint8_t val) {
    MIDI.controlChange(cc, val, ch);
}

void sendNote(uint8_t ch, uint8_t note, uint8_t vel, bool on) {
    if (on) MIDI.noteOn(note, vel, ch);
    else    MIDI.noteOff(note, 0, ch);
}

void sendAllNotesOff(uint8_t ch) {
    sendCC(ch, 123, 0);
}

void sendBankPC(uint8_t ch, uint8_t msb, uint8_t lsb, uint8_t pc) {
    MIDI.controlChange(0,  msb, ch);   // CC0  Bank Select MSB
    MIDI.controlChange(32, lsb, ch);   // CC32 Bank Select LSB
    MIDI.programChange(pc, ch);        // Program Change
}
