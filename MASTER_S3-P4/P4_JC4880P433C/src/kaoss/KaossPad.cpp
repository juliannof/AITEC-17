// kaoss/KaossPad.cpp — ExPressif XY pad logic  (AITEC 2026-06-29 → 2026-07-14: 20 slots NVS)
#include "KaossPad.h"
#include "KaosParams.h"

KaossPad kaoss;

void KaossPad::reloadInternal() {
    if (!kaosLoad(g_currentSynth, _preset, _current))
        _current = {0, 0, 0};
}

void KaossPad::setPreset(uint8_t n) {
    if (n >= KAOS_SLOTS) return;
    _preset = n;
    reloadInternal();
}

void KaossPad::reloadChannel() {
    _channel = kaosLoadChannel(g_currentSynth);
}

void KaossPad::syncToSynth() {
    reloadInternal();   // mismo índice de slot, datos del nuevo synth
    reloadChannel();    // canal es por synth, no por slot (2026-07-14)
}

void KaossPad::reload() {
    reloadInternal();
    reloadChannel();
}

const char* KaossPad::nameX() const {
    if (!hasPreset()) return "—";
    const char* n = kaosParamName(g_currentSynth, _current.ccX);
    return n ? n : "—";
}

const char* KaossPad::nameY() const {
    if (!hasPreset()) return "—";
    const char* n = kaosParamName(g_currentSynth, _current.ccY);
    return n ? n : "—";
}

uint8_t KaossPad::mapXtoCC(uint16_t pad_x) const {
    if (pad_x >= PAD_SIZE) return 127;
    return (uint8_t)((pad_x * 127UL) / (PAD_SIZE - 1));
}

uint8_t KaossPad::mapYtoCC(uint16_t pad_y) const {
    if (pad_y >= PAD_SIZE) return 0;
    return (uint8_t)(127 - (pad_y * 127UL) / (PAD_SIZE - 1));
}
