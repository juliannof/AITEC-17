// display/UIPresetPopup.cpp — Popup transitorio de preset Kaoss (AITEC 2026-10-11 19:32)
#include "UIPresetPopup.h"
#include "../config.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t*   s_box      = NULL;
static lv_obj_t*   s_lbl_num  = NULL;
static lv_obj_t*   s_lbl_x    = NULL;
static lv_obj_t*   s_lbl_y    = NULL;
static lv_timer_t* s_hide_tmr = NULL;

static void hide_cb(lv_timer_t* t) {
    (void)t;
    if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    if (s_hide_tmr) lv_timer_pause(s_hide_tmr);
}

static lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t col) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_width(l, 200);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}

void uiPresetPopupCreate(lv_obj_t* parent) {
    // Caja cuadrada: la pantalla es 480×800 nativa con texto rotado 90°.
    s_box = lv_obj_create(parent);
    lv_obj_set_size(s_box, 220, 220);
    lv_obj_center(s_box);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_box, 2, 0);
    lv_obj_set_style_border_color(s_box, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_radius(s_box, 8, 0);
    lv_obj_set_style_pad_all(s_box, 0, 0);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);

    // Contenedor rotado 90° (mismo giro que UIBrightnessPopup): sus hijos
    // fluyen en columna sin tener que rotarlos uno a uno.
    lv_obj_t* cont = lv_obj_create(s_box);
    lv_obj_set_size(cont, 200, 120);
    lv_obj_center(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_row(cont, 6, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_transform_rotation(cont, 900, 0);
    lv_obj_set_style_transform_pivot_x(cont, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(cont, LV_PCT(50), 0);

    s_lbl_num = make_label(cont, &lv_font_montserrat_24, COL_ACCENT);
    s_lbl_x   = make_label(cont, &lv_font_montserrat_16, COL_TEXT);
    s_lbl_y   = make_label(cont, &lv_font_montserrat_16, COL_TEXT);

    s_hide_tmr = lv_timer_create(hide_cb, 2400, NULL);   // 2.4 s (2026-10-11 19:36, antes 1.2 s)
    lv_timer_pause(s_hide_tmr);
}

void uiPresetPopupShow(uint8_t num, const char* nameX, const char* nameY) {
    if (!s_box || !s_lbl_num || !s_lbl_x || !s_lbl_y) return;
    char buf[20];
    snprintf(buf, sizeof(buf), "PRESET %u", (unsigned)num);
    lv_label_set_text(s_lbl_num, buf);
    lv_label_set_text(s_lbl_x, nameX ? nameX : "-");
    lv_label_set_text(s_lbl_y, nameY ? nameY : "-");
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_box);
    if (s_hide_tmr) {
        lv_timer_resume(s_hide_tmr);
        lv_timer_reset(s_hide_tmr);
    }
}
