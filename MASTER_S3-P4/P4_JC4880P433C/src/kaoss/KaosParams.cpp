// kaoss/KaosParams.cpp — Catálogo de parámetros MIDI nombrados por synth (AITEC 2026-07-14)
#include "KaosParams.h"

// JV-2080 — brief §4: Filtro (RELATIVE_OFFSET_64), Tone Level (RELATIVE_OFFSET_64,
// solo GP5/GP7 — GP6/GP8 no tienen memoria propia en el catálogo verificado),
// Sends (ABSOLUTE, solo funciona en modo Performance).
static const KaosParam kParamsJV2080[] = {
    {74, "Cutoff"},
    {71, "Resonance"},
    {80, "Tone1 Lvl"},
    {82, "Tone3 Lvl"},
    {91, "Reverb Snd"},
    {93, "Chorus Snd"},
};

// TRITON Rack — brief §4 Grupo A (D-mod Grupo B bloqueado, no incluido).
static const KaosParam kParamsTriton[] = {
    {74, "Cutoff"},
    {71, "Resonance"},
    {79, "Filter EG"},
    {72, "Release"},
    {76, "LFO1 Speed"},
    {77, "LFO1 Depth"},
};

// MOTIF-RACK — brief §4: solo Filtro verificado (1/10).
static const KaosParam kParamsMotif[] = {
    {74, "Cutoff"},
    {71, "Resonance"},
};

// WAVE (Wavestation VST) — presets originales del proyecto (2026-06-29),
// no forman parte del catálogo del brief (destino VST, no rack físico).
static const KaosParam kParamsWave[] = {
    { 1, "Mod Wheel"},
    { 2, "Breath"},
    {16, "Joy X"},
    {17, "Joy Y"},
    {74, "Cutoff"},
    {71, "Resonance"},
    {91, "Reverb Snd"},
    {93, "Chorus Snd"},
};

// TG55 / D-110 — sin catálogo verificado (brief §4: 0/10).

const KaosParam* kaosParamList(ExSynth synth, uint8_t& count) {
    switch (synth) {
        case ExSynth::JV2080: count = sizeof(kParamsJV2080) / sizeof(kParamsJV2080[0]); return kParamsJV2080;
        case ExSynth::TRITON: count = sizeof(kParamsTriton) / sizeof(kParamsTriton[0]); return kParamsTriton;
        case ExSynth::MOTIF:  count = sizeof(kParamsMotif)  / sizeof(kParamsMotif[0]);  return kParamsMotif;
        case ExSynth::WAVE:   count = sizeof(kParamsWave)   / sizeof(kParamsWave[0]);   return kParamsWave;
        case ExSynth::TG55:
        case ExSynth::D110:
        default:               count = 0; return nullptr;
    }
}

const char* kaosParamName(ExSynth synth, uint8_t cc) {
    uint8_t count;
    const KaosParam* list = kaosParamList(synth, count);
    for (uint8_t i = 0; i < count; i++)
        if (list[i].cc == cc) return list[i].name;
    return nullptr;
}
