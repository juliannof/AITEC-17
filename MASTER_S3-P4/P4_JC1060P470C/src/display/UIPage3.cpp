#include "UIPage3.h"
#include "UIMenu.h"

#include "../config.h"
#include "Display.h"
#include "lvgl.h"



// Layout landscape: cada canal es una COLUMNA (CH_W × CONTENT_H), zonas apiladas
// verticalmente de arriba a abajo — orden: botones → paneo → nombre → VU (2026-06-11)
#define SEL_TOP      6
#define SEL_H        52
#define MUTE_TOP     (SEL_TOP + SEL_H + 4)    // 62
#define MUTE_H       52
#define PAN_TOP      (MUTE_TOP + MUTE_H + 15) // 127
#define PAN_SZ       44                        // arco de panorama (cuadrado)
#define NAME_TOP     (PAN_TOP + PAN_SZ + 6)   // 178
#define NAME_H       28
#define VU_TOP       (NAME_TOP + NAME_H + 6)  // 212
#define VU_H         (CONTENT_H - VU_TOP - 4) // 296 — VU ocupa el resto

// Colores desde config.h (COL_*)

// ── Widgets ───────────────────────────────────────────────

static lv_obj_t* s_page_root     = NULL;
static lv_obj_t* s_track_bg[NUM_CH] = {};
static lv_obj_t* s_mute[NUM_CH]      = {};
static lv_obj_t* s_select[NUM_CH]    = {};
static lv_obj_t* s_trackname[NUM_CH] = {};
static lv_obj_t* s_arc[NUM_CH]       = {};
static lv_obj_t* s_vu[NUM_CH]        = {};

static void vu_draw_cb(lv_event_t* e);

static lv_obj_t* s_slider_panel      = NULL;
static lv_obj_t* s_slider            = NULL;
static bool      s_slider_visible    = false;

static bool s_page3_ready = false;


extern void sendMIDIBytes(const byte* data, size_t len);

static lv_obj_t* s_arc_lbl[NUM_CH] = {};

static uint32_t blendHex(uint32_t a, uint32_t b, uint8_t alpha);



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

        // VU — objeto único con draw callback (16 obj vs 192 segmentos)
        s_vu[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_vu[i], x + 4, VU_TOP);
        lv_obj_set_size(s_vu[i], CH_W - 8, VU_H);
        lv_obj_set_style_bg_opa(s_vu[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_vu[i], 0, 0);
        lv_obj_clear_flag(s_vu[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_vu[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_vu[i], vu_draw_cb, LV_EVENT_DRAW_MAIN, (void*)(intptr_t)i);
        lv_obj_add_event_cb(s_vu[i], [](lv_event_t* e) {
            int col = (int)(intptr_t)lv_event_get_user_data(e);
            int midiCh = (col >= P4_CH_OFFSET) ? col - P4_CH_OFFSET : col;
            byte msg[3] = { 0x90, (uint8_t)(0x18 + midiCh), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

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
        lv_obj_set_style_arc_color(s_arc[i], lv_color_hex(0x444444), LV_PART_MAIN);
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
        lv_obj_set_pos(s_select[i], x + 4, SEL_TOP);
        lv_obj_set_size(s_select[i], CH_W - 8, SEL_H);
        lv_obj_set_style_bg_color(s_select[i], lv_color_hex(COL_SOLO_OFF), 0);
        lv_obj_set_style_border_width(s_select[i], 1, 0);
        lv_obj_set_style_border_color(s_select[i], lv_color_black(), 0);
        lv_obj_set_style_radius(s_select[i], 6, 0);
        lv_obj_clear_flag(s_select[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_select[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* sel_lbl = lv_label_create(s_select[i]);
        lv_label_set_text(sel_lbl, "S");
        lv_obj_set_style_text_color(sel_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(sel_lbl);
        lv_obj_add_event_cb(s_select[i], [](lv_event_t* e) {
            int col = (int)(intptr_t)lv_event_get_user_data(e);
            int midiCh = (col >= P4_CH_OFFSET) ? col - P4_CH_OFFSET : col;
            byte msg[3] = { 0x90, (uint8_t)(0x08 + midiCh), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // MUTE (M)
        s_mute[i] = lv_obj_create(s_page_root);
        lv_obj_set_pos(s_mute[i], x + 4, MUTE_TOP);
        lv_obj_set_size(s_mute[i], CH_W - 8, MUTE_H);
        lv_obj_set_style_bg_color(s_mute[i], lv_color_hex(COL_MUTE_OFF), 0);
        lv_obj_set_style_border_width(s_mute[i], 1, 0);
        lv_obj_set_style_border_color(s_mute[i], lv_color_black(), 0);
        lv_obj_set_style_radius(s_mute[i], 6, 0);
        lv_obj_clear_flag(s_mute[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_mute[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t* mute_lbl = lv_label_create(s_mute[i]);
        lv_label_set_text(mute_lbl, "M");
        lv_obj_set_style_text_color(mute_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(mute_lbl);
        lv_obj_add_event_cb(s_mute[i], [](lv_event_t* e) {
            int col = (int)(intptr_t)lv_event_get_user_data(e);
            int midiCh = (col >= P4_CH_OFFSET) ? col - P4_CH_OFFSET : col;
            byte msg[3] = { 0x90, (uint8_t)(0x10 + midiCh), 127 };
            sendMIDIBytes(msg, 3);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    needsButtonsRedraw = true;
    
    s_page3_ready = true;

    needsTimecodeRedraw = true;
    needsButtonsRedraw  = true;

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
            int pan = (pos == 6) ? 2 : ((pos - 6) * 100) / 6;
            lv_arc_set_value(s_arc[i], pan);
            // tabla pos 0-11 → valor Logic (-64..+63)
            static const int8_t PAN_VAL[12] = {
                0, -64, -48, -36, -24, -12, 0, 10, 20, 30, 40, 63
            };
            char pan_txt[5];
            if (pos == 0 || pos == 6) snprintf(pan_txt, sizeof(pan_txt), "C");
            else if (pos < 6)         snprintf(pan_txt, sizeof(pan_txt), "%d",  PAN_VAL[pos]);
            else                      snprintf(pan_txt, sizeof(pan_txt), "+%d", PAN_VAL[pos]);
            lv_label_set_text(s_arc_lbl[i], pan_txt);
        }
        needsButtonsRedraw = false;
    }

    if (needsVUMetersRedraw) {
        for (int i = 0; i < NUM_CH; i++) {
            lv_obj_invalidate(s_vu[i]);
        }
        needsVUMetersRedraw = false;
    }
}

// ****************************************************************************
// Blend helper: mezcla dos colores 0xRRGGBB según alpha 0-255
// ****************************************************************************
static uint32_t blendHex(uint32_t a, uint32_t b, uint8_t alpha) {
    uint8_t ra = (a >> 16) & 0xFF, ga = (a >> 8) & 0xFF, ba = a & 0xFF;
    uint8_t rb = (b >> 16) & 0xFF, gb = (b >> 8) & 0xFF, bb = b & 0xFF;
    return ((uint32_t)((ra * alpha + rb * (255 - alpha)) >> 8) << 16) |
           ((uint32_t)((ga * alpha + gb * (255 - alpha)) >> 8) <<  8) |
            (uint32_t)((ba * alpha + bb * (255 - alpha)) >> 8);
}

// ****************************************************************************
// Draw callback VU — dibuja 12 segmentos directamente en el render buffer
// Sustituye 192 objetos LVGL individuales → 16 objetos con draw callback
// ****************************************************************************
static void vu_draw_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_obj_t*   obj   = (lv_obj_t*)lv_event_get_target(e);

    const int seg_w = CH_W - 8;
    const int seg_h = (VU_H - 22) / 12;
    const int gap   = 2;

    int active = (int)round(vuLevels[i] * 12.0f);
    int peak   = (int)round(vuPeakLevels[i] * 12.0f);
    if (peak > 0) peak--;
    bool showPeak = (vuPeakLevels[i] > vuLevels[i] + 0.001f) && vuPeakAlpha[i] > 0;
    if (!showPeak) peak = -1;

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius       = 1;
    dsc.border_width = 0;
    dsc.bg_opa       = LV_OPA_COVER;

    for (int s = 0; s < 12; s++) {
        uint32_t onCol  = (s < 8) ? 0x00E600 : (s < 10) ? 0xFFFF00 : 0xFF0000;
        uint32_t offCol = (s < 8) ? 0x003300 : (s < 10) ? 0x333300 : 0x330000;
        uint32_t hex;

        if (s == 11 && vuClipState[i]) {
            hex = 0xFF0000;
        } else if (s < active) {
            hex = onCol;
        } else if (s == peak) {
            hex = (vuPeakAlpha[i] == 255) ? onCol : blendHex(onCol, offCol, vuPeakAlpha[i]);
        } else {
            hex = offCol;
        }

        dsc.bg_color = lv_color_hex(hex);
        lv_area_t seg_area = {
            obj_coords.x1,
            obj_coords.y1 + (int32_t)((11 - s) * (seg_h + gap)),
            obj_coords.x1 + (int32_t)(seg_w - 1),
            obj_coords.y1 + (int32_t)((11 - s) * (seg_h + gap) + seg_h - 1)
        };
        lv_draw_rect(layer, &dsc, &seg_area);
    }
}

// ****************************************************************************
// Lógica de decaimiento — portada de S2 (hold 1s + fade 100ms)
// ****************************************************************************
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 300;
    const unsigned long PEAK_HOLD_TIME_MS = 1000;
    const unsigned long PEAK_FADE_STEP_MS = 25;
    const float         DECAY_AMOUNT      = 1.0f / 12.0f;
    const uint8_t       FADE_STEP         = 64;

    unsigned long now = millis();
    bool changed = false;

    for (int i = 0; i < NUM_CH; i++) {
        // 1. Decaimiento nivel
        if (vuLevels[i] > 0 && now - vuLastUpdateTime[i] > DECAY_INTERVAL_MS) {
            vuLevels[i] -= DECAY_AMOUNT;
            if (vuLevels[i] < 0.01f) vuLevels[i] = 0.0f;
            vuLastUpdateTime[i] = now;
            changed = true;
        }

        // 2. Peak — hold 1s + fade 4 pasos × 25ms
        if (vuPeakLevels[i] > 0) {
            bool detached = vuPeakLevels[i] > vuLevels[i] + 0.001f;
            if (!detached) {
                vuPeakAlpha[i]    = 255;
                vuPeakFadeTime[i] = 0;
            } else if (now - vuPeakLastUpdateTime[i] > PEAK_HOLD_TIME_MS) {
                if (vuPeakFadeTime[i] == 0) {
                    vuPeakFadeTime[i] = now;
                } else if (now - vuPeakFadeTime[i] >= PEAK_FADE_STEP_MS) {
                    vuPeakFadeTime[i] = now;
                    if (vuPeakAlpha[i] > FADE_STEP) {
                        vuPeakAlpha[i] -= FADE_STEP;
                    } else {
                        vuPeakAlpha[i]  = 0;
                        vuPeakLevels[i] = 0.0f;
                    }
                    changed = true;
                }
            }
        } else {
            vuPeakAlpha[i]    = 255;
            vuPeakFadeTime[i] = 0;
        }

        // 3. Peak nunca < nivel actual
        if (vuPeakLevels[i] < vuLevels[i]) {
            vuPeakLevels[i]         = vuLevels[i];
            vuPeakLastUpdateTime[i] = now;
            vuPeakAlpha[i]          = 255;
            vuPeakFadeTime[i]       = 0;
            changed = true;
        }
    }

    if (changed) needsVUMetersRedraw = true;
}

void uiPage3Destroy() {
    if (s_page_root) {
        lv_obj_del(s_page_root);
        s_page_root   = NULL;
        s_page3_ready = false;
    }
}