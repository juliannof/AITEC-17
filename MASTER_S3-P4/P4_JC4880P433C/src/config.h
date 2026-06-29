// config.h — ExPressif v1.0  (AITEC 2026-06-29)
// LVGL portrait 480×800, hardware rota a landscape 800×480
// Mapping: screen_x = LVGL_y,  screen_y = 479 − LVGL_x
#pragma once
#include <Arduino.h>

// ── Display — LVGL portrait ───────────────────────────────────────────
#define P4_W          480
#define P4_H          800

// ── Layout ExPressif (coordenadas LVGL portrait) ──────────────────────
//
//  LVGL portrait         →   pantalla landscape
//  ──────────────────────────────────────────────
//  TAP  pos(0,   0)   240×160  →  izq-abajo  (screen x:0→160,   y:240→480)
//  HOLD pos(240, 0)   240×160  →  izq-arriba (screen x:0→160,   y:0→240)
//  PAD  pos(0,   160) 480×480  →  centro     (screen x:160→640, y:0→480)
//  SCALE pos(240,640) 240×160  →  der-arriba (screen x:640→800, y:0→240)
//  PANIC pos(0,  640) 240×160  →  der-abajo  (screen x:640→800, y:240→480)
//
#define PAD_START_Y   150   // LVGL y donde empieza el pad (= 150px strip lateral en pantalla)
#define PAD_SIZE      500   // pad: extensión LVGL y (= 500px ancho en pantalla)
#define BTN_W         240   // botón: extensión en LVGL x (= 240px vertical en pantalla)
#define BTN_H         150   // botón: extensión en LVGL y (= 150px horizontal en pantalla)

// ── NeoTrellis I2C (v2) ───────────────────────────────────────────────
#define TRELLIS_SDA_PIN   33
#define TRELLIS_SCL_PIN   31
#define TRELLIS_ADDR_L    0x2F
#define TRELLIS_ADDR_R    0x2E

// ── MIDI ──────────────────────────────────────────────────────────────
#define MIDI_CH           1
#define CC_X              74    // eje horizontal (cutoff / brillo)
#define CC_Y              71    // eje vertical   (resonancia / depth)
#define NOTE_VELOCITY     100

// ── Escalas ───────────────────────────────────────────────────────────
#define SCALE_MAJOR       0
#define SCALE_MINOR       1
#define SCALE_PENTA       2
#define SCALE_CHROMATIC   3
#define NUM_SCALES        4

// ── Octava ────────────────────────────────────────────────────────────
#define OCTAVE_DEFAULT    4
#define OCTAVE_MIN        1
#define OCTAVE_MAX        7

// ── Splash ────────────────────────────────────────────────────────────
#define BOOT_SCREEN_MS    3000

// ── Scroll "ExPressive" en grilla 8×8 ────────────────────────────────
#define SCROLL_STEP_MS      80    // ms por columna
#define SCROLL_IDLE_TICKS   60    // 60×50ms = 3s idle antes de scroll

// ── Modos ────────────────────────────────────────────────────────────
enum class ExMode : uint8_t {
    KAOSS_XY    = 0,
    NOTE_GRID   = 1,
    ARPEGGIATOR = 2
};

// ── Colores NeoTrellis (RGB 24-bit) ───────────────────────────────────
#define COL_MODE_KAOSS    0xFF1100
#define COL_MODE_GRID     0x00FF44
#define COL_MODE_ARP      0xFF6600
#define COL_SCALE_ROOT    0xFFFFFF
#define COL_SCALE_NOTE    0x004488
#define COL_FUNC_HOLD     0xFFAA00
#define COL_FUNC_PANIC    0xFF0000
#define COL_TRELLIS_OFF   0x000000

// ── Colores UI LVGL ───────────────────────────────────────────────────
#define COL_BG            0x000000
#define COL_PAD_BG        0x050A14
#define COL_PAD_GRID      0x0D1A2A
#define COL_ACCENT        0x00AAFF
#define COL_NOTE_ON       0xFF6600
#define COL_BTN_BG        0x0A0F18
#define COL_BTN_ACTIVE    0x1A2840
#define COL_BTN_HOLD      0x332200
#define COL_BTN_PANIC     0x330000
#define COL_BTN_SCALE     0x002200
#define COL_BTN_TAP       0x001433
#define COL_TEXT          0xCCDDEE
#define COL_TEXT_DIM      0x445566

// ── Estado global (definidos en main.cpp) ────────────────────────────
extern volatile ExMode  g_currentMode;
extern volatile uint8_t g_currentScale;
extern volatile uint8_t g_rootNote;
extern volatile uint8_t g_currentOctave;
extern volatile bool    g_holdMode;
extern volatile int16_t g_lastCCX;
extern volatile int16_t g_lastCCY;
extern volatile bool    g_touched;
extern volatile bool    g_bootDone;
