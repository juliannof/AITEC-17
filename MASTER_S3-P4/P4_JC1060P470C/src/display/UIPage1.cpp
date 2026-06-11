// src/display/UIPage1.cpp
#include "UIPage1.h"
#include "../config.h"
#include "lvgl.h"

extern void sendMIDIBytes(const uint8_t* data, size_t len);

#define P1_COLS        10
#define P1_ROWS        5
#define P1_BTN_COUNT   BTN_PG1_COUNT
#define P1_BTN_MARGIN  4
#define P1_RADIUS      6



static lv_obj_t*  s_page               = NULL;
static lv_obj_t*  s_btns[P1_BTN_COUNT] = {};
static lv_obj_t*  s_lbls[P1_BTN_COUNT] = {};
static lv_color_t s_colActive[P1_BTN_COUNT];
static lv_color_t s_colInactive[P1_BTN_COUNT];

static lv_color_t dimHex(uint32_t hex) {
    return lv_color_make(
        ((hex >> 16) & 0xFF) >> 2,
        ((hex >>  8) & 0xFF) >> 2,
        ( hex        & 0xFF) >> 2
    );
}

static bool needsBlackText(uint8_t colorIdx) {
    uint32_t h = PALETTE_HEX[colorIdx];
    uint32_t r = (h >> 16) & 0xFF;
    uint32_t g = (h >>  8) & 0xFF;
    uint32_t b =  h        & 0xFF;
    return ((r * 299 + g * 587 + b * 114) / 1000) > 160;
}

static void applyButtonState(int i, bool active) {
    if (!s_btns[i]) return;
    lv_color_t bg = active ? s_colActive[i] : s_colInactive[i];
    lv_obj_set_style_bg_color(s_btns[i], bg, LV_PART_MAIN);

    lv_color_t txtColor = (active && needsBlackText(BTN_COLOR_IDX[i]))
        ? lv_color_black()
        : lv_color_white();
    lv_obj_set_style_text_color(s_lbls[i], txtColor, LV_PART_MAIN);
}

static void btn_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (code == LV_EVENT_PRESSED) {
        if (MIDI_NOTES_PG1[idx] != 0x00) {
            uint8_t msg[3] = { 0x90, MIDI_NOTES_PG1[idx], 0x7F };
            sendMIDIBytes(msg, 3);
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (MIDI_NOTES_PG1[idx] != 0x00) {
            uint8_t msg[3] = { 0x90, MIDI_NOTES_PG1[idx], 0x00 };
            sendMIDIBytes(msg, 3);
        }
    }
}

void uiPage1Create(lv_obj_t* parent) {
    log_i("[UIPage1] Create start, parent=%p", parent);
    s_page = lv_obj_create(parent);
    lv_obj_set_pos(s_page, 0, 0);
    lv_obj_set_size(s_page, P4_W, CONTENT_H);   // landscape, llena el content_area
    lv_obj_set_style_pad_all(s_page, 0, 0);
    lv_obj_set_style_border_width(s_page, 0, 0);
    lv_obj_set_style_bg_color(s_page, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_page, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_add_flag(s_page, LV_OBJ_FLAG_CLICKABLE);

    // 10 columnas × 5 filas → 102×102px por celda
    int32_t cell_w = P4_W      / P1_COLS;
    int32_t cell_h = CONTENT_H / P1_ROWS;
    int32_t btn_w  = cell_w - P1_BTN_MARGIN * 2;
    int32_t btn_h  = cell_h - P1_BTN_MARGIN * 2;

    for (int i = 0; i < P1_BTN_COUNT; i++) {
        // Ocultar slots sin nota MIDI
        if (MIDI_NOTES_PG1[i] == 0x00) {
            s_btns[i] = NULL;
            s_lbls[i] = NULL;
            continue;
        }

        int col = i % P1_COLS;
        int row = i / P1_COLS;

        uint8_t ci = BTN_COLOR_IDX[i];
        s_colActive[i]   = lv_color_hex(PALETTE_HEX[ci]);
        s_colInactive[i] = dimHex(PALETTE_HEX[ci]);

        lv_obj_t* btn = lv_obj_create(s_page);
        lv_obj_set_pos(btn, col * cell_w + P1_BTN_MARGIN,
                            row * cell_h + P1_BTN_MARGIN);
        lv_obj_set_size(btn, btn_w, btn_h);

        lv_obj_set_style_radius(btn, P1_RADIUS, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x555555), 0);
        lv_obj_set_style_pad_all(btn, 2, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(btn);
        // D-pad: símbolos LVGL en vez de texto
        if      (i == 44) lv_label_set_text(lbl, LV_SYMBOL_UP);
        else if (i == 45) lv_label_set_text(lbl, LV_SYMBOL_DOWN);
        else if (i == 46) lv_label_set_text(lbl, LV_SYMBOL_LEFT);
        else if (i == 47) lv_label_set_text(lbl, LV_SYMBOL_RIGHT);
        else              lv_label_set_text(lbl, LABELS_PG1[i]);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_PRESSED,  (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_RELEASED, (void*)(intptr_t)i);

        s_btns[i] = btn;
        s_lbls[i] = lbl;

        applyButtonState(i, false);
    }

    log_i("[UIPage1] Creada: cell_w=%d cell_h=%d btn=%dx%d",
          (int)cell_w, (int)cell_h, (int)btn_w);
}

void uiPage1UpdateButton(int index, bool active) {
    if (index < 0 || index >= P1_BTN_COUNT) return;
    applyButtonState(index, active);
}

void uiPage1Update() {
    if (!s_page) return;
    if (needsButtonsRedraw) {
        uiPage1UpdateAllButtons();
        needsButtonsRedraw = false;
    }
}

void uiPage1UpdateAllButtons() {
    for (int i = 0; i < P1_BTN_COUNT; i++)
        applyButtonState(i, btnStatePG1[i]);
}


void uiPage1SetVisible(bool visible) {
    if (!s_page) return;
    if (visible) lv_obj_remove_flag(s_page, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_page, LV_OBJ_FLAG_HIDDEN);
}

void uiPage1Destroy() {
    if (s_page) {
        lv_obj_delete(s_page);
        s_page = NULL;
        for (int i = 0; i < P1_BTN_COUNT; i++) {
            s_btns[i] = NULL;
            s_lbls[i] = NULL;
        }
        log_i("[UIPage1] Destruida");
    }
}

lv_obj_t* uiPage1GetRoot() { return s_page; }