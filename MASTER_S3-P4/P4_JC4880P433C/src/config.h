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
#define TRELLIS_SDA_PIN    31    // SDA=31, SCL=33 (2026-06-30)
#define TRELLIS_SCL_PIN    33
#define TRELLIS_ADDR_L     0x2F
#define TRELLIS_ADDR_R     0x2E
#define TRELLIS_BRIGHTNESS   25   // 0-255: nivel max-canal LEDs activos
#define TRELLIS_DIM_ABS       3   // 0-255: nivel max-canal LEDs en reposo

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

// ── Flags cross-core NeoTrellis→LVGL (Core0→Core1) ───────────────────
extern volatile bool   g_trellis_holdToggle;  // toggle g_holdMode
extern volatile bool   g_trellis_panic;       // CC reset a 64
extern volatile int8_t g_trellis_setPreset;   // -1=none, 0-19=preset directo (2026-07-14, antes 0-3)
extern volatile bool   g_trellis_nextSynth;   // cicla sintetizador activo
extern volatile int8_t g_trellis_setSynth;    // -1=none, selección directa L8,9,10,12,13,14 (solo modo Kaoss, 2026-07-12)
extern volatile bool   g_trellis_openBank;    // abre/cierra UIBank
extern volatile int8_t g_trellis_bankSlot;    // slot (0-7) pulsado en modo Bank (-1=ninguno) (2026-07-04: antes 0-15)
extern volatile bool   g_trellis_bankPrev;    // columna 0 (L0,4,8,12) — página anterior (2026-07-04)
extern volatile bool   g_trellis_bankNext;    // columna 7 (R3,7,11,15) — página siguiente (2026-07-04)
extern volatile bool   g_trellis_brightDown;  // L2 — brillo pantalla − (2026-07-14, antes SCALE)
extern volatile bool   g_trellis_brightUp;    // L6 — brillo pantalla + (2026-07-14)

// ── Brillo de pantalla ──────────────────────────────────────────────────
extern volatile uint8_t g_displayBrightness;  // 10-100%, paso 10, ajustable con L2/L6 NeoTrellis (2026-07-14)

// ── Bank / Canal MIDI ─────────────────────────────────────────────────────
extern volatile uint8_t g_midiChannel;   // canal MIDI activo — fijo en 1 (2026-07-12): Logic enruta por track, no el firmware
extern volatile bool    g_bankOpen;      // UIBank visible
extern volatile uint8_t g_bankTab;       // pestaña activa (0=FAV 1=SON 2=PERFORM) (2026-07-04: antes 2=CH)

// ── UIBank — grid unificado 2 cols × 4 filas = 8 ítems/página (2026-07-04) ──
// Reemplaza los grids independientes de FAV (16/pág) y Sonidos (10/pág): las
// 3 tabs (Favoritos/Sonidos/Performances) comparten la misma geometría — menos
// widgets vivos por página (antes hasta 16, hoy 8) para touch más ligero.
// Cada ítem de Sonidos/Performances corresponde a un trío de teclas NeoTrellis
// contiguas (ver NeoTrellis.cpp neotrellisBankShowPage) — botón físico grande.
#define UIBANK_GRID_COLS         2
#define UIBANK_GRID_ROWS         4
#define UIBANK_GRID_PER_PAGE     (UIBANK_GRID_COLS * UIBANK_GRID_ROWS)   // 8

#define UIBANK_FAV_SLOTS         128   // tope favSave (favLoad rechaza slot > 127)
// Tamaño de banco (128, uniforme en JV-2080 y Triton — ver ctxBankSize() en
// UIBank.cpp) y nº de páginas de Sonidos/Performances/Favoritos ya no son
// constantes fijas: se calculan en vivo según el synth activo (2026-07-04,
// integración multi-synth) — Favoritos además pagina sobre la lista ya
// filtrada por g_currentSynth, no sobre el total de slots NVS.

// Geometría física — Sonidos/Performances reservan franja de 58px para el
// selector de banco (igual que antes); Favoritos usa el ancho completo.
// UIBANK_TOPSTRIP_W (2026-07-13): franja del nombre de synth activo, extremo
// LVGL x alto (= screen_y≈0, borde físico superior, misma banda que la X de
// cerrar) — los 3 grids (Sonidos/Performances/Favoritos) recortan su ancho
// para no invadirla (antes llegaban a P4_W sin margen y la franja nueva
// tapaba el borde de la fila superior).
#define UIBANK_TOPSTRIP_W        36
#define UIBANK_TILE_X            60    // LVGL x donde empieza el grid (tras franja de banco)
#define UIBANK_TILE_W            (P4_W - UIBANK_TILE_X - UIBANK_TOPSTRIP_W)   // 384 — Sonidos/Performances
#define UIBANK_ROW_PITCH_BANK    (UIBANK_TILE_W / UIBANK_GRID_ROWS)   // 96
#define UIBANK_ROW_PITCH_FULL    ((P4_W - UIBANK_TOPSTRIP_W) / UIBANK_GRID_ROWS)   // 111 — Favoritos (sin franja de banco)
#define UIBANK_BTN_W_BANK        (UIBANK_ROW_PITCH_BANK - 2)          // 94
#define UIBANK_BTN_W_FULL        (UIBANK_ROW_PITCH_FULL - 2)          // 109
#define UIBANK_COL_PITCH         340
#define UIBANK_BTN_H             338   // LVGL y (botón) — igual en las 3 tabs, ancho físico del combo
#define UIBANK_BTN_RADIUS        3
// Máximo de bancos en la franja de Sonidos/Performances (2026-07-12) — MOTIF-RACK
// tiene 8 (Preset1-5, GM, User1, User2), el máximo actual entre todos los synths.
#define UIBANK_MAX_BANKS         8
// Retardo entre CC0/CC32/PC para synths con SynthSoundDesc.slowSend=true
// (MOTIF-RACK, cadena MIDI THRU larga en el UMC1820 — diagnóstico 2026-07-12).
#define UIBANK_SLOW_SEND_MS      5

#define COL_FAV_STAR             0xFF8800   // círculo naranja = favorito
#define UIBANK_FAV_DOT           20         // diámetro círculo favorito (2026-07-04: antes 12, muy pequeño)
#define UIBANK_FAV_DOT_MARGIN    38         // distancia borde del botón → CENTRO del círculo (2026-07-05: antes 26, borde del círculo quedaba a solo 16px del borde del botón)
#define UIBANK_FAV_RESERVE       (UIBANK_FAV_DOT_MARGIN + UIBANK_FAV_DOT / 2 + 4)   // espacio reservado al texto (~40)
#define UIBANK_LBL_W             (UIBANK_BTN_H - 4)                 // 334 — ancho combo "BANCO:NNN Nombre"
#define UIBANK_LBL_W2            (UIBANK_LBL_W - UIBANK_FAV_RESERVE) // ancho texto con margen (Sonidos/Performances)
#define UIBANK_LBL_OFS           (UIBANK_FAV_RESERVE / 2)            // desplaza el texto para dejar sitio al círculo
#define UIBANK_FAV_DOT_OFS       -((UIBANK_BTN_H / 2) - UIBANK_FAV_DOT_MARGIN)      // eje local-y (izquierda física)

// ── Sintetizador activo ───────────────────────────────────────────────
enum class ExSynth : uint8_t { JV2080=0, TRITON=1, TG55=2, D110=3, WAVE=4, MOTIF=5 };
// Modo de sonido activo en el JV-2080 (2026-07-02) — mismo valor que el byte
// de datos del SysEx DT1 "Sound Mode" (JV-2080_OM.pdf p.187-188, verificado).
enum class JVSoundMode : uint8_t { PERFORMANCE = 0, PATCH = 1 };
// Device ID del JV-2080 para todo SysEx Roland (byte 3: F0 41 [dev] 6A...)
// (2026-07-02 18:31). Configurable en el propio synth: panel SYSTEM →
// [F3](MIDI) → Device ID, rango 10H-1FH. Si no coincide con el panel, el
// JV-2080 ignora el SysEx en silencio (sin NAK) — CC/PC no llevan Device ID
// y siguen funcionando igual, por eso el síntoma es "cambia en la interfaz
// pero no en el synth". Valor de fábrica = 0x10.
#define JV2080_DEVICE_ID   0x10

// ── Triton (Korg) — SysEx MODE CHANGE (2026-07-04) ─────────────────────
// F0 42 3g 50 4E 00 mm F7 (TRITON_Rack_MIDIimp.TXT Func 4E, nota *11,
// verificado). g=canal, mm: 0=COMBI PLAY, 2=PROG PLAY — únicos modos que
// nos interesan (resto: 1=COMBI EDIT, 3=PROG EDIT, 4=MULTI, 5=DEMO/SNG,
// 6=SAMPLING, 7=GLOBAL, 8=DISK, no aplican al browser de patches).
#define KORG_ID            0x42
#define TRITON_MODEL_ID    0x50
#define TRITON_MODE_COMBI  0x00
#define TRITON_MODE_PROG   0x02

#define NUM_SYNTHS       6
#define COL_SYNTH_JV     0x0044FF
#define COL_SYNTH_TRI    0x8800FF
#define COL_SYNTH_TG     0x00CC00
#define COL_SYNTH_D110   0x00BBAA
#define COL_SYNTH_WAVE   0xFF6600
#define COL_SYNTH_MOTIF  0xFFDD00
extern volatile ExSynth g_currentSynth;
