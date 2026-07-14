// midi/MIDIClock.h — Recepción de MIDI Clock por USB (AITEC 2026-07-14)
// Metrónomo visual en L5 (NeoTrellis) sincronizado al MIDI Clock ENTRANTE
// (24 PPQN, mensajes realtime 0xF8/0xFA/0xFB/0xFC) — no hay BPM propio en
// este firmware, se sigue el reloj que manda Logic/el DAW por USB.
#pragma once
#include <stdint.h>

// Llamar en taskCore0 (main.cpp) cada ciclo — drena los paquetes USB MIDI
// entrantes. Único lector de MIDI.readPacket() en el firmware.
void midiClockPoll();

// true una sola vez por negra (cada 24 Clock) desde la última llamada —
// consume el pulso (siguiente llamada devuelve false hasta el próximo beat).
bool midiClockConsumeBeat();

// true tras Start(0xFA)/Continue(0xFB), false tras Stop(0xFC). El conteo de
// Clock sigue actualizándose aunque esté parado (para no perder la fase),
// pero el consumidor (NeoTrellis) debe ignorar los beats si esto es false.
bool midiClockIsRunning();
