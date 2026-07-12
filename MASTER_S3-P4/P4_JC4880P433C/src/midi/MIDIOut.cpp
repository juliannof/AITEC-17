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

void sendPC(uint8_t ch, uint8_t pc) {
    MIDI.programChange(pc, ch);
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

// Trocea un SysEx de longitud arbitraria (2026-07-04, para el Triton — 8 bytes,
// no múltiplo de 3 como el sendSysEx12 del JV-2080). CIN 0x04=continúa(3),
// 0x05/0x06/0x07=termina con 1/2/3 bytes (USB-MIDI class-compliant estándar).
static void sendSysEx(const uint8_t* bytes, uint8_t len) {
    uint8_t i = 0;
    while (i < len) {
        uint8_t remain = len - i;
        midiEventPacket_t pkt = {};
        if (remain >= 4)      { pkt.header = 0x04; pkt.byte1 = bytes[i]; pkt.byte2 = bytes[i+1]; pkt.byte3 = bytes[i+2]; i += 3; }
        else if (remain == 3) { pkt.header = 0x07; pkt.byte1 = bytes[i]; pkt.byte2 = bytes[i+1]; pkt.byte3 = bytes[i+2]; i += 3; }
        else if (remain == 2) { pkt.header = 0x06; pkt.byte1 = bytes[i]; pkt.byte2 = bytes[i+1]; pkt.byte3 = 0;          i += 2; }
        else                  { pkt.header = 0x05; pkt.byte1 = bytes[i]; pkt.byte2 = 0;          pkt.byte3 = 0;          i += 1; }
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

// SysEx MODE CHANGE del Triton (TRITON_Rack_MIDIimp.TXT, Func 4E, nota *11) —
// único mecanismo para alternar Program/Combination ya que comparten Bank
// Select (mismos MSB/LSB por banco, ver TritonPatches.h) (2026-07-04).
// F0 42 3g 50 4E 00 mm F7 — g=canal (nibble 0-15), mm: 0=COMBI PLAY, 2=PROG PLAY.
void sendTritonMode(uint8_t ch, bool programMode) {
    uint8_t chNibble = (uint8_t)((ch - 1) & 0x0F);
    uint8_t mode     = programMode ? TRITON_MODE_PROG : TRITON_MODE_COMBI;
    uint8_t msg[8]   = {0xF0, KORG_ID, (uint8_t)(0x30 | chNibble), TRITON_MODEL_ID, 0x4E, 0x00, mode, 0xF7};
    sendSysEx(msg, 8);
}
