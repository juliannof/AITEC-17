// src/display/UIPage3.h
#pragma once
#include "lvgl.h"

void uiPage3Create();
void uiPage3Update();  // llamar cuando cambien datos MIDI
void uiToggleSlider();
void uiPage3Destroy();
void uiPage3Create(lv_obj_t* parent);
