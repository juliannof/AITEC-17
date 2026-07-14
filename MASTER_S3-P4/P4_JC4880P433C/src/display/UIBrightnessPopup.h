// display/UIBrightnessPopup.h — Popup transitorio de brillo de pantalla (AITEC 2026-07-14)
// L2 = brillo −, L6 = brillo + (NeoTrellis panel izq., ver NeoTrellis.cpp).
#pragma once
#include "lvgl.h"

void uiBrightnessPopupCreate(lv_obj_t* parent);   // llama al final de setup (topmost z-order)
void uiBrightnessPopupShow(uint8_t percent);       // muestra el número, se oculta sola tras ~1.2s
