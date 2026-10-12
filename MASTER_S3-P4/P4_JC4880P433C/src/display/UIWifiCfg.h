// display/UIWifiCfg.h — Pantalla para introducir SSID y clave WiFi (AITEC 2026-10-12 13:50)
// Campos = caja + lv_label rotado (NO lv_textarea: iba muy lento, 2026-10-12 14:00); teclado en
// pantalla = lv_button + lv_label rotados (patrón de UIBank). NO lv_keyboard:
// su lv_buttonmatrix compara el toque sin aplicar la rotación (ver UIWifiCfg.cpp).
// Se abre desde la pantalla LIGHTS. Guarda en NVS (nvs/WifiStore) y reconecta.
#pragma once
#include "lvgl.h"

void uiWifiCfgCreate(lv_obj_t* parent);   // oculta por defecto; crear tras uiLightsCreate()
void uiWifiCfgShow();
void uiWifiCfgHide();
bool uiWifiCfgIsOpen();
