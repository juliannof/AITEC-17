// display/UIBank.h — Página Bank: Favoritos / Sonidos / Performances  (AITEC 2026-06-30)
// Tab "Canal MIDI" eliminada (2026-07-04) — Performances ocupa su hueco físico.
#pragma once
#include "lvgl.h"
#include <stdint.h>

void uiBankCreate(lv_obj_t* parent);  // llama una vez en setup (oculto por defecto)
void uiBankDestroy();
void uiBankShow();
void uiBankHide();
bool uiBankIsOpen();
void uiBankNeoKey(uint8_t slot);      // slot 0-7 (trío NeoTrellis), llamado desde Core0 cuando Bank abierto
void uiBankNeoPage(int8_t dir);       // -1=página anterior, +1=página siguiente (2026-07-04)
void uiBankRefreshFavList();          // reconstruye grid de favoritos desde NVS
void uiBankSynthChanged();            // llamar cuando g_currentSynth cambia con Bank abierto (2026-07-04)
