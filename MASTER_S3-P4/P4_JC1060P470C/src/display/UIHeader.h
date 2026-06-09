#pragma once
#include "lvgl.h"

void uiHeaderCreate(lv_obj_t* parent);
void uiHeaderEnsureCreated(lv_obj_t* parent);
void uiHeaderUpdate();
void uiHeaderDestroy();