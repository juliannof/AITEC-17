#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "config.h"
#include "RS485/RS485.h"
#include "S3Link/S3Link.h"
#include "midi/MIDIProcessor.h"
#include "display/Display.h"
#include "display/UIPage1.h"
#include "display/UIPage3.h"
#include "display/UIOffline.h"
#include "display/UIHeader.h"
#include "display/UIVPotPopup.h"
#include <LittleFS.h>

#include <Preferences.h>




USBMIDI MIDI;

volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;
uint8_t g_logicConnected = 0;
uint8_t vpotValues[16] = {};

String trackNames[16];
bool recStates[16]    = {}, soloStates[16] = {};
bool muteStates[16]   = {}, selectStates[16] = {};
float vuLevels[16]    = {};
bool vuClipState[16]  = {};
unsigned long vuLastUpdateTime[16]     = {};
float vuPeakLevels[16]                 = {};
unsigned long vuPeakLastUpdateTime[16] = {};
uint8_t  vuPeakAlpha[16]               = {};
uint32_t vuPeakFadeTime[16]            = {};
bool vuDirty[16]                       = {};
float faderPositions[16]               = {};
bool needsTOTALRedraw    = false;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsTimecodeRedraw = true;
bool needsButtonsRedraw  = true;
bool needsVUMetersRedraw = true;String assignmentString  = "--";
bool btnStatePG1[BTN_PG1_COUNT] = {};
bool btnFlashPG1[BTN_PG1_COUNT] = {};
bool rudeSoloActive = false;
bool cycleActive    = false;
bool g_clickActive  = false;
char timeCodeChars_clean[13] = {};
char beatsChars_clean[13]    = {};
DisplayMode currentTimecodeMode = MODE_BEATS;

TaskHandle_t taskCore0Handle = NULL;
TaskHandle_t taskCore1Handle = NULL;

uint8_t g_savedBrightness = 80;

extern void handleVUMeterDecay();

volatile bool g_switchToPage3 = false;
volatile uint8_t g_currentPage   = 0;
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

        // Enlace serie hacia S3: aplica canales recibidos + heartbeat (2026-08-16)
        s3Link.update();

        checkUsbLink();  // detección física real vía tud_mounted() (2026-08-18)
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
            else                         uiPage3Create(displayGetContentArea());

        } else if (g_switchToPage1) {
            g_switchToPage1 = false;
            log_e("[Task] switchToPage1 currentPage=%d", g_currentPage);

            if (g_currentPage == 0) uiPage3Destroy();
            g_currentPage = 1;
            uiHeaderEnsureCreated(displayGetRoot());
            uiPage1Create(displayGetContentArea());

        } else if (g_switchToPage3A) {
            g_switchToPage3A = false;
            if (g_currentPage == 1) uiPage1Destroy();
            g_currentPage = 0;
            uiHeaderEnsureCreated(displayGetRoot());
            uiPage3Create(displayGetContentArea());

        } else if (g_switchToOffline) {
            g_switchToOffline = false;
            if      (g_currentPage == 1) uiPage1Destroy();
            else                         uiPage3Destroy();
            uiHeaderDestroy();
            uiOfflineCreate(displayGetRoot());

        } else if (logicConnectionState == ConnectionState::DISCONNECTED) {
            uiOfflineTick();

        } else if (logicConnectionState == ConnectionState::CONNECTED) {
            handleVUMeterDecay();
            uiHeaderUpdate();
            if (g_currentPage == 1) uiPage1Update();
            else                    uiPage3Update();
            uiVPotPopupUpdate();   // sincroniza el pop-up del V-Pot si está abierto
        }

        static uint32_t lastTick = 0;
        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
        lv_tick_inc(lastTick ? nowMs - lastTick : 10);
        lastTick = nowMs;
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
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

    // 1. USB (primero — evita perder el handshake si Logic ya está abierto
    // cuando arranca el P4; antes iba detrás de LittleFS/Display y esa
    // ventana retrasaba USB varios cientos de ms, igual que en S3) (2026-07-26)
    log_i("1. USB.begin()...");
    USB.begin();
    delay(100);  // Solo 100ms
    log_i("   USB OK");

    // 2. MIDI (sin delay largo)
    log_i("2. MIDI.begin()...");
    MIDI.begin();
    log_i("   MIDI OK");

    // 3. LittleFS
    log_i("3. LittleFS.begin()...");
    if (!LittleFS.begin(false)) {
        log_e("   LittleFS FALLO");
    } else {
        log_i("   LittleFS OK");
    }

    // 4. Display + LVGL
    log_i("4. initDisplay()...");
    initDisplay();
    {
        Preferences bprefs;
        bprefs.begin("uimenu", true);
        uint8_t brightness = bprefs.getUChar("brightness", 80);
        bprefs.end();
        displaySetBrightness(brightness);
    }
    log_i("   Display OK");

    // 5. Preferences
    log_i("5. Preferences...");
    Preferences prefs;
    prefs.begin("uimenu", true);
    g_currentPage = prefs.getUChar("lastPage", 0);
    prefs.end();
    log_i("   Preferences OK. lastPage=%d", g_currentPage);

    // 6. UI Offline inicial
    log_i("6. uiOfflineCreate()...");
    uiOfflineCreate(displayGetRoot());
    log_i("   UI Offline OK");

    // 7. Timecode buffers
    memset(timeCodeChars_clean, ' ', 12); timeCodeChars_clean[12] = '\0';
    memset(beatsChars_clean,   ' ', 12); beatsChars_clean[12]   = '\0';

    // 8. RS485
    log_i("7. RS485.begin(%d)...", NUM_SLAVES);
    rs485.begin(NUM_SLAVES);
    rs485.startTask();
    log_i("   RS485 OK — TX:%d RX:%d EN:%d",
          RS485_TX_PIN, RS485_RX_PIN, RS485_ENABLE_PIN);

    // 7b. Enlace serie hacia S3 (2026-08-16)
    log_i("7b. s3Link.begin()...");
    s3Link.begin();
    log_i("   S3Link OK — TX:%d RX:%d", S3LINK_TX_PIN, S3LINK_RX_PIN);

    // 9. Crear tareas
    log_i("8. Creando tareas...");
    xTaskCreatePinnedToCore(taskCore0, "MIDI", 8192, NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "UI", 16384, NULL, 1, &taskCore1Handle, 1);
    log_i("   Tareas creadas");

    log_i("=== P4 Master ACTIVO. Slaves: %d ===", NUM_SLAVES);
}

void loop() { vTaskDelay(portMAX_DELAY); }