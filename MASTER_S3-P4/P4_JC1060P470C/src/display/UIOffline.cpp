// src/display/UIOffline.cpp
#include "UIOffline.h"
#include "../config.h"
#include "Display.h"
#include "lvgl.h"
#include <LittleFS.h>

// P4_W, P4_H → config.h
#define LOGO_W  300
#define LOGO_H   57
#define LOGO_X  ((P4_W - LOGO_W) / 2)
#define LOGO_Y  ((P4_H - LOGO_H) / 2)

static lv_obj_t*      s_root        = NULL;   // ← era s_screen
static lv_obj_t*      s_logo        = NULL;
static lv_obj_t*      s_blink_label = NULL;
static uint8_t*       s_logo_buf    = NULL;
static bool           s_logo_ready  = false;
static int            s_logo_reveal = 0;
static uint8_t        s_blink_cnt   = 0;
static uint32_t       s_lastTick    = 0;
static uint32_t       s_lastLetter  = 0;
static bool           s_offline_active = false;
static bool           s_screenOff      = false;
static lv_image_dsc_t s_img_dsc;

void uiOfflineCreate(lv_obj_t* parent) {
    s_root = lv_obj_create(parent);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, P4_W, P4_H);  // pantalla completa landscape 1024×600 (2026-06-09)
    lv_obj_set_style_bg_color(s_root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);   // sin marco del tema (2026-05-30 11:50)
    lv_obj_set_style_radius(s_root, 0, 0);         // sin esquinas redondeadas del tema
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    if (LittleFS.exists("/logo.jpg")) {
        File f = LittleFS.open("/logo.jpg", "r");
        if (f) {
            size_t sz = f.size();
            s_logo_buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
            if (s_logo_buf) {
                f.read(s_logo_buf, sz);
                f.close();

                memset(&s_img_dsc, 0, sizeof(s_img_dsc));
                s_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
                s_img_dsc.header.cf    = LV_COLOR_FORMAT_RAW;
                s_img_dsc.header.w     = LOGO_W;
                s_img_dsc.header.h     = LOGO_H;
                s_img_dsc.data_size    = sz;
                s_img_dsc.data         = s_logo_buf;

                s_logo = lv_image_create(s_root);
                // landscape nativo: sin rotación de 90° (2026-06-09)
                lv_image_set_src(s_logo, &s_img_dsc);
                lv_obj_set_pos(s_logo, LOGO_X, LOGO_Y);
                lv_obj_set_size(s_logo, 0, LOGO_H);
                s_logo_ready = true;
                log_i("[Offline] logo OK");
            } else {
                f.close();
                log_e("[Offline] malloc falló");
            }
        }
    } else {
        log_w("[Offline] logo.jpg no encontrado");
    }

    s_blink_label = lv_label_create(s_root);
    lv_label_set_text(s_blink_label, "Esperando Logic Pro...");
    lv_obj_set_style_text_color(s_blink_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_blink_label, &lv_font_montserrat_16, 0);
    // centrado horizontal bajo el logo, sin rotación (landscape nativo, 2026-06-09)
    lv_obj_set_pos(s_blink_label, LOGO_X, LOGO_Y + LOGO_H + 20);
    // label visible desde el arranque
    s_logo_reveal    = 0;
    s_blink_cnt      = 0;
    s_lastTick       = 0;
    s_lastLetter     = 0;
    s_offline_active = true;
    s_screenOff      = false;

    // lv_scr_load eliminado — Display.cpp ya cargó la pantalla raíz
    log_i("[Offline] uiOfflineCreate OK");
}

void uiOfflineTick() {
    if (!s_offline_active) return;
    uint32_t now = millis();

    uint32_t inactiveMs = lv_display_get_inactive_time(getDisplay());
    if (!s_screenOff && inactiveMs >= SPLASH_SCREEN_OFF_MS) {
        s_screenOff = true;
        displaySetBrightness(0);
        log_i("[Offline] pantalla apagada por inactividad (%lu ms)", (unsigned long)inactiveMs);
    } else if (s_screenOff && inactiveMs < SPLASH_SCREEN_OFF_MS) {
        s_screenOff = false;
        displaySetBrightness(SPLASH_BRIGHTNESS_PERCENT);
        log_i("[Offline] pantalla reactivada por touch");
    }

    if (now - s_lastTick < 33) return;
    s_lastTick = now;

    if (s_logo_ready && s_logo_reveal < LOGO_W) {
        if (now - s_lastLetter >= 100) {
            s_lastLetter  = now;
            s_logo_reveal = min(s_logo_reveal + 60, LOGO_W);
            lv_obj_set_width(s_logo, s_logo_reveal);
            lv_obj_invalidate(s_logo);
        }
    }

    // parpadeo del label siempre activo, independiente del logo
    s_blink_cnt++;
    if ((s_blink_cnt & 0x1F) == 0) {
        bool show = !(s_blink_cnt & 0x20);
        if (show) lv_obj_remove_flag(s_blink_label, LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(s_blink_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void uiOfflineDestroy() {
    s_offline_active = false;
    if (s_root)     { lv_obj_del(s_root);             s_root     = NULL; }
    if (s_logo_buf) { heap_caps_free(s_logo_buf); s_logo_buf = NULL; }
    s_logo_ready  = false;
    s_logo_reveal = 0;
    log_i("[Offline] destruido");
}