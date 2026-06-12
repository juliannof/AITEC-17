// src/display/UIVPotPopup.cpp
// Pop-up grande para mover el V-Pot (pan) al tocar el arco pequeño de UIPage3.
// Solo banco P4 (vpotValues[8..15]). El control es RELATIVO: el arrastre del
// arco grande envía pasos de V-Pot (CC 16+strip) a Logic, igual que el encoder.
#include "UIVPotPopup.h"
#include "../config.h"
#include <Arduino.h>

extern void sendMIDIBytes(const byte* data, size_t len);   // MIDIProcessor.cpp

static lv_obj_t* s_overlay  = NULL;
static lv_obj_t* s_arc      = NULL;
static lv_obj_t* s_value    = NULL;
static int  s_dispCh   = -1;
static int  s_lastSent = 0;
static bool s_open     = false;

// pos V-Pot (0=off, 6=centro, 11=extremo) -> texto L/C/R
static void fmt_pan(int pos, char* buf, size_t n) {
    if (pos < 0)  pos = 0;
    if (pos > 11) pos = 11;
    if (pos == 0)      snprintf(buf, n, "--");
    else if (pos == 6) snprintf(buf, n, "C");
    else if (pos > 6)  snprintf(buf, n, "R%d", pos - 6);
    else               snprintf(buf, n, "L%d", 6 - pos);
}

// arrastre del usuario -> envía delta como pasos relativos de V-Pot a Logic
static void arc_event_cb(lv_event_t* e) {
    if (!s_open || s_arc == NULL) return;
    int v = lv_arc_get_value(s_arc);
    int delta = v - s_lastSent;
    if (delta != 0) {
        int strip = s_dispCh - P4_CH_OFFSET;     // 0..7
        if (strip >= 0 && strip < 8) {
            int mag = abs(delta);
            if (mag > 15) mag = 15;              // limita el salto por evento
            uint8_t val = (delta > 0) ? (uint8_t)mag : (uint8_t)(0x40 | mag);
            byte msg[3] = { (byte)(0xB0 | strip), (byte)(16 + strip), val };
            sendMIDIBytes(msg, 3);
        }
        s_lastSent = v;
    }
    char buf[8];
    fmt_pan(6 + (v * 6) / 100, buf, sizeof(buf));   // feedback inmediato
    if (s_value) lv_label_set_text(s_value, buf);
}

static void close_cb(lv_event_t* e) { uiVPotPopupClose(); }

void uiVPotPopupClose() {
    if (!s_open) return;
    if (s_overlay) { lv_obj_del(s_overlay); s_overlay = NULL; }
    s_arc = s_value = NULL;
    s_dispCh = -1;
    s_open = false;
}

bool uiVPotPopupIsOpen() { return s_open; }

void uiVPotPopupOpen(int dispCh) {
    if (dispCh < P4_CH_OFFSET || dispCh >= 16) return;   // solo banco P4 (8..15)
    if (s_open) uiVPotPopupClose();
    s_dispCh = dispCh;
    s_open   = true;

    // --- fondo modal en la capa superior (tocar fuera = cerrar) ---
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, P4_W, P4_H);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_overlay, close_cb, LV_EVENT_CLICKED, NULL);

    // --- panel central (los clicks aquí NO cierran: no burbujean) ---
    lv_obj_t* panel = lv_obj_create(s_overlay);
    lv_obj_set_size(panel, 460, 540);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_BG_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COL_HEADER_DIM), 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // --- título: nombre del track ---
    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 420);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_HEADER_BRIGHT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_label_set_text(title, trackNames[dispCh].c_str());

    // --- arco grande interactivo (mismo patrón que el pequeño, pero grande) ---
    int pos0 = vpotValues[dispCh] & 0x0F;
    int pan0 = ((pos0 - 6) * 100) / 6;
    s_arc = lv_arc_create(panel);
    lv_obj_set_size(s_arc, 320, 320);
    lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, 10);
    lv_arc_set_range(s_arc, -100, 100);
    lv_arc_set_bg_angles(s_arc, 135, 405);
    lv_arc_set_mode(s_arc, LV_ARC_MODE_SYMMETRICAL);
    lv_arc_set_value(s_arc, pan0);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 16, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_arc, lv_color_hex(0x00FF00), LV_PART_KNOB);
    lv_obj_add_event_cb(s_arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_lastSent = pan0;

    // --- valor del pan, centrado dentro del arco ---
    s_value = lv_label_create(panel);
    lv_obj_align(s_value, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_text_color(s_value, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_value, &lv_font_montserrat_28, 0);
    char buf[8];
    fmt_pan(pos0, buf, sizeof(buf));
    lv_label_set_text(s_value, buf);

    // --- botón cerrar ---
    lv_obj_t* btn = lv_button_create(panel);
    lv_obj_set_size(btn, 160, 56);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_event_cb(btn, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "Cerrar");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, 0);
    lv_obj_center(bl);
}

// sincroniza el arco grande con vpotValues cuando Logic responde (sin pisar el gesto)
void uiVPotPopupUpdate() {
    if (!s_open || s_arc == NULL) return;
    if (lv_obj_has_state(s_arc, LV_STATE_PRESSED)) return;
    int pos = vpotValues[s_dispCh] & 0x0F;
    int pan = ((pos - 6) * 100) / 6;
    if (pan != lv_arc_get_value(s_arc)) {
        lv_arc_set_value(s_arc, pan);   // set programático NO dispara VALUE_CHANGED
        s_lastSent = pan;
        char buf[8];
        fmt_pan(pos, buf, sizeof(buf));
        if (s_value) lv_label_set_text(s_value, buf);
    }
}
