// nvs/FavStore.cpp — Almacén de favoritos MIDI en NVS  (AITEC 2026-06-30)
// NVS namespace "favs". Clave por índice: "0", "1", "2"...
// "n" guarda el número total de slots (puede haber huecos vacíos entre medios).
#include "FavStore.h"
#include <Preferences.h>
#include <string.h>

static Preferences s_prefs;
static int         s_count = 0;   // cache del total (máximo índice+1)

bool favInit() {
    bool ok = s_prefs.begin("favs", false);
    s_count = (int)s_prefs.getInt("n", 0);
    return ok;
}

int favCount() { return s_count; }

bool favLoad(int idx, FavEntry& out) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    size_t sz = s_prefs.getBytesLength(key);
    if (sz != sizeof(FavEntry)) return false;
    s_prefs.getBytes(key, &out, sizeof(FavEntry));
    return true;
}

bool favSave(int idx, const FavEntry& e) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    bool ok = (s_prefs.putBytes(key, &e, sizeof(FavEntry)) == sizeof(FavEntry));
    if (ok && idx >= s_count) {
        s_count = idx + 1;
        s_prefs.putInt("n", s_count);
    }
    return ok;
}

void favDelete(int idx) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    s_prefs.remove(key);
}

// Bug (2026-07-04): favDelete() no decrementa s_count ni compacta — cada
// ciclo marcar/desmarcar el mismo sonido durante pruebas dejaba un hueco
// permanente y subía el contador, empujando los favoritos nuevos a páginas
// cada vez más lejanas de UIBank (con solo ~8 ciclos ya salían de la página 0).
// Fix: reutilizar el primer hueco antes de extender al final.
int favFirstFreeSlot() {
    for (int i = 0; i < s_count; i++) {
        FavEntry tmp;
        if (!favLoad(i, tmp)) return i;
    }
    return (s_count < 128) ? s_count : -1;
}

bool favSaveLastSel(uint8_t msb, uint8_t lsb, uint8_t pc) {
    s_prefs.putUChar("lm", msb);
    s_prefs.putUChar("ll", lsb);
    s_prefs.putUChar("lp", pc);
    return true;
}

bool favLoadLastSel(uint8_t& msb, uint8_t& lsb, uint8_t& pc) {
    msb = s_prefs.getUChar("lm", 0xFF);
    lsb = s_prefs.getUChar("ll", 0xFF);
    pc  = s_prefs.getUChar("lp", 0xFF);
    return msb != 0xFF;
}

bool favSaveMidiChannel(uint8_t ch) {
    s_prefs.putUChar("mc", ch);
    return true;
}

bool favLoadMidiChannel(uint8_t& ch) {
    uint8_t v = s_prefs.getUChar("mc", 0xFF);
    if (v == 0xFF) return false;
    ch = v;
    return true;
}

void favMarkBank(ExSynth synth, uint8_t ch, uint8_t msb, uint8_t lsb, JVSoundMode mode, bool* out, int outLen) {
    for (int i = 0; i < s_count; i++) {
        FavEntry e;
        if (!favLoad(i, e)) continue;
        if (e.synth == synth && e.ch == ch && e.msb == msb && e.lsb == lsb && e.mode == mode && e.pc < outLen)
            out[e.pc] = true;
    }
}

int favFindIndex(ExSynth synth, uint8_t ch, uint8_t msb, uint8_t lsb, uint8_t pc, JVSoundMode mode) {
    for (int i = 0; i < s_count; i++) {
        FavEntry e;
        if (!favLoad(i, e)) continue;
        if (e.synth == synth && e.ch == ch && e.msb == msb && e.lsb == lsb && e.pc == pc && e.mode == mode) return i;
    }
    return -1;
}
