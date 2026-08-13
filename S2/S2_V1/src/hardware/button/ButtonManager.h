#pragma once
// ============================================================
//  ButtonManager.h  —  iMakie PTxx Track S2
// ============================================================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <functional>
#include "hardware/Hardware.h"
#include "hardware/encoder/Encoder.h"

class SatMenu;

namespace ButtonManager {

    void begin(LovyanGFX* tft, SatMenu* sat);
    void update();
    void setSatMenu(SatMenu* sat);
    void setOtaCallback(std::function<void()> cb);  // clic encoder sin Logic → activar OTA (2026-08-13)

    uint8_t getButtonFlags();
    void    clearButtonFlags();

    uint8_t getEncoderButton();    // ← AÑADIR
    void    clearEncoderButton();  // ← AÑADIR

} // namespace ButtonManager