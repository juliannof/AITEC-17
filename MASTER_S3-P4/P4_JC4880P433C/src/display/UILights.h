// display/UILights.h — Pantalla LIGHTS (AITEC 2026-10-12 13:22)
// ESTADO: RELLENO. Solo valida la lógica de modos ExPressif/Lights.
// El contenido real (control WLED por WiFi) está en docs/EXPRESSIF_WLED.md, Fase 2.
#pragma once
#include "lvgl.h"

void uiLightsCreate(lv_obj_t* parent);   // oculta por defecto; crear ANTES de los popups (z-order)
void uiLightsShow();                     // pone g_lightsOpen = true
void uiLightsHide();                     // pone g_lightsOpen = false
bool uiLightsIsOpen();
