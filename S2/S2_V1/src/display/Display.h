// src/display/Display.h
#pragma once
#include <Arduino.h>
#include "LovyanGFX_config.h"   // ← reemplaza <TFT_eSPI.h>
#include "../config.h"

extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsHeaderRedraw;
extern bool needsVUMetersRedraw;
extern bool needsVPotRedraw;

extern volatile ConnectionState logicConnectionState;

void initDisplay(bool otaOnlyMode = false);
void updateDisplay();
void setScreenBrightness(uint8_t brightness);
void drawOfflineScreen();
void drawInitializingScreen();
void drawHeaderSprite();
void drawMainArea();
void drawVUMeters();
void handleVUMeterDecay();
void setVPotLevel(int8_t level);
void setVPotRaw  (uint8_t raw);
void setAutoMode(uint8_t mode);
void setTrackId(uint8_t id);
void drawSplashScreen();


void drawButton(LGFX_Sprite &sprite,                  // ← LGFX_Sprite
                uint16_t x, uint16_t y,
                uint16_t w, uint16_t h,
                const char* label,
                bool active,
                uint16_t activeColor);

