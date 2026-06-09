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
extern volatile bool g_switchToPage1;
extern volatile bool g_switchToPage3A;
extern volatile bool g_switchToPage3B;
extern volatile uint8_t g_currentPage;

static void set_rot(lv_obj_t* obj) {
    lv_obj_set_style_transform_rotation(obj, 900, 0);
    lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
}

static void ham_cb(lv_event_t* e) {
    if (s_menu_open) uiMenuClose();
    else             uiMenuOpen();
}

static void btn_cb(lv_event_t* e) {
    const char* txt = (const char*)lv_event_get_user_data(e);
    if (strcmp(txt, "Botones") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 1);
        prefs.end();
        g_currentPage   = 1;
        g_switchToPage1 = true;
        uiMenuClose();
    } else if (strcmp(txt, "VUMetros") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 0);
        prefs.end();
        g_currentPage    = 0;
        g_switchToPage3A = true;
        uiMenuClose();
    } else if (strcmp(txt, "Faders") == 0) {
        prefs.begin("uimenu", false);
        prefs.putUChar("lastPage", 2);
        prefs.end();
        g_currentPage    = 2;
        g_switchToPage3B = true;
        uiMenuClose();
    } else if (strcmp(txt, "Reiniciar") == 0) {
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
    set_rot(lbl);

    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void*)label);
    return btn;
}

void uiMenuInit(lv_obj_t* parent) {
    prefs.begin("uimenu", false);
    s_brightness = prefs.getUChar("brightness", 80);
    prefs.end();
    displaySetBrightness(s_brightness);

    // ── Hamburguesa ───────────────────────────────────────
    s_ham_btn = lv_obj_create(parent);
    lv_obj_set_pos(s_ham_btn,
                   HEADER_X + (HEADER_W - MENU_HAM_SIZE) / 2,
                   P4_H - MENU_HAM_SIZE - 10);
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
    set_rot(s_ham_lbl);

    // ── Panel ─────────────────────────────────────────────
    s_panel = lv_obj_create(parent);
    lv_obj_set_pos(s_panel, 0, P4_H - MENU_PANEL_H);
    lv_obj_set_size(s_panel, HEADER_X, MENU_PANEL_H);
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_panel, 241, 0);
    lv_obj_set_style_border_width(s_panel, 0, 0);
    lv_obj_set_style_radius(s_panel, 0, 0);
    lv_obj_set_style_pad_all(s_panel, 0, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_shadow_width(s_panel, 20, 0);
    lv_obj_set_style_shadow_color(s_panel, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(s_panel, LV_OPA_70, 0);
    lv_obj_set_style_shadow_offset_y(s_panel, -8, 0);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);

    // ── Título ────────────────────────────────────────────
    lv_obj_t* title = lv_label_create(s_panel);
    lv_label_set_text(title, "General");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title, HEADER_X / 2 - 30, 14);
    set_rot(title);

    // Separador
    lv_obj_t* sep = lv_obj_create(s_panel);
    lv_obj_set_pos(sep, 0, 42);
    lv_obj_set_size(sep, HEADER_X, 1);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);

    // ── Botones de navegación ─────────────────────────────
    // En portrait: 4 botones en columna x=0..HEADER_X, y=50..250
    // En landscape se ven como 4 botones en fila horizontal
    int32_t bw = (HEADER_X - 20) / 4;   // ~97px cada uno
    int32_t bh = 180;                    // alto en portrait = ancho en landscape
    int32_t by = 50;

    make_btn(s_panel,  5 + 0 * bw, by, bw - 4, bh, "Botones",  0x252540, 0xFFFFFF);
    make_btn(s_panel,  5 + 1 * bw, by, bw - 4, bh, "VUMetros", 0x252540, 0xFFFFFF);
    make_btn(s_panel,  5 + 2 * bw, by, bw - 4, bh, "Faders",   0x252540, 0xFFFFFF);
    make_btn(s_panel,  5 + 3 * bw, by, bw - 4, bh, "Reiniciar",0x3A1010, 0xFF4444);

    // ── Slider brillo ─────────────────────────────────────
    s_slider_lbl = lv_label_create(s_panel);
    lv_label_set_text_fmt(s_slider_lbl, "%d%%", s_brightness);
    lv_obj_set_style_text_color(s_slider_lbl, lv_color_hex(0xCCCCFF), 0);
    lv_obj_set_style_text_font(s_slider_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(s_slider_lbl, HEADER_X / 2 - 15, MENU_PANEL_H - 40);
    set_rot(s_slider_lbl);

    s_slider = lv_slider_create(s_panel);
    lv_obj_set_pos(s_slider, 40, MENU_PANEL_H - 30);
    lv_obj_set_size(s_slider, HEADER_X - 80, 20);
    lv_slider_set_range(s_slider, 2, 100);
    lv_slider_set_value(s_slider, s_brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x444466), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x6666AA), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, lv_color_hex(0xCCCCFF), LV_PART_KNOB);
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