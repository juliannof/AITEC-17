// display/UIBank.h — Página Bank: Favoritos / Sonidos / Canal MIDI  (AITEC 2026-06-30)
#pragma once
#include "lvgl.h"
#include <stdint.h>

void uiBankCreate(lv_obj_t* parent);  // llama una vez en setup (oculto por defecto)
void uiBankDestroy();
void uiBankShow();
void uiBankHide();
bool uiBankIsOpen();
void uiBankNeoKey(uint8_t k);         // llamado desde Core0 via flag cuando Bank abierto
void uiBankRefreshFavList();          // reconstruye tileview de favoritos desde NVS
