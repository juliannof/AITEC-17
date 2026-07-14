// midi/MIDIClock.cpp — Recepción de MIDI Clock por USB (AITEC 2026-07-14)
#include "MIDIClock.h"
#include <Arduino.h>
#include <USBMIDI.h>

extern USBMIDI MIDI;

static uint8_t  s_clockCount  = 0;    // 0-23 (24 PPQN por negra)
static bool     s_running     = false;
static bool     s_beatFlag    = false;
static uint32_t s_lastBeatMs  = 0;    // 0 = sin referencia previa
static uint16_t s_bpm         = 0;    // 0 = sin dato (2026-07-14)

void midiClockPoll() {
    midiEventPacket_t pkt;
    while (MIDI.readPacket(&pkt)) {
        // No se filtra por CIN (Code Index Number) del header USB-MIDI: en
        // teoría los mensajes realtime (Clock/Start/Continue/Stop) viajan con
        // CIN 0xF ("Single Byte"), pero distintos hosts/drivers USB-MIDI son
        // inconsistentes históricamente y algunos los mandan con CIN 0x5
        // ("SysEx ends 1 byte / system common 1 byte"). El byte de estado
        // realtime (0xF8/0xFA/0xFB/0xFC) es inequívoco por sí solo — un byte
        // de datos SysEx/CC nunca vale ≥0x80, así que comparar pkt.byte1
        // directamente es seguro sin mirar el CIN.
        switch (pkt.byte1) {
            case 0xF8:   // Clock — si llega, se asume transporte corriendo
                         // (no depender de haber visto Start/Continue antes,
                         // 2026-07-14: la captura de MIDI Monitor del usuario
                         // ya mostraba Clock sin Start visible en esa ventana)
                s_running = true;
                if (s_clockCount == 0) {
                    s_beatFlag = true;
                    // BPM = 60000 / intervalo entre negras consecutivas (2026-07-14)
                    uint32_t now = millis();
                    if (s_lastBeatMs != 0) {
                        uint32_t interval = now - s_lastBeatMs;
                        if (interval > 0) s_bpm = (uint16_t)(60000UL / interval);
                    }
                    s_lastBeatMs = now;
                }
                s_clockCount = (uint8_t)((s_clockCount + 1) % 24);
                break;
            case 0xFA:   // Start
            case 0xFB:   // Continue
                log_i("[MIDIClock] Start/Continue (0x%02X)", pkt.byte1);
                s_clockCount = 0;
                s_running    = true;
                s_beatFlag   = true;   // primer beat inmediato
                // s_lastBeatMs a 0 evita mezclar el intervalo con el tramo
                // anterior (BPM falso) — pero NO se toca s_bpm: el readout
                // en pantalla debe ser persistente (2026-07-14, petición del
                // usuario), sigue mostrando el último tempo conocido hasta
                // que se mida un intervalo nuevo válido.
                s_lastBeatMs = 0;
                break;
            case 0xFC:   // Stop
                log_i("[MIDIClock] Stop");
                s_running    = false;
                s_lastBeatMs = 0;      // igual que arriba — s_bpm se conserva
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

// Persistente (2026-07-14) — no depende de s_running, mantiene el último
// tempo conocido aunque el transporte esté parado.
uint16_t midiClockGetBPM() { return s_bpm; }
