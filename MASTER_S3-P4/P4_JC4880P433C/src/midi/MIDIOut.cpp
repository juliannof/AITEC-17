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
