// midi/MIDIClock.cpp — Recepción de MIDI Clock por USB (AITEC 2026-07-14)
#include "MIDIClock.h"
#include <USBMIDI.h>

extern USBMIDI MIDI;

static uint8_t s_clockCount = 0;    // 0-23 (24 PPQN por negra)
static bool    s_running    = false;
static bool    s_beatFlag   = false;

void midiClockPoll() {
    midiEventPacket_t pkt;
    while (MIDI.readPacket(&pkt)) {
        // No se filtra por CIN (Code Index Number) del header USB-MIDI: en
        // teoría los mensajes realtime (Clock/Start/Continue/Stop) viajan con
        // CIN 0xF ("Single Byte"), pero distintos hosts/drivers USB-MIDI son
        // inconsistentes históricamente y algunos los mandan con CIN 0x5
        // ("SysEx ends 1 byte / system common 1 byte") — comprobado en vivo:
        // filtrar por CIN dejaba L5 sin parpadear (2026-07-14). El byte de
        // estado realtime (0xF8/0xFA/0xFB/0xFC) es inequívoco por sí solo —
        // un byte de datos SysEx/CC nunca vale ≥0x80, así que comparar
        // pkt.byte1 directamente es seguro sin mirar el CIN.
        switch (pkt.byte1) {
            case 0xF8:   // Clock — sigue contando aunque esté parado (fase)
                if (s_clockCount == 0) s_beatFlag = true;
                s_clockCount = (uint8_t)((s_clockCount + 1) % 24);
                break;
            case 0xFA:   // Start
            case 0xFB:   // Continue
                s_clockCount = 0;
                s_running    = true;
                s_beatFlag   = true;   // primer beat inmediato
                break;
            case 0xFC:   // Stop
                s_running = false;
                break;
            default:
                break;
        }
    }
}

bool midiClockConsumeBeat() {
    if (!s_beatFlag) return false;
    s_beatFlag = false;
    return true;
}

bool midiClockIsRunning() { return s_running; }
