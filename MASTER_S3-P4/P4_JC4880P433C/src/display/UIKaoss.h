// display/UIKaoss.h — ExPressif main screen  (AITEC 2026-06-29)
#pragma once
#include "lvgl.h"

void uiKaossCreate(lv_obj_t* parent);
void uiKaossDestroy();
void uiKaossUpdateScale();   // refresca sub-label del botón SCALE
void uiKaossUpdateHold();    // refresca visual del botón HOLD
void uiKaossUpdateLeds();    // refresca rejilla 8×8 con color del modo activo
void uiKaossStartScroll();   // arranca animación "ExPressive" (boot + idle)
