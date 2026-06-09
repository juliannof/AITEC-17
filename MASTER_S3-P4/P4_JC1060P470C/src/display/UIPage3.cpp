#include "UIPage3.h"
#include "UIMenu.h"

#include "../config.h"
#include "Display.h"
#include "lvgl.h"



// Layout landscape: cada canal es una COLUMNA (CH_W × CONTENT_H), zonas apiladas
// verticalmente de arriba a abajo (2026-06-09)
#define VU_TOP       4
#define VU_H         220                     // zona VU (12 segmentos verticales)
#define NAME_TOP     (VU_TOP + VU_H + 6)     // 230
#define NAME_H       28
#define PAN_TOP      (NAME_TOP + NAME_H + 6) // 264
#define PAN_SZ       52                      // arco de panorama (cuadrado)
#define SEL_TOP      (PAN_TOP + PAN_SZ + 8)  // 324
#define SEL_H        52
#define MUTE_TOP     (SEL_TOP + SEL_H + 6)   // 382
#define MUTE_H       52

// ── Colores ───────────────────────────────────────────────
#define COLOR_BG        lv_color_hex(0x000000)
#define COLOR_HEADER    lv_color_hex(0x000050)
#define COLOR_MUTE_ON   lv_color_hex(0xFF0000)
#define COLOR_MUTE_OFF  lv_color_hex(0x400000)
#define COLOR_SOLO_ON   lv_color_hex(0xFFAA00)
#define COLOR_SOLO_OFF  lv_color_hex(0x333333) 

#define COLOR_TRACK_BG       lv_color_hex(0x0F1218)
#define COLOR_TRACK_SEL      lv_color_hex(0x2A3040)
#define COLOR_TRACK_SEP      lv_color_hex(0x111111)

// ── Widgets ───────────────────────────────────────────────

static lv_obj_t* s_page_root     = NULL;
static lv_obj_t* s_track_bg[NUM_CH] = {};
static lv_obj_t* s_mute[NUM_CH]      = {};
static lv_obj_t* s_select[NUM_CH]    = {};
static lv_obj_t* s_trackname[NUM_CH] = {};
static lv_obj_t* s_arc[NUM_CH]       = {};
static lv_obj_t* s_vu[NUM_CH]        = {};
// VU — 12 segmentos
static lv_obj_t* s_vu_seg[NUM_CH][12] = {};

static lv_obj_t* s_slider_panel      = NULL;
static lv_obj_t* s_slider            = NULL;
static bool      s_slider_visible    = false;

static bool s_page3_ready = false;


extern void sendMIDIBytes(const byte* data, size_t len);

static lv_obj_t* s_arc_lbl[NUM_CH] = {};



void uiPage3Create(lv_obj_t* parent) {
    s_page_root = lv_obj_create(parent);
    lv_obj_set_pos(s_page_root, 0, 0);
    lv_obj_set_size(s_page_root, P4_W, CONTENT_H);   // llena el content_area (1024×512)
    lv_obj_set_style_bg_color(s_page_root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_page_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_page_root, 0, 0);
    lv_obj_set_style_border_width(s_page_root, 0, 0);
    lv_obj_clear_flag(s_page_root, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < NUM_CH; i++) {
        int x = i * CH_W;   // columna del canal (landscape)

        // fondo de la pista (columna completa)
        s_track_bg[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_track_bg[i], x, 0);
        lv_obj_set_size(s_track_bg[i], CH_W - 1, CONTENT_H);
        lv_obj_set_style_bg_color(s_track_bg[i], lv_color_hex(COL_TRACK_BG), 0);
        lv_obj_set_style_border_width(s_track_bg[i], 0, 0);
        lv_obj_set_style_radius(s_track_bg[i], 0, 0);
        lv_obj_clear_flag(s_track_bg[i], LV_OBJ_FLAG_SCROLLABLE);

        // VU — 12 segmentos apilados verticalmente (segmento 0 abajo)
        int seg_w = CH_W - 16;
        int seg_h = (VU_H - 11) / 12;
        for (int s = 0; s < 12; s++) {
            int seg_y = VU_TOP + (11 - s) * (seg_h + 1);
            s_vu_seg[i][s] = lv_obj_create(s_page_root);
            lv_obj_set_pos(s_vu_seg[i][s], x + 8, seg_y);
            lv_obj_set_size(s_vu_seg[i][s], seg_w, seg_h);
            lv_obj_set_style_border_width(s_vu_seg[i][s], 0, 0);
            lv_obj_set_style_radius(s_vu_seg[i][s], 1, 0);
            lv_obj_clear_flag(s_vu_seg[i][s], LV_OBJ_FLAG_SCROLLABLE);
            lv_color_t off_color;
            if      (s < 8)  off_color = lv_color_hex(0x003300);
            else if (s < 10) off_color = lv_color_hex(0x333300);
            else             off_color = lv_color_hex(0x330000);
            lv_obj_set_style_bg_color(s_vu_seg[i][s], off_color, 0);
            lv_obj_add_flag(s_vu_seg[i][s], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_vu_seg[i][s], [](lv_event_t* e) {
                int ch = (int)(intptr_t)lv_event_get_user_data(e);
                byte msg[3] = { 0x90, (uint8_t)(0x18 + ch), 127 };
                sendMIDIBytes(msg, 3);
            }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }

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

        // SELECT (S)
        s_select[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_select[i], x + 8, SEL_TOP);
        lv_obj_set_size(s_select[i], CH_W - 16, SEL_H);
        lv_obj_set_style_bg_color(s_select[i], lv_color_hex(COL_SOLO_OFF), 0);
        lv_obj_set_style_border_width(s_select[i], 0, 0);
        lv_obj_set_style_radius(s_select[i], 6, 0);
        lv_obj_clear_flag(s_select[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_select[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* sel_lbl = lv_label_create(s_select[i]);
        lv_label_set_text(sel_lbl, "S");
        lv_obj_set_style_text_color(sel_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(sel_lbl);
        lv_obj_add_event_cb(s_select[i], [](lv_event_t* e) {
            int ch = (int)(intptr_t)lv_event_get_user_data(e);
            byte msg[3] = { 0x90, (uint8_t)(0x08 + ch), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // MUTE (M)
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
    
    s_page3_ready = true;

    needsTimecodeRedraw = true;
    needsButtonsRedraw  = true;

    uiMenuInit(s_page_root);


    s_page3_ready = true;

    
}




void uiPage3Update() {
    if (!s_page3_ready) return;

    if (needsButtonsRedraw) {
        for (int i = 0; i < NUM_CH; i++) {
            lv_obj_set_style_bg_color(s_track_bg[i],
                selectStates[i] ? lv_color_hex(COL_TRACK_SEL)
                                : lv_color_hex(COL_TRACK_BG), 0);
            lv_obj_set_style_bg_color(s_mute[i],
                muteStates[i] ? lv_color_hex(COL_MUTE_ON)
                              : lv_color_hex(COL_MUTE_OFF), 0);
            lv_obj_set_style_bg_color(s_select[i],
                soloStates[i] ? lv_color_hex(COL_SOLO_ON)
                              : lv_color_hex(COL_SOLO_OFF), 0);
            lv_label_set_text(s_trackname[i], trackNames[i].c_str());
            int pos = (int)(vpotValues[i] & 0x0F);
            int pan = ((pos - 6) * 100) / 6;
            lv_arc_set_value(s_arc[i], pan);
            char pan_txt[5];
            if (pos == 6)      snprintf(pan_txt, sizeof(pan_txt), "C");
            else if (pos > 6)  snprintf(pan_txt, sizeof(pan_txt), "R%d", pos - 6);
            else               snprintf(pan_txt, sizeof(pan_txt), "L%d", 6 - pos);
            lv_label_set_text(s_arc_lbl[i], pan_txt);
        }
        needsButtonsRedraw = false;
    }

    if (needsVUMetersRedraw) {
        for (int i = 0; i < NUM_CH; i++) {
            int activeSegments = (int)round(vuLevels[i] * 12.0f);
            int peakSeg = (int)round(vuPeakLevels[i] * 12.0f) - 1;
            for (int s = 0; s < 12; s++) {
                lv_color_t color;
                if (s == peakSeg && vuPeakLevels[i] > vuLevels[i] + 0.001f)
                    color = lv_color_hex(0xB4B4B4);
                else if (s < activeSegments) {
                    if      (s < 8)  color = lv_color_hex(0x00E600);
                    else if (s < 10) color = lv_color_hex(0xFFFF00);
                    else             color = lv_color_hex(0xFF0000);
                } else {
                    if      (s < 8)  color = lv_color_hex(0x003300);
                    else if (s < 10) color = lv_color_hex(0x333300);
                    else             color = lv_color_hex(0x330000);
                }
                lv_obj_set_style_bg_color(s_vu_seg[i][s], color, 0);
            }
        }
        needsVUMetersRedraw = false;
    }
}

// ****************************************************************************
// Lógica de decaimiento de los vúmetros y retención de picos
// ****************************************************************************
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 100;
    const unsigned long PEAK_HOLD_TIME_MS = 2000;
    const float         DECAY_AMOUNT      = 1.0f / 12.0f;

    unsigned long currentTime    = millis();
    bool          anyVUMeterChanged = false;

    for (int i = 0; i < 8; i++) {
        if (vuLevels[i] > 0 && currentTime - vuLastUpdateTime[i] > DECAY_INTERVAL_MS) {
            vuLevels[i] -= DECAY_AMOUNT;
            if (vuLevels[i] < 0.01f) vuLevels[i] = 0.0f;
            vuLastUpdateTime[i] = currentTime;
            anyVUMeterChanged = true;
        }

        if (vuPeakLevels[i] > 0 &&
            currentTime - vuPeakLastUpdateTime[i] > PEAK_HOLD_TIME_MS &&
            vuPeakLevels[i] > vuLevels[i]) {
            vuPeakLevels[i] = vuLevels[i];
            anyVUMeterChanged = true;
        }

        if (vuPeakLevels[i] < vuLevels[i]) {
            vuPeakLevels[i]         = vuLevels[i];
            vuPeakLastUpdateTime[i] = currentTime;
            anyVUMeterChanged       = true;
        }
    }

    if (anyVUMeterChanged) needsVUMetersRedraw = true;
}

void uiPage3Destroy() {
    if (s_page_root) {
        lv_obj_del(s_page_root);
        s_page_root   = NULL;
        s_page3_ready = false;
    }
}