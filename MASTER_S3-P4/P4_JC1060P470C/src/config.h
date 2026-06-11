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
// ⚠️ PENDIENTE confirmar contra esquemático JC1060P470C (conector JST MX 1.25 4P)
//    Valores heredados de la placa antigua JC4880P433C (2026-06-09)
#define RS485_TX_PIN      52
#define RS485_RX_PIN      51
#define RS485_ENABLE_PIN  50
#define RS485_BAUD       500000

// ── Display JD9165 (MIPI-DSI, 1024×600 landscape nativo) (2026-06-09) ──
#define LCD_RST_PIN    27
#define LCD_BL_PIN     23

// ── Touch GT911 (I2C) — como la placa pequeña: RST/INT en NC (2026-06-09) ──
#define TOUCH_INT_PIN  -1
#define TOUCH_RST_PIN  -1
#define TOUCH_SDA_PIN  7
#define TOUCH_SCL_PIN  8
#define TOUCH_I2C_ADDR 0x5D

// NOTA: la JC1060P470C NO lleva NeoTrellis (defines TRELLIS_* eliminados 2026-06-09)

// --- Timing RS485 ---
#define RS485_TX_ENABLE_US    10
#define RS485_TX_DONE_US      10
#define RS485_RESP_TIMEOUT_US 5000
#define RS485_GAP_US          300
#define POLL_CYCLE_MS         20
#define LOGIC_PITCHBEND_MAX   14845


// ── Dimensiones display (JD9165 1024×600 landscape nativo) (2026-06-09) ──
#define P4_W    1024
#define P4_H    600
#define NUM_CH  16
#define CH_H    (P4_H / NUM_CH)

// ── Layout landscape nativo 1024×600 (header arriba + 16 canales en columnas) ──
#define HEADER_H    88                  // franja superior: timecode + modo BEAT/SMPT
#define CONTENT_Y   HEADER_H            // inicio del área de canales (y)
#define CONTENT_H   (P4_H - HEADER_H)   // 512px alto del área de canales
#define CH_W        (P4_W / NUM_CH)     // 64px ancho por canal (columna)
#define P4_CH_OFFSET 8                  // slots 0-7=S3 (izq), 8-15=P4 nativo (der)

// ── Header strip (LEGADO portrait — se elimina cuando todas las páginas migren) ──
// TODO landscape: HEADER_X/HEADER_W/CH_H asumían portrait 480×800
#define MENU_PANEL_H   300
#define MENU_HAM_SIZE  44
#define HEADER_X  410
#define HEADER_W  (P4_W - HEADER_X)   // 70px

// ── Colores UI P4 (LVGL lv_color_hex) — referencia Logic Pro (2026-06-11) ──
// Extraídos de captura Logic Pro con pantalla calibrada (junio 2026)
#define COL_BG            0x1A1A1A   // fondo general arrange
#define COL_BG_PANEL      0x1E1E1E   // panel inspector lateral
#define COL_BG_CELL       0x2A2A2A   // celda vacía / track inactiva
#define COL_TRACK_BG      0x222222   // cabecera canal (lista tracks)
#define COL_TRACK_SEL     0x3D3D3D   // track seleccionada (resaltado)
#define COL_TRACK_SEP     0x505050   // separador horizontal entre tracks
#define COL_BTN_INACTIVE  0x4A4A4A   // botones M/S/R/I off
#define COL_TEXT_DIM      0x999999   // texto secundario / etiquetas
#define COL_FADER_TRACK   0x606060   // ranura del fader
#define COL_FADER_THUMB   0x707070   // cabeza (thumb) del fader

// Colores funcionales
#define COL_HEADER     0x000050   // strip header azul — no cambiar
#define COL_MUTE_ON    0xFF0000
#define COL_MUTE_OFF   0x400000
#define COL_SOLO_ON    0xFFAA00
#define COL_SOLO_OFF   0x333333

// -- Automode
#define COL_AUTO_READ   0x006600
#define COL_AUTO_TOUCH  0x0000AA
#define COL_AUTO_LATCH  0xAA6600
#define COL_AUTO_WRITE  0xAA0000
#define COL_AUTO_OFF    0x333333

// NOTA: NUM_SLAVES se define una sola vez en el bloque DEVICE_* (arriba) (2026-06-09)

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
extern String trackNames[16];
extern bool recStates[16], soloStates[16], muteStates[16], selectStates[16];
extern uint8_t vpotValues[16];
extern float vuLevels[16];
extern bool vuClipState[16];
extern unsigned long vuLastUpdateTime[16];
extern float vuPeakLevels[16];
extern unsigned long vuPeakLastUpdateTime[16];
extern uint8_t vuPeakAlpha[16];
extern uint32_t vuPeakFadeTime[16];
extern float faderPositions[16];
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
extern bool rudeSoloActive;
extern bool cycleActive;
extern bool g_isPlaying;
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

