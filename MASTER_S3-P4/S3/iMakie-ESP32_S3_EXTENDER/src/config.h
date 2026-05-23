#pragma once
#include <Arduino.h>

// ====================================================================
// CONFIGURACIÓN AUTOMÁTICA SEGÚN DISPOSITIVO
// ====================================================================
#if defined(DEVICE_P4_MASTER)
    #define DEVICE_FAMILY       0x14
    #define VERSION_REPLY_CMD   0x14
    #define NUM_SLAVES          9

#elif defined(DEVICE_S3_EXTENDER)
    #define DEVICE_FAMILY       0x14
    #define VERSION_REPLY_CMD   0x14
    #define NUM_SLAVES          1

#else
    #error "DEBE DEFINIR: DEVICE_P4_MASTER o DEVICE_S3_EXTENDER en platformio.ini build_flags"
#endif


// ====================================================================
// --- ConnectionState ---
// ====================================================================
enum class ConnectionState {
    DISCONNECTED,
    INITIALIZING,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

extern volatile ConnectionState logicConnectionState;

// ====================================================================
// --- RS485 ---
// ====================================================================
#define RS485_TX_PIN        15
#define RS485_RX_PIN        16
#define RS485_ENABLE_PIN     1
#define RS485_BAUD          500000


// --- Timing (µs) ---
#define RS485_TX_ENABLE_US   30      // ← aumentado: transceiver setup típico 30-50µs
#define RS485_TX_DONE_US     30      // ← aumentado: esperar a transceiver deshabilitar
#define RS485_RESP_TIMEOUT_US 3000   // ← aumentado: margen más generoso para ISR overhead
#define RS485_GAP_US         300
#define POLL_CYCLE_MS        10   // 100Hz con 1 slave — transacción ~3ms, margen suficiente (2026-05-19)

// --- Calibración (2026-05-16 19:25) ---
#define MAX_CALIBRATION_RETRIES     5   // máx reintentos RS485 timeout durante calibración
#define SLAVE_CALIB_SETTLE_RESPONSES 5  // respuestas estables antes de disparar auto-calib (2026-05-22)

// --- Fader Logic PitchBend (2026-05-18, confirmado MIDI monitor canal 2) ---
// signed: min=-8192 (raw 0), max=+6653 (raw 14845) → span = 6653 - (-8192) = 14845
#define LOGIC_PITCHBEND_MAX  14845

// --- NeoPixel Status LED (2026-05-16 19:40) ---
#define NEOPIXEL_PIN 48              // GPIO 48 (WS2812B RGB)
#define NEOPIXEL_COUNT 1             // 1 LED
#define NEOPIXEL_BRIGHTNESS 255      // 0-255

// ====================================================================
// --- TRANSPORTE ---
// ====================================================================
#define LED_REC   12
#define BTN_REC   11
#define LED_PLAY  10
#define BTN_PLAY   9
#define LED_FF     8
#define BTN_FF     7
#define LED_STOP   6
#define BTN_STOP   5
#define LED_RW     4
#define BTN_RW     3

// ====================================================================
// --- NOTAS MIDI TRANSPORTE ---
// ====================================================================
#define MIDI_NOTE_RW    0x5B
#define MIDI_NOTE_FF    0x5C
#define MIDI_NOTE_STOP  0x5D
#define MIDI_NOTE_PLAY  0x5E
#define MIDI_NOTE_REC   0x5F

// ====================================================================
// --- MACKIE CHAR MAP ---
// ====================================================================
static const char MACKIE_CHAR_MAP[64] = {
    ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_'
};

// ====================================================================
// --- NOTAS MCU PG1 / PG2 (requeridas por MIDIProcessor) ---
// ====================================================================
static const byte MIDI_NOTES_PG1[32] = {
    0x28, 0x2A, 0x2C, 0x29, 0x2B, 0x2D, 0x32, 0x33,
    0x4A, 0x4B, 0x4D, 0x4E, 0x4C, 0x4F, 0x57, 0x35,
    0x64, 0x65, 0x66, 0x54, 0x30, 0x31, 0x2E, 0x2F,
    0x51, 0x50, 0x46, 0x47, 0x48, 0x49, 0x53, 0x00
};

static const byte MIDI_NOTES_PG2[32] = {
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
    0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
    0x64, 0x65, 0x66, 0x54, 0x30, 0x31, 0x2E, 0x2F,
    0x4C, 0x50, 0x46, 0x47, 0x48, 0x49, 0x52, 0x00
};



// ====================================================================
// --- DisplayMode (requerido por MIDIProcessor) ---
// ====================================================================
enum class DisplayMode { BEATS, SMPTE };
#define MODE_BEATS DisplayMode::BEATS
#define MODE_SMPTE DisplayMode::SMPTE