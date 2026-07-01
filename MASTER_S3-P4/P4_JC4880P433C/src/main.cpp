// main.cpp — ExPressif v1.0  (AITEC 2026-06-29)
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <LittleFS.h>
#include "config.h"
#include "midi/MIDIOut.h"
#include "display/Display.h"
#include "display/UIBoot.h"
#include "display/UIKaoss.h"
#include "kaoss/KaossPad.h"
#include "neotrellis/NeoTrellis.h"
#include "display/UIBank.h"
#include "nvs/FavStore.h"

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
volatile bool    g_trellis_nextPreset = false;
volatile bool    g_trellis_panic      = false;
volatile int8_t  g_trellis_setPreset  = -1;
volatile bool    g_trellis_nextSynth  = false;
volatile bool    g_trellis_openBank   = false;
volatile int8_t  g_trellis_bankSlot   = -1;

// ── Sintetizador activo ───────────────────────────────────────────────
volatile ExSynth g_currentSynth = ExSynth::JV2080;

// ── Canal MIDI y estado Bank ──────────────────────────────────────────
volatile uint8_t g_midiChannel = 1;
volatile bool    g_bankOpen    = false;
volatile uint8_t g_bankTab     = 0;

TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

// ── Core 0 — periféricos ──────────────────────────────────────────────
void taskCore0(void* pv) {
    for (;;) {
        neotrellisUpdate();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ── Core 1 — UI LVGL ─────────────────────────────────────────────────
void taskCore1(void* pv) {
    static bool uiReady = false;

    for (;;) {
        // ── Flags NeoTrellis (pueden llegar en cualquier momento) ────────
        if (g_trellis_panic) {
            g_trellis_panic = false;
            sendCC(MIDI_CH, kaoss.getCCX(), 64);
            sendCC(MIDI_CH, kaoss.getCCY(), 64);
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

        if (uiReady) {
            if (g_trellis_holdToggle) {
                g_trellis_holdToggle = false;
                g_holdMode = !g_holdMode;
                uiKaossUpdateHold();
            }
            if (g_trellis_nextPreset) {
                g_trellis_nextPreset = false;
                kaoss.nextPreset();
                uiKaossUpdateScale();
            }
            if (g_trellis_setPreset >= 0) {
                uint8_t target = (uint8_t)g_trellis_setPreset;
                g_trellis_setPreset = -1;
                kaoss.setPreset(target);
                uiKaossUpdateScale();
            }
            if (g_trellis_nextSynth) {
                g_trellis_nextSynth = false;
                g_currentSynth = (ExSynth)(((uint8_t)g_currentSynth + 1) % NUM_SYNTHS);
                uiKaossUpdateSynth();
            }
        }

        if (!g_bootDone) {
            uiBootTick();
        } else if (!uiReady) {
            uiBootDestroy();
            uiKaossCreate(displayGetRoot());
            uiBankCreate(displayGetRoot());   // oculto por defecto
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

    uint8_t savedCh;
    if (favLoadMidiChannel(savedCh)) g_midiChannel = savedCh;

    initDisplay();
    displaySetBrightness(80);
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
