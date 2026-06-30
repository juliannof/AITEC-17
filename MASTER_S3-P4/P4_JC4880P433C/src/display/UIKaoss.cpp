// display/UIKaoss.cpp — ExPressif main screen  (AITEC 2026-06-29)
// LVGL portrait 480×800, hardware rota a landscape 800×480
// Mapping: screen_x = LVGL_y,  screen_y = 479 − LVGL_x
//
// Layout LVGL portrait → pantalla landscape:
//   HOLD  pos(240, 0)   BTN_W×BTN_H  → izq-arriba (screen x:0→150,   y:0→240)
//   TAP   pos(0,   0)   BTN_W×BTN_H  → izq-abajo  (screen x:0→150,   y:240→480)
//   PAD   pos(0,   150) P4_W×PAD_SIZE → centro     (screen x:150→650, y:0→480)
//   SCALE pos(240, 650) BTN_W×BTN_H  → der-arriba (screen x:650→800, y:0→240)
//   PANIC pos(0,   650) BTN_W×BTN_H  → der-abajo  (screen x:650→800, y:240→480)
//
// LED behavior:
//   Idle:   todos los dots apagados (0%)
//   Toque:  d=0→100% fijo, d=1→75%, d=2→35%, d=3→15%, d≥4→0%
//   Hold:   d=0 se mantiene al 100%; adjacentes decaen a 0%
//   Decay:  d=1/2/3 decrecen con el tiempo mientras se mantiene el toque

#include "UIKaoss.h"
#include "UIBank.h"
#include "../config.h"
#include "../midi/MIDIOut.h"
#include "../kaoss/KaossPad.h"
#include "lvgl.h"

#define DOTS_N   8
#define CELL_W   (P4_W    / DOTS_N)     // 60px
#define CELL_H   (PAD_SIZE / DOTS_N)    // 62px  (500/8)
#define DOT_W    (CELL_W - 2)           // 58px
#define DOT_H    (CELL_H - 2)           // 60px

// Brillo pico de adjacentes (d=1,2,3) en escala 0-255
static const uint8_t k_peak[4] = {255, 191, 89, 38};  // 100%, 75%, 35%, 15%

static lv_obj_t*   s_pad              = NULL;
static lv_obj_t*   s_lbl_scale        = NULL;
static lv_obj_t*   s_lbl_hold         = NULL;
static lv_obj_t*   s_lbl_synth        = NULL;
static lv_obj_t*   s_btn[4]           = {};
static lv_obj_t*   s_strips[4]        = {};
static uint32_t    s_accent_cols[4]   = {};
static uint32_t    s_dim_cols[4]      = {};
static lv_obj_t*   s_dots[DOTS_N][DOTS_N] = {};
static lv_timer_t* s_decay_tmr        = NULL;

// Estado LED
static float  s_glow_t   = 0.0f;  // 1.0=pico adjacentes, 0.0=apagado
static int    s_i_near   = 0;
static int    s_j_near   = 0;
static bool   s_pressing = false;

// ── Scroll "ExPressive" ───────────────────────────────────────────────
// Font 5×7 col-based: bit0=fila top, bit6=fila bottom
// Letras mayúsculas: filas 0-6 (altura plena). Minúsculas: filas 2-6.
#define SCROLL_CHAR_COLS 5
#define SCROLL_CHAR_GAP  1
#define SCROLL_CHARS     10
#define SCROLL_TEXT_W    (SCROLL_CHARS * (SCROLL_CHAR_COLS + SCROLL_CHAR_GAP))  // 60

static const uint8_t s_font[SCROLL_CHARS][SCROLL_CHAR_COLS] = {
    /* E */ {0x7F, 0x49, 0x49, 0x49, 0x41},
    /* x */ {0x44, 0x28, 0x10, 0x28, 0x44},
    /* P */ {0x7F, 0x09, 0x09, 0x09, 0x06},
    /* r */ {0x7C, 0x04, 0x04, 0x04, 0x08},
    /* e */ {0x38, 0x54, 0x54, 0x54, 0x18},
    /* s */ {0x48, 0x54, 0x54, 0x54, 0x24},
    /* s */ {0x48, 0x54, 0x54, 0x54, 0x24},
    /* i */ {0x00, 0x48, 0x7A, 0x48, 0x00},
    /* v */ {0x0C, 0x30, 0x40, 0x30, 0x0C},
    /* e */ {0x38, 0x54, 0x54, 0x54, 0x18},
};

static int16_t     s_scroll_tick   = 0;
static bool        s_scroll_active = false;
static uint16_t    s_idle_ticks    = 0;
static lv_timer_t* s_scroll_tmr    = NULL;

void uiKaossStartScroll();  // forward declaration (definida al final)

// ── Helpers ──────────────────────────────────────────────────────────
static void rot_label(lv_obj_t* lbl) {
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
}

static const char* synthName() {
    switch (g_currentSynth) {
        case ExSynth::JV2080: return "JV-2080";
        case ExSynth::TRITON: return "TRITON";
        case ExSynth::TG55:   return "TG55";
        case ExSynth::D110:   return "D-110";
        case ExSynth::WAVE:   return "WAVE";
        default:               return "?";
    }
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

static void mode_color(uint8_t* r, uint8_t* g, uint8_t* b) {
    uint32_t col;
    switch (g_currentMode) {
        case ExMode::NOTE_GRID:   col = COL_MODE_GRID;  break;
        case ExMode::ARPEGGIATOR: col = COL_MODE_ARP;   break;
        default:                  col = COL_MODE_KAOSS; break;
    }
    *r = (col >> 16) & 0xFF;
    *g = (col >>  8) & 0xFF;
    *b =  col        & 0xFF;
}

// ── Redibuja solo los dots afectados (d<=3 desde el centro) ──────────
// Si pos_changed=true limpia primero los dots del radio anterior
static void update_leds_draw(bool pos_changed = false) {
    uint8_t mr, mg, mb;
    mode_color(&mr, &mg, &mb);

    // Si no hay nada que mostrar y no hubo cambio de posición: no hacer nada
    if (s_glow_t <= 0.0f && !s_pressing && !(bool)g_holdMode && !pos_changed) return;

    // Cuando el dedo se mueve, limpia TODOS los dots del grid anterior
    if (pos_changed) {
        for (int i = 0; i < DOTS_N; i++)
            for (int j = 0; j < DOTS_N; j++)
                if (s_dots[i][j])
                    lv_obj_set_style_bg_color(s_dots[i][j], lv_color_make(0,0,0), 0);
    }

    // Actualiza solo la zona ±3 alrededor del centro activo
    int i0 = s_i_near - 3; if (i0 < 0) i0 = 0;
    int i1 = s_i_near + 3; if (i1 >= DOTS_N) i1 = DOTS_N - 1;
    int j0 = s_j_near - 3; if (j0 < 0) j0 = 0;
    int j1 = s_j_near + 3; if (j1 >= DOTS_N) j1 = DOTS_N - 1;

    for (int i = i0; i <= i1; i++) {
        for (int j = j0; j <= j1; j++) {
            if (!s_dots[i][j]) continue;
            int di = i - s_i_near; if (di < 0) di = -di;
            int dj = j - s_j_near; if (dj < 0) dj = -dj;
            int d  = (di > dj) ? di : dj;

            uint8_t bright = 0;
            if (d == 0) {
                // Centro: 100% mientras se toca o HOLD; fade tras soltar
                bright = (s_pressing || (bool)g_holdMode)
                       ? 255
                       : (uint8_t)(255.0f * s_glow_t);
            } else if (d <= 3) {
                bright = (uint8_t)((float)k_peak[d] * s_glow_t);
            }

            lv_obj_set_style_bg_color(s_dots[i][j],
                lv_color_make(
                    (uint8_t)((uint32_t)mr * bright / 255),
                    (uint8_t)((uint32_t)mg * bright / 255),
                    (uint8_t)((uint32_t)mb * bright / 255)), 0);
        }
    }
}

// ── Scroll: dibuja columna actual del texto en la grilla ─────────────
// Mapping: text_col_en_j = scroll_tick + j - 7
//   tick=0  → j=7 muestra col 0 (primera col de 'E', entra por derecha)
//   tick=7  → j=0 muestra col 0 (primera col llega al borde izquierdo)
// Mapping filas: i=7→font_row 0 (top), i=1→font_row 6 (bottom), i=0=margen
static void scroll_draw() {
    uint8_t mr, mg, mb;
    mode_color(&mr, &mg, &mb);

    for (int j = 0; j < DOTS_N; j++) {
        int tc = s_scroll_tick + j - (DOTS_N - 1);  // columna de texto en este j
        uint8_t col_bits = 0;
        if (tc >= 0 && tc < SCROLL_TEXT_W) {
            int ci = tc / (SCROLL_CHAR_COLS + SCROLL_CHAR_GAP);
            int cc = tc % (SCROLL_CHAR_COLS + SCROLL_CHAR_GAP);
            if (cc < SCROLL_CHAR_COLS) col_bits = s_font[ci][cc];
        }

        for (int i = 0; i < DOTS_N; i++) {
            if (!s_dots[i][j]) continue;
            uint8_t bright = 0;
            if (i >= 1) {
                int font_row = (DOTS_N - 1) - i;  // i=7→0, i=1→6
                if (col_bits & (1 << font_row)) bright = 200;
            }
            lv_obj_set_style_bg_color(s_dots[i][j],
                lv_color_make((uint32_t)mr * bright / 255,
                              (uint32_t)mg * bright / 255,
                              (uint32_t)mb * bright / 255), 0);
        }
    }
}

static void scroll_timer_cb(lv_timer_t* t) {
    if (!s_scroll_active) { lv_timer_pause(t); return; }
    scroll_draw();
    s_scroll_tick++;
    if (s_scroll_tick >= SCROLL_TEXT_W + DOTS_N) {
        s_scroll_active = false;
        s_scroll_tick   = 0;
        s_idle_ticks    = 0;
        for (int i = 0; i < DOTS_N; i++)
            for (int j = 0; j < DOTS_N; j++)
                if (s_dots[i][j])
                    lv_obj_set_style_bg_color(s_dots[i][j], lv_color_make(0,0,0), 0);
        lv_timer_pause(t);
    }
}

// ── Timer: decay + idle counter ───────────────────────────────────────
static void decay_timer_cb(lv_timer_t* t) {
    (void)t;
    if (s_glow_t > 0.0f) {
        s_glow_t -= 0.07f;   // ~700ms para llegar a 0 (14 ticks × 50ms)
        if (s_glow_t < 0.0f) s_glow_t = 0.0f;
        update_leds_draw();
        s_idle_ticks = 0;
        return;
    }
    if (s_pressing || s_scroll_active) { s_idle_ticks = 0; return; }
    if (s_idle_ticks < SCROLL_IDLE_TICKS) s_idle_ticks++;
    if (s_idle_ticks >= SCROLL_IDLE_TICKS) uiKaossStartScroll();
}

// ── Callback pad ──────────────────────────────────────────────────────
static void pad_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();
    lv_point_t pt = {0, 0};
    if (indev) lv_indev_get_point(indev, &pt);

    uint16_t cx = (uint16_t)pt.x;
    uint16_t cy = (pt.y >= PAD_START_Y) ? (uint16_t)(pt.y - PAD_START_Y) : 0;
    if (cx >= P4_W)     cx = P4_W - 1;
    if (cy >= PAD_SIZE) cy = PAD_SIZE - 1;

    int ni = (int)((uint32_t)cx * DOTS_N / P4_W);
    int nj = (int)((uint32_t)cy * DOTS_N / PAD_SIZE);
    if (ni >= DOTS_N) ni = DOTS_N - 1;
    if (nj >= DOTS_N) nj = DOTS_N - 1;

    uint16_t pad_x = cy;
    uint16_t pad_y = (PAD_SIZE - 1) - cx;

    if (code == LV_EVENT_PRESSED) {
        if (s_scroll_active) {
            s_scroll_active = false;
            s_scroll_tick   = 0;
            if (s_scroll_tmr) lv_timer_pause(s_scroll_tmr);
        }
        s_idle_ticks = 0;
        s_i_near   = ni;
        s_j_near   = nj;
        s_glow_t   = 1.0f;
        s_pressing = true;

        uint8_t ccX = kaoss.mapXtoCC(pad_x);
        uint8_t ccY = kaoss.mapYtoCC(pad_y);
        sendCC(MIDI_CH, kaoss.getCCX(), ccX);
        sendCC(MIDI_CH, kaoss.getCCY(), ccY);
        g_lastCCX = ccX;
        g_lastCCY = ccY;
        g_touched  = true;
        update_leds_draw();

    } else if (code == LV_EVENT_PRESSING) {
        bool moved = (ni != s_i_near || nj != s_j_near);
        if (moved) {
            s_i_near = ni;
            s_j_near = nj;
            s_glow_t = 1.0f;
            if (s_decay_tmr) lv_timer_resume(s_decay_tmr);
        }
        uint8_t ccX = kaoss.mapXtoCC(pad_x);
        uint8_t ccY = kaoss.mapYtoCC(pad_y);
        if (ccX != (uint8_t)g_lastCCX || ccY != (uint8_t)g_lastCCY) {
            sendCC(MIDI_CH, kaoss.getCCX(), ccX);
            sendCC(MIDI_CH, kaoss.getCCY(), ccY);
            g_lastCCX = ccX;
            g_lastCCY = ccY;
        }
        update_leds_draw(moved);

    } else if (code == LV_EVENT_RELEASED) {
        s_pressing = false;
        g_touched  = false;
        // sin notas — no hace falta All Notes Off
        // No resetear s_glow_t — el decay continúa visible tras soltar
        update_leds_draw();
    }
}

// ── Strip iluminado al pulsar botón ───────────────────────────────────
static uint8_t brighten(uint8_t c) {
    uint16_t v = (uint16_t)c * 3;
    return (v > 255) ? 255 : (uint8_t)v;
}

static void strip_set_lit(uint8_t idx, bool lit) {
    if (idx >= 4 || !s_strips[idx]) return;
    if (lit) {
        uint32_t c = s_accent_cols[idx];
        lv_obj_set_style_bg_color(s_strips[idx],
            lv_color_make(brighten((c>>16)&0xFF),
                          brighten((c>> 8)&0xFF),
                          brighten( c     &0xFF)), 0);
    } else {
        lv_obj_set_style_bg_color(s_strips[idx], lv_color_hex(s_dim_cols[idx]), 0);
    }
}

// ── Callback botones ──────────────────────────────────────────────────
// btn[1] (Synth): press corto = cicla synth, press largo = abre Bank.
// Se detecta vía LONG_PRESSED: si llega, se suprime la acción de RELEASED.
static bool s_btn1_long_press = false;

static void btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        strip_set_lit(idx, true);
        if (idx == 1) {
            s_btn1_long_press = false;   // resetea flag — se confirma en LONG_PRESSED
            // acción de ciclar synth se pospone a RELEASED
        } else {
            switch (idx) {
                case 0:
                    g_holdMode = !g_holdMode;
                    uiKaossUpdateHold();
                    break;
                case 2:
                    kaoss.nextPreset();
                    uiKaossUpdateScale();
                    break;
                case 3:
                    sendCC(MIDI_CH, kaoss.getCCX(), 64);
                    sendCC(MIDI_CH, kaoss.getCCY(), 64);
                    g_lastCCX  = 64;
                    g_lastCCY  = 64;
                    g_touched  = false;
                    s_pressing = false;
                    s_glow_t   = 0.0f;
                    update_leds_draw();
                    break;
                default: break;
            }
        }
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (idx == 1) {
            s_btn1_long_press = true;
            if (uiBankIsOpen()) uiBankHide();
            else                uiBankShow();
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (idx == 0) {
            uiKaossUpdateHold();
        } else if (idx == 1) {
            if (!s_btn1_long_press) {
                // press corto: cicla sintetizador
                g_currentSynth = (ExSynth)(((uint8_t)g_currentSynth + 1) % NUM_SYNTHS);
                uiKaossUpdateSynth();
            }
            strip_set_lit(1, false);
            s_btn1_long_press = false;
        } else {
            strip_set_lit(idx, false);
        }
    }
}

// ── Helper botón ─────────────────────────────────────────────────────
static lv_obj_t* make_btn(lv_obj_t* parent, int32_t btn_x, int32_t btn_y,
                           const char* label, uint32_t accent_col, uint32_t dim_col,
                           uint32_t bg_col, uint8_t idx) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_pos(btn, btn_x, btn_y);
    lv_obj_set_size(btn, BTN_W, BTN_H);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_col), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, (void*)(uintptr_t)idx);

    int32_t strip_y = (btn_y < PAD_START_Y) ? (BTN_H - 4) : 0;
    lv_obj_t* strip = lv_obj_create(btn);
    lv_obj_set_pos(strip, 0, strip_y);
    lv_obj_set_size(strip, BTN_W, 4);
    lv_obj_set_style_bg_color(strip, lv_color_hex(dim_col), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    s_strips[idx]      = strip;
    s_accent_cols[idx] = accent_col;
    s_dim_cols[idx]    = dim_col;

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 15, 0);
    rot_label(lbl);

    return btn;
}

// ── Crea rejilla 8×8 de dots ──────────────────────────────────────────
static void pad_draw_leds(lv_obj_t* pad) {
    for (int i = 0; i < DOTS_N; i++) {
        int32_t dcx = (P4_W    * (2*i + 1)) / (2 * DOTS_N);
        for (int j = 0; j < DOTS_N; j++) {
            int32_t dcy = (PAD_SIZE * (2*j + 1)) / (2 * DOTS_N);

            lv_obj_t* d = lv_obj_create(pad);
            lv_obj_set_pos(d, dcx - DOT_W/2, dcy - DOT_H/2);
            lv_obj_set_size(d, DOT_W, DOT_H);
            lv_obj_set_style_radius(d, 3, 0);
            lv_obj_set_style_border_width(d, 0, 0);
            lv_obj_set_style_pad_all(d, 0, 0);
            lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(d, lv_color_make(0, 0, 0), 0);  // apagado
            lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
            s_dots[i][j] = d;
        }
    }
}

// ── API pública ───────────────────────────────────────────────────────
void uiKaossCreate(lv_obj_t* parent) {
    s_pad = lv_obj_create(parent);
    lv_obj_set_pos(s_pad, 0, PAD_START_Y);
    lv_obj_set_size(s_pad, P4_W, PAD_SIZE);
    lv_obj_set_style_bg_color(s_pad, lv_color_hex(COL_PAD_BG), 0);
    lv_obj_set_style_bg_opa(s_pad, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pad, 0, 0);
    lv_obj_set_style_radius(s_pad, 0, 0);
    lv_obj_set_style_pad_all(s_pad, 0, 0);
    lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_pad, pad_event_cb, LV_EVENT_ALL, NULL);

    pad_draw_leds(s_pad);

    s_btn[0] = make_btn(parent, BTN_W, 0, "HOLD", COL_FUNC_HOLD,  COL_BTN_HOLD, COL_BTN_BG, 0);
    s_btn[1] = make_btn(parent, 0,     0, "",  synthColor(),   COL_BTN_TAP,  COL_BTN_BG, 1);

    s_lbl_synth = lv_label_create(s_btn[1]);
    lv_label_set_text(s_lbl_synth, synthName());
    lv_obj_set_style_text_color(s_lbl_synth, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_synth, &lv_font_montserrat_24, 0);
    lv_obj_align(s_lbl_synth, LV_ALIGN_CENTER, 15, 0);
    rot_label(s_lbl_synth);
    s_accent_cols[1] = synthColor();

    s_lbl_hold = lv_label_create(s_btn[0]);
    lv_label_set_text(s_lbl_hold, "OFF");
    lv_obj_set_style_text_color(s_lbl_hold, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_hold, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_hold, LV_ALIGN_CENTER, -15, 0);
    rot_label(s_lbl_hold);

    s_btn[2] = make_btn(parent, BTN_W, PAD_START_Y + PAD_SIZE, "SCALE", 0x00AA44,       COL_BTN_SCALE, COL_BTN_BG,    2);
    s_btn[3] = make_btn(parent, 0,     PAD_START_Y + PAD_SIZE, "PANIC", COL_FUNC_PANIC, COL_BTN_PANIC, COL_BTN_PANIC, 3);

    s_lbl_scale = lv_label_create(s_btn[2]);
    lv_label_set_text(s_lbl_scale, kaoss.presetName());
    lv_obj_set_style_text_color(s_lbl_scale, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_scale, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_scale, LV_ALIGN_CENTER, -15, 0);
    rot_label(s_lbl_scale);

    lv_obj_t* lbl_panic = lv_label_create(s_btn[3]);
    lv_label_set_text(lbl_panic, "ALL OFF");
    lv_obj_set_style_text_color(lbl_panic, lv_color_hex(0xFF6666), 0);
    lv_obj_set_style_text_font(lbl_panic, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_panic, LV_ALIGN_CENTER, -15, 0);
    rot_label(lbl_panic);

    s_decay_tmr  = lv_timer_create(decay_timer_cb,  50,              NULL);
    s_scroll_tmr = lv_timer_create(scroll_timer_cb, SCROLL_STEP_MS,  NULL);
    lv_timer_pause(s_scroll_tmr);
}

void uiKaossUpdateScale() {
    if (s_lbl_scale) lv_label_set_text(s_lbl_scale, kaoss.presetName());
}

void uiKaossUpdateSynth() {
    if (!s_btn[1] || !s_lbl_synth) return;
    uint32_t col = synthColor();
    lv_label_set_text(s_lbl_synth, synthName());
    lv_obj_set_style_text_color(s_lbl_synth, lv_color_hex(COL_TEXT), 0);
    s_accent_cols[1] = col;
    s_dim_cols[1]    = COL_BTN_TAP;
    if (s_strips[1])
        lv_obj_set_style_bg_color(s_strips[1], lv_color_hex(COL_BTN_TAP), 0);
}

void uiKaossUpdateHold() {
    if (!s_btn[0] || !s_lbl_hold) return;
    lv_obj_set_style_bg_color(s_btn[0],
        lv_color_hex(g_holdMode ? (uint32_t)COL_BTN_HOLD : (uint32_t)COL_BTN_BG), 0);
    lv_label_set_text(s_lbl_hold, g_holdMode ? "ON" : "OFF");
    lv_obj_set_style_text_color(s_lbl_hold,
        lv_color_hex(g_holdMode ? (uint32_t)COL_FUNC_HOLD : (uint32_t)COL_TEXT_DIM), 0);
    if (s_strips[0])
        lv_obj_set_style_bg_color(s_strips[0],
            lv_color_hex(g_holdMode ? s_accent_cols[0] : s_dim_cols[0]), 0);
}

void uiKaossUpdateLeds() {
    s_glow_t = 0.0f;
    update_leds_draw();
}

void uiKaossStartScroll() {
    if (s_scroll_active) return;
    s_scroll_active = true;
    s_scroll_tick   = 0;
    s_idle_ticks    = 0;
    s_glow_t        = 0.0f;
    if (s_scroll_tmr) lv_timer_resume(s_scroll_tmr);
}

void uiKaossDestroy() {
    if (s_scroll_tmr) { lv_timer_delete(s_scroll_tmr); s_scroll_tmr = NULL; }
    if (s_decay_tmr)  { lv_timer_delete(s_decay_tmr);  s_decay_tmr  = NULL; }
    if (s_pad) { lv_obj_delete(s_pad); s_pad = NULL; }
    for (int i = 0; i < 4; i++) {
        if (s_btn[i]) { lv_obj_delete(s_btn[i]); s_btn[i] = NULL; }
        s_strips[i] = NULL;
    }
    for (int i = 0; i < DOTS_N; i++)
        for (int j = 0; j < DOTS_N; j++)
            s_dots[i][j] = NULL;
    s_lbl_scale = s_lbl_hold = s_lbl_synth = NULL;
}
