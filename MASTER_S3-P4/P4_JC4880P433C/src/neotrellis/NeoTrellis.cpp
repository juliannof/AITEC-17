// neotrellis/NeoTrellis.cpp — ExPressif NeoTrellis driver (AITEC 2026-06-30)
//
// Layout (4 filas × 8 columnas):
//   Panel izq 0x2F  │  Panel der 0x2E
//   Col: 0  1  2  3 │  4  5  6  7
//   Row0:HOLD PAN SCL -- │ MOD JOY FLT FX   ← función/presets
//   Row1:SYNT --  --  -- │ --  --  --  --   ← sintetizador activo
//   Rows2-3: apagadas
//
// Modelo de brillo (dos constantes independientes en config.h):
//   TRELLIS_BRIGHTNESS = nivel max-canal para LEDs ACTIVOS  (0-255)
//   TRELLIS_DIM_ABS    = nivel max-canal para LEDs EN REPOSO (0-255)
//   No se usa setBrightness — todo el control es por software.
//
// Thread safety: callbacks corren en Core0. Solo setean flags volatile;
//   LVGL (Core1) los procesa.
//
#include "NeoTrellis.h"
#include <Wire.h>
#include <Adafruit_NeoTrellis.h>
#include "../config.h"
#include "../kaoss/KaossPad.h"

static Adafruit_NeoTrellis s_left;   // 0x2F — col 0-3
static Adafruit_NeoTrellis s_right;  // 0x2E — col 4-7

// ── Helpers color ──────────────────────────────────────────────────────
// Escala col para que su canal máximo sea 'level' (0-255).
static uint32_t scaleCol(uint32_t col, uint8_t level) {
    uint8_t r = (col >> 16) & 0xFF;
    uint8_t g = (col >>  8) & 0xFF;
    uint8_t b =  col        & 0xFF;
    uint8_t m = r > g ? (r > b ? r : b) : (g > b ? g : b);
    if (m == 0) return 0;
    return ((uint32_t)(r * level / m) << 16) |
           ((uint32_t)(g * level / m) <<  8) |
            (uint32_t)(b * level / m);
}

// LED activo: canal máximo = TRELLIS_BRIGHTNESS
static uint32_t bright(uint32_t col) { return scaleCol(col, TRELLIS_BRIGHTNESS); }

// LED en reposo: canal máximo = TRELLIS_DIM_ABS (independiente de BRIGHTNESS)
static uint32_t dim(uint32_t col)    { return scaleCol(col, TRELLIS_DIM_ABS); }

// ── Modo Bank — mapeo por tríos (2026-07-04) ──────────────────────────
// Columna 0 (L0,L4,L8,L12) = página anterior; columna 7 (R3,R7,R11,R15) =
// página siguiente. Columnas 1-3 (panel izq) y 4-6 (panel der) forman 8
// "botones virtuales" (trío de 3 teclas contiguas = 1 sonido), fila×2 + lado:
//   fila0: L1,L2,L3→slot0   R0,R1,R2→slot1
//   fila1: L5,L6,L7→slot2   R4,R5,R6→slot3
//   fila2: L9,L10,L11→slot4 R8,R9,R10→slot5
//   fila3: L13,L14,L15→slot6 R12,R13,R14→slot7
#define COL_BANK_SEL   0x0044FFu   // azul  — sonido seleccionado
#define COL_BANK_FAV   0xFF8800u   // naranja — favorito
#define COL_BANK_NAV   0x223344u   // tenue — columnas de página

// col local 1-3 (panel izq) → slot izquierdo de su fila; col 0 no es slot.
static int8_t leftTripletSlot(uint8_t k) {
    int row = k / 4, col = k % 4;
    return (col == 0) ? -1 : row * 2;
}
// col local 0-2 (panel der) → slot derecho de su fila; col 3 no es slot.
static int8_t rightTripletSlot(uint8_t k) {
    int row = k / 4, col = k % 4;
    return (col == 3) ? -1 : row * 2 + 1;
}

static uint32_t synthColor() {
    switch (g_currentSynth) {
        case ExSynth::JV2080: return COL_SYNTH_JV;
        case ExSynth::TRITON: return COL_SYNTH_TRI;
        case ExSynth::TG55:   return COL_SYNTH_TG;
        case ExSynth::D110:   return COL_SYNTH_D110;
        case ExSynth::WAVE:   return COL_SYNTH_WAVE;
        default:               return COL_ACCENT;
    }
}

static uint32_t modeColor() {
    switch (g_currentMode) {
        case ExMode::NOTE_GRID:   return COL_MODE_GRID;
        case ExMode::ARPEGGIATOR: return COL_MODE_ARP;
        default:                   return COL_MODE_KAOSS;
    }
}

// ── LED refresh ────────────────────────────────────────────────────────
static void refreshLeft() {
    s_left.pixels.setPixelColor(0, (bool)g_holdMode ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
    s_left.pixels.setPixelColor(1, dim(COL_FUNC_PANIC));
    s_left.pixels.setPixelColor(2, dim(0x00AA44u));
    s_left.pixels.setPixelColor(3, 0u);
    s_left.pixels.setPixelColor(4, dim(synthColor()));
    for (int i = 5; i < NEO_TRELLIS_NUM_KEYS; i++) s_left.pixels.setPixelColor(i, 0u);
    s_left.pixels.show();
}

static void refreshRight() {
    uint8_t  preset = kaoss.getPreset();
    uint32_t mc     = modeColor();
    for (int i = 0; i < 4; i++)
        s_right.pixels.setPixelColor(i, (i == (int)preset) ? bright(mc) : dim(mc));
    for (int i = 4; i < NEO_TRELLIS_NUM_KEYS; i++) s_right.pixels.setPixelColor(i, 0u);
    s_right.pixels.show();
}

// ── Callbacks (Core 0) ─────────────────────────────────────────────────
static TrellisCallback cb_left(keyEvent evt) {
    uint8_t k    = evt.bit.NUM;
    bool pressed = (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);
    log_d("[TR-L] k=%d %s", k, pressed ? "DN" : "UP");

    if (k == 4) {
        // SYNTH con Bank cerrado (tap=cicla synth) / atrás+abre-cierra con
        // Bank abierto (tap=página anterior). Long-press SIEMPRE abre/cierra
        // Bank, sea cual sea el estado — es el único mecanismo para cerrarlo
        // desde el NeoTrellis (2026-07-04).
        static uint32_t s_press_ms = 0;
        if (pressed) {
            s_press_ms = millis();
            s_left.pixels.setPixelColor(4, bright(synthColor()));
            s_left.pixels.show();
        } else {
            s_left.pixels.setPixelColor(4, dim(synthColor()));
            s_left.pixels.show();
            if (millis() - s_press_ms >= 600)     g_trellis_openBank  = true;
            else if ((bool)g_bankOpen)            g_trellis_bankPrev  = true;
            else                                   g_trellis_nextSynth = true;
        }
        return 0;
    }

    if ((bool)g_bankOpen) {
        if (k == 0 || k == 8 || k == 12) {            // resto columna 0 — página anterior
            if (pressed) g_trellis_bankPrev = true;
            return 0;
        }
        int8_t slot = leftTripletSlot(k);
        if (slot >= 0 && pressed) g_trellis_bankSlot = slot;
        return 0;
    }

    if (k == 0) {                                    // HOLD — toggle
        if (pressed) {
            g_trellis_holdToggle = true;
            bool futureHold = !(bool)g_holdMode;
            s_left.pixels.setPixelColor(0, futureHold ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
            s_left.pixels.show();
        }
    } else if (k == 1) {                             // PANIC
        s_left.pixels.setPixelColor(1, pressed ? bright(COL_FUNC_PANIC) : dim(COL_FUNC_PANIC));
        s_left.pixels.show();
        if (pressed) g_trellis_panic = true;
    } else if (k == 2) {                             // SCALE — cicla presets
        s_left.pixels.setPixelColor(2, pressed ? bright(0xFFFFFFu) : dim(0x00AA44u));
        s_left.pixels.show();
        if (pressed) g_trellis_nextPreset = true;
    }
    return 0;
}

static TrellisCallback cb_right(keyEvent evt) {
    uint8_t k    = evt.bit.NUM;
    bool pressed = (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING);
    log_d("[TR-R] k=%d %s", k, pressed ? "DN" : "UP");

    if ((bool)g_bankOpen) {
        if (k == 3 || k == 7 || k == 11 || k == 15) { // columna 7 — página siguiente
            if (pressed) g_trellis_bankNext = true;
            return 0;
        }
        int8_t slot = rightTripletSlot(k);
        if (slot >= 0 && pressed) g_trellis_bankSlot = slot;
        return 0;
    }

    if (pressed && k < 4) {                          // selección directa de preset
        g_trellis_setPreset = (int8_t)k;
        uint32_t mc = modeColor();
        for (int i = 0; i < 4; i++)
            s_right.pixels.setPixelColor(i, (i == (int)k) ? bright(mc) : dim(mc));
        s_right.pixels.show();
    }
    return 0;
}

// ── API Bank (llamado desde UIBank.cpp, Core1) ────────────────────────
void neotrellisBankShowPage(const uint8_t* state, int count) {
    uint32_t nav = dim(COL_BANK_NAV);
    for (int row = 0; row < 4; row++) {
        s_left.pixels.setPixelColor(row * 4 + 0, nav);    // L0,4,8,12  — atrás
        s_right.pixels.setPixelColor(row * 4 + 3, nav);   // R3,7,11,15 — adelante
    }
    for (int slot = 0; slot < 8; slot++) {
        uint8_t st = (slot < count) ? state[slot] : 0;
        uint32_t col = (st == 1) ? bright(COL_BANK_SEL)
                     : (st == 2) ? bright(COL_BANK_FAV)
                                  : dim(0x111111u);
        int row = slot / 2;
        if ((slot % 2) == 0) {   // trío izquierdo (L1,L2,L3 de esa fila)
            s_left.pixels.setPixelColor(row * 4 + 1, col);
            s_left.pixels.setPixelColor(row * 4 + 2, col);
            s_left.pixels.setPixelColor(row * 4 + 3, col);
        } else {                 // trío derecho (R0,R1,R2 de esa fila)
            s_right.pixels.setPixelColor(row * 4 + 0, col);
            s_right.pixels.setPixelColor(row * 4 + 1, col);
            s_right.pixels.setPixelColor(row * 4 + 2, col);
        }
    }
    s_left.pixels.show();
    s_right.pixels.show();
}

void neotrellisRestoreKaoss() {
    refreshLeft();
    refreshRight();
}

// ── API ────────────────────────────────────────────────────────────────
void neotrellisInit() {
    Wire.begin(TRELLIS_SDA_PIN, TRELLIS_SCL_PIN);

    if (!s_left.begin(TRELLIS_ADDR_L))
        log_e("[NeoTrellis] LEFT 0x%02X no encontrado", TRELLIS_ADDR_L);
    if (!s_right.begin(TRELLIS_ADDR_R))
        log_e("[NeoTrellis] RIGHT 0x%02X no encontrado", TRELLIS_ADDR_R);

    s_left.pixels.setBrightness(255);   // sin atenuación hw — todo controlado en software
    s_right.pixels.setBrightness(255);

    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        s_left.activateKey(i,  SEESAW_KEYPAD_EDGE_RISING);
        s_left.activateKey(i,  SEESAW_KEYPAD_EDGE_FALLING);
        s_right.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
        s_right.activateKey(i, SEESAW_KEYPAD_EDGE_FALLING);
        s_left.registerCallback(i,  cb_left);
        s_right.registerCallback(i, cb_right);
    }

    refreshLeft();
    refreshRight();
    log_i("[NeoTrellis] Init OK — SDA=%d SCL=%d  L=0x%02X R=0x%02X",
          TRELLIS_SDA_PIN, TRELLIS_SCL_PIN, TRELLIS_ADDR_L, TRELLIS_ADDR_R);
}

void neotrellisUpdate() {
    s_left.read();
    s_right.read();

    static uint8_t  s_lastPreset = 255;
    static bool     s_lastHold   = false;
    static ExSynth  s_lastSynth  = ExSynth::JV2080;
    uint8_t curPreset = kaoss.getPreset();
    bool    curHold   = (bool)g_holdMode;
    ExSynth curSynth  = g_currentSynth;

    if (curPreset != s_lastPreset) {
        s_lastPreset = curPreset;
        refreshRight();
    }
    if (curHold != s_lastHold) {
        s_lastHold = curHold;
        s_left.pixels.setPixelColor(0, curHold ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
        s_left.pixels.show();
    }
    if (curSynth != s_lastSynth) {
        s_lastSynth = curSynth;
        s_left.pixels.setPixelColor(4, dim(synthColor()));
        s_left.pixels.show();
    }
}
