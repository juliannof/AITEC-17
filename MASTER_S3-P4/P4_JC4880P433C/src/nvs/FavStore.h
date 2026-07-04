// nvs/FavStore.h — Almacén de favoritos MIDI en NVS  (AITEC 2026-06-30)
// Cada entrada guarda {synth, canal, banco, PC, nombre}.
// El nombre se rellena automáticamente via jvPatchName() al guardar.
#pragma once
#include <stdint.h>
#include "../config.h"   // ExSynth

struct FavEntry {
    ExSynth     synth;    // sintetizador al que pertenece
    uint8_t     ch;       // canal MIDI (1-16)
    uint8_t     msb;      // Bank Select MSB
    uint8_t     lsb;      // Bank Select LSB
    uint8_t     pc;       // Program Change (0-based, 0-127)
    JVSoundMode mode;     // Patch o Performance (2026-07-02) — mismos msb/lsb en
                           // ambos modos, hace falta para no confundir favoritos
    char        name[20]; // nombre del patch, null-terminated
};

bool favInit();          // llama una vez en setup()
int  favCount();         // número de slots guardados (0 = vacío)
bool favLoad(int idx, FavEntry& out);   // false si slot vacío
bool favSave(int idx, const FavEntry& e);
void favDelete(int idx);

// Primer slot libre para guardar un favorito nuevo — reutiliza huecos dejados
// por favDelete() (que no decrementa el contador ni compacta) antes de
// extender al final. -1 si no hay hueco y ya se llegó al tope (128) (2026-07-04).
int favFirstFreeSlot();

// Última selección en Tab Sonidos (persiste entre boots) — solo Patch mode;
// Performance no se persiste como "último banco" porque UIBank siempre
// arranca en Patch mode (2026-07-02, evita desincronizar el Sound Mode real
// del JV-2080, que no se puede leer de vuelta).
bool favSaveLastSel(uint8_t msb, uint8_t lsb, uint8_t pc);
bool favLoadLastSel(uint8_t& msb, uint8_t& lsb, uint8_t& pc);

// Canal MIDI activo (persiste entre boots)
bool favSaveMidiChannel(uint8_t ch);
bool favLoadMidiChannel(uint8_t& ch);   // false si no había nada guardado (deja ch sin tocar)

// Marca out[pc]=true para cada patch del banco (msb/lsb/mode) guardado como favorito.
// out debe tener ≥ outLen elementos (uso: grid Tab Sonidos, un solo escaneo de NVS por reconstrucción).
void favMarkBank(ExSynth synth, uint8_t ch, uint8_t msb, uint8_t lsb, JVSoundMode mode, bool* out, int outLen);

// Índice del slot que guarda este patch como favorito, o -1 si no está guardado.
int favFindIndex(ExSynth synth, uint8_t ch, uint8_t msb, uint8_t lsb, uint8_t pc, JVSoundMode mode);
