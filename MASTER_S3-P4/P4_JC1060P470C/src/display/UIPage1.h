#pragma once
#include "lvgl.h"

// ⚠️  THREAD SAFETY: todas estas funciones deben llamarse desde Core 1
//     (el task LVGL). Si se invoca desde Core 0 (MIDI), usar un flag
//     y ejecutar la actualización en el tick de LVGL.

// Crea el grid de 32 botones dentro de `parent`.
// Usar displayGetContentArea() como parent.
void uiPage1Create(lv_obj_t* parent);

void uiPage1Update();

// Actualiza el estado visual de un botón (0–31).
// Llama esto cuando llega feedback de Logic via RS485.
void uiPage1UpdateButton(int index, bool active);

// Refresca los 32 botones desde btnStatePG1[] de config.h
void uiPage1UpdateAllButtons();

// Cambia texto a amarillo + etiquetas SHIFT cuando shiftActive=true
void uiPage1SetShift(bool shiftActive);

// Muestra u oculta toda la página
void uiPage1SetVisible(bool visible);

// Destruye todos los objetos LVGL
void uiPage1Destroy();

// Devuelve el objeto raíz
lv_obj_t* uiPage1GetRoot();