// neotrellis/NeoTrellis.cpp — ExPressif NeoTrellis driver (AITEC 2026-06-30 → 2026-07-14: 20 presets)
//
// Layout (4 filas × 8 columnas):
//   Panel izq 0x2F  │  Panel der 0x2E
//   Col: 0  1  2  3 │  4  5  6  7
//   Row0:SLP  PAN BR+ PRE│ PRE PRE PRE PRE   ← BR=brillo pantalla, PRE=preset directo
//   Row1:SYNT HOLD BR- PRE│ PRE PRE PRE PRE
//   Row2:SYNT SYNT SYNT PRE│ PRE PRE PRE PRE
//   Row3:SYNT SYNT SYNT PRE│ PRE PRE PRE PRE
//
// Preset directo (2026-07-14) — 20 memorias, selección SIEMPRE directa (sin
// ciclar, L2/SCALE retirado): L3,L7,L11,L15 (col3 panel izq, una por fila) +
// R0-R15 completo (panel der) = 4 + 16 = 20. Fórmula: preset = fila×5 + (0 si
// es Lx, o 1+col si es Rx).
// L2/L6 (2026-07-14) — brillo de pantalla +/− (ver UIBrightnessPopup.cpp),
// popup en pantalla con el número. L0 (2026-10-11) — metrónomo visual + dormir/despertar (antes L5; HOLD pasó a L5),
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

// Nivel base escalado por el brillo de pantalla (2026-10-11 19:28):
// base × g_displayBrightness / TRELLIS_BR_REF, mínimo 1 (los LEDs en reposo
// no desaparecen), máximo 255. Con el brillo por defecto (80 %) = base.
static uint8_t brLevel(uint8_t base) {
    uint32_t v = (uint32_t)base * g_displayBrightness / TRELLIS_BR_REF;
    if (v < 1)   v = 1;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

// LED activo: canal máximo = TRELLIS_BRIGHTNESS (escalado por brillo pantalla)
static uint32_t bright(uint32_t col) { return scaleCol(col, brLevel(TRELLIS_BRIGHTNESS)); }

// LED en reposo: canal máximo = TRELLIS_DIM_ABS (escalado por brillo pantalla)
static uint32_t dim(uint32_t col)    { return scaleCol(col, brLevel(TRELLIS_DIM_ABS)); }

// ── Secuencia de entrada "matrix azul" (2026-10-11 19:41) ──────────────────
// Estado compartido entre callbacks y neotrellisUpdate(), ambos en Core0.
static uint32_t s_introStart   = 0;
static uint32_t s_introDur     = 0;       // 0 = inactiva (solo se activa en neotrellisInit)

static void introStart(uint32_t ms) {
    s_introStart = millis();
    s_introDur   = ms;
}

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
    s_left.pixels.setPixelColor(5, (bool)g_holdMode ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
    s_left.pixels.setPixelColor(1, dim(COL_FUNC_PANIC));
    s_left.pixels.setPixelColor(2, dim(COL_ACCENT));   // brillo pantalla + (2026-07-14, antes SCALE)
    s_left.pixels.setPixelColor(4, dim(synthColor()));
    s_left.pixels.setPixelColor(0, 0u);                 // metrónomo — apagado en reposo, ver metronomeUpdate()
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

    // Reposo (2026-10-11 19:22): L0 alterna dormir/despertar. En reposo el
    // resto de teclas se ignoran (L0 es la única que despierta desde aquí).
    g_lastActivityMs = millis();
    if (s_introDur) return 0;            // intro de encendido activa: teclas ignoradas, no se interrumpe (2026-10-11 19:44)
    if (k == 0) {
        if (pressed) g_trellis_sleepToggle = true;
        return 0;
    }
    if ((bool)g_sleeping) return 0;

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

    if (k == 5) {                                    // HOLD — toggle
        if (pressed) {
            g_trellis_holdToggle = true;
            bool futureHold = !(bool)g_holdMode;
            s_left.pixels.setPixelColor(5, futureHold ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
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

    g_lastActivityMs = millis();            // reposo (2026-10-11 19:22)
    if (s_introDur) return 0;               // intro de encendido activa: teclas ignoradas (2026-10-11 19:44)
    if ((bool)g_sleeping) return 0;

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
// Caché de la última página Bank (2026-10-11 19:28): permite repintar con el
// nuevo brillo desde Core0 sin tocar UIBank.cpp.
static uint8_t s_bankState[8] = {};
static int     s_bankCount    = 0;

static void paintBank(const uint8_t* state, int count) {
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

void neotrellisBankShowPage(const uint8_t* state, int count) {
    s_bankCount = (count > 8) ? 8 : count;
    for (int i = 0; i < 8; i++) s_bankState[i] = (i < s_bankCount) ? state[i] : 0;
    paintBank(s_bankState, s_bankCount);
}

// Repinta según el modo activo: Bank abierto → caché Bank, si no → Kaoss.
static void repaintAll() {
    if ((bool)g_bankOpen) {
        paintBank(s_bankState, s_bankCount);
    } else {
        refreshLeft();
        refreshRight();
    }
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
    introStart(NEO_INTRO_MS);   // matrix azul durante la pantalla de arranque (2026-10-11 19:41)
    log_i("[NeoTrellis] Init OK — SDA=%d SCL=%d  L=0x%02X R=0x%02X",
          TRELLIS_SDA_PIN, TRELLIS_SCL_PIN, TRELLIS_ADDR_L, TRELLIS_ADDR_R);
}

// ── Metrónomo visual — L0, sincronizado a MIDI Clock entrante (2026-07-14) ──
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
            s_left.pixels.setPixelColor(0, 0u);
            s_left.pixels.show();
        }
        return;
    }
    if (s_metroGlow <= 0.0f) return;

    uint8_t lvl = (uint8_t)((float)brLevel(TRELLIS_BRIGHTNESS) * s_metroGlow);
    s_left.pixels.setPixelColor(0, scaleCol(COL_ACCENT, lvl));
    s_left.pixels.show();

    s_metroGlow -= 0.25f;   // ~4 ticks × 20ms = 80ms de flash
    if (s_metroGlow < 0.0f) s_metroGlow = 0.0f;
}

// Lluvia "matrix azul" (2026-10-11 19:41): las 8 columnas físicas (4 por panel)
// son columnas de lluvia; cada una tiene una gota con cabeza clara y estela
// azul que sube atenuándose. Velocidad y fase distintas por columna. Fundido
// a negro en los últimos 400 ms. Devuelve true mientras sigue activa.
static bool introTick() {
    uint32_t now = millis();
    uint32_t el  = now - s_introStart;
    if (el >= s_introDur) {
        s_introDur = 0;
        repaintAll();
        return false;
    }

    static uint32_t s_lastFrame = 0;
    if ((now - s_lastFrame) < 30) return true;   // ~33 fps
    s_lastFrame = now;

    // 2026-10-11 19:48: la lluvia avanza de IZQUIERDA a DERECHA y mucho más
    // lenta (antes caía de arriba a abajo a 5..11 filas/s). Cada una de las
    // 4 filas es un carril: gota con cabeza a la derecha y estela a su
    // izquierda; velocidad 1..2,5 columnas/s y entrada escalonada por fila.
    const float TAIL = 4.0f;                      // columnas de estela
    const float CYC  = 8.0f + TAIL;               // 8 columnas visibles + salida
    float t    = (float)el / 1000.0f;
    uint32_t rem = s_introDur - el;
    float fade = (rem < 400) ? (float)rem / 400.0f : 1.0f;
    uint8_t base = brLevel(TRELLIS_BRIGHTNESS);

    for (int r = 0; r < 4; r++) {
        float speed = 1.0f + 0.5f * (float)((r * 37) % 4);          // 1.0..2.5 col/s
        float head  = speed * t - 1.5f * (float)((r * 3) % 4);      // entra escalonado (fuera por la izquierda)
        if (head >= 0.0f) head = fmodf(head, CYC);
        for (int c = 0; c < 8; c++) {
            float d = head - (float)c;                              // distancia tras la cabeza
            uint32_t col = 0;
            if (d >= 0.0f && d < TAIL) {
                float I = 1.0f - d / TAIL;
                I = I * I * fade;
                uint8_t lvl = (uint8_t)((float)base * I + 0.5f);
                if (lvl) col = scaleCol((d < 1.0f) ? NEO_INTRO_HEAD : NEO_INTRO_COL, lvl);
            }
            Adafruit_NeoTrellis& dev = (c < 4) ? s_left : s_right;
            dev.pixels.setPixelColor(r * 4 + (c % 4), col);
        }
    }
    s_left.pixels.show();
    s_right.pixels.show();
    return true;
}

void neotrellisUpdate() {
    // En reposo el bucle de Core0 corre a 5 ms (latido fluido, ver más abajo);
    // las teclas se siguen leyendo cada 20 ms como siempre.
    static uint32_t s_lastRead = 0;
    uint32_t nowMs = millis();
    if (!g_sleeping || (nowMs - s_lastRead) >= 20) {
        s_lastRead = nowMs;
        s_left.read();
        s_right.read();
    }

    static uint8_t  s_lastPreset = 255;
    static bool     s_lastHold   = false;
    static ExSynth  s_lastSynth  = ExSynth::JV2080;
    uint8_t curPreset = kaoss.getPreset();
    bool    curHold   = (bool)g_holdMode;
    ExSynth curSynth  = g_currentSynth;

    // Reposo (2026-10-11 19:22): LEDs apagados salvo L0 con "latido" tenue
    // (0..2×TRELLIS_DIM_ABS, ciclo 3 s). Los pinta Core0 al detectar el
    // cambio de g_sleeping, así el bus I2C no se comparte entre cores.
    static bool     s_wasSleeping = false;
    static uint8_t  s_pulseLvl    = 255;
    if (g_sleeping) {
        if (!s_wasSleeping) {
            s_wasSleeping = true;
            s_metroGlow   = 0.0f;
            s_pulseLvl    = 255;
            for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
                s_left.pixels.setPixelColor(i, 0u);
                s_right.pixels.setPixelColor(i, 0u);
            }
            s_right.pixels.show();
        }
        // Latido fluido (2026-10-11 19:36): onda senoidal con gamma 2 y pico
        // = mitad del nivel activo (antes triángulo de solo 7 pasos 0..6).
        // Con tan pocos niveles de 8 bits los saltos se ven, así que se
        // reparte la parte fraccional en el tiempo (sigma-delta, el nivel
        // alterna entre floor y ceil cada 20 ms) → brillo efectivo continuo.
        static float s_acc = 0.0f;
        float x   = 0.5f - 0.5f * cosf(6.2831853f * (float)(millis() % 3000) / 3000.0f);
        // 2026-10-11 19:39: pico = nivel de reposo (TRELLIS_DIM_ABS, ~3), antes
        // la mitad del activo (~12, demasiado luminoso). Actualización a 5 ms
        // (200 Hz) → el dithering ya no se percibe como escalones.
        float tgt = (float)brLevel(TRELLIS_DIM_ABS) * x * x;
        s_acc += tgt;
        uint8_t lvl = (uint8_t)s_acc;
        s_acc -= (float)lvl;
        if (lvl != s_pulseLvl) {
            s_pulseLvl = lvl;
            s_left.pixels.setPixelColor(0, scaleCol(COL_ACCENT, lvl));
            s_left.pixels.show();
        }
        return;
    }
    if (s_wasSleeping) {                 // acaba de despertar → repinta (Bank o Kaoss)
        s_wasSleeping = false;
        repaintAll();
    }

    // Secuencia de encendido (2026-10-11 19:44): solo al arrancar, ocupa los
    // LEDs hasta que termina; al acabar repinta (introTick → repaintAll).
    if (s_introDur && introTick()) return;

    // Brillo de pantalla (L2/L6) → también escala los NeoTrellis (2026-10-11 19:28)
    static uint8_t s_lastBr = 0;
    uint8_t curBr = g_displayBrightness;
    if (curBr != s_lastBr) {
        s_lastBr = curBr;
        repaintAll();
    }

    if (curPreset != s_lastPreset) {
        s_lastPreset = curPreset;
        refreshRight();
        refreshLeft();   // L3,L7,L11,L15 también son preset directo (2026-07-14)
    }
    if (curHold != s_lastHold) {
        s_lastHold = curHold;
        s_left.pixels.setPixelColor(5, curHold ? bright(COL_FUNC_HOLD) : dim(COL_FUNC_HOLD));
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

    metronomeUpdate();   // L0 — parpadeo sincronizado a MIDI Clock (2026-07-14)
}
