// display/UILights.cpp — Pantalla LIGHTS, RELLENO (AITEC 2026-10-12 13:22)
// Tocar la pantalla vuelve al modo ExPressif, para tener salida táctil.
#include "UILights.h"
#include "UIMode.h"
#include "UIWifiCfg.h"
#include "../config.h"
#include "lvgl.h"

static lv_obj_t* s_cont = NULL;
static bool      s_open = false;

static void lights_click_cb(lv_event_t* e) {
    (void)e;
    uiModeNext();   // Lights → ExPressif
}

static void wifi_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uiWifiCfgShow();   // configurar SSID y clave (2026-10-12 13:50)
}

void uiLightsCreate(lv_obj_t* parent) {
    s_cont = lv_obj_create(parent);
    lv_obj_set_size(s_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_cont, 0, 0);
    lv_obj_set_style_bg_color(s_cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_cont, 0, 0);
    lv_obj_set_style_radius(s_cont, 0, 0);
    lv_obj_set_style_pad_all(s_cont, 0, 0);
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_cont, lights_click_cb, LV_EVENT_CLICKED, NULL);

    // Texto rotado 90° como UIBrightnessPopup (la pantalla es 480×800 nativa)
    lv_obj_t* lbl = lv_label_create(s_cont);
    lv_label_set_text(lbl, "LIGHTS\nproximamente");
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_UIMODE_LIGHTS), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);

    // Botón WIFI (2026-10-12 13:50): abre la pantalla de SSID/clave. Coordenadas físicas
    // (landscape 800×480): esquina (300,330) 200×60 → LVGL pos(x=480-330-60=90, y=300), size(60,200).
    lv_obj_t* btn = lv_btn_create(s_cont);
    lv_obj_set_pos(btn, 90, 300);
    lv_obj_set_size(btn, 60, 200);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(COL_UIMODE_LIGHTS), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, wifi_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "CONFIGURAR WIFI");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(COL_UIMODE_LIGHTS), 0);
    lv_obj_center(bl);
    lv_obj_set_style_transform_rotation(bl, 900, 0);
    lv_obj_set_style_transform_pivot_x(bl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(bl, LV_PCT(50), 0);
}

void uiLightsShow() {
    if (!s_cont) return;
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    s_open = true;
    g_lightsOpen = true;
}

void uiLightsHide() {
    if (!s_cont) return;
    uiWifiCfgHide();   // la pantalla de WiFi pertenece al modo Lights
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    s_open = false;
    g_lightsOpen = false;
}

bool uiLightsIsOpen() { return s_open; }
