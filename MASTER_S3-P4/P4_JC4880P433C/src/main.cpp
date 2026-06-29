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

TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

// ── Core 0 — periféricos (NeoTrellis en v2) ──────────────────────────
void taskCore0(void* pv) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Core 1 — UI LVGL ─────────────────────────────────────────────────
void taskCore1(void* pv) {
    static bool uiReady = false;

    for (;;) {
        if (!g_bootDone) {
            uiBootTick();
        } else if (!uiReady) {
            uiBootDestroy();
            uiKaossCreate(displayGetRoot());
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

    initDisplay();
    displaySetBrightness(80);
    log_i("Display OK — 800x480 landscape");

    uiBootCreate(displayGetRoot());

    USB.productName("ExPressif V1");
    USB.manufacturerName("AITEC");
    USB.begin();
    MIDI.begin();
    log_i("USB MIDI OK");

    xTaskCreatePinnedToCore(taskCore0, "Periph", 4096,  NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "UI",    16384,  NULL, 1, &taskCore1Handle, 1);

    log_i("=== ExPressif ACTIVO ===");
}

void loop() { vTaskDelay(portMAX_DELAY); }
