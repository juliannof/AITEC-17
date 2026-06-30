// kaoss/KaossPad.h — ExPressif XY pad logic  (AITEC 2026-06-29)
#pragma once
#include <Arduino.h>
#include "../config.h"

struct CCPreset {
    uint8_t     ccX;
    uint8_t     ccY;
    const char* name;
};

class KaossPad {
public:
    // Convierte coordenada de pantalla (relativa al pad) → CC 0-127
    uint8_t mapXtoCC(uint16_t pad_x) const;
    uint8_t mapYtoCC(uint16_t pad_y) const;   // invertido: arriba=127

    // Preset de CC activo
    void        nextPreset();
    void        setPreset(uint8_t n);
    uint8_t     getCCX()       const { return _presets[_preset].ccX; }
    uint8_t     getCCY()       const { return _presets[_preset].ccY; }
    const char* presetName()   const { return _presets[_preset].name; }
    uint8_t     getPreset()    const { return _preset; }

private:
    uint8_t _preset = 0;

    static const CCPreset _presets[NUM_SCALES];
};

extern KaossPad kaoss;
