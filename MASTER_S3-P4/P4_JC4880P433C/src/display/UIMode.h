// display/UIMode.h — Modos de interfaz ExPressif / Lights (2026-10-12 13:22)
// Botón de función: L1 del NeoTrellis y botón "MODO" de la pantalla.
// Dos modos: EXPRESSIF (Kaos + Banco) ↔ LIGHTS. El Banco es una superposición
// dentro de ExPressif, no un modo.
#pragma once
#include "../config.h"

UiMode uiModeCurrent();      // LIGHTS si la pantalla Lights está abierta; si no, EXPRESSIF
void   uiModeNext();         // alterna EXPRESSIF ↔ LIGHTS (L1, botón táctil, toque en Lights)
void   uiModeBankToggle();   // L4 largo / tecla de synth ya activo: abre/cierra Banco (solo en EXPRESSIF)
