// src/hardware/Neopixels/Neopixel.h
#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../Hardware.h"
#include "../../config.h"

extern Adafruit_NeoPixel neopixels;

void initNeopixels();
void setNeopixelState(int idx, uint8_t r, uint8_t g, uint8_t b);
void clearNeopixel(int idx);
void clearAllNeopixels();
void showNeopixels();
void setNeopixelGlobalBrightness(uint8_t brightness);
uint8_t getNeopixelBrightness();

extern bool neoWaitingHandshake;

void handleButtonLedState(ButtonId id);
void updateAllNeopixels();
void forceNeopixelRefresh();  // invalida la caché de updateAllNeopixels() (2026-08-13)
