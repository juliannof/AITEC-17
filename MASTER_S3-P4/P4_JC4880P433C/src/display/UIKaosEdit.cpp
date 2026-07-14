// display/UIKaosEdit.cpp — Editor de memoria Kaos: parámetros X/Y + canal MIDI (AITEC 2026-07-14)
// LVGL portrait 480×800, hardware rota a landscape 800×480 — mismo convenio
// que UIKaoss.cpp/UIBank.cpp: screen_x = LVGL_y, screen_y = 479 − LVGL_x.
//
// Overlay a pantalla completa (igual patrón que UIBank: crear una vez oculto,
// Show()/Hide() alternan LV_OBJ_FLAG_HIDDEN). Se abre desde el botón PRESET
// de UIKaoss.cpp. Cambios se guardan en NVS (KaosStore) solo al pulsar
// GUARDAR — CANCELAR descarta. aitec_kaos_brief_2026-07-13.md §2, decisión
// "parámetros sueltos" (Opción B) del 2026-07-14.
//
// Canal MIDI es ÚNICO POR SYNTH, no por slot (corrección 2026-07-14 — "el
// canal midi es unico para el sinte, no por preset"): se edita en esta misma
// pantalla porque es donde tiene sentido tocarlo, pero al Guardar afecta a
// los 20 slots del synth activo, no solo al que se está editando. Readout
// colocado justo debajo del título (mismo synth al que pertenece el canal).
//
// Layout físico (800×480 landscape):
//   Top-right: título "SYNTH · P<n>", debajo el canal MIDI del synth, botón cerrar
//   Mitad izquierda: lista "EJE X" (nombre de parámetro, toca uno → asigna X)
//   Mitad derecha:   lista "EJE Y"
//   Franja inferior izquierda: [-] / [+] canal ... CANCELAR ... GUARDAR
//
// Nota: geometría de primera pasada — no se ha podido validar visualmente en
// hardware (regla del proyecto: no compilar). Es muy probable que necesite
// ajuste fino de posiciones tras verlo en pantalla real.
#include "UIKaosEdit.h"
#include "UIKaoss.h"
#include "../config.h"
#include "../kaoss/KaossPad.h"
#include "../kaoss/KaosParams.h"
#include "../nvs/KaosStore.h"
#include "lvgl.h"
#include <stdio.h>

#define KE_MAX_ROWS      8      // tamaño de la lista más grande (WAVE, 8 parámetros)
#define KE_ROW_H         32     // LVGL x-extent (physical height) de cada fila
#define KE_ROW_PITCH     36
#define KE_LIST_TOP_X    320    // LVGL x de la primera fila (baja al aumentar el índice)
#define KE_COL_W         300    // LVGL y-extent (physical width) de cada columna
#define KE_COL_X_HDR     20     // EJE X: columna y=20 (físico: izquierda)
#define KE_COL_Y_HDR     380    // EJE Y: columna y=380 (físico: centro-derecha, antes de Guardar/Cancelar)

static lv_obj_t* s_cont        = NULL;
static lv_obj_t* s_lbl_title   = NULL;
static lv_obj_t* s_rowsX[KE_MAX_ROWS] = {};
static lv_obj_t* s_rowsY[KE_MAX_ROWS] = {};
static lv_obj_t* s_lbl_empty   = NULL;   // "sin parámetros verificados" (TG55/D110)
static lv_obj_t* s_lbl_ch      = NULL;
static lv_obj_t* s_btn_save    = NULL;
static bool      s_open        = false;

static ExSynth  s_synth   = ExSynth::JV2080;   // synth que se está editando (capturado al abrir)
static uint8_t  s_slot    = 0;                  // slot que se está editando
static KaosSlot s_pending{0, 0, 0};             // ccX/ccY en edición, no guardados hasta GUARDAR
static uint8_t  s_pendingCh = 1;                // canal del SYNTH (no del slot) en edición

struct RowCtx { bool isX; uint8_t cc; };
static RowCtx s_rowCtxX[KE_MAX_ROWS];
static RowCtx s_rowCtxY[KE_MAX_ROWS];

static const char* synthLabel(ExSynth s) {
    switch (s) {
        case ExSynth::JV2080: return "JV-2080";
        case ExSynth::TRITON: return "TRITON";
        case ExSynth::TG55:   return "TG55";
        case ExSynth::D110:   return "D-110";
        case ExSynth::WAVE:   return "WAVE";
        case ExSynth::MOTIF:  return "MOTIF";
        default:               return "?";
    }
}

static void rot_label(lv_obj_t* lbl) {
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
}

static void dark_bg(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void update_ch_label() {
    if (!s_lbl_ch) return;
    static char buf[8];
    snprintf(buf, sizeof(buf), "CH%02u", (unsigned)s_pendingCh);
    lv_label_set_text(s_lbl_ch, buf);
}

// ── Filas de parámetros ─────────────────────────────────────────────────
static void row_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    RowCtx* ctx = (RowCtx*)lv_event_get_user_data(e);
    if (ctx->isX) s_pending.ccX = ctx->cc;
    else          s_pending.ccY = ctx->cc;
    // Repinta ambas listas (el nombre elegido puede coincidir en X e Y — se
    // resalta cada una de forma independiente).
    for (int i = 0; i < KE_MAX_ROWS; i++) {
        if (s_rowsX[i]) {
            bool sel = (s_rowCtxX[i].cc == s_pending.ccX);
            lv_obj_set_style_bg_color(s_rowsX[i], lv_color_hex(sel ? COL_ACCENT : COL_BTN_BG), 0);
        }
        if (s_rowsY[i]) {
            bool sel = (s_rowCtxY[i].cc == s_pending.ccY);
            lv_obj_set_style_bg_color(s_rowsY[i], lv_color_hex(sel ? COL_ACCENT : COL_BTN_BG), 0);
        }
    }
    update_ch_label();
}

static void build_column(lv_obj_t* parent, bool isX, int32_t colY) {
    uint8_t count;
    const KaosParam* list = kaosParamList(s_synth, count);
    lv_obj_t** rows   = isX ? s_rowsX   : s_rowsY;
    RowCtx*    ctxArr = isX ? s_rowCtxX : s_rowCtxY;
    uint8_t    curCC  = isX ? s_pending.ccX : s_pending.ccY;

    for (int i = 0; i < KE_MAX_ROWS; i++) {
        if (i >= count) { rows[i] = NULL; continue; }
        ctxArr[i] = {isX, list[i].cc};
        bool sel = (list[i].cc == curCC);

        lv_obj_t* btn = lv_obj_create(parent);
        lv_obj_set_pos(btn, KE_LIST_TOP_X - i * KE_ROW_PITCH, colY);
        lv_obj_set_size(btn, KE_ROW_H, KE_COL_W);
        lv_obj_set_style_bg_color(btn, lv_color_hex(sel ? COL_ACCENT : COL_BTN_BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, row_btn_cb, LV_EVENT_CLICKED, &ctxArr[i]);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, list[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(sel ? 0x000000 : COL_TEXT), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
        rot_label(lbl);

        rows[i] = btn;
    }
}

// ── Canal MIDI (único por synth, no por slot — 2026-07-14) ───────────────
static void ch_dec_cb(lv_event_t* e) {
    (void)e;
    s_pendingCh = (s_pendingCh <= 1) ? 16 : (uint8_t)(s_pendingCh - 1);
    update_ch_label();
}
static void ch_inc_cb(lv_event_t* e) {
    (void)e;
    s_pendingCh = (s_pendingCh >= 16) ? 1 : (uint8_t)(s_pendingCh + 1);
    update_ch_label();
}

// ── Guardar / Cancelar / Cerrar ───────────────────────────────────────────
static void save_cb(lv_event_t* e) {
    (void)e;
    KaosSlot toSave{s_pending.ccX, s_pending.ccY, 1};   // configured=1
    kaosSave(s_synth, s_slot, toSave);
    kaosSaveChannel(s_synth, s_pendingCh);   // afecta a los 20 slots del synth, no solo a este
    kaoss.reload();       // refresca cache RAM si sigue siendo el slot/synth activo
    uiKaossUpdatePreset();
    uiKaosEditHide();
}
static void cancel_cb(lv_event_t* e) { (void)e; uiKaosEditHide(); }
static void close_cb(lv_event_t* e)  { (void)e; uiKaosEditHide(); }

// ── API pública ────────────────────────────────────────────────────────
void uiKaosEditCreate(lv_obj_t* parent) {
    s_cont = lv_obj_create(parent);
    lv_obj_set_size(s_cont, P4_W, P4_H);
    lv_obj_set_pos(s_cont, 0, 0);
    dark_bg(s_cont);
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);

    // ══ Regla de coordenadas (verificada contra el botón cerrar de UIBank,
    // ya probado en hardware) ══
    //   x GRANDE → arriba físico   |   x PEQUEÑO → abajo físico
    //   y PEQUEÑO → izquierda física | y GRANDE → derecha física
    // Fallo detectado en la versión anterior (2026-07-14, feedback en vivo
    // "canal sale a la izquierda"/"todo pegado abajo"): la franja inferior
    // usaba x=0 fijo variando solo 'y' — eso apila TODO en el mismo borde
    // físico (abajo), solo separado en horizontal. Corregido abajo.

    s_lbl_title = lv_label_create(s_cont);
    lv_obj_set_pos(s_lbl_title, 465, 10);   // arriba-izquierda
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_title, &lv_font_montserrat_18, 0);
    rot_label(s_lbl_title);

    // Canal MIDI del synth — fila horizontal [-] CHnn [+] justo debajo del
    // título (2026-07-14, petición del usuario). Ajuste de SYNTH, no de
    // slot — al Guardar afecta a los 20 slots del synth activo.
    lv_obj_t* btn_dec = lv_btn_create(s_cont);
    lv_obj_set_pos(btn_dec, 420, 150);
    lv_obj_set_size(btn_dec, 59, 90);
    lv_obj_set_style_bg_color(btn_dec, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_border_width(btn_dec, 0, 0);
    lv_obj_add_event_cb(btn_dec, ch_dec_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_dec = lv_label_create(btn_dec);
    lv_label_set_text(lbl_dec, "-");
    lv_obj_set_style_text_color(lbl_dec, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(lbl_dec, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_dec);
    rot_label(lbl_dec);

    lv_obj_t* box_ch = lv_obj_create(s_cont);
    lv_obj_set_pos(box_ch, 420, 250);
    lv_obj_set_size(box_ch, 59, 110);
    lv_obj_set_style_bg_color(box_ch, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_bg_opa(box_ch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box_ch, 0, 0);
    lv_obj_set_style_radius(box_ch, 3, 0);
    lv_obj_set_style_pad_all(box_ch, 0, 0);
    lv_obj_clear_flag(box_ch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box_ch, LV_OBJ_FLAG_CLICKABLE);

    s_lbl_ch = lv_label_create(box_ch);
    lv_obj_set_style_text_color(s_lbl_ch, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(s_lbl_ch, &lv_font_montserrat_24, 0);
    lv_obj_center(s_lbl_ch);
    rot_label(s_lbl_ch);

    lv_obj_t* btn_inc = lv_btn_create(s_cont);
    lv_obj_set_pos(btn_inc, 420, 370);
    lv_obj_set_size(btn_inc, 59, 90);
    lv_obj_set_style_bg_color(btn_inc, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_border_width(btn_inc, 0, 0);
    lv_obj_add_event_cb(btn_inc, ch_inc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_inc = lv_label_create(btn_inc);
    lv_label_set_text(lbl_inc, "+");
    lv_obj_set_style_text_color(lbl_inc, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(lbl_inc, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_inc);
    rot_label(lbl_inc);

    lv_obj_t* hdrX = lv_label_create(s_cont);
    lv_label_set_text(hdrX, "EJE X");
    lv_obj_set_pos(hdrX, 340, KE_COL_X_HDR);
    lv_obj_set_style_text_color(hdrX, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(hdrX, &lv_font_montserrat_14, 0);
    rot_label(hdrX);

    lv_obj_t* hdrY = lv_label_create(s_cont);
    lv_label_set_text(hdrY, "EJE Y");
    lv_obj_set_pos(hdrY, 340, KE_COL_Y_HDR);
    lv_obj_set_style_text_color(hdrY, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(hdrY, &lv_font_montserrat_14, 0);
    rot_label(hdrY);

    s_lbl_empty = lv_label_create(s_cont);
    lv_label_set_text(s_lbl_empty, "Sin parametros\nverificados");
    lv_obj_set_pos(s_lbl_empty, KE_LIST_TOP_X - 40, KE_COL_X_HDR);
    lv_obj_set_style_text_color(s_lbl_empty, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_empty, &lv_font_montserrat_14, 0);
    rot_label(s_lbl_empty);
    lv_obj_add_flag(s_lbl_empty, LV_OBJ_FLAG_HIDDEN);

    // ── Borde derecho: Guardar arriba, Cancelar debajo (más aire que antes,
    // 2026-07-14 feedback "guardar y cerrar muy pegados abajo") ──────────
    s_btn_save = lv_btn_create(s_cont);
    lv_obj_set_pos(s_btn_save, 280, 700);
    lv_obj_set_size(s_btn_save, 150, 90);
    lv_obj_set_style_bg_color(s_btn_save, lv_color_hex(0x003300), 0);
    lv_obj_set_style_border_width(s_btn_save, 0, 0);
    lv_obj_add_event_cb(s_btn_save, save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_save = lv_label_create(s_btn_save);
    lv_label_set_text(lbl_save, "GUARDAR");
    lv_obj_set_style_text_color(lbl_save, lv_color_hex(0x66FF66), 0);
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_save);
    rot_label(lbl_save);

    lv_obj_t* btn_cancel = lv_btn_create(s_cont);
    lv_obj_set_pos(btn_cancel, 120, 700);
    lv_obj_set_size(btn_cancel, 150, 90);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x330000), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "CANCELAR");
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0xFF6666), 0);
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_cancel);
    rot_label(lbl_cancel);

    // ── Botón cerrar (top-right, igual que UIBank — posición ya probada en hardware) ──
    lv_obj_t* btn_close = lv_btn_create(s_cont);
    lv_obj_set_size(btn_close, 36, 55);
    lv_obj_set_pos(btn_close, 444, 745);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x220000), 0);
    lv_obj_set_style_border_width(btn_close, 0, 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_set_style_shadow_width(btn_close, 0, 0);
    lv_obj_add_event_cb(btn_close, close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_x);
    rot_label(lbl_x);
}

void uiKaosEditShow() {
    if (!s_cont) return;
    s_synth = g_currentSynth;
    s_slot  = kaoss.getPreset();
    if (!kaosLoad(s_synth, s_slot, s_pending)) s_pending = {0, 0, 0};
    s_pendingCh = kaosLoadChannel(s_synth);   // canal del synth, no del slot (2026-07-14)

    static char titleBuf[24];
    snprintf(titleBuf, sizeof(titleBuf), "%s - P%02u", synthLabel(s_synth), (unsigned)(s_slot + 1));
    lv_label_set_text(s_lbl_title, titleBuf);
    update_ch_label();

    // Reconstruye las dos columnas (limpia filas previas del synth anterior)
    for (int i = 0; i < KE_MAX_ROWS; i++) {
        if (s_rowsX[i]) { lv_obj_delete(s_rowsX[i]); s_rowsX[i] = NULL; }
        if (s_rowsY[i]) { lv_obj_delete(s_rowsY[i]); s_rowsY[i] = NULL; }
    }
    uint8_t count;
    kaosParamList(s_synth, count);
    if (count == 0) {
        lv_obj_clear_flag(s_lbl_empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lbl_empty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_save, LV_OBJ_FLAG_HIDDEN);
        build_column(s_cont, true,  KE_COL_X_HDR);
        build_column(s_cont, false, KE_COL_Y_HDR);
    }

    s_open = true;
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
}

void uiKaosEditHide() {
    if (!s_cont) return;
    s_open = false;
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
}

bool uiKaosEditIsOpen() { return s_open; }
