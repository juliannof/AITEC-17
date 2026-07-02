// display/UIBank.cpp — Página Bank (AITEC 2026-06-30)
// LVGL portrait 480×800 → landscape 800×480
// Mapping: screen_x = LVGL_y,  screen_y = 479 − LVGL_x
// Tab bar LV_DIR_TOP (y=0) → aparece en el borde físico izquierdo (landscape).
// Pestañas físicas bottom→top: FAV(0) / SON(1) / CH(2)
//
// Tab 0 — Favoritos:  lv_tileview, páginas de 16 slots, 2 cols × 8 filas
// Tab 1 — Sonidos:    placeholder
// Tab 2 — Canal MIDI: selector estilo S2 (∧ número ∨)
//
#include "UIBank.h"
#include "../config.h"
#include "../midi/MIDIOut.h"
#include "../midi/JVPatches.h"
#include "../nvs/FavStore.h"
#include "lvgl.h"
#include <stdio.h>

// ── Estado estático ───────────────────────────────────────────────────────
static lv_obj_t* s_cont     = NULL;   // contenedor full-screen
static lv_obj_t* s_tabview  = NULL;
static lv_obj_t* s_tab_fav  = NULL;
static lv_obj_t* s_tab_son  = NULL;
static lv_obj_t* s_tab_ch   = NULL;
static lv_obj_t* s_tileview = NULL;   // Tab 0 — tileview favoritos
static lv_obj_t* s_lbl_ch   = NULL;   // Tab 2 — valor canal MIDI
static uint8_t   s_cur_page = 0;      // página activa en tileview (Tab 0)
static bool      s_open     = false;

// ── Sonidos state (Tab 1) ─────────────────────────────────────────────────
static uint8_t   s_son_msb          = 0x51;
static uint8_t   s_son_lsb          = 0;
static lv_obj_t* s_son_tileview     = NULL;
static lv_obj_t* s_son_tab_ref      = NULL;
static lv_obj_t* s_son_bank_btns[6] = {};
static lv_obj_t* s_son_selected_btn = NULL;   // botón patch activo (nullptr = ninguno)
static uint8_t   s_son_last_msb     = 0xFF;   // último patch seleccionado (NVS)
static uint8_t   s_son_last_lsb     = 0xFF;
static uint8_t   s_son_last_pc      = 0xFF;
static lv_obj_t* s_son_tiles[UIBANK_SON_MAX_PAGES]     = {};   // contenedores de página (siempre vivos)
static bool      s_son_pageBuilt[UIBANK_SON_MAX_PAGES] = {};   // lazy-build: ¿tiene botones vivos?
static bool      s_son_bankFav[UIBANK_SON_PATCHES_BANK] = {};  // cache favoritos del banco activo
static uint8_t   s_son_cur_page     = 0;
// Toggle Patch/Performance (2026-07-02 17:55, toque simple tab "SON"). Arrays
// arriba ya dimensionados al máximo (Patch=128/13 páginas), de sobra para Performance (32/4).
static bool      s_son_perfMode     = false;   // false=Patch("SON"), true=Performance("PERFORM")
static bool      s_son_last_perfMode = false;  // modo del patch actualmente resaltado (highlight)

// ── FAV selection state (Tab 0) ───────────────────────────────────────────
static lv_obj_t* s_fav_selected_btn = NULL;
static uint8_t   s_fav_last_slot    = 0xFF;
static lv_obj_t* s_fav_tiles[UIBANK_FAV_MAX_PAGES]     = {};   // contenedores de página (siempre vivos)
static bool      s_fav_pageBuilt[UIBANK_FAV_MAX_PAGES] = {};   // lazy-build: ¿tiene botones vivos?

struct BankDef { uint8_t msb, lsb; const char* label; };
static const BankDef kBanks[6] = {
    {0x50, 0, "USER"},
    {0x51, 0, "PR-A"},
    {0x51, 1, "PR-B"},
    {0x51, 2, "PR-C"},
    {0x51, 3, "GM"},
    {0x51, 4, "PR-E"},
};
// Performance solo tiene USER/PR-A/PR-B (CARD sin instalar) — mismos MSB/LSB
// que los 3 primeros de kBanks, así que comparten label (2026-07-02).
static const BankDef kBanksPerf[3] = {
    {0x50, 0, "USER"},
    {0x51, 0, "PR-A"},
    {0x51, 1, "PR-B"},
};

// ── Helper rotación (mismo que UIKaoss) ──────────────────────────────────
static void rot_label(lv_obj_t* lbl) {
    lv_obj_set_style_transform_rotation(lbl, 900, 0);
    lv_obj_set_style_transform_pivot_x(lbl, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(lbl, LV_PCT(50), 0);
}

// ── Fondo oscuro uniforme en objeto ──────────────────────────────────────
static void dark_bg(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

// ── Canal MIDI: refresca el label grande ─────────────────────────────────
static void ch_refresh() {
    if (!s_lbl_ch) return;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", (int)g_midiChannel);
    lv_label_set_text(s_lbl_ch, buf);
}

// ── Forward declarations ──────────────────────────────────────────────────
static void fav_update_page_window(int active);
// Recall de un favorito: sincroniza Sound Mode (con SysEx si cambia) y banco
// activo del tab Sonidos antes de mandar Bank Select + PC (2026-07-02).
// Definida más abajo (necesita son_build_tiles/son_bank_refresh_btns).
static void son_apply_recall(const FavEntry& e);

// ── Callback favorito pulsado (recall) ───────────────────────────────────
static void fav_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    FavEntry entry;
    if (!favLoad(slot, entry)) return;
    // Actualizar selección visual
    if (s_fav_selected_btn)
        lv_obj_set_style_bg_color(s_fav_selected_btn, lv_color_hex(COL_BTN_BG), 0);
    s_fav_selected_btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_set_style_bg_color(s_fav_selected_btn, lv_color_hex(0x003366), 0);
    s_fav_last_slot = slot;
    son_apply_recall(entry);
}

// ── Callback scroll tileview (actualiza página activa + ventana lazy-build) ─
static void tv_scroll_cb(lv_event_t* e) {
    if (!s_tileview) return;
    lv_obj_t* tile = lv_tileview_get_tile_active(s_tileview);
    if (!tile) return;
    s_cur_page = (uint8_t)lv_obj_get_index(tile);
    fav_update_page_window(s_cur_page);
}

// ── Callback ∧ canal ─────────────────────────────────────────────────────
static void cb_ch_up(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_midiChannel < 16) {
        g_midiChannel = g_midiChannel + 1;
        favSaveMidiChannel(g_midiChannel);
        ch_refresh();
    }
}

// ── Callback ∨ canal ─────────────────────────────────────────────────────
static void cb_ch_dn(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_midiChannel > 1) {
        g_midiChannel = g_midiChannel - 1;
        favSaveMidiChannel(g_midiChannel);
        ch_refresh();
    }
}

// ── Callback cerrar ──────────────────────────────────────────────────────
static void close_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uiBankHide();
}

// ── Construye / reconstruye tileview de favoritos ────────────────────────
// Layout en landscape: cada tile = 16 botones en grid 2 cols × 8 filas.
// LVGL portrait: cada botón (60 × 340) → físico 340px ancho × 60px alto.
// Col 0: LVGL y = 0-340  (físico x: 120-460)
// Col 1: LVGL y = 340-680 (físico x: 460-800)
// Filas: LVGL x = 0,60,120,...,420 → fila 0-7 (invertido, ver fav_populate_page)
//
// Lazy-build: igual que Tab Sonidos (2026-07-01) — solo se construye el
// contenido de la página activa ± UIBANK_FAV_PAGE_KEEP, no las 8 de golpe.

// Construye el contenido (16 botones) de UNA página, si no lo estaba ya.
static void fav_populate_page(int p) {
    if (p < 0 || p >= UIBANK_FAV_MAX_PAGES || s_fav_pageBuilt[p] || !s_fav_tiles[p]) return;
    lv_obj_t* tile = s_fav_tiles[p];
    s_fav_pageBuilt[p] = true;

    for (int i = 0; i < UIBANK_FAV_PER_PAGE; i++) {
        int slot = p * UIBANK_FAV_PER_PAGE + i;
        // screen_y = 479 − LVGL_x → invertido para que i=0 quede arriba (mismo fix que Sonidos).
        int row = (UIBANK_FAV_ROWS - 1) - (i % UIBANK_FAV_ROWS);
        int col = i / UIBANK_FAV_ROWS;

        FavEntry e;
        bool valid = favLoad(slot, e);
        bool is_sel = (slot == (int)s_fav_last_slot && valid);

        lv_obj_t* btn = lv_btn_create(tile);
        lv_obj_set_size(btn, 58, 338);
        lv_obj_set_pos(btn, row * 60 + 1, col * 340 + 1);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (valid) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(is_sel ? 0x003366 : COL_BTN_BG), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_ACTIVE), LV_STATE_PRESSED);
            if (is_sel) s_fav_selected_btn = btn;
            lv_obj_add_event_cb(btn, fav_btn_cb, LV_EVENT_CLICKED,
                                (void*)(uintptr_t)slot);

            char numstr[4];
            snprintf(numstr, sizeof(numstr), "%02d", slot + 1);
            lv_obj_t* lbl_n = lv_label_create(btn);
            lv_label_set_text(lbl_n, numstr);
            lv_obj_set_style_text_font(lbl_n, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_n, lv_color_hex(COL_TEXT_DIM), 0);
            lv_obj_align(lbl_n, LV_ALIGN_CENTER, -18, 0);
            rot_label(lbl_n);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, e.name);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 6, 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(lbl, 44);
            rot_label(lbl);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x060A10), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);

            char numstr[4];
            snprintf(numstr, sizeof(numstr), "%02d", slot + 1);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, numstr);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x1A2030), 0);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
            rot_label(lbl);
        }
    }
}

// Borra el contenido de una página ya construida (el tile en sí queda vacío,
// necesario para que el scroll-snap del tileview conozca todas las páginas).
static void fav_depopulate_page(int p) {
    if (p < 0 || p >= UIBANK_FAV_MAX_PAGES || !s_fav_pageBuilt[p] || !s_fav_tiles[p]) return;
    if (s_fav_selected_btn && lv_obj_get_parent(s_fav_selected_btn) == s_fav_tiles[p])
        s_fav_selected_btn = NULL;   // iba a quedar colgante tras el lv_obj_clean
    lv_obj_clean(s_fav_tiles[p]);
    s_fav_pageBuilt[p] = false;
}

// Mantiene construida solo la página activa ± UIBANK_FAV_PAGE_KEEP; libera el resto.
static void fav_update_page_window(int active) {
    for (int p = 0; p < UIBANK_FAV_MAX_PAGES; p++) {
        bool keep = (p >= active - UIBANK_FAV_PAGE_KEEP && p <= active + UIBANK_FAV_PAGE_KEEP);
        if (keep) fav_populate_page(p);
        else      fav_depopulate_page(p);
    }
}

static void fav_build_tiles() {
    if (s_tileview) {
        lv_obj_delete(s_tileview);
        s_tileview = NULL;
    }

    s_fav_selected_btn = NULL;   // punteros anteriores ya inválidos
    for (int p = 0; p < UIBANK_FAV_MAX_PAGES; p++) { s_fav_tiles[p] = NULL; s_fav_pageBuilt[p] = false; }

    s_tileview = lv_tileview_create(s_tab_fav);
    lv_obj_set_size(s_tileview, LV_PCT(100), LV_PCT(100));
    dark_bg(s_tileview);
    lv_obj_set_style_radius(s_tileview, 0, 0);
    lv_obj_add_event_cb(s_tileview, tv_scroll_cb, LV_EVENT_SCROLL_END, NULL);

    int count = favCount();
    int pages = (count + UIBANK_FAV_PER_PAGE - 1) / UIBANK_FAV_PER_PAGE;
    if (pages < 1) pages = 1;
    if (pages > UIBANK_FAV_MAX_PAGES) pages = UIBANK_FAV_MAX_PAGES;

    for (int p = 0; p < pages; p++) {
        lv_obj_t* tile = lv_tileview_add_tile(s_tileview, p, 0,
                         p == 0 ? LV_DIR_RIGHT : (p == pages-1 ? LV_DIR_LEFT : (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT)));
        dark_bg(tile);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        s_fav_tiles[p] = tile;
    }

    s_cur_page = 0;
    fav_update_page_window(0);
}

// ── Tab 0 — Favoritos ────────────────────────────────────────────────────
static void build_tab_fav(lv_obj_t* tab) {
    dark_bg(tab);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    fav_build_tiles();
}

// ── Forward declarations ──────────────────────────────────────────────────
static void fav_build_tiles();

// ── Sonidos helpers (Tab 1) ───────────────────────────────────────────────
// Dispatchers Patch/Performance (2026-07-02) — los arrays de widgets/estado
// siguen dimensionados al máximo (Patch), estos solo acotan cuánto se usa.
static const BankDef* son_banks()     { return s_son_perfMode ? kBanksPerf : kBanks; }
static int  son_bank_count()          { return s_son_perfMode ? 3 : 6; }
static int  son_bank_size()           { return s_son_perfMode ? UIBANK_PERF_PATCHES_BANK : UIBANK_SON_PATCHES_BANK; }
static int  son_max_pages()           { return s_son_perfMode ? UIBANK_PERF_MAX_PAGES : UIBANK_SON_MAX_PAGES; }
static JVSoundMode son_mode()         { return s_son_perfMode ? JVSoundMode::PERFORMANCE : JVSoundMode::PATCH; }
static const char* son_patch_name(uint8_t msb, uint8_t lsb, uint8_t pc) {
    return s_son_perfMode ? jvPerfName(msb, lsb, pc) : jvPatchName(msb, lsb, pc);
}

static const char* son_bank_label() {
    const BankDef* banks = son_banks();
    int n = son_bank_count();
    for (int i = 0; i < n; i++)
        if (banks[i].msb == s_son_msb && banks[i].lsb == s_son_lsb) return banks[i].label;
    return "";
}

// Resalta el banco activo y oculta los botones que no existen en Performance
// (índices 3-5: PR-C/GM/PR-E, solo Patch — 2026-07-02).
static void son_bank_refresh_btns() {
    const BankDef* banks = son_banks();
    int n = son_bank_count();
    for (int i = 0; i < 6; i++) {
        if (!s_son_bank_btns[i]) continue;
        if (i >= n) { lv_obj_add_flag(s_son_bank_btns[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s_son_bank_btns[i], LV_OBJ_FLAG_HIDDEN);
        bool active = (banks[i].msb == s_son_msb && banks[i].lsb == s_son_lsb);
        lv_obj_set_style_bg_color(s_son_bank_btns[i],
            lv_color_hex(active ? 0x003366 : 0x0A0F18), 0);
    }
}

// Círculo naranja = favorito guardado (a la izquierda del texto, eje local-y = screen_x)
static void son_add_fav_dot(lv_obj_t* btn) {
    lv_obj_t* fav = lv_obj_create(btn);
    lv_obj_set_size(fav, UIBANK_SON_FAV_DOT, UIBANK_SON_FAV_DOT);
    lv_obj_set_style_radius(fav, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(fav, lv_color_hex(COL_FAV_STAR), 0);
    lv_obj_set_style_bg_opa(fav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fav, 0, 0);
    lv_obj_set_style_pad_all(fav, 0, 0);
    lv_obj_set_style_shadow_width(fav, 0, 0);
    lv_obj_clear_flag(fav, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(fav, LV_ALIGN_CENTER, 0, UIBANK_SON_FAV_DOT_OFS);
    lv_obj_set_user_data(btn, fav);   // referencia para poder borrarlo sin reconstruir la página
}

// Quita el círculo de favorito de un botón (si lo tenía)
static void son_remove_fav_dot(lv_obj_t* btn) {
    lv_obj_t* fav = (lv_obj_t*)lv_obj_get_user_data(btn);
    if (!fav) return;
    lv_obj_delete(fav);
    lv_obj_set_user_data(btn, NULL);
}

// Pulsación sobre un patch distinto → selección/recall normal.
// Pulsación sobre el patch YA seleccionado → ciclo: sin favorito → favorito → sin favorito.
static void son_patch_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t pc = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    bool already_selected = (pc == s_son_last_pc && s_son_msb == s_son_last_msb && s_son_lsb == s_son_last_lsb
                              && s_son_perfMode == s_son_last_perfMode);

    if (already_selected) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (s_son_bankFav[pc]) {
            // Ya era favorito → quitarlo
            int idx = favFindIndex(g_currentSynth, (uint8_t)g_midiChannel, s_son_msb, s_son_lsb, pc, son_mode());
            if (idx >= 0) favDelete(idx);
            s_son_bankFav[pc] = false;
            son_remove_fav_dot(btn);
            fav_build_tiles();   // actualiza Tab 0 (el slot queda vacío)
            return;
        }
        int slot = favCount();
        if (slot > 127) return;
        FavEntry entry;
        entry.synth = g_currentSynth;
        entry.ch    = (uint8_t)g_midiChannel;
        entry.msb   = s_son_msb;
        entry.lsb   = s_son_lsb;
        entry.pc    = pc;
        entry.mode  = son_mode();
        const char* name = son_patch_name(s_son_msb, s_son_lsb, pc);
        strncpy(entry.name, name ? name : "---", sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        favSave(slot, entry);
        s_son_bankFav[pc] = true;
        son_add_fav_dot(btn);
        fav_build_tiles();   // actualiza Tab 0 con el nuevo favorito
        return;
    }

    // Actualizar selección visual
    if (s_son_selected_btn)
        lv_obj_set_style_bg_color(s_son_selected_btn, lv_color_hex(COL_BTN_BG), 0);
    s_son_selected_btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_set_style_bg_color(s_son_selected_btn, lv_color_hex(0x003366), 0);
    s_son_last_msb = s_son_msb;
    s_son_last_lsb = s_son_lsb;
    s_son_last_pc  = pc;
    s_son_last_perfMode = s_son_perfMode;
    // "Último banco" solo se persiste en Patch mode: UIBank siempre arranca
    // en Patch, así que restaurar un banco Performance no tendría sentido
    // sin antes reenviar el SysEx Sound Mode (2026-07-02).
    if (!s_son_perfMode) favSaveLastSel(s_son_msb, s_son_lsb, pc);
    sendBankPC(g_midiChannel, s_son_msb, s_son_lsb, pc);
}

// Construye el contenido (botones+labels+favorito) de UNA página, si no lo estaba ya.
static void son_populate_page(int p) {
    int maxPages = son_max_pages();
    if (p < 0 || p >= maxPages || s_son_pageBuilt[p] || !s_son_tiles[p]) return;
    lv_obj_t* tile = s_son_tiles[p];
    s_son_pageBuilt[p] = true;

    const int PER_PAGE  = UIBANK_SON_PER_PAGE;
    const int ROWS      = UIBANK_SON_ROWS;
    const int ROW_PITCH = UIBANK_SON_ROW_PITCH;
    int on_page = (p == maxPages - 1) ? (son_bank_size() - p * PER_PAGE) : PER_PAGE;

    for (int i = 0; i < on_page; i++) {
        uint8_t pc  = (uint8_t)(p * PER_PAGE + i);
        // screen_y = 479 − LVGL_x → a más LVGL_x, más arriba en pantalla.
        // Invertido para que i=0 (primer patch) quede arriba y descienda con i.
        int row = (ROWS - 1) - (i % ROWS);   // 0-4 en LVGL x → físico y
        int col = i / ROWS;                  // 0-1 en LVGL y → físico x

        lv_obj_t* btn = lv_btn_create(tile);
        lv_obj_set_size(btn, UIBANK_SON_BTN_W, UIBANK_SON_BTN_H);
        lv_obj_set_pos(btn, row * ROW_PITCH + 1, col * UIBANK_SON_COL_PITCH + 1);
        lv_obj_set_style_radius(btn, UIBANK_SON_BTN_RADIUS, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        bool is_sel = (pc == s_son_last_pc && s_son_msb == s_son_last_msb && s_son_lsb == s_son_last_lsb
                       && s_son_perfMode == s_son_last_perfMode);
        lv_obj_set_style_bg_color(btn, lv_color_hex(is_sel ? 0x003366 : COL_BTN_BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_ACTIVE), LV_STATE_PRESSED);
        if (is_sel) s_son_selected_btn = btn;
        lv_obj_add_event_cb(btn, son_patch_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)pc);

        // Combo "BANCO:NNN Nombre" (estilo JV-2080, p.ej. "PR-C:023 Mary-AnneVox")
        const char* name = son_patch_name(s_son_msb, s_son_lsb, pc);
        char combo[40];
        snprintf(combo, sizeof(combo), "%s:%03d %s", son_bank_label(), (int)pc + 1, name ? name : "---");
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, combo);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        // Desplazado a la derecha (eje local-y = screen_x) para dejar sitio al círculo de favorito
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, UIBANK_SON_LBL_OFS);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, UIBANK_SON_LBL_W2);
        lv_obj_set_height(lbl, lv_font_get_line_height(&lv_font_montserrat_18));
        rot_label(lbl);

        if (s_son_bankFav[pc]) son_add_fav_dot(btn);
    }
}

// Borra el contenido de una página ya construida (el tile en sí queda vacío,
// necesario para que el scroll-snap del tileview conozca todas las páginas).
static void son_depopulate_page(int p) {
    int maxPages = son_max_pages();
    if (p < 0 || p >= maxPages || !s_son_pageBuilt[p] || !s_son_tiles[p]) return;
    if (s_son_selected_btn && lv_obj_get_parent(s_son_selected_btn) == s_son_tiles[p])
        s_son_selected_btn = NULL;   // iba a quedar colgante tras el lv_obj_clean
    lv_obj_clean(s_son_tiles[p]);
    s_son_pageBuilt[p] = false;
}

// Mantiene construida solo la página activa ± UIBANK_SON_PAGE_KEEP; libera el resto.
// Evita tener ~260-390 widgets rotados vivos a la vez (tacto lento, 2026-07-01).
static void son_update_page_window(int active) {
    int maxPages = son_max_pages();
    for (int p = 0; p < maxPages; p++) {
        bool keep = (p >= active - UIBANK_SON_PAGE_KEEP && p <= active + UIBANK_SON_PAGE_KEEP);
        if (keep) son_populate_page(p);
        else      son_depopulate_page(p);
    }
}

static void son_tv_scroll_cb(lv_event_t* e) {
    if (!s_son_tileview) return;
    lv_obj_t* tile = lv_tileview_get_tile_active(s_son_tileview);
    if (!tile) return;
    s_son_cur_page = (uint8_t)lv_obj_get_index(tile);
    son_update_page_window(s_son_cur_page);
}

static void son_build_tiles() {
    if (s_son_tileview) { lv_obj_delete(s_son_tileview); s_son_tileview = NULL; }
    s_son_selected_btn = NULL;   // punteros del tileview anterior ya son inválidos
    for (int p = 0; p < UIBANK_SON_MAX_PAGES; p++) { s_son_tiles[p] = NULL; s_son_pageBuilt[p] = false; }
    if (!s_son_tab_ref) return;

    // Tileview: LVGL x=UIBANK_SON_TILE_X..P4_W, y=100%
    // Grid UIBANK_SON_ROWS filas × 2 cols = UIBANK_SON_PER_PAGE/página.
    // Solo se construyen los botones de la página activa ± UIBANK_SON_PAGE_KEEP
    // (son_update_page_window); las UIBANK_SON_MAX_PAGES páginas existen como
    // tiles vacíos para el scroll-snap, pero no todas tienen contenido vivo a la vez.
    s_son_tileview = lv_tileview_create(s_son_tab_ref);
    lv_obj_set_size(s_son_tileview, UIBANK_SON_TILE_W, LV_PCT(100));
    lv_obj_set_pos(s_son_tileview, UIBANK_SON_TILE_X, 0);
    dark_bg(s_son_tileview);
    lv_obj_add_event_cb(s_son_tileview, son_tv_scroll_cb, LV_EVENT_SCROLL_END, NULL);

    // Un solo escaneo de NVS para todo el banco (en vez de uno por botón)
    memset(s_son_bankFav, 0, sizeof(s_son_bankFav));
    favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, s_son_msb, s_son_lsb, son_mode(), s_son_bankFav, son_bank_size());

    int pages = son_max_pages();
    for (int p = 0; p < pages; p++) {
        lv_obj_t* tile = lv_tileview_add_tile(s_son_tileview, p, 0,
            p == 0 ? LV_DIR_RIGHT :
            (p == pages-1 ? LV_DIR_LEFT : (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT)));
        dark_bg(tile);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        s_son_tiles[p] = tile;
    }

    s_son_cur_page = 0;
    son_update_page_window(0);
}

static void son_bank_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t idx = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (idx >= (uint8_t)son_bank_count()) return;   // banco oculto en este modo (botón no debería ser visible)
    const BankDef* banks = son_banks();
    s_son_msb = banks[idx].msb;
    s_son_lsb = banks[idx].lsb;
    son_bank_refresh_btns();
    son_build_tiles();
}

// Sincroniza el canal MIDI de salida con el Sound Mode activo (2026-07-02
// 17:55) — Patch y Performance son tracks distintos en Logic Pro.
static void son_sync_channel_to_mode() {
    g_midiChannel = s_son_perfMode ? MIDI_CH_PERFORM : MIDI_CH_PATCH;
    favSaveMidiChannel(g_midiChannel);
    ch_refresh();
}

// Toggle Patch↔Performance (toque simple en el tab "SON") (2026-07-02 17:55).
static void son_tab_long_press_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_obj_t* tbtn = (lv_obj_t*)lv_event_get_target(e);

    s_son_perfMode = !s_son_perfMode;
    son_sync_channel_to_mode();
    sendSoundMode(son_mode());   // único mecanismo que distingue Patch/Performance en el JV-2080

    lv_obj_t* lbl = lv_obj_get_child(tbtn, 0);
    if (lbl) lv_label_set_text(lbl, s_son_perfMode ? "PERFORM" : "SON");

    // Si el banco activo no existe en el nuevo modo (PR-C/GM/PR-E solo Patch), cae a USER.
    const BankDef* banks = son_banks();
    int n = son_bank_count();
    bool valid = false;
    for (int i = 0; i < n; i++)
        if (banks[i].msb == s_son_msb && banks[i].lsb == s_son_lsb) { valid = true; break; }
    if (!valid) { s_son_msb = banks[0].msb; s_son_lsb = banks[0].lsb; }

    son_bank_refresh_btns();
    son_build_tiles();
}

// Recall de un favorito (touchscreen Tab FAV o NeoTrellis): sincroniza Sound
// Mode con SysEx si el favorito es de otro tipo, y el banco/página del tab
// Sonidos, antes de mandar Bank Select + PC (2026-07-02).
static void son_apply_recall(const FavEntry& e) {
    bool wantPerf = (e.mode == JVSoundMode::PERFORMANCE);
    if (wantPerf != s_son_perfMode) {
        s_son_perfMode = wantPerf;
        son_sync_channel_to_mode();
        sendSoundMode(son_mode());
        if (s_tabview) {
            lv_obj_t* tabbar = lv_tabview_get_tab_bar(s_tabview);
            lv_obj_t* tbtn = lv_obj_get_child(tabbar, 1);   // tab "SON"
            lv_obj_t* lbl  = tbtn ? lv_obj_get_child(tbtn, 0) : NULL;
            if (lbl) lv_label_set_text(lbl, s_son_perfMode ? "PERFORM" : "SON");
        }
    }
    s_son_msb = e.msb;
    s_son_lsb = e.lsb;
    son_bank_refresh_btns();
    son_build_tiles();

    s_son_last_msb = e.msb;
    s_son_last_lsb = e.lsb;
    s_son_last_pc  = e.pc;
    s_son_last_perfMode = s_son_perfMode;
    if (!s_son_perfMode) favSaveLastSel(e.msb, e.lsb, e.pc);
    // Canal del modo activo (12/1), no e.ch histórico — mantiene el PC
    // sincronizado con el track de Logic tras son_sync_channel_to_mode() (2026-07-02 17:55).
    sendBankPC(g_midiChannel, e.msb, e.lsb, e.pc);
}

// ── Tab 1 — Sonidos ───────────────────────────────────────────────────────
// Layout físico landscape: 6 botones banco abajo (LVGL x=0..58) + tileview patches arriba
static void build_tab_sounds(lv_obj_t* tab) {
    dark_bg(tab);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    s_son_tab_ref = tab;

    // 6 botones banco: LVGL size=(58, 113) stacked en LVGL y
    // → físico: 113px ancho × 58px alto, fila en borde inferior pantalla
    const int BTN_H_SON = (P4_H - 120) / 6;   // 680/6 = 113
    for (int i = 0; i < 6; i++) {
        lv_obj_t* btn = lv_btn_create(tab);
        lv_obj_set_size(btn, 58, BTN_H_SON);
        lv_obj_set_pos(btn, 1, i * BTN_H_SON + 1);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0A0F18), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, son_bank_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        s_son_bank_btns[i] = btn;

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, kBanks[i].label);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_obj_center(lbl);
        rot_label(lbl);
    }
    son_bank_refresh_btns();   // resalta PR-A (default)
    son_build_tiles();
}

// ── Tab 2 — Canal MIDI (estilo S2 _drawValEdit) ──────────────────────────
// Físico (landscape): título arriba → ∧ → número grande → [1-16] → ∨ abajo.
// LVGL portrait: x_ofs positivo = hacia arriba físico (479 - LVGL_x = top).
static void build_tab_channel(lv_obj_t* tab) {
    dark_bg(tab);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    // Título
    lv_obj_t* title = lv_label_create(tab);
    lv_label_set_text(title, "Canal MIDI");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COL_TEXT), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 155, 0);
    rot_label(title);

    // ∧ botón arriba
    lv_obj_t* btn_up = lv_btn_create(tab);
    lv_obj_set_size(btn_up, 65, 220);  // 65h×220w LVGL → físico 220px ancho × 65px alto
    lv_obj_set_style_bg_color(btn_up, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_border_width(btn_up, 0, 0);
    lv_obj_set_style_radius(btn_up, 8, 0);
    lv_obj_set_style_shadow_width(btn_up, 0, 0);
    lv_obj_align(btn_up, LV_ALIGN_CENTER, 85, 0);
    lv_obj_add_event_cb(btn_up, cb_ch_up, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl_up, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_up, lv_color_hex(COL_ACCENT), 0);
    lv_obj_center(lbl_up);
    rot_label(lbl_up);

    // Valor — número grande
    s_lbl_ch = lv_label_create(tab);
    lv_obj_set_style_text_font(s_lbl_ch, &lv_font_montserrat_44, 0);
    lv_obj_set_style_text_color(s_lbl_ch, lv_color_hex(COL_TEXT), 0);
    lv_obj_align(s_lbl_ch, LV_ALIGN_CENTER, 0, 0);
    rot_label(s_lbl_ch);
    ch_refresh();

    // Rango
    lv_obj_t* lbl_range = lv_label_create(tab);
    lv_label_set_text(lbl_range, "[ 1 - 16 ]");
    lv_obj_set_style_text_font(lbl_range, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_range, lv_color_hex(COL_TEXT_DIM), 0);
    lv_obj_align(lbl_range, LV_ALIGN_CENTER, -52, 0);
    rot_label(lbl_range);

    // ∨ botón abajo
    lv_obj_t* btn_dn = lv_btn_create(tab);
    lv_obj_set_size(btn_dn, 65, 220);
    lv_obj_set_style_bg_color(btn_dn, lv_color_hex(COL_BTN_BG), 0);
    lv_obj_set_style_border_width(btn_dn, 0, 0);
    lv_obj_set_style_radius(btn_dn, 8, 0);
    lv_obj_set_style_shadow_width(btn_dn, 0, 0);
    lv_obj_align(btn_dn, LV_ALIGN_CENTER, -120, 0);
    lv_obj_add_event_cb(btn_dn, cb_ch_dn, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_dn = lv_label_create(btn_dn);
    lv_label_set_text(lbl_dn, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl_dn, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_dn, lv_color_hex(COL_ACCENT), 0);
    lv_obj_center(lbl_dn);
    rot_label(lbl_dn);
}

// ── API pública ───────────────────────────────────────────────────────────
void uiBankCreate(lv_obj_t* parent) {
    // Cargar última selección de Sonidos desde NVS
    favLoadLastSel(s_son_last_msb, s_son_last_lsb, s_son_last_pc);
    // Si el banco guardado coincide con PR-A (default), activarlo
    if (s_son_last_msb != 0xFF) {
        s_son_msb = s_son_last_msb;
        s_son_lsb = s_son_last_lsb;
    }

    s_cont = lv_obj_create(parent);
    lv_obj_set_size(s_cont, P4_W, P4_H);
    lv_obj_set_pos(s_cont, 0, 0);
    dark_bg(s_cont);
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);   // oculto al inicio

    // ── Tabview ──────────────────────────────────────────────────────────
    s_tabview = lv_tabview_create(s_cont);
    lv_tabview_set_tab_bar_position(s_tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(s_tabview, 120);
    lv_obj_set_size(s_tabview, P4_W, P4_H);
    lv_obj_set_pos(s_tabview, 0, 0);
    dark_bg(s_tabview);

    s_tab_fav = lv_tabview_add_tab(s_tabview, "FAV");
    s_tab_son = lv_tabview_add_tab(s_tabview, "SON");
    s_tab_ch  = lv_tabview_add_tab(s_tabview, "CH");

    // Fondo de tabs y tabbar
    lv_obj_t* tabbar = lv_tabview_get_tab_bar(s_tabview);
    lv_obj_set_style_bg_color(tabbar, lv_color_hex(0x080C12), 0);
    lv_obj_set_style_bg_opa(tabbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tabbar, 0, 0);
    lv_obj_set_style_pad_all(tabbar, 2, 0);

    // Rotar etiquetas de los tabs para que sean legibles en landscape
    uint32_t n = lv_obj_get_child_count(tabbar);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t* tbtn = lv_obj_get_child(tabbar, i);
        lv_obj_set_style_bg_color(tbtn, lv_color_hex(0x080C12), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(tbtn, lv_color_hex(COL_TEXT_DIM), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(tbtn, lv_color_hex(COL_ACCENT), LV_STATE_CHECKED);
        lv_obj_set_style_text_font(tbtn, &lv_font_montserrat_18, 0);
        lv_obj_set_style_border_width(tbtn, 0, 0);
        // Rotar el label hijo
        lv_obj_t* lbl = lv_obj_get_child(tbtn, 0);
        if (lbl) rot_label(lbl);
        // Tab "SON" (i==1): toque simple alterna Patch/Performance (2026-07-02 17:55)
        if (i == 1) lv_obj_add_event_cb(tbtn, son_tab_long_press_cb, LV_EVENT_CLICKED, NULL);
    }

    // ── Contenido de cada tab ─────────────────────────────────────────────
    build_tab_fav(s_tab_fav);
    build_tab_sounds(s_tab_son);
    build_tab_channel(s_tab_ch);

    // ── Botón cerrar (físico: top-right) ─────────────────────────────────
    // Físico top-right = LVGL (x≈455, y≈760)
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

void uiBankShow() {
    if (!s_cont) return;
    uiBankRefreshFavList();
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_cont);
    lv_tabview_set_active(s_tabview, 0, LV_ANIM_OFF);
    s_open = true;
    g_bankOpen = true;
    g_bankTab  = 0;
}

void uiBankHide() {
    if (!s_cont) return;
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    s_open = false;
    g_bankOpen = false;
}

bool uiBankIsOpen() { return s_open; }

void uiBankRefreshFavList() {
    fav_build_tiles();
}

void uiBankNeoKey(uint8_t k) {
    if (!s_open || !s_tabview) return;
    uint32_t tab = lv_tabview_get_tab_active(s_tabview);
    g_bankTab = (uint8_t)tab;

    if (tab == 0) {
        uint8_t slot = s_cur_page * 16 + k;
        FavEntry e;
        if (favLoad(slot, e)) son_apply_recall(e);
    } else if (tab == 2) {
        g_midiChannel = k + 1;
        favSaveMidiChannel(g_midiChannel);
        ch_refresh();
    }
}

void uiBankDestroy() {
    if (s_cont) { lv_obj_delete(s_cont); s_cont = NULL; }
    s_tabview = s_tab_fav = s_tab_son = s_tab_ch = NULL;
    s_tileview = s_lbl_ch = NULL;
    s_son_tileview = s_son_tab_ref = NULL;
    s_son_selected_btn = s_fav_selected_btn = NULL;
    for (int i = 0; i < 6; i++) s_son_bank_btns[i] = NULL;
    s_son_perfMode = s_son_last_perfMode = false;
    s_open = false;
    g_bankOpen = false;
}
