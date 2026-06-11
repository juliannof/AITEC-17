#include "UIHeader.h"
#include "UIMenu.h"
#include "../config.h"
#include "lvgl.h"

LV_FONT_DECLARE(lv_font_dseg7_44);

static lv_obj_t* s_strip      = NULL;
static lv_obj_t* s_timecode   = NULL;
static lv_obj_t* s_tc_ghost   = NULL;
static lv_obj_t* s_mode_lbl   = NULL;
static lv_obj_t* s_solo_lbl   = NULL;
static lv_obj_t* s_cycle_lbl  = NULL;
static lv_obj_t* s_click_lbl  = NULL;

// Beats: contenedor por bloque, ghost+real dentro al (0,0)
static lv_obj_t* s_beat_cont[4]  = {};
static lv_obj_t* s_beat_ghost[4] = {};
static lv_obj_t* s_beat_real[4]  = {};
static lv_obj_t* s_beat_dot[3]   = {};

extern String formatTimecodeString();
extern char timeCodeChars_clean[13];

static bool hasDigit(const char* s) {
    for (; *s; s++) if (*s >= '0' && *s <= '9') return true;
    return false;
}

void uiHeaderCreate(lv_obj_t* parent) {
    // ── Strip azul ────────────────────────────────────────────────
    s_strip = lv_obj_create(parent);
    lv_obj_set_pos(s_strip, 0, 0);
    lv_obj_set_size(s_strip, P4_W, HEADER_H);
    lv_obj_set_style_bg_color(s_strip, lv_color_hex(COL_HEADER), 0);
    lv_obj_set_style_border_width(s_strip, 0, 0);
    lv_obj_set_style_radius(s_strip, 0, 0);
    lv_obj_set_style_pad_all(s_strip, 0, 0);
    lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_strip, 15, 0);
    lv_obj_set_style_shadow_color(s_strip, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(s_strip, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_x(s_strip, 0, 0);
    lv_obj_set_style_shadow_offset_y(s_strip, 10, 0);

    // ── Indicador BEAT/SMPT ───────────────────────────────────────
    s_mode_lbl = lv_obj_create(parent);
    lv_obj_set_pos(s_mode_lbl, 12, (HEADER_H - 34) / 2);
    lv_obj_set_size(s_mode_lbl, 68, 34);
    lv_obj_set_style_bg_opa(s_mode_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_mode_lbl, 1, 0);
    lv_obj_set_style_border_color(s_mode_lbl, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_radius(s_mode_lbl, 4, 0);
    lv_obj_set_style_pad_all(s_mode_lbl, 0, 0);
    lv_obj_clear_flag(s_mode_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_mode_lbl, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t* t = lv_label_create(s_mode_lbl);
        lv_label_set_text(t, (currentTimecodeMode == MODE_BEATS) ? "BEAT" : "SMPT");
        lv_obj_set_style_text_color(t, lv_color_hex(0x00FFFF), 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_center(t);
    }

    // ── Indicador LOOP/CICLO ──────────────────────────────────────
    s_cycle_lbl = lv_obj_create(parent);
    lv_obj_set_pos(s_cycle_lbl, 88, (HEADER_H - 34) / 2);
    lv_obj_set_size(s_cycle_lbl, 44, 34);
    lv_obj_set_style_bg_opa(s_cycle_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_cycle_lbl, 1, 0);
    lv_obj_set_style_border_color(s_cycle_lbl, lv_color_hex(0x006666), 0);
    lv_obj_set_style_radius(s_cycle_lbl, 4, 0);
    lv_obj_set_style_pad_all(s_cycle_lbl, 0, 0);
    lv_obj_clear_flag(s_cycle_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_cycle_lbl, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t* t = lv_label_create(s_cycle_lbl);
        lv_label_set_text(t, LV_SYMBOL_LEFT " " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(t, lv_color_hex(0x006666), 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_center(t);
    }

    // ── Indicador RUDE SOLO ───────────────────────────────────────
    s_solo_lbl = lv_obj_create(parent);
    lv_obj_set_pos(s_solo_lbl, 140, (HEADER_H - 34) / 2);
    lv_obj_set_size(s_solo_lbl, 44, 34);
    lv_obj_set_style_bg_opa(s_solo_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_solo_lbl, 1, 0);
    lv_obj_set_style_border_color(s_solo_lbl, lv_color_hex(0x006666), 0);
    lv_obj_set_style_radius(s_solo_lbl, 4, 0);
    lv_obj_set_style_pad_all(s_solo_lbl, 0, 0);
    lv_obj_clear_flag(s_solo_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_solo_lbl, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t* t = lv_label_create(s_solo_lbl);
        lv_label_set_text(t, "S");
        lv_obj_set_style_text_color(t, lv_color_hex(0x006666), 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_28, 0);
        lv_obj_center(t);
    }

    // ── Indicador CLICK ──────────────────────────────────────────
    s_click_lbl = lv_obj_create(parent);
    lv_obj_set_pos(s_click_lbl, 192, (HEADER_H - 34) / 2);
    lv_obj_set_size(s_click_lbl, 44, 34);
    lv_obj_set_style_bg_opa(s_click_lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_click_lbl, 1, 0);
    lv_obj_set_style_border_color(s_click_lbl, lv_color_hex(0x006666), 0);
    lv_obj_set_style_radius(s_click_lbl, 4, 0);
    lv_obj_set_style_pad_all(s_click_lbl, 0, 0);
    lv_obj_clear_flag(s_click_lbl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_click_lbl, LV_OBJ_FLAG_CLICKABLE);
    {
        lv_obj_t* t = lv_label_create(s_click_lbl);
        lv_label_set_text(t, LV_SYMBOL_AUDIO);
        lv_obj_set_style_text_color(t, lv_color_hex(0x006666), 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_center(t);
    }

    // ── Timecode SMPTE ghost + real (sin cambios) ─────────────────
    s_tc_ghost = lv_label_create(parent);
    lv_label_set_text(s_tc_ghost, "00:00:00: 00");
    lv_obj_set_style_text_color(s_tc_ghost, lv_color_hex(0x006666), 0);
    lv_obj_set_style_text_font(s_tc_ghost, &lv_font_dseg7_44, 0);

    s_timecode = lv_label_create(parent);
    lv_label_set_text(s_timecode, "00:00:00: 00");
    lv_obj_set_style_text_color(s_timecode, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(s_timecode, &lv_font_dseg7_44, 0);

    lv_obj_update_layout(parent);
    int th    = lv_obj_get_height(s_timecode);
    int pos_y = (HEADER_H - th) / 2;

    lv_obj_align(s_tc_ghost, LV_ALIGN_TOP_MID, 0, pos_y);
    lv_obj_align(s_timecode, LV_ALIGN_TOP_MID, 0, pos_y);

    // ── BEATS: 4 bloques + 3 puntos separadores ──────────────────
    {
        const int dw          = 40;
        const int bwidths[4]  = {4*dw, dw, dw, 3*dw};  // 160,40,40,120
        const int dot_gap     = 12;
        const char* ghost_str[4] = {"0000", "0", "0", "000"};

        int bx[4], dx[3];
        bx[0] = 320;
        for (int b = 0; b < 3; b++) {
            dx[b]     = bx[b] + bwidths[b];
            bx[b + 1] = dx[b] + dot_gap;
        }

        bool beatMode = (currentTimecodeMode == MODE_BEATS);

        for (int b = 0; b < 4; b++) {
            s_beat_cont[b] = lv_obj_create(parent);
            lv_obj_set_pos(s_beat_cont[b], bx[b], pos_y);
            lv_obj_set_size(s_beat_cont[b], bwidths[b], th);
            lv_obj_set_style_bg_opa(s_beat_cont[b], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(s_beat_cont[b], 0, 0);
            lv_obj_set_style_pad_all(s_beat_cont[b], 0, 0);
            lv_obj_clear_flag(s_beat_cont[b], LV_OBJ_FLAG_SCROLLABLE);

            s_beat_ghost[b] = lv_label_create(s_beat_cont[b]);
            lv_obj_set_pos(s_beat_ghost[b], 0, 0);
            lv_obj_set_size(s_beat_ghost[b], bwidths[b], th);
            lv_label_set_long_mode(s_beat_ghost[b], LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(s_beat_ghost[b], LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_style_text_color(s_beat_ghost[b], lv_color_hex(0x006666), 0);
            lv_obj_set_style_text_font(s_beat_ghost[b], &lv_font_dseg7_44, 0);
            lv_obj_set_style_bg_opa(s_beat_ghost[b], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(s_beat_ghost[b], 0, 0);
            lv_obj_set_style_pad_all(s_beat_ghost[b], 0, 0);
            lv_obj_clear_flag(s_beat_ghost[b], LV_OBJ_FLAG_SCROLLABLE);
            lv_label_set_text(s_beat_ghost[b], ghost_str[b]);

            s_beat_real[b] = lv_label_create(s_beat_cont[b]);
            lv_obj_set_pos(s_beat_real[b], 0, 0);
            lv_obj_set_size(s_beat_real[b], bwidths[b], th);
            lv_label_set_long_mode(s_beat_real[b], LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(s_beat_real[b], LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_style_text_color(s_beat_real[b], lv_color_hex(0x00FFFF), 0);
            lv_obj_set_style_text_font(s_beat_real[b], &lv_font_dseg7_44, 0);
            lv_obj_set_style_bg_opa(s_beat_real[b], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(s_beat_real[b], 0, 0);
            lv_obj_set_style_pad_all(s_beat_real[b], 0, 0);
            lv_obj_clear_flag(s_beat_real[b], LV_OBJ_FLAG_SCROLLABLE);
            lv_label_set_text(s_beat_real[b], "");

            if (!beatMode) lv_obj_add_flag(s_beat_cont[b], LV_OBJ_FLAG_HIDDEN);
        }

        for (int i = 0; i < 3; i++) {
            s_beat_dot[i] = lv_label_create(parent);
            lv_obj_set_pos(s_beat_dot[i], dx[i], pos_y);
            lv_obj_set_style_text_color(s_beat_dot[i], lv_color_hex(0x006666), 0);
            lv_obj_set_style_text_font(s_beat_dot[i], &lv_font_dseg7_44, 0);
            lv_label_set_text(s_beat_dot[i], ".");
            if (!beatMode) lv_obj_add_flag(s_beat_dot[i], LV_OBJ_FLAG_HIDDEN);
        }

        if (beatMode) {
            lv_obj_add_flag(s_timecode, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_tc_ghost, LV_OBJ_FLAG_HIDDEN);
        }
    }

    uiMenuInit(parent);
}

void uiHeaderUpdate() {
    if (!needsTimecodeRedraw) return;
    static uint32_t lastRedraw = 0;
    uint32_t now = millis();
    if (now - lastRedraw < 16) return;
    lastRedraw = now;
    needsTimecodeRedraw = false;
    if (!s_timecode || !s_tc_ghost) return;

    bool isBeats = (currentTimecodeMode == MODE_BEATS);

    // Indicador BEAT/SMPT
    if (s_mode_lbl) {
        lv_obj_t* lbl = lv_obj_get_child(s_mode_lbl, 0);
        if (lbl) lv_label_set_text(lbl, isBeats ? "BEAT" : "SMPT");
    }

    // Indicador LOOP
    if (s_cycle_lbl) {
        lv_obj_t* lbl = lv_obj_get_child(s_cycle_lbl, 0);
        if (cycleActive) {
            lv_obj_set_style_bg_color(s_cycle_lbl, lv_color_hex(COL_AUTO_LATCH), 0);
            lv_obj_set_style_bg_opa(s_cycle_lbl, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_cycle_lbl, lv_color_hex(COL_AUTO_LATCH), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_opa(s_cycle_lbl, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(s_cycle_lbl, lv_color_hex(0x006666), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0x006666), 0);
        }
    }

    // Indicador RUDE SOLO
    if (s_solo_lbl) {
        lv_obj_t* lbl = lv_obj_get_child(s_solo_lbl, 0);
        if (rudeSoloActive) {
            lv_obj_set_style_bg_color(s_solo_lbl, lv_color_hex(COL_AUTO_LATCH), 0);
            lv_obj_set_style_bg_opa(s_solo_lbl, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_solo_lbl, lv_color_hex(COL_AUTO_LATCH), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_opa(s_solo_lbl, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(s_solo_lbl, lv_color_hex(0x006666), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0x006666), 0);
        }
    }

    // Indicador CLICK
    if (s_click_lbl) {
        lv_obj_t* lbl = lv_obj_get_child(s_click_lbl, 0);
        if (g_clickActive) {
            lv_obj_set_style_bg_color(s_click_lbl, lv_color_hex(0xAA00CC), 0);
            lv_obj_set_style_bg_opa(s_click_lbl, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(s_click_lbl, lv_color_hex(0xAA00CC), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_opa(s_click_lbl, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(s_click_lbl, lv_color_hex(0x006666), 0);
            if (lbl) lv_obj_set_style_text_color(lbl, lv_color_hex(0x006666), 0);
        }
    }

    // Conmutar SMPTE ↔ BEATS
    if (isBeats) {
        lv_obj_add_flag(s_timecode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_tc_ghost, LV_OBJ_FLAG_HIDDEN);
        for (int b = 0; b < 4; b++)
            if (s_beat_cont[b]) lv_obj_clear_flag(s_beat_cont[b], LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 3; i++)
            if (s_beat_dot[i]) lv_obj_clear_flag(s_beat_dot[i], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_timecode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_tc_ghost, LV_OBJ_FLAG_HIDDEN);
        for (int b = 0; b < 4; b++)
            if (s_beat_cont[b]) lv_obj_add_flag(s_beat_cont[b], LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 3; i++)
            if (s_beat_dot[i]) lv_obj_add_flag(s_beat_dot[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (!hasDigit(timeCodeChars_clean)) return;

    if (isBeats) {
        static const int starts[4] = {0, 6, 4, 7};
        static const int counts[4] = {3, 1, 1, 3};
        for (int b = 0; b < 4; b++) {
            if (!s_beat_real[b]) continue;
            char display[8] = {};
            for (int j = 0; j < counts[b]; j++) {
                char c = (char)(beatsChars_clean[starts[b] + j] & 0x7F);
                if (c < '0' || c > '9') c = '0';
                display[j] = c;
            }
            lv_label_set_text(s_beat_real[b], display);
        }
    } else {
        lv_label_set_text(s_timecode, formatTimecodeString().c_str());
        lv_label_set_text(s_tc_ghost, "00:00:00: 00");
    }
}

void uiHeaderDestroy() {
    uiMenuDestroy();
    for (int b = 0; b < 4; b++) {
        if (s_beat_cont[b]) {
            lv_obj_delete(s_beat_cont[b]);  // borra contenedor + hijos
            s_beat_cont[b]  = NULL;
            s_beat_ghost[b] = NULL;
            s_beat_real[b]  = NULL;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (s_beat_dot[i]) { lv_obj_delete(s_beat_dot[i]); s_beat_dot[i] = NULL; }
    }
    if (s_click_lbl)  { lv_obj_delete(s_click_lbl);  s_click_lbl  = NULL; }
    if (s_cycle_lbl) { lv_obj_delete(s_cycle_lbl); s_cycle_lbl = NULL; }
    if (s_solo_lbl)  { lv_obj_delete(s_solo_lbl);  s_solo_lbl  = NULL; }
    if (s_mode_lbl)  { lv_obj_delete(s_mode_lbl);  s_mode_lbl  = NULL; }
    if (s_tc_ghost)  { lv_obj_delete(s_tc_ghost);  s_tc_ghost  = NULL; }
    if (s_timecode)  { lv_obj_delete(s_timecode);  s_timecode  = NULL; }
    if (s_strip)     { lv_obj_delete(s_strip);      s_strip     = NULL; }
}

void uiHeaderEnsureCreated(lv_obj_t* parent) {
    if (s_strip) return;
    uiHeaderCreate(parent);
}
