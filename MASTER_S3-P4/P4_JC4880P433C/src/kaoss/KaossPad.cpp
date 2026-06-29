// kaoss/KaossPad.cpp — ExPressif XY pad logic  (AITEC 2026-06-29)
#include "KaossPad.h"

KaossPad kaoss;

// ── Definición de escalas ─────────────────────────────────────────────
const uint8_t KaossPad::SCALES[NUM_SCALES][12] = {
    {0,2,4,5,7,9,11, 0,0,0,0,0},   // Major      (7 notas)
    {0,2,3,5,7,8,10, 0,0,0,0,0},   // Minor nat. (7 notas)
    {0,2,4,7,9,  0,0,0,0,0,0,0},   // Pentatonic (5 notas)
    {0,1,2,3,4,5,6,7,8,9,10,11},   // Chromatic  (12 notas)
};
const uint8_t KaossPad::SCALE_LEN[NUM_SCALES] = {7, 7, 5, 12};

const char* KaossPad::SCALE_NAMES[NUM_SCALES] = {"MAJOR","MINOR","PENTA","CHROM"};
const char* KaossPad::ROOT_NAMES[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

// ── Setters ───────────────────────────────────────────────────────────
void KaossPad::setScale(uint8_t s)  { _scale  = (s < NUM_SCALES)  ? s : 0; }
void KaossPad::setRoot(uint8_t r)   { _root   = (r < 12)          ? r : 0; }
void KaossPad::setOctave(uint8_t o) { _octave = (o >= OCTAVE_MIN && o <= OCTAVE_MAX) ? o : OCTAVE_DEFAULT; }

// ── CC mapping ───────────────────────────────────────────────────────
uint8_t KaossPad::mapXtoCC(uint16_t pad_x) const {
    if (pad_x >= PAD_SIZE) return 127;
    return (uint8_t)((pad_x * 127UL) / (PAD_SIZE - 1));
}

uint8_t KaossPad::mapYtoCC(uint16_t pad_y) const {
    if (pad_y >= PAD_SIZE) return 0;
    // Invertido: arriba del pad (y=0) → CC 127
    return (uint8_t)(127 - (pad_y * 127UL) / (PAD_SIZE - 1));
}

// ── Nota desde posición Y ─────────────────────────────────────────────
uint8_t KaossPad::noteFromY(uint16_t pad_y) const {
    const uint8_t* scale = SCALES[_scale];
    uint8_t        len   = SCALE_LEN[_scale];
    uint8_t total        = len * 2;   // 2 octavas de rango

    // Arriba = nota más alta
    uint16_t inv_y = (pad_y < PAD_SIZE) ? (PAD_SIZE - 1 - pad_y) : 0;
    uint8_t  idx   = (uint8_t)((inv_y * (uint32_t)total) / PAD_SIZE);
    if (idx >= total) idx = total - 1;

    uint8_t oct_offset = (idx / len) * 12;
    uint8_t note = _root + _octave * 12 + scale[idx % len] + oct_offset;
    return (note > 127) ? 127 : note;
}

// ── Nombres ───────────────────────────────────────────────────────────
const char* KaossPad::scaleName() const { return SCALE_NAMES[_scale]; }
const char* KaossPad::rootName()  const { return ROOT_NAMES[_root]; }
