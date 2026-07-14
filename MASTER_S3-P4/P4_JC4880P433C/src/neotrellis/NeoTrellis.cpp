// neotrellis/NeoTrellis.cpp — ExPressif NeoTrellis driver (AITEC 2026-06-30 → 2026-07-14: 20 presets)
//
// Layout (4 filas × 8 columnas):
//   Panel izq 0x2F  │  Panel der 0x2E
//   Col: 0  1  2  3 │  4  5  6  7
//   Row0:HOLD PAN BR+ PRE│ PRE PRE PRE PRE   ← BR=brillo pantalla, PRE=preset directo
//   Row1:SYNT --  BR- PRE│ PRE PRE PRE PRE
//   Row2:SYNT SYNT SYNT PRE│ PRE PRE PRE PRE
//   Row3:SYNT SYNT SYNT PRE│ PRE PRE PRE PRE
//
// Preset directo (2026-07-14) — 20 memorias, selección SIEMPRE directa (sin
// ciclar, L2/SCALE retirado): L3,L7,L11,L15 (col3 panel izq, una por fila) +
// R0-R15 completo (panel der) = 4 + 16 = 20. Fórmula: preset = fila×5 + (0 si
// es Lx, o 1+col si es Rx).
// L2/L6 (2026-07-14) — brillo de pantalla +/− (ver UIBrightnessPopup.cpp),
// popup en pantalla con el número. L5 (2026-07-14) — metrónomo visual,
// parpadea sincronizado al MIDI Clock USB entrante (ver metronomeUpdate()).
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
#include "../midi/MIDIClock.h"

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

static uint32_t colorForSynth(ExSynth s) {
    switch (s) {
        case ExSynth::JV2080: return COL_SYNTH_JV;
        case ExSynth::TRITON: return COL_SYNTH_TRI;
        case ExSynth::TG55:   return COL_SYNTH_TG;
        case ExSynth::D110:   return COL_SYNTH_D110;
        case ExSynth::WAVE:   return COL_SYNTH_WAVE;
        case ExSynth::MOTIF:  return COL_SYNTH_MOTIF;
        default:               return COL_ACCENT;
    }
}
static uint32_t synthColor() { return colorForSynth(g_currentSynth); }

// ── Selección directa de synth — L8,9,10,12,13,14, solo modo Kaoss (2026-07-12) ──
// Fila2 izq (L8,L9,L10) y fila3 izq (L12,L13,L14); L11/L15 (col3) son preset
// directo (ver presetIndexLeft, 2026-07-14), no synth-select. Con Bank abierto
// estas mismas teclas son tríos de contenido (ver leftTripletSlot) — por eso
// esta rama solo se evalúa cuando g_bankOpen es false (estructural, ver cb_left).
struct SynthKeyMap { uint8_t key; ExSynth synth; };
static const SynthKeyMap kSynthKeys[6] = {
    {8,  ExSynth::JV2080},
    {9,  ExSynth::TRITON},
    {10, ExSynth::TG55},
    {12, ExSynth::D110},
    {13, ExSynth::WAVE},
    {14, ExSynth::MOTIF},
};
static int8_t synthForKey(uint8_t k) {
    for (const auto& m : kSynthKeys) if (m.key == k) return (int8_t)m.synth;
    return -1;
}
static void refreshSynthSelectKeys() {
    bool active = (g_currentMode == ExMode::KAOSS_XY);
    for (const auto& m : kSynthKeys) {
        uint32_t col = 0;
        if (active) {
            uint32_t c = colorForSynth(m.synth);
            col = (g_currentSynth == m.synth) ? bright(c) : dim(c);
        }
        s_left.pixels.setPixelColor(m.key, col);
    }
}

// ── Preset directo — mapeo físico → índice 0-19 (2026-07-14) ─────────────
// Panel izq: solo col3 (L3,L7,L11,L15) es preset, una por fila → fila×5.
// Panel der: las 16 teclas son preset → fila×5 + 1 + col.
static int8_t presetIndexLeft(uint8_t k) {
    return ((k % 4) == 3) ? (int8_t)((k / 4) * 5) : -1;
}
static int8_t presetIndexRight(uint8_t k) {
    int row = k / 4, col = k % 4;
    return (int8_t)(row * 5 + 1 + col);
}

// ── LED refresh ────────────────────────────────────────────────────────
static void refreshLeft() {
    s_left.pixels.setPixelColor(0, (bool)g_holdMode ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
    s_left.pixels.setPixelColor(1, dim(COL_FUNC_PANIC));
    s_left.pixels.setPixelColor(2, dim(COL_ACCENT));   // brillo pantalla + (2026-07-14, antes SCALE)
    s_left.pixels.setPixelColor(4, dim(synthColor()));
    s_left.pixels.setPixelColor(5, 0u);                 // metrónomo — apagado en reposo, ver metronomeUpdate()
    s_left.pixels.setPixelColor(6, dim(COL_ACCENT));   // brillo pantalla − (2026-07-14)

    uint8_t  preset = kaoss.getPreset();
    uint32_t sc     = synthColor();   // color por synth (2026-07-14) — antes modeColor()
    for (int row = 0; row < 4; row++) {
        int k = row * 4 + 3;              // L3,L7,L11,L15
        s_left.pixels.setPixelColor(k, (presetIndexLeft(k) == (int8_t)preset) ? bright(sc) : dim(sc));
    }
    refreshSynthSelectKeys();             // sobreescribe L8,9,10,12,13,14
    s_left.pixels.show();
}

static void refreshRight() {
    uint8_t  preset = kaoss.getPreset();
    uint32_t sc     = synthColor();   // color por synth (2026-07-14) — antes modeColor()
    for (int k = 0; k < NEO_TRELLIS_NUM_KEYS; k++)
        s_right.pixels.setPixelColor(k, (presetIndexRight(k) == (int8_t)preset) ? bright(sc) : dim(sc));
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
    } else if (k == 2) {                              // brillo pantalla + (2026-07-14, antes SCALE)
        s_left.pixels.setPixelColor(2, pressed ? bright(COL_ACCENT) : dim(COL_ACCENT));
        s_left.pixels.show();
        if (pressed) g_trellis_brightUp = true;
    } else if (k == 6) {                              // brillo pantalla − (2026-07-14)
        s_left.pixels.setPixelColor(6, pressed ? bright(COL_ACCENT) : dim(COL_ACCENT));
        s_left.pixels.show();
        if (pressed) g_trellis_brightDown = true;
    } else if ((k % 4) == 3) {                        // L3,L7,L11,L15 — preset directo (2026-07-14)
        if (pressed) g_trellis_setPreset = presetIndexLeft(k);
    } else if (g_currentMode == ExMode::KAOSS_XY) {   // L8,9,10,12,13,14 — selección directa synth
        int8_t syn = synthForKey(k);
        if (syn >= 0 && pressed) {
            if ((ExSynth)syn == g_currentSynth) g_trellis_openBank = true;  // ya activo → abre Bank
            else                                 g_trellis_setSynth = syn;  // distinto → selecciona
        }
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

    if (pressed) {                                   // R0-R15 — preset directo (2026-07-14)
        g_trellis_setPreset = presetIndexRight(k);
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

// ── Metrónomo visual — L5, sincronizado a MIDI Clock entrante (2026-07-14) ──
// midiClockPoll() ya se llamó este ciclo (taskCore0, main.cpp) — aquí solo
// se consume el beat y se dibuja el flash con decay, mismo patrón que
// s_glow_t del pad XY (UIKaoss.cpp).
static float s_metroGlow = 0.0f;

static void metronomeUpdate() {
    bool running = midiClockIsRunning();
    if (running && midiClockConsumeBeat()) s_metroGlow = 1.0f;

    if (!running) {
        if (s_metroGlow != 0.0f) {
            s_metroGlow = 0.0f;
            s_left.pixels.setPixelColor(5, 0u);
            s_left.pixels.show();
        }
        return;
    }
    if (s_metroGlow <= 0.0f) return;

    uint8_t lvl = (uint8_t)((float)TRELLIS_BRIGHTNESS * s_metroGlow);
    s_left.pixels.setPixelColor(5, scaleCol(COL_ACCENT, lvl));
    s_left.pixels.show();

    s_metroGlow -= 0.25f;   // ~4 ticks × 20ms = 80ms de flash
    if (s_metroGlow < 0.0f) s_metroGlow = 0.0f;
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
        refreshLeft();   // L3,L7,L11,L15 también son preset directo (2026-07-14)
    }
    if (curHold != s_lastHold) {
        s_lastHold = curHold;
        s_left.pixels.setPixelColor(0, curHold ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
        s_left.pixels.show();
    }
    if (curSynth != s_lastSynth) {
        s_lastSynth = curSynth;
        // Bug (2026-07-14, reportado en vivo): esto solo refrescaba pixel 4 +
        // selección de synth — las 20 teclas de preset (color por synth desde
        // hoy) se quedaban con el color del synth ANTERIOR hasta el próximo
        // cambio de preset. refreshLeft()/refreshRight() ya recalculan todo
        // (incluye synthColor() en las 20 teclas), sea cual sea el camino que
        // cambió g_currentSynth (botón táctil, cicla o selección directa).
        refreshLeft();
        refreshRight();
    }

    metronomeUpdate();   // L5 — parpadeo sincronizado a MIDI Clock (2026-07-14)
}
