// nvs/FavStore.h — Almacén de favoritos MIDI en NVS  (AITEC 2026-06-30)
// Cada entrada guarda {synth, canal, banco, PC, nombre}.
// El nombre se rellena automáticamente via jvPatchName() al guardar.
#pragma once
#include <stdint.h>
#include "../config.h"   // ExSynth

struct FavEntry {
    ExSynth synth;      // sintetizador al que pertenece
    uint8_t ch;         // canal MIDI (1-16)
    uint8_t msb;        // Bank Select MSB
    uint8_t lsb;        // Bank Select LSB
    uint8_t pc;         // Program Change (0-based, 0-127)
    char    name[20];   // nombre del patch, null-terminated
};

bool favInit();          // llama una vez en setup()
int  favCount();         // número de slots guardados (0 = vacío)
bool favLoad(int idx, FavEntry& out);   // false si slot vacío
bool favSave(int idx, const FavEntry& e);
void favDelete(int idx);

// Última selección en Tab Sonidos (persiste entre boots)
bool favSaveLastSel(uint8_t msb, uint8_t lsb, uint8_t pc);
bool favLoadLastSel(uint8_t& msb, uint8_t& lsb, uint8_t& pc);
