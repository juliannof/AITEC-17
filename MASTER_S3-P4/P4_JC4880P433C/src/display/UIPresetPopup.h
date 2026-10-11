// display/UIPresetPopup.h — Popup transitorio de preset Kaoss (AITEC 2026-10-11 19:32)
// Aparece al pulsar una tecla de preset del NeoTrellis (R0-R15, L3/L7/L11/L15).
// Mismo patrón que UIBrightnessPopup.
#pragma once
#include <stdint.h>
#include "lvgl.h"

void uiPresetPopupCreate(lv_obj_t* parent);   // llamar al final de setup (topmost z-order)
// num = número a mostrar (1-20); nameX/nameY = nombres de los parámetros del preset.
// Se oculta sola tras ~1.2 s.
void uiPresetPopupShow(uint8_t num, const char* nameX, const char* nameY);
