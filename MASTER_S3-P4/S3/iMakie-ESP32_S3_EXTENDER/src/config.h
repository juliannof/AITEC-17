#pragma once
#include <Arduino.h>

// ====================================================================
// CONFIGURACIÓN AUTOMÁTICA SEGÚN DISPOSITIVO
// ====================================================================
#if defined(DEVICE_P4_MASTER)
    #define DEVICE_FAMILY       0x14
    #define VERSION_REPLY_CMD   0x14
    #define NUM_SLAVES          1

#elif defined(DEVICE_S3_EXTENDER)
    // FIX 2026-08-07: DEVICE_FAMILY/VERSION_REPLY_CMD estaban copiados de la rama
    // DEVICE_P4_MASTER (0x14) — el S3 se identificaba ante Logic con el MISMO byte
    // de familia que el P4, impidiendo que Logic distinga master de extender.
    // Corregido a 0x15, igual que la plantilla correcta en P4_JC1060P470C/src/config.h.
    //
    // EXPERIMENTO 2026-08-16: revertido a 0x14 a propósito — se confirmó en banco
    // que Logic nunca manda las notas 74-78 (AutoMode) al Extender (familia 0x15),
    // ni siquiera seleccionando el canal directamente en el S3 (descarta que sea un
    // tema de selección). Se prueba si identificándose como Main Unit (misma familia
    // que P4) Logic empieza a mandarle el AutoMode también. RIESGO: este mismo valor
    // ya causó, por error de copia, que "Logic no pudiera distinguir master de
    // extender" (ver arriba) — no se conoce el síntoma exacto de aquella vez. Si el
    // S3 deja de conectar bien, de mantener banking sincronizado con el P4, o de
    // recibir transport/LCD con normalidad, revertir a 0x15 inmediatamente.
    #define DEVICE_FAMILY       0x14
    #define VERSION_REPLY_CMD   0x14
    #define NUM_SLAVES          8   // TESTING= 61 a 8 | PRODUCCIÓN=8 — no cambiar aquí sin hardware real

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
#define RS485_BAUD          500000   // probado 250000 (2026-08-13): mismos CRC/ID MISMATCH bajo carga de motor — no era problema de velocidad, revertido

// ====================================================================
// --- Enlace serie S3↔P4 (Serial2, independiente del RS485 propio) (2026-08-16) ---
// ====================================================================
#define S3LINK_TX_PIN        18
#define S3LINK_RX_PIN        17
#define S3LINK_BAUD          115200


// --- Timing (µs) ---
#define RS485_TX_ENABLE_US   30      // ← aumentado: transceiver setup típico 30-50µs
#define RS485_TX_DONE_US     30      // ← aumentado: esperar a transceiver deshabilitar
#define RS485_RESP_TIMEOUT_US 8000   // ← 8ms: margen extra para loop S2 variable (2026-05-30, era 5ms)
#define RS485_GAP_US         300
#define POLL_CYCLE_MS        20   // Tiempo mínimo de ronda completa (todos los slaves).
                                  // Con 1 slave (testing): 20ms → 50Hz, da margen al loop S2 (~10ms worst-case).
                                  // Con 8 slaves (producción): ronda natural ~14ms > 20ms → este valor se ignora,
                                  // cada slave se actualiza a ~71Hz sin espera adicional. (2026-05-23)

// --- Calibración (2026-05-16 19:25) ---
#define MAX_CALIBRATION_RETRIES     5   // máx reintentos RS485 timeout durante calibración
#define SLAVE_CALIB_SETTLE_RESPONSES 5  // respuestas estables antes de disparar auto-calib (2026-05-22)

// --- Fader Logic PitchBend (2026-07-20, corregido — confirmado MIDI monitor: Logic manda hasta 16383) ---
// Rango completo MIDI 14-bit: 0-16383. El valor previo (14845) subestimaba
// el tope real de Logic — provocaba targets calculados más allá de
// calibratedMax (motor persiguiendo una posición físicamente inalcanzable).
#define LOGIC_PITCHBEND_MAX  16383

// --- Nombre de pista: debounce anti-flash (2026-08-13 15:10) ---
// TODO BANCO: ventana real observada del flash "Seleccionar"/"Selecting" fue
// ~34ms — 100ms da margen sin ser perceptible en un renombrado real.
#define TRACK_NAME_DEBOUNCE_MS 100

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

// Brillo LEDs transporte — PWM 8-bit, invertido por ánodo común (2026-08-16 21:59)
#define TRANSPORT_LED_BRIGHTNESS 40   // 0-255: 0=apagado, 255=brillo máximo

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