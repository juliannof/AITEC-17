#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "config.h"
#include "RS485/RS485.h"
#include "midi/MIDIProcessor.h"
#include "display/Display.h"
#include "display/UIPage1.h"
#include "display/UIPage3.h"
#include "display/UIPage3B.h"                                                                                  
#include "display/UIOffline.h"
#include "display/UIHeader.h"
#include <LittleFS.h>

#include <Preferences.h>




USBMIDI MIDI;

volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;
uint8_t g_logicConnected = 0;
uint8_t vpotValues[8] = {};

String trackNames[9];
bool recStates[8]    = {}, soloStates[8] = {};
bool muteStates[8]   = {}, selectStates[8] = {};
float vuLevels[9]    = {};
bool vuClipState[9]  = {};
unsigned long vuLastUpdateTime[9]     = {};
float vuPeakLevels[9]                 = {};
unsigned long vuPeakLastUpdateTime[9] = {};
float faderPositions[9]               = {};
bool needsTOTALRedraw    = false;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsTimecodeRedraw = true;
bool needsButtonsRedraw  = true;
bool needsVUMetersRedraw = true;String assignmentString  = "--";
bool btnStatePG1[32] = {}, btnStatePG2[32] = {};
bool btnFlashPG1[32] = {}, btnFlashPG2[32] = {};
char timeCodeChars_clean[13] = {};
char beatsChars_clean[13]    = {};
DisplayMode currentTimecodeMode = MODE_BEATS;

TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

uint8_t g_savedBrightness = 80;

extern void handleVUMeterDecay();

volatile bool g_switchToPage3 = false;
volatile uint8_t g_currentPage   = 0;
volatile bool g_switchToPage3B   = false;
volatile bool g_switchToOffline = false;
volatile bool g_switchToPage3A = false;
volatile bool g_switchToPage1 = false;


void updateLeds() {}

static void processSlaveResponse(uint8_t slaveId) {
    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    if (ch.touchState) {
        uint16_t pb  = ch.faderPos;
        byte msg[3]  = { (byte)(0xE0 | midiCh),
                         (byte)(pb & 0x7F),
                         (byte)(pb >> 7) };
        sendMIDIBytes(msg, 3);
    }

    uint8_t changed = ch.buttons ^ ch.prevButtons;
    if (changed) {
        const uint8_t noteBase[4] = { 0, 8, 16, 24 };
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (changed & (1 << bit)) {
                bool isOn    = (ch.buttons & (1 << bit)) != 0;
                uint8_t note = noteBase[bit] + midiCh;
                uint8_t vel  = isOn ? 127 : 0;
                byte msg[3]  = { (byte)(isOn ? 0x90 : 0x80), note, vel };
                sendMIDIBytes(msg, 3);
            }
        }
    }

    if (ch.encoderDelta != 0) {
        uint8_t cc  = 16 + midiCh;
        uint8_t val = (ch.encoderDelta > 0) ? 65 : 63;
        byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
        sendMIDIBytes(msg, 3);
    }
}

void taskCore0(void* pvParameters) {
    log_e("MIDI task en Core %d", xPortGetCoreID());
    for (;;) {
        uint8_t rx_buf[64];
        uint32_t count = tud_midi_stream_read(rx_buf, sizeof(rx_buf));
        if (count > 0) {
            for (uint32_t i = 0; i < count; i++)
                processMidiByte(rx_buf[i]);
        }

        if (logicConnectionState == ConnectionState::CONNECTED) {
            for (uint8_t id = 1; id <= NUM_SLAVES; id++) {
                if (rs485.hasNewSlaveData(id))
                    processSlaveResponse(id);
            }
        }

        tickCalibracion();
        // checkMidiTimeout();
        vTaskDelay(1);
    }
}

void taskCore1(void* pvParameters) {
    for (;;) {
        if (g_switchToPage3) {
            g_switchToPage3 = false;
            uiOfflineDestroy();
            uiHeaderEnsureCreated(displayGetRoot());
            if      (g_currentPage == 1) uiPage1Create(displayGetContentArea());
            else if (g_currentPage == 2) uiPage3BCreate(displayGetContentArea());
            else                         uiPage3Create(displayGetContentArea());

        } else if (g_switchToPage1) {
            g_switchToPage1 = false;
            log_e("[Task] switchToPage1 currentPage=%d", g_currentPage);

            if      (g_currentPage == 0) uiPage3Destroy();
            else if (g_currentPage == 2) uiPage3BDestroy();
            g_currentPage = 1;
            uiHeaderEnsureCreated(displayGetRoot());
            uiPage1Create(displayGetContentArea());

        } else if (g_switchToPage3A) {
            g_switchToPage3A = false;
            if      (g_currentPage == 1) uiPage1Destroy();
            else if (g_currentPage == 2) uiPage3BDestroy();
            g_currentPage = 0;
            uiHeaderEnsureCreated(displayGetRoot());
            uiPage3Create(displayGetContentArea());

        } else if (g_switchToPage3B) {
            g_switchToPage3B = false;
            if      (g_currentPage == 1) uiPage1Destroy();
            else if (g_currentPage == 0) uiPage3Destroy();
            g_currentPage = 2;
            uiHeaderEnsureCreated(displayGetRoot());
            uiPage3BCreate(displayGetContentArea());

        } else if (g_switchToOffline) {
            g_switchToOffline = false;
            if      (g_currentPage == 1) uiPage1Destroy();
            else if (g_currentPage == 2) uiPage3BDestroy();
            else                         uiPage3Destroy();
            uiHeaderDestroy();
            uiOfflineCreate(displayGetRoot());

        } else if (logicConnectionState == ConnectionState::DISCONNECTED) {
            uiOfflineTick();

        } else if (logicConnectionState == ConnectionState::CONNECTED) {
            handleVUMeterDecay();
            uiHeaderUpdate();
            if      (g_currentPage == 1) uiPage1Update();
            else if (g_currentPage == 2) uiPage3BUpdate();
            else                         uiPage3Update();
        }

        lv_tick_inc(10);
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    static unsigned long lastStatusLog = 0;
    // Log de estado cada 2 segundos
        if (millis() - lastStatusLog > 2000) {
            lastStatusLog = millis();
            
            const char* stateStr = "UNKNOWN";
            if (logicConnectionState == ConnectionState::DISCONNECTED) stateStr = "DISCONNECTED";
            else if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) stateStr = "HANDSHAKE_OK";
            else if (logicConnectionState == ConnectionState::CONNECTED) stateStr = "CONNECTED";
            
            log_i("[STATUS] %s | g_logicConnected=%d | Page=%d", 
                  stateStr, g_logicConnected, g_currentPage);
        }
}



void setup() {
    randomSeed(esp_random());  // ← AÑADIR al principio
    Serial.begin(115200);
    log_i("=== BOOT P4 Master ===");

    // 1. LittleFS
    log_i("1. LittleFS.begin()...");
    if (!LittleFS.begin(false)) {
        log_e("   LittleFS FALLO");
    } else {
        log_i("   LittleFS OK");
    }

    // 2. Display + LVGL
    log_i("2. initDisplay()...");
    initDisplay();
    {
        Preferences bprefs;
        bprefs.begin("uimenu", true);
        uint8_t brightness = bprefs.getUChar("brightness", 80);
        bprefs.end();
        displaySetBrightness(brightness);
    }
    log_i("   Display OK");

    // 3. Preferences
    log_i("3. Preferences...");
    Preferences prefs;
    prefs.begin("uimenu", true);
    g_currentPage = prefs.getUChar("lastPage", 0);
    prefs.end();
    log_i("   Preferences OK. lastPage=%d", g_currentPage);

    // 4. UI Offline inicial
    log_i("4. uiOfflineCreate()...");
    uiOfflineCreate(displayGetRoot());
    log_i("   UI Offline OK");

    // 5. Timecode buffers
    memset(timeCodeChars_clean, ' ', 12); timeCodeChars_clean[12] = '\0';
    memset(beatsChars_clean,   ' ', 12); beatsChars_clean[12]   = '\0';

    // 6. USB
    log_i("5. USB.begin()...");
    USB.begin();
    delay(100);  // Solo 100ms
    log_i("   USB OK");

    // 7. MIDI (sin delay largo)
    log_i("6. MIDI.begin()...");
    MIDI.begin();
    log_i("   MIDI OK");

    // 8. RS485
    log_i("7. RS485.begin(%d)...", NUM_SLAVES);
    rs485.begin(NUM_SLAVES);
    log_i("   RS485 OK — TX:%d RX:%d EN:%d", 
          RS485_TX_PIN, RS485_RX_PIN, RS485_ENABLE_PIN);

    // 9. Crear tareas
    log_i("8. Creando tareas...");
    xTaskCreatePinnedToCore(taskCore0, "MIDI", 4096, NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "UI", 16384, NULL, 1, &taskCore1Handle, 1);
    log_i("   Tareas creadas");

    log_i("=== P4 Master ACTIVO. Slaves: %d ===", NUM_SLAVES);
}

void loop() { vTaskDelay(portMAX_DELAY); }