// display/UIBoot.cpp — ExPressif splash screen  (AITEC 2026-06-29)
// LVGL portrait 480×800, hardware rota a landscape 800×480
// Labels/imágenes con transform_rotation=900 para aparecer legibles en landscape
// Líneas decorativas: verticales en LVGL (x constante) → horizontales en pantalla

#include "UIBoot.h"
#include "../config.h"
#include "lvgl.h"
#include <LittleFS.h>

#define LOGO_W  300
#define LOGO_H   57

static lv_obj_t*      s_boot_root = NULL;
static uint32_t       s_boot_time = 0;
static uint8_t*       s_logo_buf  = NULL;
static lv_image_dsc_t s_img_dsc;

static void rot_label(lv_obj_t* lbl) {
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
}

void uiBootCreate(lv_obj_t* parent) {
    s_boot_root = lv_obj_create(parent);
    lv_obj_set_pos(s_boot_root, 0, 0);
    lv_obj_set_size(s_boot_root, P4_W, P4_H);   // 480×800 portrait LVGL
    lv_obj_set_style_bg_color(s_boot_root, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_boot_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_boot_root, 0, 0);
    lv_obj_set_style_pad_all(s_boot_root, 0, 0);
    lv_obj_clear_flag(s_boot_root, LV_OBJ_FLAG_SCROLLABLE);

    // ── Logo AITEC desde LittleFS ─────────────────────────────────────
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

                // Logo centrado en pantalla landscape con rotación 90°
                // LVGL 300×57 + transform_rotation=900 → pantalla 300 wide × 57 tall
                lv_obj_t* logo = lv_image_create(s_boot_root);
                lv_image_set_src(logo, &s_img_dsc);
                lv_obj_set_size(logo, LOGO_W, LOGO_H);
                lv_obj_align(logo, LV_ALIGN_CENTER, 30, 0);  // LVGL_x=270 → screen_y=209
                lv_obj_set_style_transform_rotation(logo, 900, 0);
                lv_obj_set_style_transform_pivot_x(logo, LV_PCT(50), 0);
                lv_obj_set_style_transform_pivot_y(logo, LV_PCT(50), 0);
                log_i("[Boot] logo OK");
            } else {
                f.close();
                log_e("[Boot] malloc logo falló");
            }
        }
    } else {
        log_w("[Boot] logo.jpg no encontrado en LittleFS");
    }

    // ── Línea decorativa superior ──────────────────────────────────────
    // vertical en LVGL (x=329) → horizontal en pantalla a screen_y≈150
    static lv_point_precise_t line_pts[2]  = {{329, 50}, {329, 750}};
    lv_obj_t* line_top = lv_line_create(s_boot_root);
    lv_line_set_points(line_top, line_pts, 2);
    lv_obj_set_style_line_color(line_top, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_line_width(line_top, 1, 0);
    lv_obj_set_style_line_opa(line_top, LV_OPA_40, 0);

    // ── "ExPressif" — screen_y≈239 (centro) ───────────────────────────
    // LV_ALIGN_CENTER ox=0: LVGL_x=240 → screen_y=479-240=239
    lv_obj_t* lbl_name = lv_label_create(s_boot_root);
    lv_label_set_text(lbl_name, "ExPressif");
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_44, 0);
    lv_obj_align(lbl_name, LV_ALIGN_CENTER, 0, 0);
    rot_label(lbl_name);

    // ── Línea decorativa inferior ──────────────────────────────────────
    // LVGL_x=184 → screen_y = 479-184 = 295
    static lv_point_precise_t line_pts2[2] = {{184, 50}, {184, 750}};
    lv_obj_t* line_bot = lv_line_create(s_boot_root);
    lv_line_set_points(line_bot, line_pts2, 2);
    lv_obj_set_style_line_color(line_bot, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_line_width(line_bot, 1, 0);
    lv_obj_set_style_line_opa(line_bot, LV_OPA_40, 0);

    s_boot_time = millis();
}

void uiBootTick() {
    if (s_boot_root && (millis() - s_boot_time >= BOOT_SCREEN_MS)) {
        extern volatile bool g_bootDone;
        g_bootDone = true;
    }
}

void uiBootDestroy() {
    if (s_boot_root) {
        lv_obj_delete(s_boot_root);
        s_boot_root = NULL;
    }
    if (s_logo_buf) {
        heap_caps_free(s_logo_buf);
        s_logo_buf = NULL;
    }
}
