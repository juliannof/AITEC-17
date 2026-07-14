// nvs/KaosStore.cpp — Almacén NVS de memorias Kaos por synth (AITEC 2026-07-14)
// NVS namespace "kaos". Clave slot: "s<synth>_<slot 2 dígitos>". Clave canal: "c<synth>".
#include "KaosStore.h"
#include <Preferences.h>
#include <stdio.h>

static Preferences s_prefs;

static void keyFor(ExSynth synth, uint8_t slot, char* out, size_t outLen) {
    snprintf(out, outLen, "s%u_%02u", (unsigned)synth, (unsigned)slot);
}

bool kaosLoad(ExSynth synth, uint8_t slot, KaosSlot& out) {
    if (slot >= KAOS_SLOTS) { out = {0, 0, 0}; return false; }
    char key[8];
    keyFor(synth, slot, key, sizeof(key));
    size_t sz = s_prefs.getBytesLength(key);
    if (sz != sizeof(KaosSlot)) { out = {0, 0, 0}; return false; }
    s_prefs.getBytes(key, &out, sizeof(KaosSlot));
    return out.configured != 0;
}

bool kaosSave(ExSynth synth, uint8_t slot, const KaosSlot& s) {
    if (slot >= KAOS_SLOTS) return false;
    char key[8];
    keyFor(synth, slot, key, sizeof(key));
    return s_prefs.putBytes(key, &s, sizeof(KaosSlot)) == sizeof(KaosSlot);
}

uint8_t kaosLoadChannel(ExSynth synth) {
    char key[4];
    snprintf(key, sizeof(key), "c%u", (unsigned)synth);
    uint8_t ch = s_prefs.getUChar(key, 1);
    return (ch >= 1 && ch <= 16) ? ch : 1;
}

bool kaosSaveChannel(ExSynth synth, uint8_t ch) {
    if (ch < 1 || ch > 16) return false;
    char key[4];
    snprintf(key, sizeof(key), "c%u", (unsigned)synth);
    return s_prefs.putUChar(key, ch) == 1;
}

// ── Semilla inicial — catálogo verificado aitec_kaos_brief_2026-07-13.md §4 ──
// Canal MIDI NO se siembra aquí — kaosLoadChannel() ya devuelve 1 por defecto
// si nunca se guardó (un único valor por synth, editable después desde
// UIKaosEdit). Se escribe una sola vez (flag "seeded"). WAVE conserva los 4
// presets originales del proyecto (2026-06-29), fuera del catálogo del brief
// (destino VST, no rack físico).
struct KaosDefault { ExSynth synth; uint8_t slot; uint8_t ccX; uint8_t ccY; };
static const KaosDefault kDefaults[] = {
    {ExSynth::JV2080, 0, 74, 71},   // Cutoff / Resonance
    {ExSynth::JV2080, 1, 80, 82},   // Tone1 Lvl / Tone3 Lvl
    {ExSynth::JV2080, 2, 91, 93},   // Reverb Snd / Chorus Snd — ⚠️ solo Performance
    {ExSynth::TRITON, 0, 74, 71},   // Cutoff / Resonance
    {ExSynth::TRITON, 1, 74, 79},   // Cutoff / Filter EG
    {ExSynth::TRITON, 2, 74, 72},   // Cutoff / Release
    {ExSynth::TRITON, 3, 76, 77},   // LFO1 Speed / LFO1 Depth
    {ExSynth::MOTIF,  0, 74, 71},   // Cutoff / Resonance
    {ExSynth::WAVE,   0,  1,  2},   // Mod Wheel / Breath
    {ExSynth::WAVE,   1, 16, 17},   // Joy X / Joy Y
    {ExSynth::WAVE,   2, 74, 71},   // Cutoff / Resonance
    {ExSynth::WAVE,   3, 91, 93},   // Reverb Snd / Chorus Snd
};
#define KAOS_DEFAULTS_N (sizeof(kDefaults) / sizeof(kDefaults[0]))

bool kaosInit() {
    bool ok = s_prefs.begin("kaos", false);
    if (!s_prefs.getUChar("seeded", 0)) {
        for (size_t i = 0; i < KAOS_DEFAULTS_N; i++) {
            const KaosDefault& d = kDefaults[i];
            KaosSlot s{d.ccX, d.ccY, 1};
            kaosSave(d.synth, d.slot, s);
        }
        s_prefs.putUChar("seeded", 1);
    }
    return ok;
}
