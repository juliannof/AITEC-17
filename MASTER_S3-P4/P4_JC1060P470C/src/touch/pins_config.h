#pragma once

// config.h es fuente única de pines/resolución — aquí solo se heredan (2026-06-09)
#include "../config.h"

#define EXAMPLE_LVGL_PORT_TASK_MAX_DELAY_MS 500 // range 2 to 2000
#define EXAMPLE_LVGL_PORT_TASK_MIN_DELAY_MS 5   // range 1 to 100
#define EXAMPLE_LVGL_PORT_TASK_PRIORITY 4
#define EXAMPLE_LVGL_PORT_TASK_STACK_SIZE_KB 32 // KB - increased for EEZ Flow
#define EXAMPLE_LVGL_PORT_TASK_CORE -1          // range -1 to 1
#define EXAMPLE_LVGL_PORT_TICK 2                // ragne 1 to 100

#define EXAMPLE_LVGL_PORT_AVOID_TEAR_ENABLE 1

#ifdef EXAMPLE_LVGL_PORT_AVOID_TEAR_ENABLE
#define EXAMPLE_LVGL_PORT_AVOID_TEAR_MODE 3 // range 1 to 3

#define EXAMPLE_LVGL_PORT_ROTATION_DEGREE_ 0 // landscape nativo JD9165 — sin rotación (2026-06-09)
#define EXAMPLE_LVGL_PORT_PPA_ROTATION_ENABLE 1
#endif

// JD9165 1024×600 landscape nativo (2026-06-09)
#define LCD_H_RES 1024
#define LCD_V_RES 600

// Alias heredados de config.h (fuente única) (2026-06-09)
#define LCD_RST    LCD_RST_PIN
#define LCD_LED    LCD_BL_PIN

#define TP_I2C_SDA TOUCH_SDA_PIN
#define TP_I2C_SCL TOUCH_SCL_PIN
#define TP_RST     TOUCH_RST_PIN
#define TP_INT     TOUCH_INT_PIN
