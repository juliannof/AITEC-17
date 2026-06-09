// src/display/Display.h
#pragma once
#include <Arduino.h>
#include "lvgl.h"

void initDisplay();
void displaySetBrightness(uint8_t percent);
lv_display_t* getDisplay();
lv_obj_t* displayGetRoot();
lv_obj_t* displayGetContentArea();