// src/display/UIPage3B.cpp
#include "UIPage3B.h"
#include "UIMenu.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"

// Layout landscape: cada canal = columna CH_W × CONTENT_H, zonas apiladas (2026-06-09)
#define FADER_TOP    8
#define FADER_H      210                 // fader vertical
#define FADER_WIDTH  44
#define NAME_TOP     (FADER_TOP + FADER_H + 6)   // 224
#define NAME_H       28
#define PAN_TOP      (NAME_TOP + NAME_H + 6)      // 258
#define PAN_SZ       52
#define AUTO_TOP     (PAN_TOP + PAN_SZ + 8)       // 318
#define AUTO_H       52
#define MUTE_TOP     (AUTO_TOP + AUTO_H + 6)      // 376
#define MUTE_H       52

static constexpr int P3B_CH = 8;  // UIPage3B siempre muestra 8 canales

// Colores desde config.h (COL_*)

// ── Widgets ───────────────────────────────────────────────
static lv_obj_t* s_page_root = NULL;
static lv_obj_t* s_track_bg[P3B_CH] = {};
static lv_obj_t* s_fader[P3B_CH]    = {};
static lv_obj_t* s_automode[P3B_CH] = {};
static lv_obj_t* s_automode_lbl[P3B_CH] = {};
static lv_obj_t* s_mute[P3B_CH]     = {};
static lv_obj_t* s_trackname[P3B_CH]= {};
static lv_obj_t* s_arc[P3B_CH]      = {};
static lv_obj_t* s_arc_lbl[P3B_CH]  = {};

static bool s_page3b_ready = false;

// Automode state per channel — 0=READ 1=TOUCH 2=LATCH 3=WRITE
static uint8_t s_automode_state[P3B_CH] = {};

// Mackie automode values:
// 0=READ 1=WRITE 2=TRIM 3=TOUCH 4=LATCH 5=OFF
static const char* AUTOMODE_LABELS[] = { "R", "W", "T", "TCH", "L", "OFF" };
static const lv_color_t AUTOMODE_COLORS[] = {
    lv_color_hex(0x006600),  // READ — verde
    lv_color_hex(0xAA0000),  // WRITE — rojo
    lv_color_hex(0x006600),  // TRIM — verde claro
    lv_color_hex(0x0000AA),  // TOUCH — azul
    lv_color_hex(0xAA6600),  // LATCH — naranja
    lv_color_hex(COL_AUTO_OFF),  // OFF — gris
};
static const uint8_t AUTOMODE_NOTES[] = { 0x4A, 0x4D, 0x4E, 0x4B }; // READ TOUCH LATCH WRITE

extern void sendMIDIBytes(const byte* data, size_t len);
extern String formatBeatString();
extern String formatTimecodeString();
extern uint8_t g_channelAutoMode[8];


static lv_color_t automode_color(uint8_t state) {
    switch (state) {
        case 0: return lv_color_hex(COL_AUTO_READ);
        case 1: return lv_color_hex(COL_AUTO_TOUCH);
        case 2: return lv_color_hex(COL_AUTO_LATCH);
        case 3: return lv_color_hex(COL_AUTO_WRITE);
        default: return lv_color_hex(COL_AUTO_OFF);
    }
}

void uiPage3BCreate(lv_obj_t* parent) {
    s_page_root = lv_obj_create(parent);
    lv_obj_set_pos(s_page_root, 0, 0);
    lv_obj_set_size(s_page_root, P4_W, CONTENT_H);   // landscape (1024×512)
    lv_obj_set_style_bg_color(s_page_root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_page_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_page_root, 0, 0);
    lv_obj_set_style_border_width(s_page_root, 0, 0);
    lv_obj_clear_flag(s_page_root, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < P3B_CH; i++) {
        int x = i * CH_W;   // columna del canal (landscape)

        s_track_bg[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_track_bg[i], x, 0);
        lv_obj_set_size(s_track_bg[i], CH_W - 1, CONTENT_H);
        lv_obj_set_style_bg_color(s_track_bg[i], lv_color_hex(COL_TRACK_BG), 0);
        lv_obj_set_style_border_width(s_track_bg[i], 0, 0);
        lv_obj_set_style_radius(s_track_bg[i], 0, 0);
        lv_obj_clear_flag(s_track_bg[i], LV_OBJ_FLAG_SCROLLABLE);

        // Fader vertical
        s_fader[i] = lv_slider_create(s_page_root);
        lv_obj_set_pos(s_fader[i], x + (CH_W - FADER_WIDTH) / 2, FADER_TOP);
        lv_obj_set_size(s_fader[i], FADER_WIDTH, FADER_H);
        lv_slider_set_range(s_fader[i], 0, 16383);
        lv_slider_set_value(s_fader[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_fader[i], lv_color_hex(COL_FADER_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_fader[i], lv_color_hex(COL_FADER_THUMB), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_fader[i], lv_color_hex(COL_FADER_THUMB), LV_PART_KNOB);

        lv_obj_add_event_cb(s_fader[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            byte sel[3] = { 0x90, (uint8_t)(0x08 + ch), 127 };
            sendMIDIBytes(sel, 3);
        }, LV_EVENT_PRESSED, (void*)(intptr_t)i);

        lv_obj_add_event_cb(s_fader[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
            int val = lv_slider_get_value(sl);
            byte lsb = val & 0x7F;
            byte msb = (val >> 7) & 0x7F;
            byte msg[3] = { (byte)(0xE0 + ch), lsb, msb };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);

        // nombre de pista
        lv_obj_t* tn_cont = lv_obj_create(s_page_root);
        lv_obj_set_pos(tn_cont, x, NAME_TOP);
        lv_obj_set_size(tn_cont, CH_W, NAME_H);
        lv_obj_set_style_bg_opa(tn_cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tn_cont, 0, 0);
        lv_obj_clear_flag(tn_cont, LV_OBJ_FLAG_SCROLLABLE);
        s_trackname[i] = lv_label_create(tn_cont);
        lv_label_set_text(s_trackname[i], "---");
        lv_obj_set_style_text_color(s_trackname[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(s_trackname[i], &lv_font_montserrat_14, 0);
        lv_obj_center(s_trackname[i]);

        // panorama (arco)
        s_arc[i] = lv_arc_create(s_page_root);
        lv_obj_set_pos(s_arc[i], x + (CH_W - PAN_SZ) / 2, PAN_TOP);
        lv_obj_set_size(s_arc[i], PAN_SZ, PAN_SZ);
        lv_arc_set_range(s_arc[i], -100, 100);
        lv_arc_set_value(s_arc[i], 0);
        lv_arc_set_bg_angles(s_arc[i], 135, 405);
        lv_arc_set_mode(s_arc[i], LV_ARC_MODE_SYMMETRICAL);
        lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x00FF00), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_arc[i], 4, LV_PART_INDICATOR);
        lv_obj_set_style_opa(s_arc[i], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(s_arc[i], LV_OBJ_FLAG_CLICKABLE);

        s_arc_lbl[i] = lv_label_create(s_arc[i]);
        lv_label_set_text(s_arc_lbl[i], "C");
        lv_obj_set_style_text_color(s_arc_lbl[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(s_arc_lbl[i], &lv_font_montserrat_12, 0);
        lv_obj_center(s_arc_lbl[i]);

        // AUTOMODE
        s_automode[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_automode[i], x + 8, AUTO_TOP);
        lv_obj_set_size(s_automode[i], CH_W - 16, AUTO_H);
        lv_obj_set_style_bg_color(s_automode[i], lv_color_hex(COL_AUTO_READ), 0);
        lv_obj_set_style_border_width(s_automode[i], 0, 0);
        lv_obj_set_style_radius(s_automode[i], 6, 0);
        lv_obj_clear_flag(s_automode[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_automode[i], LV_OBJ_FLAG_CLICKABLE);
        s_automode_lbl[i] = lv_label_create(s_automode[i]);
        lv_label_set_text(s_automode_lbl[i], "R");
        lv_obj_set_style_text_color(s_automode_lbl[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(s_automode_lbl[i]);
        lv_obj_add_event_cb(s_automode[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            s_automode_state[ch] = (s_automode_state[ch] + 1) % 4;
            byte msg[3] = { 0x90, AUTOMODE_NOTES[s_automode_state[ch]], 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // MUTE
        s_mute[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_mute[i], x + 8, MUTE_TOP);
        lv_obj_set_size(s_mute[i], CH_W - 16, MUTE_H);
        lv_obj_set_style_bg_color(s_mute[i], lv_color_hex(COL_MUTE_OFF), 0);
        lv_obj_set_style_border_width(s_mute[i], 0, 0);
        lv_obj_set_style_radius(s_mute[i], 6, 0);
        lv_obj_clear_flag(s_mute[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_mute[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* mute_lbl = lv_label_create(s_mute[i]);
        lv_label_set_text(mute_lbl, "M");
        lv_obj_set_style_text_color(mute_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(mute_lbl);
        lv_obj_add_event_cb(s_mute[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            byte msg[3] = { 0x90, (uint8_t)(0x10 + ch), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    needsButtonsRedraw = true;
    s_page3b_ready = true;
}

void uiPage3BUpdate() {
    if (!s_page3b_ready) return;

    // needsTimecodeRedraw eliminado — lo gestiona uiHeaderUpdate()

    if (needsButtonsRedraw) {
        for (int i = 0; i < P3B_CH; i++) {
            lv_obj_set_style_bg_color(s_track_bg[i],
                selectStates[i + P4_CH_OFFSET] ? lv_color_hex(COL_TRACK_SEL)
                                               : lv_color_hex(COL_TRACK_BG), 0);
            lv_obj_set_style_bg_color(s_mute[i],
                muteStates[i + P4_CH_OFFSET] ? lv_color_hex(COL_MUTE_ON)
                                             : lv_color_hex(COL_MUTE_OFF), 0);
            uint8_t am = g_channelAutoMode[i];
            lv_obj_set_style_bg_color(s_automode[i],
                AUTOMODE_COLORS[am < 6 ? am : 0], 0);
            lv_label_set_text(s_automode_lbl[i],
                AUTOMODE_LABELS[am < 6 ? am : 0]);
            lv_label_set_text(s_trackname[i], trackNames[i + P4_CH_OFFSET].c_str());
            int pos = (int)(vpotValues[i + P4_CH_OFFSET] & 0x0F);
            int pan = ((pos - 6) * 100) / 6;
            lv_arc_set_value(s_arc[i], pan);
            lv_obj_invalidate(s_arc[i]);
            char pan_txt[5];
            if (pos == 6)      snprintf(pan_txt, sizeof(pan_txt), "C");
            else if (pos > 6)  snprintf(pan_txt, sizeof(pan_txt), "R%d", pos - 6);
            else               snprintf(pan_txt, sizeof(pan_txt), "L%d", 6 - pos);
            lv_label_set_text(s_arc_lbl[i], pan_txt);
            int fval = (int)(faderPositions[i + P4_CH_OFFSET] * 16383.0f);
            lv_slider_set_value(s_fader[i], fval, LV_ANIM_OFF);
        }
        needsButtonsRedraw = false;
    }
}

void uiPage3BDestroy() {
    if (s_page_root) {
        lv_obj_del(s_page_root);
        s_page_root    = NULL;
        s_page3b_ready = false;
    }
}
