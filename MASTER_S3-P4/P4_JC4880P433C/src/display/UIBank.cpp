// display/UIBank.cpp — Página Bank (AITEC 2026-06-30)
// LVGL portrait 480×800 → landscape 800×480
// Mapping: screen_x = LVGL_y,  screen_y = 479 − LVGL_x
// Tab bar LV_DIR_TOP (y=0) → aparece en el borde físico izquierdo (landscape).
// Pestañas físicas bottom→top: FAV(0) / SON(1) / PERFORM(2)
//
// Rediseño 2026-07-04 — objetivo: aligerar la interfaz para touch operativo:
//   - Grid único 2 cols × 4 filas = 8 ítems/página, igual en las 3 tabs.
//   - Sin lv_tileview: paginación explícita, solo desde NeoTrellis (columnas
//     de página) — no hay gesto de swipe.
//   - Construcción diferida por tab: uiBankCreate() solo crea el tabview
//     vacío; cada tab construye su contenido la primera vez que se activa.
//   - Tab "Canal MIDI" eliminada — Performances ocupa su hueco físico.
//   - NeoTrellis: cada ítem = trío de 3 teclas contiguas (ver NeoTrellis.cpp).
//
// Multi-synth (2026-07-04, segunda entrega) — Sonidos/Performances ya no
// están cableados al JV-2080: cada synth tiene un descriptor (SynthSoundDesc)
// con sus bancos/nombres/SysEx de modo, seleccionado en vivo por
// g_currentSynth. uiBankSynthChanged() se llama desde main.cpp cuando el
// usuario cicla de teclado (SYNTH) con Bank abierto, para refrescar banco,
// página y LEDs sin esperar a reabrir. Favoritos filtra por g_currentSynth —
// cada synth ve solo los suyos, repaginado sobre la lista filtrada (no hay
// correspondencia 1:1 entre índice NVS y posición en pantalla).
#include "UIBank.h"
#include "../config.h"
#include "../midi/MIDIOut.h"
#include "../midi/JVPatches.h"
#include "../midi/TritonPatches.h"
#include "../midi/MotifPatches.h"
#include "../midi/TG55Patches.h"
#include "../nvs/FavStore.h"
#include "../neotrellis/NeoTrellis.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// ── Estado estático común ─────────────────────────────────────────────────
static lv_obj_t* s_cont     = NULL;   // contenedor full-screen
static lv_obj_t* s_tabview  = NULL;
static lv_obj_t* s_tab_fav  = NULL;
static lv_obj_t* s_tab_son  = NULL;
static lv_obj_t* s_tab_perf = NULL;
static lv_obj_t* s_lbl_synth = NULL;   // nombre synth activo, franja top-right (2026-07-13)
static lv_obj_t* s_box_synth = NULL;   // fondo de la franja — color por synth (2026-07-14)
static bool      s_open     = false;

struct BankDef { uint8_t msb, lsb; const char* label; };

// Duplicado local del switch de UIKaoss.cpp::synthName() — sin header compartido,
// mismo patrón ya usado en ese archivo (2026-07-13).
static const char* synthLabelText() {
    switch (g_currentSynth) {
        case ExSynth::JV2080: return "JV-2080";
        case ExSynth::TRITON: return "TRITON";
        case ExSynth::TG55:   return "TG55";
        case ExSynth::D110:   return "D-110";
        case ExSynth::WAVE:   return "WAVE";
        case ExSynth::MOTIF:  return "MOTIF";
        default:              return "?";
    }
}

// Duplicado local del switch de UIKaoss.cpp::synthColor() — mismo patrón que
// synthLabelText() de arriba (2026-07-14, franja de synth pasa de blanco fijo
// a color por synth, "extender este color" — petición del usuario).
static uint32_t synthColorHex() {
    switch (g_currentSynth) {
        case ExSynth::JV2080: return COL_SYNTH_JV;
        case ExSynth::TRITON: return COL_SYNTH_TRI;
        case ExSynth::TG55:   return COL_SYNTH_TG;
        case ExSynth::D110:   return COL_SYNTH_D110;
        case ExSynth::WAVE:   return COL_SYNTH_WAVE;
        case ExSynth::MOTIF:  return COL_SYNTH_MOTIF;
        default:               return COL_ACCENT;
    }
}

// ── JV-2080 ────────────────────────────────────────────────────────────
static const BankDef kBanks[6] = {
    {0x50, 0, "USER"},
    {0x51, 0, "PR-A"},
    {0x51, 1, "PR-B"},
    {0x51, 2, "PR-C"},
    {0x51, 3, "GM"},
    {0x51, 4, "PR-E"},
};
// Performance solo tiene USER/PR-A/PR-B (CARD sin instalar).
static const BankDef kBanksPerf[3] = {
    {0x50, 0, "USER"},
    {0x51, 0, "PR-A"},
    {0x51, 1, "PR-B"},
};
// SysEx Sound Mode del JV-2080 no lleva canal — adapta a la firma común.
static void jvSetModeAdapter(uint8_t ch, bool progMode) {
    (void)ch;
    sendSoundMode(progMode ? JVSoundMode::PATCH : JVSoundMode::PERFORMANCE);
}

// ── Triton (Rack) ──────────────────────────────────────────────────────
// Program y Combination comparten MSB/LSB (docs/Korg_Triton_Patches_Verificado.md)
// Bank g(d) (GM Drums, 2026-07-13) es Program-only, msb=0x78 — distinto de
// Bank G (msb=0x79). Solo 9 PC tienen nombre (ver TritonPatches.cpp kBankGd),
// el resto se muestra como "---" (mismo comportamiento que un banco vacío).
static const BankDef kTritonProgBanks[6] = {
    {0x00, 0, "INT-A"},
    {0x00, 1, "INT-B"},
    {0x00, 2, "INT-C"},
    {0x00, 3, "INT-D"},
    {0x79, 0, "Bank G"},
    {0x78, 0, "G-Drum"},
};
static const BankDef kTritonCombiBanks[4] = {
    {0x00, 0, "INT-A"},
    {0x00, 1, "INT-B"},
    {0x00, 2, "INT-C"},
    {0x00, 3, "INT-D"},
};

// ── MOTIF-RACK (Yamaha) ────────────────────────────────────────────────
// Solo Normal Voice (docs/Yamaha_MOTIF-RACK_Normal_Voice_List.md) — sin Multi
// verificado, por eso no hay combiBanks (tab Performance queda vacía).
// MSB=0x3F (63 decimal, tabla "Available Bank Select/Program Change" de
// MOTIFRACKE2.pdf p.46) — NO 0x63 (99 decimal, bug corregido 2026-07-12).
static const BankDef kMotifBanks[8] = {
    {0x3F, 0, "Preset1"},
    {0x3F, 1, "Preset2"},
    {0x3F, 2, "Preset3"},
    {0x3F, 3, "Preset4"},
    {0x3F, 4, "Preset5"},
    {0x00, 0, "GM"},
    {0x3F, 8, "User1"},
    {0x3F, 9, "User2"},
};

// ── TG55 (Yamaha) ──────────────────────────────────────────────────────
// Solo PRESET (64 voces) — sin Bank Select verificado (docs/Yamaha_TG55_
// Brief_Implementacion.md §7), msb/lsb aquí son solo clave interna, nunca
// se envían por MIDI (ver noBankSelect en SynthSoundDesc). Sin Performances
// (Multi no verificado).
static const BankDef kTG55Banks[1] = {
    {0, 0, "Preset"},
};

// ── Descriptor por synth (2026-07-04, canal fijo desde 2026-07-12) ────────
// Sonidos usa progBanks/progName ("Program"-como-Patch), Performances usa
// combiBanks/combiName ("Combination"-como-Performance). El canal MIDI ya no
// lo decide el firmware — g_midiChannel es fijo en 1, Logic enruta por track
// (revierte el forzado Patch=12/Performance=1 del JV-2080, 2026-07-12).
struct SynthSoundDesc {
    const BankDef* progBanks;   uint8_t progBankCount;
    const BankDef* combiBanks;  uint8_t combiBankCount;
    const char* (*progName)(uint8_t msb, uint8_t lsb, uint8_t pc);
    const char* (*combiName)(uint8_t msb, uint8_t lsb, uint8_t pc);
    void (*setMode)(uint8_t ch, bool progMode);   // nullptr = sin SysEx de modo
    uint8_t progSize;    // patches por banco en Sonidos (2026-07-12)
    uint8_t combiSize;   // patches por banco en Performances — JV-2080 Performance
                          // es 32, no 128 (JVPatches.h:22, jvPerfName pc<32)
    bool    noBankSelect; // true = sin Bank Select verificado, solo Program Change
                           // (TG55 — docs/Yamaha_TG55_Brief_Implementacion.md §7)
    bool    slowSend;     // true = CC0→delay→CC32→delay→PC en vez de ráfaga junta
                           // (MOTIF-RACK — cadena MIDI THRU larga en el UMC1820,
                           // diagnóstico 2026-07-12, ver UIBANK_SLOW_SEND_MS)
};

static const SynthSoundDesc kSynthDesc[NUM_SYNTHS] = {
    /* JV2080 */ { kBanks, 6, kBanksPerf, 3, jvPatchName, jvPerfName, jvSetModeAdapter, 128, 32, false, false },
    /* TRITON */ { kTritonProgBanks, 6, kTritonCombiBanks, 4, tritonProgName, tritonCombiName, sendTritonMode, 128, 128, false, false },
    /* TG55   */ { kTG55Banks, 1, nullptr, 0, tg55ProgName, nullptr, nullptr, 64, 128, true, false },
    /* D110   */ { nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 128, 128, false, false },
    /* WAVE   */ { nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 128, 128, false, false },
    /* MOTIF  */ { kMotifBanks, 8, nullptr, 0, motifProgName, nullptr, nullptr, 128, 128, false, true },
};
static const SynthSoundDesc& activeDesc() { return kSynthDesc[(int)g_currentSynth]; }

// Manda la selección de patch al synth activo — Bank Select + PC, salvo que
// el descriptor marque noBankSelect (TG55, sin Bank Select verificado) o
// slowSend (MOTIF, retardo entre mensajes — ver SynthSoundDesc).
static void sendPatchSelect(uint8_t msb, uint8_t lsb, uint8_t pc) {
    uint8_t ch = g_midiChannel;
    if (activeDesc().noBankSelect) {
        sendPC(ch, pc);
    } else if (activeDesc().slowSend) {
        sendCC(ch, 0, msb);
        vTaskDelay(pdMS_TO_TICKS(UIBANK_SLOW_SEND_MS));
        sendCC(ch, 32, lsb);
        vTaskDelay(pdMS_TO_TICKS(UIBANK_SLOW_SEND_MS));
        sendPC(ch, pc);
    } else {
        sendBankPC(ch, msb, lsb, pc);
    }
}

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

// Círculo naranja = favorito guardado (a la izquierda del texto, eje local-y = screen_x)
static void bank_add_fav_dot(lv_obj_t* btn) {
    lv_obj_t* fav = lv_obj_create(btn);
    lv_obj_set_size(fav, UIBANK_FAV_DOT, UIBANK_FAV_DOT);
    lv_obj_set_style_radius(fav, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(fav, lv_color_hex(COL_FAV_STAR), 0);
    lv_obj_set_style_bg_opa(fav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fav, 0, 0);
    lv_obj_set_style_pad_all(fav, 0, 0);
    lv_obj_set_style_shadow_width(fav, 0, 0);
    lv_obj_clear_flag(fav, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(fav, LV_ALIGN_CENTER, 0, UIBANK_FAV_DOT_OFS);
}

// ── Forward declarations ──────────────────────────────────────────────────
static void apply_recall(const FavEntry& e);
static void fav_render_page(int page);
static void ensure_tab_built(uint32_t tab);
struct SoundTabCtx;
static void soundtab_click_pc(SoundTabCtx& ctx, uint8_t pc);
static void activate_sound_mode(bool perfMode);

// ══════════════════════════════════════════════════════════════════════
// Tab 0 — Favoritos (filtra por g_currentSynth, 2026-07-04)
// ══════════════════════════════════════════════════════════════════════
static lv_obj_t* s_fav_cont         = NULL;
static lv_obj_t* s_fav_selected_btn = NULL;
static uint8_t   s_fav_last_slot    = 0xFF;
static uint8_t   s_fav_curPage      = 0;
static bool      s_fav_built        = false;

// Slots NVS reales (no posiciones en pantalla) de los favoritos del synth
// activo, en orden. Devuelve cuántos hay (<= maxOut).
static int fav_collect_matches(int* out, int maxOut) {
    int count = 0;
    for (int i = 0; i < UIBANK_FAV_SLOTS && count < maxOut; i++) {
        FavEntry e;
        if (favLoad(i, e) && e.synth == g_currentSynth) out[count++] = i;
    }
    return count;
}

static void fav_btn_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    FavEntry entry;
    if (!favLoad(slot, entry)) return;
    s_fav_last_slot = slot;
    apply_recall(entry);
    fav_render_page(s_fav_curPage);   // refresca highlight en Tab FAV
}

// Reconstruye la página activa (8 slots) con los favoritos del synth activo.
static void fav_render_page(int page) {
    if (!s_fav_cont) return;

    int matches[UIBANK_FAV_SLOTS];
    int matchCount = fav_collect_matches(matches, UIBANK_FAV_SLOTS);
    int maxPages = (matchCount + UIBANK_GRID_PER_PAGE - 1) / UIBANK_GRID_PER_PAGE;
    if (maxPages < 1) maxPages = 1;
    if (page < 0) page = 0;
    if (page >= maxPages) page = maxPages - 1;
    s_fav_curPage = (uint8_t)page;
    lv_obj_clean(s_fav_cont);
    s_fav_selected_btn = NULL;

    int base = page * UIBANK_GRID_PER_PAGE;
    uint8_t ledState[UIBANK_GRID_PER_PAGE] = {};

    for (int i = 0; i < UIBANK_GRID_PER_PAGE; i++) {
        int matchIdx = base + i;
        int row  = i / UIBANK_GRID_COLS;
        int col  = i % UIBANK_GRID_COLS;
        // screen_y = 479 − LVGL_x → a más LVGL_x, más ARRIBA en pantalla.
        int lvglRow = (UIBANK_GRID_ROWS - 1) - row;

        bool valid = matchIdx < matchCount;
        int slot = valid ? matches[matchIdx] : -1;
        FavEntry e;
        if (valid) favLoad(slot, e);   // ya sabemos que existe (viene de fav_collect_matches)
        bool is_sel = valid && (slot == (int)s_fav_last_slot);
        ledState[i] = is_sel ? 1 : 0;

        lv_obj_t* btn = lv_btn_create(s_fav_cont);
        lv_obj_set_size(btn, UIBANK_BTN_W_FULL, UIBANK_BTN_H);
        lv_obj_set_pos(btn, lvglRow * UIBANK_ROW_PITCH_FULL + 1, col * UIBANK_COL_PITCH + 1);
        lv_obj_set_style_radius(btn, UIBANK_BTN_RADIUS, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (valid) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(is_sel ? 0x003366 : COL_BTN_BG), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_ACTIVE), LV_STATE_PRESSED);
            if (is_sel) s_fav_selected_btn = btn;
            lv_obj_add_event_cb(btn, fav_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)slot);

            // Numeración = posición entre los favoritos DE ESTE synth (no el
            // slot NVS global, que ahora puede tener huecos de otros synths).
            char combo[24];
            snprintf(combo, sizeof(combo), "%02d %s", matchIdx + 1, e.name);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, combo);
            // montserrat_16 (antes 18, 2026-07-04) — más caracteres visibles
            // antes de truncar con "..." (el hueco de texto es fijo).
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            lv_obj_set_width(lbl, UIBANK_LBL_W);
            lv_obj_set_height(lbl, lv_font_get_line_height(&lv_font_montserrat_16));
            rot_label(lbl);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x060A10), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);

            char numstr[4];
            snprintf(numstr, sizeof(numstr), "%02d", matchIdx + 1);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, numstr);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x1A2030), 0);
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
            rot_label(lbl);
        }
    }
    // Solo escribe los LEDs si Favoritos sigue siendo la tab activa — evita
    // pisar los LEDs de Sonidos/Performances cuando esta función se llama
    // solo para refrescar el estado interno tras un recall.
    if (lv_tabview_get_tab_active(s_tabview) == 0)
        neotrellisBankShowPage(ledState, UIBANK_GRID_PER_PAGE);
}

// ══════════════════════════════════════════════════════════════════════
// Tab 1/2 — Sonidos y Performances — mismo patrón, banco según synth activo
// ══════════════════════════════════════════════════════════════════════
struct SoundTabCtx {
    bool            perfMode;   // false=Sonidos (rol "Program"), true=Performances (rol "Combination")
    uint8_t         msb, lsb;
    lv_obj_t*       tabRef;
    lv_obj_t*       gridCont;
    lv_obj_t*       bankBtns[UIBANK_MAX_BANKS];
    uint8_t         curPage;
    bool            built;
    bool            bankFav[128];   // de sobra para cualquier synth (máx. 128/banco)
    uint8_t         lastMsb, lastLsb, lastPc;
};

static SoundTabCtx s_son  = { false, 0, 0, nullptr, nullptr, {}, 0, false, {}, 0xFF, 0xFF, 0xFF };
static SoundTabCtx s_perf = { true,  0, 0, nullptr, nullptr, {}, 0, false, {}, 0xFF, 0xFF, 0xFF };

// El "modo" de favorito (JVSoundMode) se reutiliza como discriminador
// genérico de rol (Sonidos/Program=PATCH, Performances/Combination=PERFORMANCE)
// para cualquier synth — junto con FavEntry.synth identifica unívocamente
// dónde recuperar/comparar un favorito (2026-07-04).
static JVSoundMode ctxRole(SoundTabCtx& ctx) { return ctx.perfMode ? JVSoundMode::PERFORMANCE : JVSoundMode::PATCH; }

static const BankDef* ctxBanks(SoundTabCtx& ctx) {
    return ctx.perfMode ? activeDesc().combiBanks : activeDesc().progBanks;
}
static int ctxBankCount(SoundTabCtx& ctx) {
    return ctx.perfMode ? activeDesc().combiBankCount : activeDesc().progBankCount;
}
// Tamaño de banco por synth/rol (2026-07-12) — JV-2080 Performance es 32,
// el resto 128. Si algún synth futuro tuviera bancos de tamaño distinto
// entre sí (no solo entre Sonidos/Performances), habría que hacerlo por-banco.
static int ctxBankSize(SoundTabCtx& ctx) {
    return ctx.perfMode ? activeDesc().combiSize : activeDesc().progSize;
}
static int ctxMaxPages(SoundTabCtx& ctx) {
    int pages = (ctxBankSize(ctx) + UIBANK_GRID_PER_PAGE - 1) / UIBANK_GRID_PER_PAGE;
    return pages < 1 ? 1 : pages;
}
static const char* ctxPatchName(SoundTabCtx& ctx, uint8_t pc) {
    auto fn = ctx.perfMode ? activeDesc().combiName : activeDesc().progName;
    return fn ? fn(ctx.msb, ctx.lsb, pc) : nullptr;
}

static const char* ctxBankLabel(SoundTabCtx& ctx) {
    const BankDef* banks = ctxBanks(ctx);
    int count = ctxBankCount(ctx);
    if (!banks) return "";
    for (int i = 0; i < count; i++)
        if (banks[i].msb == ctx.msb && banks[i].lsb == ctx.lsb) return banks[i].label;
    return "";
}

static void ctxBankRefreshBtns(SoundTabCtx& ctx) {
    const BankDef* banks = ctxBanks(ctx);
    int count = ctxBankCount(ctx);
    for (int i = 0; i < UIBANK_MAX_BANKS; i++) {
        if (!ctx.bankBtns[i]) continue;
        if (i >= count || !banks) { lv_obj_add_flag(ctx.bankBtns[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(ctx.bankBtns[i], LV_OBJ_FLAG_HIDDEN);
        bool active = (banks[i].msb == ctx.msb && banks[i].lsb == ctx.lsb);
        lv_obj_set_style_bg_color(ctx.bankBtns[i], lv_color_hex(active ? 0x003366 : 0x0A0F18), 0);
        lv_obj_t* lbl = lv_obj_get_child(ctx.bankBtns[i], 0);
        if (lbl) lv_label_set_text(lbl, banks[i].label);
    }
}

static void son_item_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    soundtab_click_pc(s_son, (uint8_t)(uintptr_t)lv_event_get_user_data(e));
}
static void perf_item_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    soundtab_click_pc(s_perf, (uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

// Reconstruye la página activa (8 botones grandes) del banco/synth actual.
static void soundtab_render_page(SoundTabCtx& ctx, int page) {
    if (!ctx.gridCont) return;
    int maxPages = ctxMaxPages(ctx);
    if (page < 0) page = 0;
    if (page >= maxPages) page = maxPages - 1;
    ctx.curPage = (uint8_t)page;
    lv_obj_clean(ctx.gridCont);

    int base   = page * UIBANK_GRID_PER_PAGE;
    int onPage = ctxBankSize(ctx) - base;
    if (onPage > UIBANK_GRID_PER_PAGE) onPage = UIBANK_GRID_PER_PAGE;
    if (onPage < 0) onPage = 0;

    uint8_t ledState[UIBANK_GRID_PER_PAGE] = {};

    for (int i = 0; i < onPage; i++) {
        uint8_t pc  = (uint8_t)(base + i);
        int row = i / UIBANK_GRID_COLS;
        int col = i % UIBANK_GRID_COLS;
        int lvglRow = (UIBANK_GRID_ROWS - 1) - row;

        bool is_sel = (pc == ctx.lastPc && ctx.msb == ctx.lastMsb && ctx.lsb == ctx.lastLsb);
        bool is_fav = ctx.bankFav[pc];
        ledState[i] = is_sel ? 1 : (is_fav ? 2 : 0);

        lv_obj_t* btn = lv_btn_create(ctx.gridCont);
        lv_obj_set_size(btn, UIBANK_BTN_W_BANK, UIBANK_BTN_H);
        lv_obj_set_pos(btn, lvglRow * UIBANK_ROW_PITCH_BANK + 1, col * UIBANK_COL_PITCH + 1);
        lv_obj_set_style_radius(btn, UIBANK_BTN_RADIUS, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(is_sel ? 0x003366 : COL_BTN_BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN_ACTIVE), LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, ctx.perfMode ? perf_item_cb : son_item_cb,
                             LV_EVENT_CLICKED, (void*)(uintptr_t)pc);

        const char* name = ctxPatchName(ctx, pc);
        char combo[40];
        snprintf(combo, sizeof(combo), "%s:%03d %s", ctxBankLabel(ctx), (int)pc + 1, name ? name : "---");
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, combo);
        // montserrat_16 (antes 18, 2026-07-04) — más caracteres visibles
        // antes de truncar con "..." (nombres largos de Triton/JV-2080).
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, UIBANK_LBL_OFS);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, UIBANK_LBL_W2);
        lv_obj_set_height(lbl, lv_font_get_line_height(&lv_font_montserrat_16));
        rot_label(lbl);

        if (is_fav) bank_add_fav_dot(btn);
    }
    // Solo sincroniza LEDs si esta tab es la activa (evita pisar los LEDs de
    // otra tab tras un recall).
    uint32_t myTab = ctx.perfMode ? 2 : 1;
    if (lv_tabview_get_tab_active(s_tabview) == myTab)
        neotrellisBankShowPage(ledState, onPage);
}

// Cambia la selección visual (highlight) entre dos PC de la MISMA página ya
// renderizada, sin lv_obj_clean ni recrear botones/labels — evita el coste
// de reconstruir hasta 8 botones con texto rotado en cada tap (2026-07-13,
// causa nº2 del lag táctil percibido en Sonidos/Performances; nº1 era la
// escritura NVS síncrona, ya resuelta). oldPc puede no estar en la página
// actual (ej. el usuario pasó de página sin tocar nada) — se ignora sin más,
// el índice simplemente no coincide con ningún hijo del grid. Si newPc no
// está en la página actual (no debería poder pasar: solo se puede tocar un
// botón visible), hace fallback al render completo.
static void soundtab_update_selection(SoundTabCtx& ctx, int oldPc, int newPc) {
    if (!ctx.gridCont) return;
    int base   = ctx.curPage * UIBANK_GRID_PER_PAGE;
    int onPage = ctxBankSize(ctx) - base;
    if (onPage > UIBANK_GRID_PER_PAGE) onPage = UIBANK_GRID_PER_PAGE;
    if (onPage < 0) onPage = 0;

    int newI = newPc - base;
    if (newI < 0 || newI >= onPage) { soundtab_render_page(ctx, ctx.curPage); return; }
    int oldI = oldPc - base;

    uint8_t ledState[UIBANK_GRID_PER_PAGE] = {};
    for (int i = 0; i < onPage; i++) {
        uint8_t pc = (uint8_t)(base + i);
        bool is_sel = (i == newI);
        ledState[i] = is_sel ? 1 : (ctx.bankFav[pc] ? 2 : 0);
        if (i != newI && i != oldI) continue;   // solo los botones que cambian de estado
        lv_obj_t* btn = lv_obj_get_child(ctx.gridCont, i);
        if (btn) lv_obj_set_style_bg_color(btn, lv_color_hex(is_sel ? 0x003366 : COL_BTN_BG), 0);
    }
    uint32_t myTab = ctx.perfMode ? 2 : 1;
    if (lv_tabview_get_tab_active(s_tabview) == myTab)
        neotrellisBankShowPage(ledState, onPage);
}

// Pulsación sobre un patch distinto → selección/recall. Pulsación sobre el
// patch YA seleccionado → ciclo: sin favorito → favorito → sin favorito.
static void soundtab_click_pc(SoundTabCtx& ctx, uint8_t pc) {
    bool already = (pc == ctx.lastPc && ctx.msb == ctx.lastMsb && ctx.lsb == ctx.lastLsb);

    if (already) {
        if (ctx.bankFav[pc]) {
            int idx = favFindIndex(g_currentSynth, (uint8_t)g_midiChannel, ctx.msb, ctx.lsb, pc, ctxRole(ctx));
            if (idx >= 0) favDelete(idx);
            ctx.bankFav[pc] = false;
        } else {
            int slot = favFirstFreeSlot();   // reutiliza huecos (ver FavStore.cpp)
            if (slot >= 0) {
                FavEntry entry{};
                entry.synth = g_currentSynth;
                entry.ch    = (uint8_t)g_midiChannel;
                entry.msb   = ctx.msb;
                entry.lsb   = ctx.lsb;
                entry.pc    = pc;
                entry.mode  = ctxRole(ctx);
                const char* name = ctxPatchName(ctx, pc);
                strncpy(entry.name, name ? name : "---", sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                favSave(slot, entry);
                ctx.bankFav[pc] = true;
            }
        }
        soundtab_render_page(ctx, ctx.curPage);
        if (s_fav_built) fav_render_page(s_fav_curPage);   // el slot pudo cambiar en Tab FAV
        return;
    }

    uint8_t oldPc = ctx.lastPc;
    ctx.lastMsb = ctx.msb;
    ctx.lastLsb = ctx.lsb;
    ctx.lastPc  = pc;
    // favSaveLastSel() NO se llama aquí (2026-07-13) — era una escritura NVS
    // síncrona (4× putUChar, FavStore.cpp) en el hilo de touch en CADA tap,
    // causando lag perceptible. El estado vive en ctx.lastMsb/lastLsb/lastPc
    // (RAM) durante la sesión; se persiste una sola vez en uiBankHide().
    // bankLastSelSet() tampoco escribe NVS aquí — solo RAM, dirty flag
    // (2026-07-13, ver FavStore.h) para que cambiar de banco luego salte a
    // este PC en vez de a la página 0.
    if (!ctx.perfMode) bankLastSelSet(g_currentSynth, ctx.msb, ctx.lsb, pc);
    sendPatchSelect(ctx.msb, ctx.lsb, pc);
    // soundtab_update_selection() en vez de soundtab_render_page() completo
    // (2026-07-13) — misma página, mismo banco: solo cambian 1-2 botones de
    // estado, no hace falta destruir y recrear los 8.
    soundtab_update_selection(ctx, oldPc, pc);
}

static void bank_select(SoundTabCtx& ctx, uint8_t idx) {
    const BankDef* banks = ctxBanks(ctx);
    if (!banks || idx >= (uint8_t)ctxBankCount(ctx)) return;
    ctx.msb = banks[idx].msb;
    ctx.lsb = banks[idx].lsb;
    ctxBankRefreshBtns(ctx);
    memset(ctx.bankFav, 0, sizeof(ctx.bankFav));
    favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, ctx.msb, ctx.lsb, ctxRole(ctx), ctx.bankFav, ctxBankSize(ctx));

    // Salta al último sonido usado en este banco (PC 0 si es la primera vez)
    // y lo envía por MIDI — el synth debe sonar lo mismo que se resalta en
    // pantalla (2026-07-13). Solo Sonidos (Program); Performances se queda
    // en página 0 sin auto-enviar, igual que favSaveLastSel().
    uint8_t pc = 0;
    if (!ctx.perfMode) bankLastSelGet(g_currentSynth, ctx.msb, ctx.lsb, pc);   // pc=0 si no hay memoria
    ctx.lastMsb = ctx.msb;
    ctx.lastLsb = ctx.lsb;
    ctx.lastPc  = pc;
    if (!ctx.perfMode) {
        sendPatchSelect(ctx.msb, ctx.lsb, pc);
        bankLastSelSet(g_currentSynth, ctx.msb, ctx.lsb, pc);   // primera vez → registra PC 0 como "último"
    }
    soundtab_render_page(ctx, pc / UIBANK_GRID_PER_PAGE);
}

static void son_bank_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    bank_select(s_son, (uint8_t)(uintptr_t)lv_event_get_user_data(e));
}
static void perf_bank_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    bank_select(s_perf, (uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

// Construye franja de banco + contenedor de grid (una sola vez, al primer visit).
static void build_sound_tab(SoundTabCtx& ctx, lv_obj_t* tab, lv_event_cb_t bankCb) {
    dark_bg(tab);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    ctx.tabRef = tab;

    const BankDef* banks = ctxBanks(ctx);
    int count = ctxBankCount(ctx);
    const int BTN_H_STRIP = (P4_H - 120) / UIBANK_MAX_BANKS;
    for (int i = 0; i < UIBANK_MAX_BANKS; i++) {
        lv_obj_t* btn = lv_btn_create(tab);
        lv_obj_set_size(btn, 58, BTN_H_STRIP);
        lv_obj_set_pos(btn, 1, i * BTN_H_STRIP + 1);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x0A0F18), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, bankCb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        ctx.bankBtns[i] = btn;

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, (banks && i < count) ? banks[i].label : "");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_obj_center(lbl);
        rot_label(lbl);
    }

    ctx.gridCont = lv_obj_create(tab);
    lv_obj_set_size(ctx.gridCont, UIBANK_TILE_W, LV_PCT(100));
    lv_obj_set_pos(ctx.gridCont, UIBANK_TILE_X, 0);
    dark_bg(ctx.gridCont);
    lv_obj_clear_flag(ctx.gridCont, LV_OBJ_FLAG_SCROLLABLE);
}

// ── Recall de un favorito (touchscreen Tab FAV o NeoTrellis) ──────────────
// Cambia a la tab (Sonidos/Performances) correspondiente al rol del
// favorito, sincroniza Sound Mode (SysEx si cambia) y canal, banco/página,
// antes de mandar Bank Select + PC. e.synth ya coincide con g_currentSynth
// (Favoritos filtra por synth activo — no hace falta cambiar de teclado).
static void apply_recall(const FavEntry& e) {
    bool wantPerf = (e.mode == JVSoundMode::PERFORMANCE);
    SoundTabCtx& ctx = wantPerf ? s_perf : s_son;
    uint32_t targetTab = wantPerf ? 2 : 1;

    if (lv_tabview_get_tab_active(s_tabview) != targetTab)
        lv_tabview_set_active(s_tabview, targetTab, LV_ANIM_OFF);
    ensure_tab_built(targetTab);
    // Explícito, no depende de que lv_tabview_set_active() dispare
    // LV_EVENT_VALUE_CHANGED de forma síncrona (idempotente si ya se envió).
    activate_sound_mode(wantPerf);

    ctx.msb = e.msb;
    ctx.lsb = e.lsb;
    ctxBankRefreshBtns(ctx);
    memset(ctx.bankFav, 0, sizeof(ctx.bankFav));
    favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, ctx.msb, ctx.lsb, ctxRole(ctx), ctx.bankFav, ctxBankSize(ctx));

    ctx.lastMsb = e.msb;
    ctx.lastLsb = e.lsb;
    ctx.lastPc  = e.pc;
    // Igual que en soundtab_click_pc() (2026-07-13): sin favSaveLastSel() aquí,
    // se persiste una sola vez en uiBankHide(). bankLastSelSet() sí, pero solo
    // marca dirty en RAM (ver FavStore.h).
    if (!wantPerf) bankLastSelSet(e.synth, e.msb, e.lsb, e.pc);

    soundtab_render_page(ctx, e.pc / UIBANK_GRID_PER_PAGE);
    sendPatchSelect(e.msb, e.lsb, e.pc);
    g_bankTab = (uint8_t)targetTab;
}

// ── Sincroniza Sound Mode (SysEx) + canal MIDI con el modo/synth activo ───
static bool    s_modeSent      = false;
static bool    s_lastSentPerf  = false;
static ExSynth s_lastSentSynth = ExSynth::JV2080;

static void activate_sound_mode(bool perfMode) {
    auto setFn = activeDesc().setMode;
    bool changed = !s_modeSent || s_lastSentPerf != perfMode || s_lastSentSynth != g_currentSynth;
    if (setFn && changed) {
        setFn(g_midiChannel, !perfMode);
        s_lastSentPerf  = perfMode;
        s_lastSentSynth = g_currentSynth;
        s_modeSent = true;
    }
}

// ── Construcción diferida por tab (primera vez que se activa) ────────────
static void ensure_tab_built(uint32_t tab) {
    if (tab == 0 && !s_fav_built) {
        s_fav_built = true;
        s_fav_cont = lv_obj_create(s_tab_fav);
        lv_obj_set_size(s_fav_cont, LV_PCT(100), LV_PCT(100));
        dark_bg(s_fav_cont);
        lv_obj_clear_flag(s_fav_cont, LV_OBJ_FLAG_SCROLLABLE);
        fav_render_page(0);
    } else if (tab == 1 && !s_son.built) {
        s_son.built = true;
        build_sound_tab(s_son, s_tab_son, son_bank_cb);
        ctxBankRefreshBtns(s_son);
        memset(s_son.bankFav, 0, sizeof(s_son.bankFav));
        favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, s_son.msb, s_son.lsb, ctxRole(s_son), s_son.bankFav, ctxBankSize(s_son));
        soundtab_render_page(s_son, 0);
    } else if (tab == 2 && !s_perf.built) {
        s_perf.built = true;
        build_sound_tab(s_perf, s_tab_perf, perf_bank_cb);
        ctxBankRefreshBtns(s_perf);
        memset(s_perf.bankFav, 0, sizeof(s_perf.bankFav));
        favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, s_perf.msb, s_perf.lsb, ctxRole(s_perf), s_perf.bankFav, ctxBankSize(s_perf));
        soundtab_render_page(s_perf, 0);
    }
}

static void tabview_changed_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    uint32_t tab = lv_tabview_get_tab_active(s_tabview);
    g_bankTab = (uint8_t)tab;
    ensure_tab_built(tab);
    if (tab == 1)      activate_sound_mode(false);
    else if (tab == 2) activate_sound_mode(true);
}

// ── Callback cerrar ──────────────────────────────────────────────────────
static void close_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    uiBankHide();
}

// ── API pública ───────────────────────────────────────────────────────────
void uiBankCreate(lv_obj_t* parent) {
    // Cargar última selección de Sonidos desde NVS (rol Sonidos; Performances no se persiste).
    // Solo se aplica si pertenece al synth activo al arrancar (2026-07-05) —
    // si no, msb/lsb de otro synth no significan nada en las tablas de este.
    ExSynth lastSynth;
    favLoadLastSel(lastSynth, s_son.lastMsb, s_son.lastLsb, s_son.lastPc);
    if (s_son.lastMsb != 0xFF && lastSynth == g_currentSynth) {
        s_son.msb = s_son.lastMsb;
        s_son.lsb = s_son.lastLsb;
    } else if (ctxBanks(s_son)) {
        s_son.msb = ctxBanks(s_son)[0].msb;
        s_son.lsb = ctxBanks(s_son)[0].lsb;
    }
    if (ctxBanks(s_perf)) {
        s_perf.msb = ctxBanks(s_perf)[0].msb;
        s_perf.lsb = ctxBanks(s_perf)[0].lsb;
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

    s_tab_fav  = lv_tabview_add_tab(s_tabview, "FAV");
    s_tab_son  = lv_tabview_add_tab(s_tabview, "SON");
    s_tab_perf = lv_tabview_add_tab(s_tabview, "PERF");
    s_son.tabRef  = s_tab_son;
    s_perf.tabRef = s_tab_perf;

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
        lv_obj_t* lbl = lv_obj_get_child(tbtn, 0);
        if (lbl) rot_label(lbl);
    }
    lv_obj_add_event_cb(s_tabview, tabview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Contenido de cada tab: solo fondo — construcción real diferida
    // a ensure_tab_built(), disparada la primera vez que cada tab se activa.
    dark_bg(s_tab_fav);  lv_obj_clear_flag(s_tab_fav,  LV_OBJ_FLAG_SCROLLABLE);
    dark_bg(s_tab_son);  lv_obj_clear_flag(s_tab_son,  LV_OBJ_FLAG_SCROLLABLE);
    dark_bg(s_tab_perf); lv_obj_clear_flag(s_tab_perf, LV_OBJ_FLAG_SCROLLABLE);

    // ── Nombre synth activo (misma franja física que la X, empieza a su
    // izquierda; la X se queda en su posición actual) (2026-07-13) ────────
    // y=130 (no 0): la franja de pestañas (tab_bar_size=120, línea ~727) vive
    // en LVGL x=0..480 → screen_y=0..479 completo, con PERFORM en el extremo
    // LVGL x alto = screen_y bajo (comentario top del archivo: "bottom→top:
    // FAV/SON/PERFORM"). Empezar en y=0 tapaba PERFORM con la franja blanca.
    // Probado mandar la franja al fondo (move_background) para llegar a y=0
    // sin tapar PERFORM — descartado: s_tabview tiene fondo opaco a pantalla
    // completa (dark_bg(s_tabview)), así que detrás de él la franja entera
    // desaparece, no solo en la zona de pestañas. El hueco de 130px es la
    // solución estable.
    s_box_synth = lv_obj_create(s_cont);
    lv_obj_t* box_synth = s_box_synth;
    lv_obj_set_size(box_synth, UIBANK_TOPSTRIP_W, 745 - 130);   // hasta la X (y=745), tras la franja de pestañas
    lv_obj_set_pos(box_synth, P4_W - UIBANK_TOPSTRIP_W, 130);
    lv_obj_set_style_bg_color(box_synth, lv_color_hex(synthColorHex()), 0);   // color por synth (2026-07-14, antes blanco fijo)
    lv_obj_set_style_bg_opa(box_synth, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box_synth, 0, 0);
    lv_obj_set_style_radius(box_synth, 0, 0);
    lv_obj_set_style_pad_all(box_synth, 0, 0);
    lv_obj_clear_flag(box_synth, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(box_synth, LV_OBJ_FLAG_CLICKABLE);
    s_lbl_synth = lv_label_create(box_synth);
    lv_label_set_text(s_lbl_synth, synthLabelText());
    lv_obj_set_style_text_color(s_lbl_synth, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(s_lbl_synth, &lv_font_montserrat_18, 0);
    // TOP_MID = principio de la faja (LVGL y≈0, ya empieza en 130 absoluto).
    // offset x: screen_y = 479 - LVGL_x → subir 3px físico = +3 aquí (-6→-3).
    // offset y: screen_x = LVGL_y → derecha 10px físico = +10 aquí (30→40).
    lv_obj_align(s_lbl_synth, LV_ALIGN_TOP_MID, -3, 40);
    rot_label(s_lbl_synth);

    // ── Botón cerrar (físico: top-right) ─────────────────────────────────
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
    g_bankOpen = true;
    ensure_tab_built(0);
    fav_render_page(s_fav_curPage);
    lv_obj_clear_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_cont);
    lv_tabview_set_active(s_tabview, 0, LV_ANIM_OFF);
    s_open = true;
    g_bankTab = 0;
}

void uiBankHide() {
    if (!s_cont) return;
    // Único punto de persistencia NVS de "última selección" (2026-07-13) —
    // antes se escribía en cada tap (soundtab_click_pc/apply_recall), aquí se
    // hace una sola vez al cerrar. Guard lastPc!=0xFF: evita persistir el
    // placeholder "sin selección" si Bank se abre y cierra sin tocar nada.
    if (s_son.lastPc != 0xFF)
        favSaveLastSel(g_currentSynth, s_son.lastMsb, s_son.lastLsb, s_son.lastPc);
    bankLastSelFlushIfDirty();   // último PC por banco (2026-07-13) — flush al cerrar además del periódico
    lv_obj_add_flag(s_cont, LV_OBJ_FLAG_HIDDEN);
    s_open = false;
    g_bankOpen = false;
    neotrellisRestoreKaoss();
}

bool uiBankIsOpen() { return s_open; }

void uiBankRefreshFavList() {
    if (s_fav_built) fav_render_page(s_fav_curPage);
}

// Sincroniza msb/lsb de Sonidos/Performances con el primer banco del synth
// activo — SIEMPRE, esté Bank abierto o no (2026-07-12). Sin esto, cambiar de
// synth con Bank cerrado (selección directa L8-L14) deja msb/lsb del synth
// anterior, y al abrir Sonidos después no encuentra nada (progName recibe
// msb/lsb que no le pertenecen).
static void syncBankSelToSynth() {
    const BankDef* pb = ctxBanks(s_son);
    if (pb) { s_son.msb = pb[0].msb; s_son.lsb = pb[0].lsb; }
    s_son.lastMsb = s_son.lastLsb = s_son.lastPc = 0xFF;

    const BankDef* cb = ctxBanks(s_perf);
    if (cb) { s_perf.msb = cb[0].msb; s_perf.lsb = cb[0].lsb; }
    s_perf.lastMsb = s_perf.lastLsb = s_perf.lastPc = 0xFF;
}

// Se llama desde main.cpp cuando g_currentSynth cambia (tap/selección directa
// de SYNTH en NeoTrellis) — refresca los widgets visibles si Bank está
// abierto; si está cerrado, syncBankSelToSynth() ya dejó los datos listos
// para cuando se abra.
void uiBankSynthChanged() {
    syncBankSelToSynth();
    if (s_lbl_synth) lv_label_set_text(s_lbl_synth, synthLabelText());
    if (s_box_synth) lv_obj_set_style_bg_color(s_box_synth, lv_color_hex(synthColorHex()), 0);
    if (!s_open) return;

    if (s_son.built) {
        const BankDef* banks = ctxBanks(s_son);
        ctxBankRefreshBtns(s_son);
        memset(s_son.bankFav, 0, sizeof(s_son.bankFav));
        if (banks) favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, s_son.msb, s_son.lsb, ctxRole(s_son), s_son.bankFav, ctxBankSize(s_son));
        soundtab_render_page(s_son, 0);
    }
    if (s_perf.built) {
        const BankDef* banks = ctxBanks(s_perf);
        ctxBankRefreshBtns(s_perf);
        memset(s_perf.bankFav, 0, sizeof(s_perf.bankFav));
        if (banks) favMarkBank(g_currentSynth, (uint8_t)g_midiChannel, s_perf.msb, s_perf.lsb, ctxRole(s_perf), s_perf.bankFav, ctxBankSize(s_perf));
        soundtab_render_page(s_perf, 0);
    }
    if (s_fav_built) fav_render_page(0);   // repagina sobre los favoritos del nuevo synth

    uint32_t tab = lv_tabview_get_tab_active(s_tabview);
    if (tab == 1)      activate_sound_mode(false);
    else if (tab == 2) activate_sound_mode(true);
}

void uiBankNeoKey(uint8_t slot) {
    if (!s_open || !s_tabview || slot >= UIBANK_GRID_PER_PAGE) return;
    uint32_t tab = lv_tabview_get_tab_active(s_tabview);

    if (tab == 0) {
        int matches[UIBANK_FAV_SLOTS];
        int matchCount = fav_collect_matches(matches, UIBANK_FAV_SLOTS);
        int matchIdx = s_fav_curPage * UIBANK_GRID_PER_PAGE + slot;
        if (matchIdx >= matchCount) return;
        int realSlot = matches[matchIdx];
        FavEntry e;
        if (favLoad(realSlot, e)) { s_fav_last_slot = (uint8_t)realSlot; apply_recall(e); fav_render_page(s_fav_curPage); }
    } else if (tab == 1) {
        int pc = s_son.curPage * UIBANK_GRID_PER_PAGE + slot;
        if (pc < ctxBankSize(s_son)) soundtab_click_pc(s_son, (uint8_t)pc);
    } else if (tab == 2) {
        int pc = s_perf.curPage * UIBANK_GRID_PER_PAGE + slot;
        if (pc < ctxBankSize(s_perf)) soundtab_click_pc(s_perf, (uint8_t)pc);
    }
}

void uiBankNeoPage(int8_t dir) {
    if (!s_open || !s_tabview) return;
    uint32_t tab = lv_tabview_get_tab_active(s_tabview);

    if (tab == 0)      fav_render_page((int)s_fav_curPage + dir);
    else if (tab == 1) soundtab_render_page(s_son,  (int)s_son.curPage  + dir);
    else if (tab == 2) soundtab_render_page(s_perf, (int)s_perf.curPage + dir);
}

static void resetCtxRuntime(SoundTabCtx& ctx) {
    const BankDef* banks = ctxBanks(ctx);
    if (banks) { ctx.msb = banks[0].msb; ctx.lsb = banks[0].lsb; }
    ctx.tabRef = ctx.gridCont = nullptr;
    for (int i = 0; i < UIBANK_MAX_BANKS; i++) ctx.bankBtns[i] = nullptr;
    ctx.curPage = 0;
    ctx.built = false;
    memset(ctx.bankFav, 0, sizeof(ctx.bankFav));
    ctx.lastMsb = ctx.lastLsb = ctx.lastPc = 0xFF;
}

void uiBankDestroy() {
    if (s_cont) { lv_obj_delete(s_cont); s_cont = NULL; }
    s_tabview = s_tab_fav = s_tab_son = s_tab_perf = NULL;
    s_lbl_synth = NULL;
    s_box_synth = NULL;
    s_fav_cont = NULL;
    s_fav_selected_btn = NULL;
    s_fav_last_slot = 0xFF;
    s_fav_curPage = 0;
    s_fav_built = false;
    resetCtxRuntime(s_son);
    resetCtxRuntime(s_perf);
    s_modeSent = false;
    s_lastSentPerf = false;
    s_lastSentSynth = ExSynth::JV2080;
    s_open = false;
    g_bankOpen = false;
}
