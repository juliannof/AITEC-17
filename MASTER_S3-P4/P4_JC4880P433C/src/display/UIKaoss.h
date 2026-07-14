// display/UIKaoss.h — ExPressif main screen  (AITEC 2026-06-29 → 2026-07-14: botón PRESET)
#pragma once
#include "lvgl.h"

void uiKaossCreate(lv_obj_t* parent);
void uiKaossDestroy();
void uiKaossUpdatePreset();  // refresca sub-label del botón PRESET (antes SCALE) — nombre param X/Y
void uiKaossUpdateHold();    // refresca visual del botón HOLD
void uiKaossUpdateSynth();   // refresca botón TAP con sintetizador activo
void uiKaossUpdateLeds();    // refresca rejilla 8×8 con color del modo activo
void uiKaossStartScroll();   // arranca animación "ExPressive" (boot + idle)
