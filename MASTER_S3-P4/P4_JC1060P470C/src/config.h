// include/config.h — P4 versión mínima
#pragma once
#include <Arduino.h>

// ====================================================================
// CONFIGURACIÓN AUTOMÁTICA SEGÚN DISPOSITIVO
// ====================================================================
#if defined(DEVICE_P4_MASTER)
    #define DEVICE_FAMILY       0x14
    #define VERSION_REPLY_CMD   0x14
    #define NUM_SLAVES          0

#elif defined(DEVICE_S3_EXTENDER)
    #define DEVICE_FAMILY       0x15
    #define VERSION_REPLY_CMD   0x15
    #define NUM_SLAVES          8

#else
    #error "DEBE DEFINIR: DEVICE_P4_MASTER o DEVICE_S3_EXTENDER en platformio.ini build_flags"
#endif



// --- RS485 pines P4 ---
#define RS485_TX_PIN      52
#define RS485_RX_PIN      51
#define RS485_ENABLE_PIN  50
#define RS485_BAUD       500000

// ── I2C ──────────────────────────────────────────────────
// NeoTrellis — I2C_NUM_0
#define TRELLIS_SDA_PIN  33
#define TRELLIS_SCL_PIN  31
#define TRELLIS_ADDR_L   0x2F   // tile izquierdo
#define TRELLIS_ADDR_R   0x2E   // tile derecho

// --- Timing RS485 ---
#define RS485_TX_ENABLE_US    10
#define RS485_TX_DONE_US      10
#define RS485_RESP_TIMEOUT_US 5000
#define RS485_GAP_US          300
#define POLL_CYCLE_MS         20


// ── Dimensiones display ──────────────────────────────────────────
#define P4_W    480
#define P4_H    800
#define NUM_CH  8
#define CH_H    (P4_H / NUM_CH)   // 100px

// ── Header strip ─────────────────────────────────────────────────
#define MENU_PANEL_H   300
#define MENU_HAM_SIZE  44
#define HEADER_X  410
#define HEADER_W  (P4_W - HEADER_X)   // 70px

// ── Colores UI — usar con lv_color_hex() en archivos LVGL ────────
#define COL_BG         0x000000
#define COL_HEADER     0x000050
#define COL_MUTE_ON    0xFF0000
#define COL_MUTE_OFF   0x400000
#define COL_SOLO_ON    0xFFAA00
#define COL_SOLO_OFF   0x333333
#define COL_TRACK_BG   0x0F1218
#define COL_TRACK_SEL  0x2A3040
#define COL_TRACK_SEP  0x111111

// -- Automode
#define COL_AUTO_READ   0x006600
#define COL_AUTO_TOUCH  0x0000AA
#define COL_AUTO_LATCH  0xAA6600
#define COL_AUTO_WRITE  0xAA0000
#define COL_AUTO_OFF    0x333333

// --- Slaves ---
#define NUM_SLAVES  9


// --- Enums ---
enum class ConnectionState {
    DISCONNECTED,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

enum DisplayMode { MODE_BEATS, MODE_SMPTE };

// --- Estado de conexión global ---
extern volatile ConnectionState logicConnectionState;
extern uint8_t g_logicConnected;

// --- Variables de display ---
extern String trackNames[9];
extern bool recStates[8], soloStates[8], muteStates[8], selectStates[8];
extern uint8_t vpotValues[8];
extern float vuLevels[9];
extern bool vuClipState[9];
extern unsigned long vuLastUpdateTime[9];
extern float vuPeakLevels[9];
extern unsigned long vuPeakLastUpdateTime[9];
extern float faderPositions[9];
extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsTimecodeRedraw;
extern bool needsButtonsRedraw;
extern bool needsVUMetersRedraw;
extern bool needsHeaderRedraw;
extern String assignmentString;
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern bool btnFlashPG1[32];
extern bool btnFlashPG2[32];
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];
extern DisplayMode currentTimecodeMode;

extern volatile bool g_switchToPage3;
extern volatile uint8_t g_currentPage;  // 0=P3A 1=P1 2=P3B
extern volatile bool g_switchToPage3A;
extern volatile bool g_switchToPage3B;
extern volatile bool g_switchToOffline;
extern volatile bool g_sessionActive;
extern volatile bool g_switchToPage1;


// --- Mackie char map ---
const char MACKIE_CHAR_MAP[64] = {
    ' ','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
    'P','Q','R','S','T','U','V','W','X','Y','Z','[','\\',']','^','_',
    ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
    '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?'
};

static const uint32_t PALETTE_HEX[9] = {
    0x000000,  // 0: off
    0xFF0000,  // 1: rojo
    0x00BB00,  // 2: verde
    0x0000FF,  // 3: azul
    0xFFFF00,  // 4: amarillo
    0x00CCCC,  // 5: cian
    0xCC00CC,  // 6: magenta/lila
    0xDDDDDD,  // 7: blanco
    0xFF6600,  // 8: naranja
};
static const char* LABELS_PG1[32] = {
    "TRACK","PAN",  "EQ",   "SEND", "PLUG", "INST", "FLIP", "GLOB",
    "READ", "WRITE", "TOUCH",  "LATCH", "TRIM", "OFF",  "SOLO0","SMPT",
    "CALIB","SCRUB","NUDGE","MARK", "CHAN<","CHAN>", "BANK<","BANK>",
    "UNDO", "SAVE", "SHIFT","CTRL", "OPT",  "CMD",  "ENTER",">>PG2"
};

static const char* LABELS_PG1_SHIFT[32] = {
    "GLOBAL","FINE",   "LOW",  "MID",   "HI",    "FREQ",  "___",   "___",
    "OFF",   "TRIM",   "LTCH", "TCH",   "WRIT",  "READ",  "UNSOLO","UNMUTE",
    "SHIFT", "ALT",    "OPT",  "CMD",   "CHAN<", "CHAN>", "ZOOM-", "ZOOM+",
    "REDO",  "SAVE AS","OK",   "CNCL",  "MARK",  "NUDGE", "TAP",   ">>PG2"
};

static const uint8_t BTN_COLOR_IDX[32] = {
    5, 5, 5, 5, 5, 5, 6, 6,
    2, 6, 4, 8, 8, 2, 2, 2,
    4, 4, 4, 4, 3, 3, 3, 3,
    6, 2, 7, 7, 7, 7, 2, 1
};

// --- Notas MIDI ---
static const byte MIDI_NOTES_PG1[32] = {
    0x28,0x2A,0x2C,0x29,0x2B,0x2D,0x32,0x33,
    0x4A,0x4B,0x4D,0x4E,0x4C,0x4F,0x57,0x35,
    0x00,0x65,0x66,0x54,0x30,0x31,0x2E,0x2F,
    0x51,0x50,0x46,0x47,0x48,0x49,0x53,0x00
};

static const byte MIDI_NOTES_PG2[32] = {
    0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,
    0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x45,
    0x64,0x65,0x66,0x54,0x30,0x31,0x2E,0x2F,
    0x4C,0x50,0x46,0x47,0x48,0x49,0x52,0x00
};

