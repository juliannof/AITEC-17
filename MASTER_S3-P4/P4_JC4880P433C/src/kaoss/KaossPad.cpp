// kaoss/KaossPad.cpp — ExPressif XY pad logic  (AITEC 2026-06-29)
#include "KaossPad.h"

KaossPad kaoss;

// Presets CC: cada uno asigna un par de CCs al pad XY
// NUM_SCALES = 4 (reutilizamos la constante)
const CCPreset KaossPad::_presets[NUM_SCALES] = {
    { 1,  2, "MOD "},   // mod wheel / controller 2  (Wavestation CC1/CC2)
    {16, 17, "JOY "},   // joystick X / joystick Y   (Wavestation CC16/CC17)
    {74, 71, "FILT"},   // cutoff / resonance
    {91, 93, "FX  "},   // reverb / chorus
};

void KaossPad::nextPreset() {
    _preset = (_preset + 1) % NUM_SCALES;
}

uint8_t KaossPad::mapXtoCC(uint16_t pad_x) const {
    if (pad_x >= PAD_SIZE) return 127;
    return (uint8_t)((pad_x * 127UL) / (PAD_SIZE - 1));
}

uint8_t KaossPad::mapYtoCC(uint16_t pad_y) const {
    if (pad_y >= PAD_SIZE) return 0;
    return (uint8_t)(127 - (pad_y * 127UL) / (PAD_SIZE - 1));
}
