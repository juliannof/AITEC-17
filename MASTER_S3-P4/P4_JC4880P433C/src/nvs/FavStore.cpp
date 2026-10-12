// nvs/FavStore.cpp — Almacén de favoritos MIDI en NVS  (AITEC 2026-06-30)
// NVS namespace "favs". Clave por índice: "0", "1", "2"...
// "n" guarda el número total de slots (puede haber huecos vacíos entre medios).
#include "FavStore.h"
#include <Preferences.h>
#include <string.h>

static Preferences s_prefs;
static int         s_count = 0;   // cache del total (máximo índice+1)

static void bankLastSelLoad();   // definida más abajo, junto al resto de bankLastSel*

bool favInit() {
    bool ok = s_prefs.begin("favs", false);
    s_count = (int)s_prefs.getInt("n", 0);
    bankLastSelLoad();
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

bool favSaveLastSel(ExSynth synth, uint8_t msb, uint8_t lsb, uint8_t pc) {
    s_prefs.putUChar("ls", (uint8_t)synth);
    s_prefs.putUChar("lm", msb);
    s_prefs.putUChar("ll", lsb);
    s_prefs.putUChar("lp", pc);
    return true;
}

bool favLoadLastSel(ExSynth& synth, uint8_t& msb, uint8_t& lsb, uint8_t& pc) {
    synth = (ExSynth)s_prefs.getUChar("ls", 0xFF);
    msb   = s_prefs.getUChar("lm", 0xFF);
    lsb   = s_prefs.getUChar("ll", 0xFF);
    pc    = s_prefs.getUChar("lp", 0xFF);
    return msb != 0xFF;
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

// ── Último PC por banco (2026-07-13) ──────────────────────────────────
// Cache RAM cargado una vez en favInit(). bankLastSelSet() nunca toca NVS
// directamente — solo bankLastSelFlushIfDirty() escribe, y solo si hay
// cambios (ver FavStore.h para el porqué).
struct BankLastSel { ExSynth synth; uint8_t msb; uint8_t lsb; uint8_t pc; };
#define BANK_LASTSEL_MAX 48   // 6 synths × hasta 8 bancos (MOTIF-RACK, el máximo actual)

// Dos roles (2026-10-12 13:27): [0]=Sonidos (claves "bls"/"blsn", las de siempre),
// [1]=Performances ("blc"/"blcn").
static BankLastSel s_bankLastSel[2][BANK_LASTSEL_MAX];
static int         s_bankLastSelCount[2] = {0, 0};
static bool        s_bankLastSelDirty[2] = {false, false};
static const char* const kBlsKey[2]  = {"bls",  "blc"};
static const char* const kBlsnKey[2] = {"blsn", "blcn"};

// Banco activo por instrumento y rol (2026-10-12 13:27). 0xFF = sin registro.
struct ActiveBank { uint8_t msb; uint8_t lsb; };
static ActiveBank s_activeBank[NUM_SYNTHS][2];
static bool       s_activeBankDirty = false;

static void bankLastSelLoad() {
    for (int r = 0; r < 2; r++) {
        int n = s_prefs.getInt(kBlsnKey[r], 0);
        if (n < 0) n = 0;
        if (n > BANK_LASTSEL_MAX) n = BANK_LASTSEL_MAX;
        size_t want = sizeof(BankLastSel) * (size_t)n;
        size_t got  = (want > 0) ? s_prefs.getBytes(kBlsKey[r], s_bankLastSel[r], want) : 0;
        s_bankLastSelCount[r] = (got == want) ? n : 0;   // corrupto/ausente → vacío, no reventar
        s_bankLastSelDirty[r] = false;
    }
    for (int i = 0; i < NUM_SYNTHS; i++)
        for (int r = 0; r < 2; r++) s_activeBank[i][r] = {0xFF, 0xFF};
    size_t want = sizeof(s_activeBank);
    if (s_prefs.getBytesLength("sab") == want) s_prefs.getBytes("sab", s_activeBank, want);
    s_activeBankDirty = false;
}

void bankLastSelSet(ExSynth synth, uint8_t msb, uint8_t lsb, uint8_t pc, bool perf) {
    int r = perf ? 1 : 0;
    for (int i = 0; i < s_bankLastSelCount[r]; i++) {
        BankLastSel& e = s_bankLastSel[r][i];
        if (e.synth == synth && e.msb == msb && e.lsb == lsb) {
            if (e.pc == pc) return;   // sin cambio real, no marcar dirty
            e.pc = pc;
            s_bankLastSelDirty[r] = true;
            return;
        }
    }
    if (s_bankLastSelCount[r] < BANK_LASTSEL_MAX) {
        s_bankLastSel[r][s_bankLastSelCount[r]++] = {synth, msb, lsb, pc};
        s_bankLastSelDirty[r] = true;
    }
    // Si se llena BANK_LASTSEL_MAX, se ignoran bancos nuevos en silencio —
    // no debería pasar en la práctica (48 > cualquier combinación real hoy).
}

bool bankLastSelGet(ExSynth synth, uint8_t msb, uint8_t lsb, uint8_t& pc, bool perf) {
    int r = perf ? 1 : 0;
    for (int i = 0; i < s_bankLastSelCount[r]; i++) {
        const BankLastSel& e = s_bankLastSel[r][i];
        if (e.synth == synth && e.msb == msb && e.lsb == lsb) {
            pc = e.pc;
            return true;
        }
    }
    return false;
}

void synthActiveBankSet(ExSynth synth, bool perf, uint8_t msb, uint8_t lsb) {
    int i = (int)synth;
    if (i < 0 || i >= NUM_SYNTHS) return;
    ActiveBank& a = s_activeBank[i][perf ? 1 : 0];
    if (a.msb == msb && a.lsb == lsb) return;   // sin cambio real
    a.msb = msb;
    a.lsb = lsb;
    s_activeBankDirty = true;
}

bool synthActiveBankGet(ExSynth synth, bool perf, uint8_t& msb, uint8_t& lsb) {
    int i = (int)synth;
    if (i < 0 || i >= NUM_SYNTHS) return false;
    const ActiveBank& a = s_activeBank[i][perf ? 1 : 0];
    if (a.msb == 0xFF) return false;
    msb = a.msb;
    lsb = a.lsb;
    return true;
}

void bankLastSelFlushIfDirty() {
    for (int r = 0; r < 2; r++) {
        if (!s_bankLastSelDirty[r]) continue;
        size_t sz = sizeof(BankLastSel) * (size_t)s_bankLastSelCount[r];
        if (s_prefs.putBytes(kBlsKey[r], s_bankLastSel[r], sz) == sz) {
            s_prefs.putInt(kBlsnKey[r], s_bankLastSelCount[r]);
            s_bankLastSelDirty[r] = false;
        }
    }
    if (s_activeBankDirty) {
        if (s_prefs.putBytes("sab", s_activeBank, sizeof(s_activeBank)) == sizeof(s_activeBank))
            s_activeBankDirty = false;
    }
}
