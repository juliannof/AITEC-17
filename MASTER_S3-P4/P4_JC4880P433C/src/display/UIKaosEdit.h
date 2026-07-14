// display/UIKaosEdit.h — Editor de memoria Kaos: parámetros X/Y + canal MIDI (AITEC 2026-07-14)
#pragma once
#include "lvgl.h"

void uiKaosEditCreate(lv_obj_t* parent);   // llama una vez en setup (oculto por defecto)
void uiKaosEditShow();                      // abre editor del slot activo (kaoss.getPreset(), g_currentSynth)
void uiKaosEditHide();
bool uiKaosEditIsOpen();
