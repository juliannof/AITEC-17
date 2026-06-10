#include "UIHeader.h"
#include "UIMenu.h"
#include "../config.h"
#include "lvgl.h"

LV_FONT_DECLARE(lv_font_dseg7_44);

static lv_obj_t* s_strip    = NULL;
static lv_obj_t* s_timecode = NULL;
static lv_obj_t* s_tc_ghost = NULL;
static lv_obj_t* s_mode_lbl = NULL;

extern String formatBeatString();
extern String formatTimecodeString();
extern char timeCodeChars_clean[13];

static bool hasDigit(const char* s) {
    for (; *s; s++) if (*s >= '0' && *s <= '9') return true;
    return false;
}

void uiHeaderCreate(lv_obj_t* parent) {
    // ── Strip azul ────────────────────────────────────────────────
    s_strip = lv_obj_create(parent);
    // Franja superior landscape: ancho completo × HEADER_H (2026-06-09)
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
    lv_obj_set_style_shadow_offset_y(s_strip, 10, 0);   // sombra hacia abajo

    // ── Indicador BEAT/SMPT — read-only, controlado por Logic vía note 113/114 ──
s_mode_lbl = lv_obj_create(parent);
lv_obj_set_pos(s_mode_lbl, 12, (HEADER_H - 30) / 2);
lv_obj_set_size(s_mode_lbl, 68, 30);
lv_obj_set_style_bg_opa(s_mode_lbl, LV_OPA_TRANSP, 0);
lv_obj_set_style_border_width(s_mode_lbl, 1, 0);
lv_obj_set_style_border_color(s_mode_lbl, lv_color_hex(0x00FFFF), 0);
lv_obj_set_style_radius(s_mode_lbl, 4, 0);
lv_obj_set_style_pad_all(s_mode_lbl, 0, 0);
lv_obj_clear_flag(s_mode_lbl, LV_OBJ_FLAG_SCROLLABLE);
lv_obj_clear_flag(s_mode_lbl, LV_OBJ_FLAG_CLICKABLE);

lv_obj_t* mode_txt = lv_label_create(s_mode_lbl);
lv_label_set_text(mode_txt,
                  (currentTimecodeMode == MODE_BEATS) ? "BEAT" : "SMPT");
lv_obj_set_style_text_color(mode_txt, lv_color_hex(0x00FFFF), 0);
lv_obj_set_style_text_font(mode_txt, &lv_font_montserrat_16, 0);
lv_obj_center(mode_txt);

    // ── Timecode ghost + real ─────────────────────────────────────
    const char* init_text = (currentTimecodeMode == MODE_BEATS)
                            ? "0. 0. 0. 000" : "00:00:00: 00";

    s_tc_ghost = lv_label_create(parent);
    lv_label_set_text(s_tc_ghost, init_text);
    lv_obj_set_style_text_color(s_tc_ghost, lv_color_hex(0x006666), 0);
    lv_obj_set_style_text_font(s_tc_ghost, &lv_font_dseg7_44, 0);

    s_timecode = lv_label_create(parent);
    lv_label_set_text(s_timecode, init_text);
    lv_obj_set_style_text_color(s_timecode, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_text_font(s_timecode, &lv_font_dseg7_44, 0);

    lv_obj_update_layout(parent);
    int tw    = lv_obj_get_width(s_timecode);
    int th    = lv_obj_get_height(s_timecode);
    // centrado horizontal en la franja superior, sin rotación (2026-06-09)
    int pos_x = (P4_W - tw) / 2;
    int pos_y = (HEADER_H - th) / 2;

    lv_obj_set_pos(s_tc_ghost, pos_x, pos_y);
    lv_obj_set_pos(s_timecode, pos_x, pos_y);

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

    // actualizar indicador de modo
    if (s_mode_lbl) {
        lv_obj_t* lbl = lv_obj_get_child(s_mode_lbl, 0);
        if (lbl) lv_label_set_text(lbl, isBeats ? "BEAT" : "SMPT");
    }

    if (!hasDigit(timeCodeChars_clean)) return;

    String displayText = isBeats ? formatBeatString() : formatTimecodeString();
    lv_label_set_text(s_timecode, displayText.c_str());
    lv_label_set_text(s_tc_ghost, isBeats ? "0. 0. 0.  000" : "00:00:00: 00");
}

void uiHeaderDestroy() {
    uiMenuDestroy();
    if (s_mode_lbl) { lv_obj_delete(s_mode_lbl); s_mode_lbl = NULL; }
    if (s_tc_ghost) { lv_obj_delete(s_tc_ghost);  s_tc_ghost = NULL; }
    if (s_timecode) { lv_obj_delete(s_timecode);  s_timecode = NULL; }
    if (s_strip)    { lv_obj_delete(s_strip);      s_strip    = NULL; }
}

void uiHeaderEnsureCreated(lv_obj_t* parent) {
    if (s_strip) return;
    uiHeaderCreate(parent);
}