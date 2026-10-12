// display/UIMode.cpp — Modos de interfaz ExPressif / Lights (2026-10-12 13:22)
// Solo Core1 (LVGL). El modo se deriva del estado real de la pantalla Lights,
// sin variable propia que pueda quedar desincronizada.
#include "UIMode.h"
#include "UIBank.h"
#include "UILights.h"

UiMode uiModeCurrent() {
    return uiLightsIsOpen() ? UiMode::LIGHTS : UiMode::EXPRESSIF;
}

void uiModeNext() {
    if (uiModeCurrent() == UiMode::EXPRESSIF) {      // → LIGHTS
        if (uiBankIsOpen()) uiBankHide();            // el Banco pertenece a ExPressif
        uiLightsShow();
    } else {                                         // → EXPRESSIF
        uiLightsHide();
    }
}

void uiModeBankToggle() {
    if (uiModeCurrent() != UiMode::EXPRESSIF) return;   // en Lights no hay Banco
    if (uiBankIsOpen()) uiBankHide();
    else                uiBankShow();
}
