// src/display/UIVPotPopup.h
#pragma once
#include "lvgl.h"

// Pop-up grande para mover un V-Pot (pan) al tocar el arco pequeño.
// dispCh = índice en vpotValues[] (solo banco P4: 8..15).
void uiVPotPopupOpen(int dispCh);
void uiVPotPopupUpdate();   // sincroniza el arco grande con vpotValues si está abierto
void uiVPotPopupClose();
bool uiVPotPopupIsOpen();
