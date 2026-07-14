// main.cpp — ExPressif v1.0  (AITEC 2026-06-29)
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <LittleFS.h>
#include "config.h"
#include "midi/MIDIOut.h"
#include "midi/MIDIClock.h"
#include "display/Display.h"
#include "display/UIBoot.h"
#include "display/UIKaoss.h"
#include "kaoss/KaossPad.h"
#include "neotrellis/NeoTrellis.h"
#include "display/UIBank.h"
#include "display/UIKaosEdit.h"
#include "display/UIBrightnessPopup.h"
#include "nvs/FavStore.h"
#include "nvs/KaosStore.h"

USBMIDI MIDI;

// ── Estado global ────────────────────────────────────────────────────
volatile ExMode  g_currentMode   = ExMode::KAOSS_XY;
volatile uint8_t g_currentScale  = SCALE_MAJOR;
volatile uint8_t g_rootNote      = 0;
volatile uint8_t g_currentOctave = OCTAVE_DEFAULT;
volatile bool    g_holdMode      = false;
volatile int16_t g_lastCCX       = -1;
volatile int16_t g_lastCCY       = -1;
volatile bool    g_touched       = false;
volatile bool    g_bootDone      = false;

// ── Flags NeoTrellis → LVGL (Core 0 → Core 1) ────────────────────────
volatile bool    g_trellis_holdToggle = false;
volatile bool    g_trellis_panic      = false;
volatile int8_t  g_trellis_setPreset  = -1;   // -1=none, 0-19=preset directo (2026-07-14)
volatile bool    g_trellis_nextSynth  = false;
volatile int8_t  g_trellis_setSynth   = -1;
volatile bool    g_trellis_openBank   = false;
volatile int8_t  g_trellis_bankSlot   = -1;
volatile bool    g_trellis_bankPrev   = false;   // columna 0 (2026-07-04)
volatile bool    g_trellis_bankNext   = false;   // columna 7 (2026-07-04)
volatile bool    g_trellis_brightDown = false;   // L2 (2026-07-14)
volatile bool    g_trellis_brightUp   = false;   // L6 (2026-07-14)

volatile uint8_t g_displayBrightness  = 80;      // 10-100%, ajustable con L2/L6 (2026-07-14)

// ── Sintetizador activo ───────────────────────────────────────────────
volatile ExSynth g_currentSynth = ExSynth::JV2080;

// ── Canal MIDI y estado Bank ──────────────────────────────────────────
// Canal fijo = 1 para todos los synths (2026-07-12) — Logic enruta por
// track, no el firmware. Ver UIBank.cpp:activate_sound_mode().
volatile uint8_t g_midiChannel = 1;
volatile bool    g_bankOpen    = false;
volatile uint8_t g_bankTab     = 0;

TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

// ── Core 0 — periféricos ──────────────────────────────────────────────
void taskCore0(void* pv) {
    for (;;) {
        midiClockPoll();   // metrónomo visual L5 (2026-07-14) — único lector de MIDI entrante
        neotrellisUpdate();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ── Core 1 — UI LVGL ─────────────────────────────────────────────────
void taskCore1(void* pv) {
    static bool uiReady = false;
    static uint32_t s_lastBankFlush = 0;   // último PC por banco (2026-07-13) — flush periódico, no por tap

    for (;;) {
        // Flush NVS de "último PC por banco" cada 60s SOLO si hay cambios
        // (bankLastSelFlushIfDirty() comprueba el dirty flag internamente) —
        // evita escribir flash en el camino del touch (ver FavStore.h).
        // También se hace al cerrar Bank (uiBankHide()); esto es el respaldo
        // por si la sesión de Bank se queda abierta mucho tiempo.
        uint32_t now = millis();
        if (now - s_lastBankFlush >= 60000) {
            s_lastBankFlush = now;
            bankLastSelFlushIfDirty();
        }

        // ── Flags NeoTrellis (pueden llegar en cualquier momento) ────────
        if (g_trellis_panic) {
            g_trellis_panic = false;
            if (kaoss.hasPreset()) {
                uint8_t ch = kaoss.getChannel();
                sendCC(ch, kaoss.getCCX(), 64);
                sendCC(ch, kaoss.getCCY(), 64);
            }
            g_lastCCX = 64;
            g_lastCCY = 64;
            g_touched = false;
        }
        // ── Flag Bank (abre/cierra sin importar uiReady) ─────────────────
        if (g_trellis_openBank) {
            g_trellis_openBank = false;
            if (uiReady) {
                if (uiBankIsOpen()) uiBankHide();
                else                uiBankShow();
            }
        }
        // ── Slot NeoTrellis en modo Bank ─────────────────────────────────
        if (g_trellis_bankSlot >= 0 && uiReady) {
            uint8_t k = (uint8_t)g_trellis_bankSlot;
            g_trellis_bankSlot = -1;
            uiBankNeoKey(k);
        }
        // ── Página anterior/siguiente en modo Bank (2026-07-04) ──────────
        if (g_trellis_bankPrev && uiReady) { g_trellis_bankPrev = false; uiBankNeoPage(-1); }
        if (g_trellis_bankNext && uiReady) { g_trellis_bankNext = false; uiBankNeoPage(+1); }

        if (uiReady) {
            if (g_trellis_holdToggle) {
                g_trellis_holdToggle = false;
                g_holdMode = !g_holdMode;
                uiKaossUpdateHold();
            }
            if (g_trellis_setPreset >= 0) {
                uint8_t target = (uint8_t)g_trellis_setPreset;
                g_trellis_setPreset = -1;
                kaoss.setPreset(target);
                uiKaossUpdatePreset();
            }
            if (g_trellis_nextSynth) {
                g_trellis_nextSynth = false;
                g_currentSynth = (ExSynth)(((uint8_t)g_currentSynth + 1) % NUM_SYNTHS);
                uiKaossUpdateSynth();
                uiBankSynthChanged();   // sincroniza banco (2026-07-04); refresca Sonidos/Performances/Favoritos si Bank está abierto (2026-07-12)
            }
            if (g_trellis_setSynth >= 0) {
                g_currentSynth = (ExSynth)(uint8_t)g_trellis_setSynth;
                g_trellis_setSynth = -1;
                uiKaossUpdateSynth();
                uiBankSynthChanged();   // selección directa L8,9,10,12,13,14 (2026-07-12)
            }
            if (g_trellis_brightDown) {   // L2 (2026-07-14)
                g_trellis_brightDown = false;
                g_displayBrightness = (g_displayBrightness > 20) ? (uint8_t)(g_displayBrightness - 10) : 10;
                displaySetBrightness(g_displayBrightness);
                uiBrightnessPopupShow(g_displayBrightness);
            }
            if (g_trellis_brightUp) {     // L6 (2026-07-14)
                g_trellis_brightUp = false;
                g_displayBrightness = (g_displayBrightness < 100) ? (uint8_t)(g_displayBrightness + 10) : 100;
                displaySetBrightness(g_displayBrightness);
                uiBrightnessPopupShow(g_displayBrightness);
            }
        }

        if (!g_bootDone) {
            uiBootTick();
        } else if (!uiReady) {
            uiBootDestroy();
            uiKaossCreate(displayGetRoot());
            uiBankCreate(displayGetRoot());          // oculto por defecto
            uiKaosEditCreate(displayGetRoot());      // oculto por defecto (2026-07-14)
            uiBrightnessPopupCreate(displayGetRoot()); // topmost, oculto por defecto (2026-07-14)
            uiKaossStartScroll();
            uiReady = true;
        }

        lv_tick_inc(10);
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Setup ─────────────────────────────────────────────────────────────
void setup() {
    log_i("=== BOOT ExPressif (AITEC) ===");

    if (!LittleFS.begin(false)) log_e("LittleFS FALLO");
    else                        log_i("LittleFS OK");

    if (!favInit()) log_e("FavStore FALLO");
    else            log_i("FavStore OK (%d favoritos)", favCount());

    if (!kaosInit()) log_e("KaosStore FALLO");
    else              log_i("KaosStore OK");
    kaoss.reload();   // carga el slot 0 del synth por defecto (2026-07-14) — si no,
                       // el botón PRESET arranca vacío hasta el primer toque NeoTrellis

    initDisplay();
    displaySetBrightness(g_displayBrightness);
    log_i("Display OK — 800x480 landscape");

    neotrellisInit();

    uiBootCreate(displayGetRoot());

    USB.productName("ExPressif V1");
    USB.manufacturerName("AITEC");
    USB.begin();
    MIDI.begin();
    log_i("USB MIDI OK");

    xTaskCreatePinnedToCore(taskCore0, "Periph", 8192,  NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "UI",    16384,  NULL, 1, &taskCore1Handle, 1);

    log_i("=== ExPressif ACTIVO ===");
}

void loop() { vTaskDelay(portMAX_DELAY); }
