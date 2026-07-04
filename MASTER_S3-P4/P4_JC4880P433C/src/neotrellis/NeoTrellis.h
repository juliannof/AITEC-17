// neotrellis/NeoTrellis.h — ExPressif NeoTrellis driver (AITEC 2026-06-30)
#pragma once
#include <stdint.h>

void neotrellisInit();
void neotrellisUpdate();

// Modo Bank (2026-07-04) — llamadas desde UIBank.cpp (Core1/LVGL).
// state[i]: 0=vacío/normal, 1=seleccionado (azul), 2=favorito (naranja).
// count = ítems válidos en la página activa (<= 8); el resto quedan apagados.
void neotrellisBankShowPage(const uint8_t* state, int count);
// Restaura los LEDs de modo Kaoss al cerrar Bank.
void neotrellisRestoreKaoss();
