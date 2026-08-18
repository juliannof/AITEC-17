// src/display/UIMenu.cpp
#include "UIMenu.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"
#include <Arduino.h>
#include <Preferences.h>

// ── Estado ───────────────────────────────────────────────
static lv_obj_t*   s_ham_btn    = NULL;
static lv_obj_t*   s_ham_lbl    = NULL;
static lv_obj_t*   s_panel      = NULL;
static lv_obj_t*   s_slider     = NULL;
static lv_obj_t*   s_slider_lbl = NULL;
static bool        s_menu_open  = false;
static uint8_t     s_brightness = 80;
static lv_timer_t* s_save_timer = NULL;

static Preferences prefs;

extern void displaySetBrightness(uint8_t brightness);
extern volatile uint8_t g_currentPage;

static void ham_cb(lv_event_t* e) {
    if (s_menu_open) uiMenuClose();
    else             uiMenuOpen();
}

static void btn_cb(lv_event_t* e) {
    const char* txt = (const char*)lv_event_get_user_data(e);
    if (strcmp(txt, "Reiniciar") == 0) {
        displaySetBrightness(0);
        delay(50);
        ESP.restart();
    }
}

static void slider_cb(lv_event_t* e) {
    lv_obj_t* s = (lv_obj_t*)lv_event_get_target(e);
    s_brightness = (uint8_t)lv_slider_get_value(s);
    displaySetBrightness(s_brightness);
    lv_label_set_text_fmt(s_slider_lbl, "%d%%", s_brightness);

    if (s_save_timer) lv_timer_del(s_save_timer);
    s_save_timer = lv_timer_create([](lv_timer_t* t) {
        prefs.begin("uimenu", false);
        prefs.putUChar("brightness", s_brightness);
        prefs.end();
        lv_timer_del(t);
        s_save_timer = NULL;
    }, 500, NULL);
}

// ── Helper: crea botón simple sin lv_list ────────────────
static lv_obj_t* make_btn(lv_obj_t* parent,
                           int32_t x, int32_t y,
                           int32_t w, int32_t h,
                           const char* label,
                           uint32_t bg_col,
                           uint32_t txt_col) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_col), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(txt_col), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void*)label);
    return btn;
}

void uiMenuInit(lv_obj_t* parent) {
    prefs.begin("uimenu", false);
    s_brightness = prefs.getUChar("brightness", 80);
    prefs.end();
    displaySetBrightness(s_brightness);

    // ── Hamburguesa — en la franja del header, arriba derecha (landscape) ──
    s_ham_btn = lv_obj_create(parent);
    lv_obj_set_pos(s_ham_btn,
                   P4_W - MENU_HAM_SIZE - 12,
                   (HEADER_H - MENU_HAM_SIZE) / 2);
    lv_obj_set_size(s_ham_btn, MENU_HAM_SIZE, MENU_HAM_SIZE);
    lv_obj_set_style_bg_color(s_ham_btn, lv_color_hex(0x001080), 0);
    lv_obj_set_style_bg_opa(s_ham_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ham_btn, 0, 0);
    lv_obj_set_style_radius(s_ham_btn, 8, 0);
    lv_obj_set_style_pad_all(s_ham_btn, 0, 0);
    lv_obj_clear_flag(s_ham_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ham_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ham_btn, ham_cb, LV_EVENT_CLICKED, NULL);

    s_ham_lbl = lv_label_create(s_ham_btn);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(s_ham_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_ham_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(s_ham_lbl);

    // ── Panel — cubre el área de contenido bajo el header (landscape) ──
    s_panel = lv_obj_create(parent);
    lv_obj_set_pos(s_panel, 0, CONTENT_Y);
    lv_obj_set_size(s_panel, P4_W, CONTENT_H);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_opa(s_panel, 241, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, 0, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_panel, 20, 0);
    lv_obj_set_style_shadow_color(s_panel, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(s_panel, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_y(s_panel, 8, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    // ── Botón Reiniciar — arriba a la izquierda (2026-08-18: título "General" y
    // separador retirados junto con los botones de vista Botones/VUMetros/Faders
    // — el panel ya solo tiene Reiniciar + brillo, un título genérico no aporta
    // nada; la navegación de páginas sigue viva en el header "Bo"/"Vu") ──
    make_btn(s_panel, 40, 30, 220, 100, "Reiniciar", 0x3A1010, 0xFF4444);

    // ── Slider brillo — vertical (2026-08-18: girado 90° a la izquierda; en LVGL
    // un lv_slider es vertical automáticamente cuando alto > ancho — el extremo
    // derecho del slider horizontal original (máximo) queda arriba) ──
    int32_t slider_h = CONTENT_H - 120;
    int32_t slider_x = P4_W - 160;
    int32_t slider_y = 30;

    s_slider_lbl = lv_label_create(s_panel);
    lv_label_set_text_fmt(s_slider_lbl, "%d%%", s_brightness);
    lv_obj_set_style_text_color(s_slider_lbl, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_slider_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_slider_lbl, slider_x - 4, slider_y - 24);

    s_slider = lv_slider_create(s_panel);
    lv_obj_set_pos(s_slider, slider_x, slider_y);
    lv_obj_set_size(s_slider, 40, slider_h);
    lv_slider_set_range(s_slider, 2, 100);
    lv_slider_set_value(s_slider, s_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(COL_FADER_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(COL_FADER_THUMB), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(COL_FADER_THUMB), LV_PART_KNOB);
    lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

void uiMenuOpen() {
    if (!s_panel) return;
    s_menu_open = true;
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
    lv_obj_move_foreground(s_ham_btn);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_CLOSE);
}

void uiMenuClose() {
    if (!s_panel) return;
    s_menu_open = false;
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_ham_lbl, LV_SYMBOL_LIST);
}

void uiMenuDestroy() {
    s_menu_open = false;
    if (s_save_timer) {
        lv_timer_del(s_save_timer);
        s_save_timer = NULL;
    }
    if (s_panel) {
        lv_obj_del(s_panel);
        s_panel = NULL;
    }
    if (s_ham_btn) {
        lv_obj_del(s_ham_btn);
        s_ham_btn = NULL;
    }
    s_ham_lbl    = NULL;
    s_slider     = NULL;
    s_slider_lbl = NULL;
}