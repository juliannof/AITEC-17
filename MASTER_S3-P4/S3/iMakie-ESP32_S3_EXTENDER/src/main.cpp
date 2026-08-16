#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "tusb.h"
#include "config.h"
#include "midi/MIDIProcessor.h"
#include "RS485/RS485.h"
#include "hardware/Transporte.h"
#include <Adafruit_NeoPixel.h>
#include "esp_system.h"

// ====================================================================
// --- MIDI ---
// ====================================================================
USBMIDI MIDI;

// ====================================================================
// --- Boot LED (2026-05-16 19:50) ---
// ====================================================================
extern Adafruit_NeoPixel pixels;  // Definido en RS485.cpp
static uint32_t bootLEDTime = 0;  // Timestamp cuando encender LED verde

// ====================================================================
// --- ESTADO GLOBAL ---
// ====================================================================
volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;
uint8_t g_logicConnected = 0;

// --- Redraw flags (stubs — sin pantalla) ---
bool needsTOTALRedraw    = false;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsVUMetersRedraw = false;
bool needsButtonsRedraw  = false;
bool needsTimecodeRedraw = false;

// --- UI flags (stubs) ---
volatile bool g_switchToOffline = false;
volatile bool g_switchToPage3   = false;

// --- Timecode (stubs) ---
DisplayMode currentTimecodeMode = MODE_BEATS;
char timeCodeChars_clean[13]    = {};
char beatsChars_clean[13]       = {};

// --- Estados de canales ---
bool recStates[9]    = {false};
bool soloStates[9]   = {false};
bool muteStates[9]   = {false};
bool selectStates[9] = {false};

float vuLevels[9]                    = {0};
float vuPeakLevels[9]                = {0};
bool  vuClipState[9]                 = {false};
unsigned long vuLastUpdateTime[9]    = {0};
unsigned long vuPeakLastUpdateTime[9]= {0};
float faderPositions[9]              = {0};

// --- Botones (stubs — sin NeoTrellis) ---
bool btnStatePG1[32]  = {false};
bool btnStatePG2[32]  = {false};
bool btnFlashPG1[32]  = {false};
bool btnFlashPG2[32]  = {false};

// --- Track info ---
String trackNames[8];
String assignmentString = "--";
uint8_t vpotValues[8]   = {0};

// --- Handles de tareas ---
TaskHandle_t taskCore0Handle = nullptr;
TaskHandle_t taskCore1Handle = nullptr;

// ====================================================================
// --- HELPER RS485 → MIDI ---
// ====================================================================
static void processSlaveResponse(uint8_t slaveId) {
    // Gate de conexión (2026-08-07): simétrico al gate ya existente en la entrada
    // (MIDIProcessor.cpp, PitchBend/CC "Gate de conexión 2026-08-02"). Sin esto, el
    // feedback de fader/touch/botones sigue saliendo hacia Logic aunque la conexión
    // esté cayéndose o cerrada (ej. el S2 bajando a 0 por desconexión) — puede
    // colarse en Logic durante una renegociación de handshake y volver como si
    // fuera un movimiento real del usuario.
    if (logicConnectionState != ConnectionState::CONNECTED) return;

    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    // --- Fader → Pitch Bend ---
    // Touch → SELECT MIDI: rising edge = Note On, falling = Note Off (2026-05-27)
    // Mismo comportamiento que botón físico SELECT — S3 es punto único de conversión
    static uint8_t _prevTouch[9] = {0};
    if (ch.touchState != _prevTouch[slaveId]) {
        log_w("[FADER→LOGIC] slave=%d touchState=%d faderPos=%d", slaveId, ch.touchState, ch.faderPos);
        _prevTouch[slaveId] = ch.touchState;
        uint8_t note = 24 + midiCh;
        byte selMsg[3] = { (byte)(ch.touchState ? 0x90 : 0x80), note, (byte)(ch.touchState ? 127 : 0) };
        sendMIDIBytes(selMsg, 3);
    }

    // Reporte de posición decidido por el propio S2 (2026-08-14) — S3 es transparente,
    // no almacena heurística propia (antes: lastSentPb[]/motorSettled/FADER_SYNC_DEADBAND
    // comparando contra faderTarget, roto en AUTO_WRITE porque faderTarget queda
    // congelado con el motor inhibido). Ver S2/S2_V1/src/RS485/RS485Handler.cpp
    // buildResponse() y protocol.h SLAVE_FLAG_REPORT_FADER.
    if (ch.buttons & SLAVE_FLAG_REPORT_FADER) {
        // faderPos ya llega en PitchBend 0-16383 — el S2 mapea localmente con su rango
        // calibrado (2026-07-20). El S3 solo transporta, sin recalcular nada. El snap a
        // extremos ya no hace falta: el S2 satura su propio rango y entrega 0/16383 exactos.
        uint16_t pb = ch.faderPos;
        if (pb > 16383) pb = 16383;  // clamp defensivo

        log_i("[FADER→LOGIC] SEND pb=%d touch=%d (pos=%d)", pb, ch.touchState, ch.faderPos);
        byte msg[3] = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
        sendMIDIBytes(msg, 3);
    }

    // --- Botones → Note On/Off ---
    uint8_t changed = ch.buttons ^ ch.prevButtons;
    if (changed) {
        const uint8_t noteBase[4] = { 0, 8, 16, 24 };
        for (uint8_t bit = 0; bit < 4; bit++) {
            if (changed & (1 << bit)) {
                bool    isOn = (ch.buttons & (1 << bit)) != 0;
                uint8_t note = noteBase[bit] + midiCh;
                uint8_t vel  = isOn ? 127 : 0;
                byte msg[3]  = { (byte)(isOn ? 0x90 : 0x80), note, vel };
                sendMIDIBytes(msg, 3);
            }
        }
    }

    // --- Encoder → CC ---
    if (ch.encoderDelta != 0) {
        uint8_t cc  = 16 + midiCh;
        uint8_t val;

        if (ch.encoderDelta > 0) {
            // CW: valores 1-62
            val = constrain((uint8_t)ch.encoderDelta, 1, 62);
        } else {
            // CCW: valores 64-127 (64 + ticks)
            val = 64 + constrain((uint8_t)(-ch.encoderDelta), 1, 64);
        }

        byte msg[3] = { (byte)(0xB0 | midiCh), cc, val };
        sendMIDIBytes(msg, 3);
    }
}

// ====================================================================
// --- TAREA CORE 0 — MIDI + RS485 ---
// ====================================================================
void taskCore0(void* pvParameters) {
    log_e("MIDI task arrancando en Core %d", xPortGetCoreID());
    static unsigned long lastStatusLog = 0;  // ← MOVER AQUÍ

    for (;;) {
        // ── Apagar LED verde después de 200ms (2026-05-16 21:30) ──
        if (bootLEDTime > 0 && millis() - bootLEDTime > 200) {
            pixels.setPixelColor(0, pixels.Color(0, 0, 0));  // Apagar
            pixels.show();
            bootLEDTime = 0;  // Reset
        }

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

        // Esperar a que DISCONNECT SEQUENCE se complete antes de cambiar a offline
        if (g_switchToOffline && rs485.isDisconnectComplete()) {
            g_switchToOffline = false;
            log_i("[MAIN] Desconexión completada — todos los slaves en DISCONNECTED");
            // Aquí iría el cambio de UI a offline (cuando se implemente pantalla)
        }

        // tickCalibracion gestiona calibración post-conexión (disparada por SysEx 0x21)
        tickCalibracion();

        // tickTrackNameDebounce aplica nombres de pista pendientes tras la ventana
        // anti-flash (2026-08-13 15:10) — ver MIDIProcessor.cpp case 0x12
        tickTrackNameDebounce();

        // VU timeout — Logic deja de enviar Channel Pressure cuando no hay audio.
        // S3 mantiene el último vuLevel indefinidamente → S2 nunca decae.
        // Fix: reset a 0 si no llega Channel Pressure en >200ms. (2026-05-26)
        static uint32_t lastVuTimeoutCheck = 0;
        if (millis() - lastVuTimeoutCheck > 50) {
            lastVuTimeoutCheck = millis();
            for (int i = 0; i < NUM_SLAVES; i++) {
                if (vuLevels[i] > 0.0f && millis() - vuLastUpdateTime[i] > 200) {
                    vuLevels[i] = 0.0f;
                    rs485.setVuLevel(i + 1, 0);
                }
            }
        }

        // ← LOG DE ESTADO (DENTRO DEL LOOP):
        if (millis() - lastStatusLog > 2000) {
            lastStatusLog = millis();
            
            const char* stateStr = "UNKNOWN";
            if (logicConnectionState == ConnectionState::DISCONNECTED) stateStr = "DISCONNECTED";
            else if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) stateStr = "HANDSHAKE_OK";
            else if (logicConnectionState == ConnectionState::CONNECTED) stateStr = "CONNECTED";
            
            log_v("[STATUS] %s | g_logicConnected=%d", stateStr, g_logicConnected);

            // Diagnóstico temporal (2026-08-16) — resumen de los 8 modos consolidado
            // en una sola línea, para verlos de un vistazo sin rastrear eventos
            // dispersos. Nombres en orden del enum interno AutoMode. Quitar tras validar.
            extern uint8_t g_channelAutoMode[8];
            static const char* internalNames[6] = {"OFF","READ","WRITE","TRIM","TOUCH","LATCH"};
            log_i("[AUTOMODE-ALL] ch0-7: %s %s %s %s %s %s %s %s",
                  internalNames[g_channelAutoMode[0] < 6 ? g_channelAutoMode[0] : 0],
                  internalNames[g_channelAutoMode[1] < 6 ? g_channelAutoMode[1] : 0],
                  internalNames[g_channelAutoMode[2] < 6 ? g_channelAutoMode[2] : 0],
                  internalNames[g_channelAutoMode[3] < 6 ? g_channelAutoMode[3] : 0],
                  internalNames[g_channelAutoMode[4] < 6 ? g_channelAutoMode[4] : 0],
                  internalNames[g_channelAutoMode[5] < 6 ? g_channelAutoMode[5] : 0],
                  internalNames[g_channelAutoMode[6] < 6 ? g_channelAutoMode[6] : 0],
                  internalNames[g_channelAutoMode[7] < 6 ? g_channelAutoMode[7] : 0]);
        }
        
        vTaskDelay(1);
    }
}

// ====================================================================
// --- TAREA CORE 1 — TRANSPORTE ---
// ====================================================================
void taskCore1(void* pvParameters) {
    for (;;) {
        Transporte::update();
        vTaskDelay(10);
    }
}

// ====================================================================
// --- DIAGNÓSTICO — motivo del último reset (2026-08-02) ──────────
// WHY: investigar resincronizaciones MCU repetidas (~37s) vistas en
// MIDI Monitor — confirmar si el S3 se reinicia físicamente (brownout,
// watchdog, panic) o si es Logic quien redispara el handshake por su
// cuenta sin que el S3 haya rebooteado.
// ====================================================================
static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT_PIN";
        case ESP_RST_SW:        return "SW_RESET";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "OTHER_WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

// ====================================================================
// --- SETUP ---
// ====================================================================
void setup() {
    randomSeed(esp_random());
    Serial.begin(115200);
    log_i("=== BOOT S3-02 Extender ===");
    log_e("[BOOT] Reset reason: %s (t=%lu ms desde power-on)",
          resetReasonStr(esp_reset_reason()), millis());

    // 1. USB
    log_i("1. USB.begin()...");
    USB.begin();
    delay(100);
    log_i("   USB OK");

    // 2. Transporte
    log_i("2. Transporte::begin()...");
    Transporte::begin();
    log_i("   Transporte OK");

    // 3. RS485
    log_i("3. rs485.begin(%d)...", NUM_SLAVES);
    rs485.begin(NUM_SLAVES);
    log_i("   RS485 OK. Slaves: %d", NUM_SLAVES);

    // 4. MIDI (sin delay largo)
    log_i("4. MIDI.begin()...");
    MIDI.begin();
    log_i("   MIDI OK");

    // 5. Info PSRAM
    log_i("PSRAM: %d bytes total, %d bytes libre",
          ESP.getPsramSize(), ESP.getFreePsram());

    // 6. Crear tareas
    log_i("5. Creando tareas...");
    xTaskCreatePinnedToCore(taskCore0, "MIDI", 4096, NULL, 2, &taskCore0Handle, 0);
    xTaskCreatePinnedToCore(taskCore1, "TRANSP", 4096, NULL, 1, &taskCore1Handle, 1);
    rs485.startTask();
    log_i("   Tareas creadas");

    // ── LED verde por 1s (no bloqueante) (2026-05-16 19:50) ──
    pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Verde
    pixels.show();
    bootLEDTime = millis();  // Guardar timestamp

    log_i("=== S3-02 Extender ACTIVO. Slaves: %d ===", NUM_SLAVES);
}

void loop() { vTaskDelay(portMAX_DELAY); }