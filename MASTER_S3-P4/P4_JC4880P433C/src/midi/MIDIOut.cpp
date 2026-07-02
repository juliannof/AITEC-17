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

// Trocea un SysEx de 12 bytes (múltiplo exacto de 3) en paquetes USB-MIDI
// class-compliant: CIN 0x4 = SysEx continúa, CIN 0x7 = termina con 3 bytes.
static void sendSysEx12(const uint8_t bytes[12]) {
    for (uint8_t i = 0; i < 12; i += 3) {
        midiEventPacket_t pkt;
        pkt.header = (i + 3 >= 12) ? 0x07 : 0x04;
        pkt.byte1  = bytes[i];
        pkt.byte2  = bytes[i + 1];
        pkt.byte3  = bytes[i + 2];
        MIDI.writePacket(&pkt);
    }
}

// SysEx DT1 "Sound Mode" (JV-2080_OM.pdf p.187-188, checksum verificado 2026-07-02):
// F0 41 <dev> 6A 12 <addr 00 00 00 00> <data> <checksum> F7
// <dev> = JV2080_DEVICE_ID (config.h) — debe coincidir con el Device ID del
// panel del synth o el JV-2080 ignora el mensaje en silencio (2026-07-02 18:31).
void sendSoundMode(JVSoundMode mode) {
    uint8_t kPerformance[12] = {0xF0,0x41,JV2080_DEVICE_ID,0x6A,0x12, 0x00,0x00,0x00,0x00, 0x00, 0x00, 0xF7};
    uint8_t kPatch[12]       = {0xF0,0x41,JV2080_DEVICE_ID,0x6A,0x12, 0x00,0x00,0x00,0x00, 0x01, 0x7F, 0xF7};
    sendSysEx12(mode == JVSoundMode::PERFORMANCE ? kPerformance : kPatch);
}
