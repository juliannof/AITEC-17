// kaoss/KaossPad.h — ExPressif XY pad logic  (AITEC 2026-06-29 → 2026-07-14: 20 slots NVS)
#pragma once
#include <Arduino.h>
#include "../config.h"
#include "../nvs/KaosStore.h"

class KaossPad {
public:
    // Convierte coordenada de pantalla (relativa al pad) → CC 0-127
    uint8_t mapXtoCC(uint16_t pad_x) const;
    uint8_t mapYtoCC(uint16_t pad_y) const;   // invertido: arriba=127

    // Preset activo — selección SIEMPRE directa (0-19, ver mapeo físico
    // NeoTrellis L3/L7/L11/L15 + R0-R15). Recarga el slot desde NVS al
    // cambiar — no hay "siguiente" (L2 retirado, 2026-07-14).
    void        setPreset(uint8_t n);
    void        syncToSynth();          // recarga slot+canal tras cambiar de synth
    void        reload();                // recarga slot+canal (llamar tras guardar en el editor)
    uint8_t     getPreset()   const { return _preset; }

    bool        hasPreset()   const { return _current.configured != 0; }
    uint8_t     getCCX()      const { return _current.ccX; }
    uint8_t     getCCY()      const { return _current.ccY; }
    // Canal MIDI del synth activo (1-16) — único por synth, no por slot
    // (corrección 2026-07-14: "el canal midi es unico para el sinte, no por preset").
    uint8_t     getChannel()  const { return _channel; }
    const char* nameX()       const;    // nombre del parámetro X (KaosParams) — "—" si vacío/no catalogado
    const char* nameY()       const;

private:
    uint8_t  _preset  = 0;
    KaosSlot _current{0, 0, 0};
    uint8_t  _channel = 1;

    void reloadInternal();   // recarga _current (slot) — llamado por setPreset()
    void reloadChannel();    // recarga _channel (por synth) — llamado por syncToSynth()/reload()
};

extern KaossPad kaoss;
