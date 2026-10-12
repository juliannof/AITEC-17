// display/UIWifiCfg.cpp — Pantalla para introducir SSID y clave WiFi (AITEC 2026-10-12 13:58)
//
// Convenio de coordenadas (igual que UIKaoss/UIBank/UIKaosEdit): LVGL portrait
// 480×800, hardware rota a landscape 800×480 → screen_x = LVGL_y,
// screen_y = 479 − LVGL_x. Aquí todo se define en coordenadas FÍSICAS
// (landscape, esquina sup-izq + w×h) y put()/place_rot() las convierten.
//
// Por qué NO lv_keyboard (verificado en LVGL 9.5.0 instalada): lv_keyboard es un
// lv_buttonmatrix y su get_button_from_point() compara lv_indev_get_point() (toque
// en bruto) contra las áreas SIN transformar, sin aplicar transform_rotation.
// Rotado, tocaría una tecla distinta de la que se ve. Rotar el display entero
// (lv_display_set_rotation) rompería el layout manual del resto de la UI.
// Solución: teclas lv_button + lv_label con rot_label(), igual que el grid de
// UIBank. El hit-test de objetos sí aplica la transformación
// (lv_indev_search_obj), así que los botones responden bien.
//
// Campos (2026-10-12 13:58): caja lv_obj SIN rotar + lv_label rotado, NO lv_textarea. La
// primera versión usaba lv_textarea y el teclado iba "muy, muy lento": con
// render PARCIAL (bandas de 100 líneas, búfers en PSRAM) LVGL re-transforma
// un objeto rotado una vez por banda, y un lv_textarea rotado de 684×48 con
// cursor parpadeante (animación continua) mantenía el Core1 saturado. El label
// rotado es el patrón ligero que ya usa el resto de la UI.
//
// Layout físico (800×480):
//   y   6: título "WIFI" + estado de conexión (derecha)
//   y  44: campo SSID   y 100: campo CLAVE
//   y 160..340: 4 filas × 10 teclas (72×52)
//   y 408: ?123 | MAYUS | ESPACIO | BORRAR | VER | CANCELAR | GUARDAR
//
// Nota: geometría de primera pasada, sin validar visualmente en hardware (regla
// del proyecto: no compilar). Es probable que necesite ajuste fino.
#include "UIWifiCfg.h"
#include "../config.h"
#include "../nvs/WifiStore.h"
#include "../lights/WledClient.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#define PH_H        480     // alto físico = ancho LVGL
#define KEY_W       72
#define KEY_H       52
#define KEY_GAP     8
#define KEY_COLS    10
#define KEY_ROWS    4
#define KEYS_LEFT   4       // (800 − (10·72 + 9·8)) / 2
#define KEYS_TOP    160
#define ACT_TOP     408
#define FIELD_X     100
#define FIELD_W     684
#define VAL_W       640     // ancho del label de valor (antes de rotar)
#define SHOW_MAX    52      // caracteres visibles; si hay más se muestra la cola
#define SCAN_ROWS   8       // filas visibles del barrido de redes (2026-10-12 13:58)
#define SCAN_TOP    44
#define SCAN_PITCH  46
#define SCAN_ROW_H  40
#define FIELD_H     48
#define FIELD_SSID_Y 44
#define FIELD_PASS_Y 100

static lv_obj_t*   s_cont      = NULL;
static lv_obj_t*   s_box[2]    = {NULL, NULL};   // cajas de campo (SSID, CLAVE), sin rotar
static lv_obj_t*   s_val[2]    = {NULL, NULL};   // etiquetas rotadas con el valor
static char        s_buf[2][WIFI_PASS_MAX + 1];  // texto en edición
static char        s_shown[2][WIFI_PASS_MAX + 8]; // último texto pintado (evita invalidar sin cambios)
static char        s_statTxt[48];
static lv_obj_t*   s_lbl_stat  = NULL;
static lv_obj_t*   s_lbl_hint  = NULL;
static lv_obj_t*   s_key_btn[KEY_ROWS][KEY_COLS];
static lv_obj_t*   s_key_lbl[KEY_ROWS][KEY_COLS];
static lv_obj_t*   s_btn_sym   = NULL;
static lv_obj_t*   s_lbl_sym   = NULL;
static lv_obj_t*   s_btn_shift = NULL;
static lv_obj_t*   s_lbl_shift = NULL;
static bool        s_capsLock  = false;   // BLOQ MAYUS (2026-10-12 15:12): 1 toque = una mayúscula, 2º = bloqueo, 3º = minúsculas
static lv_timer_t* s_stat_tmr  = NULL;
static bool        s_open      = false;

// Panel de barrido de redes: elige la red y solo hay que escribir la clave.
static lv_obj_t*   s_scan_cont = NULL;
static lv_obj_t*   s_scan_btn[SCAN_ROWS];
static lv_obj_t*   s_scan_lbl[SCAN_ROWS];
static lv_obj_t*   s_scan_stat = NULL;
static WledNet     s_nets[SCAN_ROWS];
static int         s_netCount  = 0;
static uint32_t    s_seenSeq   = 0;
static bool        s_scanOpen  = false;
static char        s_scanShown[SCAN_ROWS][64];
static char        s_scanStatTxt[48];
static bool        s_showPass  = false;
static uint8_t     s_page      = 0;     // 0=minúsculas 1=mayúsculas 2=símbolos
static uint8_t     s_active    = 0;     // 0=SSID 1=CLAVE

// 40 teclas por página; filas más cortas ocultan las teclas sobrantes.
static const char* const kPages[3][KEY_ROWS] = {
    { "1234567890", "qwertyuiop", "asdfghjkl",  "zxcvbnm-._" },
    { "1234567890", "QWERTYUIOP", "ASDFGHJKL",  "ZXCVBNM-._" },
    { "!@#$%^&*()", "-_=+[]{};:", "'\"\\|/<>?,.", "`~" },
};

enum ActKey : uint8_t { ACT_SYM, ACT_SHIFT, ACT_SPACE, ACT_BACK, ACT_VIEW, ACT_CANCEL, ACT_SAVE,
                        ACT_NETS, ACT_MANUAL, ACT_RESCAN };

static void scan_open();
static void scan_close();

static void rot_label(lv_obj_t* lbl) {
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
}

// Objeto SIN rotar (botón/contenedor) en coordenadas físicas: esquina (sx,sy), w×h.
static void put(lv_obj_t* o, int sx, int sy, int w, int h) {
    lv_obj_set_pos(o, PH_H - sy - h, sx);
    lv_obj_set_size(o, h, w);
}

// Objeto ROTADO 90° (label, textarea) cuyo tamaño de diseño es landscape w×h:
// se coloca centrado en el rectángulo físico (sx,sy,w,h) y se rota sobre su centro.
static void place_rot(lv_obj_t* o, int sx, int sy, int w, int h) {
    int cx = PH_H - sy - h / 2;     // centro en LVGL x
    int cy = sx + w / 2;            // centro en LVGL y
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, cx - w / 2, cy - h / 2);
    rot_label(o);
}

static void style_key(lv_obj_t* b, uint32_t bg) {
    lv_obj_remove_style_all(b);     // sin tema por defecto: sin transiciones ni outline animados
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COL_BTN_ACTIVE), LV_STATE_PRESSED);
}

static lv_obj_t* make_key(lv_obj_t* parent, int sx, int sy, int w, int h,
                          const char* text, const lv_font_t* font, uint32_t bg,
                          uint32_t fg, lv_obj_t** outLbl) {
    lv_obj_t* b = lv_btn_create(parent);
    // (2026-10-12 15:05) ORDEN IMPORTANTE: style_key() hace lv_obj_remove_style_all(), que en
    // LVGL 9 borra también el estilo LOCAL donde lv_obj_set_pos/size guardan la
    // geometría. put() va DESPUÉS o el botón queda apilado en (0,0) con tamaño
    // por defecto: se ve el texto (label aparte) pero el toque no le llega.
    style_key(b, bg);
    put(b, sx, sy, w, h);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
    lv_obj_center(l);
    rot_label(l);
    if (outLbl) *outLbl = l;
    return b;
}

// ── Lógica ───────────────────────────────────────────────────────────────
static const char* const kPlaceholder[2] = { "nombre de la red", "clave" };
static const uint8_t     kMaxLen[2]      = { WIFI_SSID_MAX, WIFI_PASS_MAX };

// Repinta un campo SOLO si el texto mostrado cambió (cada lv_label_set_text
// invalida el label rotado entero en todas las bandas de render).
static void refresh_field(int f) {
    char tmp[WIFI_PASS_MAX + 8];
    size_t n = strlen(s_buf[f]);
    uint32_t col = COL_TEXT;
    if (n == 0 && s_active != f) {
        snprintf(tmp, sizeof(tmp), "%s", kPlaceholder[f]);
        col = COL_TEXT_DIM;
    } else {
        const char* src = s_buf[f];
        char mask[WIFI_PASS_MAX + 1];
        if (f == 1 && !s_showPass) {                 // clave enmascarada
            memset(mask, '*', n);
            mask[n] = '\0';
            src = mask;
        }
        size_t skip = (n > SHOW_MAX) ? n - SHOW_MAX : 0;   // muestra la cola
        snprintf(tmp, sizeof(tmp), "%s%s%s", skip ? "<" : "", src + skip, (s_active == f) ? "_" : "");
    }
    if (strcmp(tmp, s_shown[f]) == 0) return;
    strlcpy(s_shown[f], tmp, sizeof(s_shown[f]));
    lv_label_set_text(s_val[f], tmp);
    lv_obj_set_style_text_color(s_val[f], lv_color_hex(col), 0);
}

static void set_active(uint8_t which) {
    s_active = which;
    lv_obj_set_style_border_color(s_box[which],     lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_border_color(s_box[1 - which], lv_color_hex(0x223344), 0);
    refresh_field(0);
    refresh_field(1);
}

static void type_char(char ch) {
    size_t n = strlen(s_buf[s_active]);
    if (n >= kMaxLen[s_active]) return;
    s_buf[s_active][n]     = ch;
    s_buf[s_active][n + 1] = '\0';
    refresh_field(s_active);
}

static void delete_char() {
    size_t n = strlen(s_buf[s_active]);
    if (!n) return;
    s_buf[s_active][n - 1] = '\0';
    refresh_field(s_active);
}

static void refresh_keys() {
    for (int r = 0; r < KEY_ROWS; r++) {
        const char* row = kPages[s_page][r];
        int n = (int)strlen(row);
        for (int c = 0; c < KEY_COLS; c++) {
            if (c < n) {
                char t[2] = { row[c], '\0' };
                lv_label_set_text(s_key_lbl[r][c], t);
                lv_obj_clear_flag(s_key_btn[r][c], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_key_btn[r][c], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    lv_label_set_text(s_lbl_sym, s_page == 2 ? "abc" : "?123");
    // MAYUS: apagado / una mayúscula (resaltado) / BLOQ (acento + texto "BLOQ")
    bool locked = (s_page == 1 && s_capsLock);
    lv_obj_set_style_bg_color(s_btn_shift,
        lv_color_hex(locked ? COL_ACCENT : (s_page == 1 ? COL_BTN_ACTIVE : COL_BTN_BG)), 0);
    lv_label_set_text(s_lbl_shift, locked ? "BLOQ" : "MAYUS");
    lv_obj_set_style_text_color(s_lbl_shift, lv_color_hex(locked ? 0x000000 : COL_TEXT), 0);
}

static void set_hint(const char* text, uint32_t col) {
    lv_label_set_text(s_lbl_hint, text);
    lv_obj_set_style_text_color(s_lbl_hint, lv_color_hex(col), 0);
}

static void refresh_status() {
    if (!s_lbl_stat) return;
    char ip[20];
    char buf[48];
    switch (wledWifiState()) {
        case WLED_WIFI_CONNECTED:
            wledWifiIp(ip, sizeof(ip));
            snprintf(buf, sizeof(buf), "CONECTADO  %s", ip);
            lv_obj_set_style_text_color(s_lbl_stat, lv_color_hex(0x00CC66), 0);
            break;
        case WLED_WIFI_CONNECTING:
            snprintf(buf, sizeof(buf), "CONECTANDO...");
            lv_obj_set_style_text_color(s_lbl_stat, lv_color_hex(COL_ACCENT), 0);
            break;
        case WLED_WIFI_FAILED:
            snprintf(buf, sizeof(buf), "ERROR: SSID/CLAVE?");
            lv_obj_set_style_text_color(s_lbl_stat, lv_color_hex(0xFF5555), 0);
            break;
        default:
            snprintf(buf, sizeof(buf), "SIN CREDENCIALES");
            lv_obj_set_style_text_color(s_lbl_stat, lv_color_hex(COL_TEXT_DIM), 0);
            break;
    }
    if (strcmp(buf, s_statTxt) == 0) return;   // no invalidar si no hay cambio
    strlcpy(s_statTxt, buf, sizeof(s_statTxt));
    lv_label_set_text(s_lbl_stat, buf);
}

static void refresh_scan();
static void stat_timer_cb(lv_timer_t* t) {
    (void)t;
    if (!s_open) return;
    refresh_status();
    if (s_scanOpen) refresh_scan();
}

static void field_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    set_active((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

static void char_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    int r = (int)(id / KEY_COLS), c = (int)(id % KEY_COLS);
    const char* row = kPages[s_page][r];
    if (c >= (int)strlen(row)) return;
    type_char(row[c]);
    if (s_page == 1 && !s_capsLock) { s_page = 0; refresh_keys(); }   // mayúscula de un solo uso (BLOQ la mantiene)
    set_hint("", COL_TEXT_DIM);
}

static void act_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    switch ((ActKey)(uintptr_t)lv_event_get_user_data(e)) {
        case ACT_SYM:
            s_page = (s_page == 2) ? 0 : 2;
            s_capsLock = false;
            refresh_keys();
            break;
        case ACT_SHIFT:
            if (s_page != 1)       { s_page = 1; s_capsLock = false; }   // minúsculas/símbolos → una mayúscula
            else if (!s_capsLock)  { s_capsLock = true; }                // una mayúscula → BLOQ MAYUS
            else                   { s_page = 0; s_capsLock = false; }   // BLOQ → minúsculas
            refresh_keys();
            break;
        case ACT_SPACE:
            type_char(' ');
            break;
        case ACT_BACK:
            delete_char();
            break;
        case ACT_VIEW:
            s_showPass = !s_showPass;
            refresh_field(1);
            break;
        case ACT_CANCEL:
            uiWifiCfgHide();
            break;
        case ACT_NETS:
        case ACT_RESCAN:
            scan_open();           // pide barrido y muestra la lista
            break;
        case ACT_MANUAL:
            scan_close();          // escribir el SSID a mano (redes ocultas)
            set_active(0);
            break;
        case ACT_SAVE: {
            const char* ssid = s_buf[0];
            const char* pass = s_buf[1];
            if (!ssid || !ssid[0]) { set_hint("EL SSID NO PUEDE ESTAR VACIO", 0xFF5555); break; }
            if (!wifiStoreSave(ssid, pass)) { set_hint("ERROR AL GUARDAR EN NVS", 0xFF5555); break; }
            wledCredentialsChanged();      // reconecta sin reiniciar
            uiWifiCfgHide();
            break;
        }
    }
}

// ── Barrido de redes (2026-10-12 13:58) ───────────────────────────────────────────────
static void set_scan_status(const char* t, uint32_t col) {
    if (strcmp(t, s_scanStatTxt) == 0) return;          // no invalidar sin cambios
    strlcpy(s_scanStatTxt, t, sizeof(s_scanStatTxt));
    lv_label_set_text(s_scan_stat, t);
    lv_obj_set_style_text_color(s_scan_stat, lv_color_hex(col), 0);
}

static void refresh_scan() {
    uint32_t seq = wledScanSeq();
    if (seq != s_seenSeq) {
        s_seenSeq  = seq;
        s_netCount = wledScanGet(s_nets, SCAN_ROWS);
    }
    for (int i = 0; i < SCAN_ROWS; i++) {
        if (i < s_netCount) {
            char t[64];
            // (2026-10-12 14:49) look & feel estándar de lista LVGL: icono LV_SYMBOL_WIFI + texto
            snprintf(t, sizeof(t), LV_SYMBOL_WIFI "   %s   %d dBm%s", s_nets[i].ssid, (int)s_nets[i].rssi,
                     s_nets[i].secure ? "" : "   (abierta)");
            if (strcmp(t, s_scanShown[i]) != 0) {
                strlcpy(s_scanShown[i], t, sizeof(s_scanShown[i]));
                lv_label_set_text(s_scan_lbl[i], t);
            }
            lv_obj_clear_flag(s_scan_btn[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_scan_lbl[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            s_scanShown[i][0] = '\0';
            lv_obj_add_flag(s_scan_btn[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_scan_lbl[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (wledScanBusy())      set_scan_status("BUSCANDO REDES...", COL_ACCENT);
    else if (s_netCount > 0) set_scan_status("TOCA TU RED", COL_UIMODE_LIGHTS);
    else {                                              // (2026-10-12 14:25) el motivo real, no "0 redes"
        switch (wledScanError()) {
            case -1: set_scan_status("ERROR: EL C6 NO INICIA EL WIFI (MIRA EL LOG)", 0xFF5555); break;
            case -2: set_scan_status("ERROR: EL C6 NO LANZA EL BARRIDO - REINTENTA", 0xFF5555); break;
            case -3: set_scan_status("ERROR: TIMEOUT DEL BARRIDO (C6 SIN RESPUESTA)", 0xFF5555); break;
            default: set_scan_status("0 REDES VISTAS - REESCANEA (EL C6 SOLO VE 2.4 GHZ)", 0xFF5555); break;
        }
    }
}

static void scan_open() {
    wledScanRequest();
    s_seenSeq  = wledScanSeq();
    s_netCount = 0;            // oculta la lista anterior mientras barre
    s_scanOpen = true;
    lv_obj_clear_flag(s_scan_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_scan_cont);
    refresh_scan();
}

static void scan_close() {
    s_scanOpen = false;
    if (s_scan_cont) lv_obj_add_flag(s_scan_cont, LV_OBJ_FLAG_HIDDEN);
}

static void net_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int i = (int)(uintptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= s_netCount) return;
    strlcpy(s_buf[0], s_nets[i].ssid, sizeof(s_buf[0]));
    s_buf[1][0] = '\0';                                 // clave nueva para esta red
    scan_close();
    set_active(1);                                       // directo a la clave
    if (!s_nets[i].secure) set_hint("RED ABIERTA: NO NECESITA CLAVE, PULSA GUARDAR", COL_ACCENT);
    else                   set_hint("", COL_TEXT_DIM);
}

// ── Construcción ─────────────────────────────────────────────────────────
// Campo = caja SIN rotar (barata) + label rotado con el valor (patrón de UIBank).
static void make_field(lv_obj_t* parent, int sy, const char* caption, uint8_t which) {
    lv_obj_t* cap = lv_label_create(parent);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_LEFT, 0);
    place_rot(cap, 16, sy + (FIELD_H - 24) / 2, 80, 24);

    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    put(box, FIELD_X, sy, FIELD_W, FIELD_H);
    lv_obj_set_style_bg_color(box, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x223344), 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(box, field_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)which);
    s_box[which] = box;

    lv_obj_t* val = lv_label_create(parent);
    lv_label_set_text(val, "");
    lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(val, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_clear_flag(val, LV_OBJ_FLAG_CLICKABLE);       // el toque lo recibe la caja
    place_rot(val, FIELD_X + 12, sy + (FIELD_H - 24) / 2, VAL_W, 24);
    s_val[which] = val;
}

void uiWifiCfgCreate(lv_obj_t* parent) {
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

    // Título y estado
    lv_obj_t* title = lv_label_create(s_cont);
    lv_label_set_text(title, "CONFIGURAR WIFI");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_UIMODE_LIGHTS), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);
    place_rot(title, 16, 8, 300, 28);

    s_lbl_stat = lv_label_create(s_cont);
    lv_label_set_text(s_lbl_stat, "");
    lv_obj_set_style_text_font(s_lbl_stat, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_lbl_stat, LV_TEXT_ALIGN_RIGHT, 0);
    place_rot(s_lbl_stat, 360, 10, 424, 24);

    // Campos
    s_shown[0][0] = s_shown[1][0] = '\0';
    s_buf[0][0] = s_buf[1][0] = '\0';
    make_field(s_cont, FIELD_SSID_Y, "SSID",  0);
    make_field(s_cont, FIELD_PASS_Y, "CLAVE", 1);

    // Aviso (errores de validación)
    s_lbl_hint = lv_label_create(s_cont);
    lv_label_set_text(s_lbl_hint, "");
    lv_obj_set_style_text_font(s_lbl_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_lbl_hint, LV_TEXT_ALIGN_LEFT, 0);
    place_rot(s_lbl_hint, 16, 130, 768, 22);

    // Teclas de carácter (40)
    for (int r = 0; r < KEY_ROWS; r++) {
        for (int c = 0; c < KEY_COLS; c++) {
            int sx = KEYS_LEFT + c * (KEY_W + KEY_GAP);
            int sy = KEYS_TOP + r * (KEY_H + KEY_GAP);
            lv_obj_t* b = make_key(s_cont, sx, sy, KEY_W, KEY_H, "", &lv_font_montserrat_24,
                                   COL_BTN_BG, COL_TEXT, &s_key_lbl[r][c]);
            lv_obj_add_event_cb(b, char_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)(r * KEY_COLS + c));
            s_key_btn[r][c] = b;
        }
    }

    // Fila de acciones: ?123 | MAYUS | ESPACIO | BORRAR | VER | REDES | CANCELAR | GUARDAR
    const int AY = ACT_TOP;
    int x = 16;
    s_btn_sym = make_key(s_cont, x, AY, 76, KEY_H, "?123", &lv_font_montserrat_16, COL_BTN_BG, COL_TEXT, &s_lbl_sym);
    lv_obj_add_event_cb(s_btn_sym, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_SYM);
    x += 76 + KEY_GAP;

    s_btn_shift = make_key(s_cont, x, AY, 80, KEY_H, "MAYUS", &lv_font_montserrat_14, COL_BTN_BG, COL_TEXT, &s_lbl_shift);
    lv_obj_add_event_cb(s_btn_shift, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_SHIFT);
    x += 80 + KEY_GAP;

    lv_obj_t* b = make_key(s_cont, x, AY, 130, KEY_H, "ESPACIO", &lv_font_montserrat_14, COL_BTN_BG, COL_TEXT, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_SPACE);
    x += 130 + KEY_GAP;

    b = make_key(s_cont, x, AY, 90, KEY_H, "BORRAR", &lv_font_montserrat_14, COL_BTN_BG, COL_TEXT, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_BACK);
    x += 90 + KEY_GAP;

    b = make_key(s_cont, x, AY, 60, KEY_H, "VER", &lv_font_montserrat_14, COL_BTN_BG, COL_TEXT, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_VIEW);
    x += 60 + KEY_GAP;

    b = make_key(s_cont, x, AY, 80, KEY_H, "REDES", &lv_font_montserrat_14, COL_BTN_ACTIVE, COL_UIMODE_LIGHTS, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_NETS);
    x += 80 + KEY_GAP;

    b = make_key(s_cont, x, AY, 96, KEY_H, "CANCELAR", &lv_font_montserrat_14, 0x330000, 0xFF8888, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_CANCEL);
    x += 96 + KEY_GAP;

    b = make_key(s_cont, x, AY, 96, KEY_H, "GUARDAR", &lv_font_montserrat_14, COL_ACCENT, 0x000000, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_SAVE);

    // ── Panel de barrido de redes (encima del formulario, oculto) ──────────
    s_scan_cont = lv_obj_create(s_cont);
    lv_obj_remove_style_all(s_scan_cont);
    lv_obj_set_size(s_scan_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(s_scan_cont, 0, 0);
    lv_obj_set_style_bg_color(s_scan_cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_scan_cont, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scan_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scan_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* st = lv_label_create(s_scan_cont);
    lv_label_set_text(st, "");
    lv_obj_set_style_text_font(st, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(st, LV_TEXT_ALIGN_LEFT, 0);
    place_rot(st, 16, 8, 768, 28);
    s_scan_stat = st;

    for (int i = 0; i < SCAN_ROWS; i++) {
        int sy = SCAN_TOP + i * SCAN_PITCH;
        lv_obj_t* rb = lv_btn_create(s_scan_cont);
        style_key(rb, COL_BTN_BG);          // (2026-10-12 15:05) antes que put(): ver nota en make_key()
        put(rb, 16, sy, 768, SCAN_ROW_H);
        // Separador de fila como en lv_list estándar (2026-10-12 14:49). Borde inferior FÍSICO
        // = lado izquierdo en LVGL nativo (screen_y = 479 − LVGL_x).
        lv_obj_set_style_radius(rb, 0, 0);
        lv_obj_set_style_border_width(rb, 1, 0);
        lv_obj_set_style_border_side(rb, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_border_color(rb, lv_color_hex(COL_TEXT_DIM), 0);
        lv_obj_clear_flag(rb, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(rb, net_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        s_scan_btn[i] = rb;

        lv_obj_t* rl = lv_label_create(s_scan_cont);
        lv_label_set_text(rl, "");
        lv_label_set_long_mode(rl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(rl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(rl, lv_color_hex(COL_TEXT), 0);
        lv_obj_set_style_text_align(rl, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_clear_flag(rl, LV_OBJ_FLAG_CLICKABLE);    // el toque lo recibe el botón de la fila
        place_rot(rl, 28, sy + (SCAN_ROW_H - 22) / 2, 740, 22);
        s_scan_lbl[i] = rl;
        lv_obj_add_flag(rb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(rl, LV_OBJ_FLAG_HIDDEN);
    }

    b = make_key(s_scan_cont, 16, ACT_TOP, 260, KEY_H, "ESCRIBIR A MANO", &lv_font_montserrat_14, COL_BTN_BG, COL_TEXT, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_MANUAL);
    b = make_key(s_scan_cont, 16 + 260 + KEY_GAP, ACT_TOP, 200, KEY_H, "REESCANEAR", &lv_font_montserrat_14, COL_BTN_ACTIVE, COL_UIMODE_LIGHTS, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_RESCAN);
    b = make_key(s_scan_cont, 16 + 260 + 200 + 2 * KEY_GAP, ACT_TOP, 160, KEY_H, "CANCELAR", &lv_font_montserrat_14, 0x330000, 0xFF8888, NULL);
    lv_obj_add_event_cb(b, act_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)ACT_CANCEL);

    refresh_keys();
    set_active(0);

    s_stat_tmr = lv_timer_create(stat_timer_cb, 500, NULL);
}

void uiWifiCfgShow() {
    if (!s_cont) return;
    // Precarga lo guardado para poder corregir en vez de reescribir.
    char ssid[WIFI_SSID_MAX + 1];
    char pass[WIFI_PASS_MAX + 1];
    wifiStoreLoad(ssid, sizeof(ssid), pass, sizeof(pass));
    strlcpy(s_buf[0], ssid, sizeof(s_buf[0]));
    strlcpy(s_buf[1], pass, sizeof(s_buf[1]));
    s_showPass = false;
    s_shown[0][0] = s_shown[1][0] = '\0';     // fuerza repintado
    s_page = 0;
    s_capsLock = false;
    refresh_keys();
    set_hint("", COL_TEXT_DIM);
    set_active(ssid[0] ? 1 : 0);   // si ya hay SSID, empieza en la clave
    refresh_status();
    // (2026-10-12 13:58) Sin SSID guardado: barrido de redes directo, solo hay que poner la clave.
    if (ssid[0]) scan_close(); else scan_open();
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_cont);
    s_open = true;
}

void uiWifiCfgHide() {
    if (!s_cont) return;
    scan_close();
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    s_open = false;
}

bool uiWifiCfgIsOpen() { return s_open; }
