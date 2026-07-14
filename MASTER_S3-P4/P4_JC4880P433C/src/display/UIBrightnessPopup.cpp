// display/UIBrightnessPopup.cpp — Popup transitorio de brillo de pantalla (AITEC 2026-07-14)
#include "UIBrightnessPopup.h"
#include "../config.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t*   s_box     = NULL;
static lv_obj_t*   s_lbl     = NULL;
static lv_timer_t* s_hide_tmr = NULL;

static void hide_cb(lv_timer_t* t) {
    (void)t;
    if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    if (s_hide_tmr) lv_timer_pause(s_hide_tmr);
}

void uiBrightnessPopupCreate(lv_obj_t* parent) {
    s_box = lv_obj_create(parent);
    lv_obj_set_size(s_box, 220, 160);
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

    s_lbl = lv_label_create(s_box);
    lv_obj_set_style_text_color(s_lbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(s_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_lbl);
    lv_obj_set_style_transform_rotation(s_lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(s_lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(s_lbl, LV_PCT(50), 0);

    s_hide_tmr = lv_timer_create(hide_cb, 1200, NULL);
    lv_timer_pause(s_hide_tmr);
}

void uiBrightnessPopupShow(uint8_t percent) {
    if (!s_box || !s_lbl) return;
    static char buf[16];
    snprintf(buf, sizeof(buf), "BRILLO\n%u%%", (unsigned)percent);
    lv_label_set_text(s_lbl, buf);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_box);
    if (s_hide_tmr) {
        lv_timer_resume(s_hide_tmr);
        lv_timer_reset(s_hide_tmr);
    }
}
