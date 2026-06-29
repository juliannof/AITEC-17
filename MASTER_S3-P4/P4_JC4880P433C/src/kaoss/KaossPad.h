// kaoss/KaossPad.h — ExPressif XY pad logic  (AITEC 2026-06-29)
#pragma once
#include <Arduino.h>
#include "../config.h"

class KaossPad {
public:
    void setScale(uint8_t scale);
    void setRoot(uint8_t root);
    void setOctave(uint8_t octave);

    // Convierte coordenada de pantalla (relativa al pad) → CC 0-127
    uint8_t mapXtoCC(uint16_t pad_x) const;
    uint8_t mapYtoCC(uint16_t pad_y) const;   // invertido: arriba=127

    // Y → nota MIDI en la escala activa (abarca 2 octavas)
    uint8_t noteFromY(uint16_t pad_y) const;

    uint8_t     getScale()  const { return _scale; }
    uint8_t     getRoot()   const { return _root; }
    uint8_t     getOctave() const { return _octave; }
    const char* scaleName() const;
    const char* rootName()  const;

private:
    uint8_t _scale  = SCALE_MAJOR;
    uint8_t _root   = 0;   // C
    uint8_t _octave = OCTAVE_DEFAULT;

    static const uint8_t SCALES[NUM_SCALES][12];
    static const uint8_t SCALE_LEN[NUM_SCALES];
    static const char*   SCALE_NAMES[NUM_SCALES];
    static const char*   ROOT_NAMES[12];
};

extern KaossPad kaoss;
