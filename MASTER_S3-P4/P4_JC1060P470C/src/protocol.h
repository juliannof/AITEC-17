#pragma once
#include <Arduino.h>

// ============================================================
//  protocol.h  –  Estructuras RS485 compartidas
//  Master (ESP32-S3) ↔ Slave (ESP32-S2)
// ============================================================

#define RS485_START_BYTE  0xAA
#define RS485_RESP_BYTE   0xBB

// --- FLAGS (byte flags de MasterPacket) ---
// bits 0-3: estado de botones
#define FLAG_REC    (1 << 0)
#define FLAG_SOLO   (1 << 1)
#define FLAG_MUTE   (1 << 2)
#define FLAG_SELECT (1 << 3)
// bit 4: orden de calibración (one-shot)
#define FLAG_CALIB  (1 << 4)
// bits 0-3: botones (FLAG_REC, SOLO, MUTE, SELECT ya definidos)
#define SLAVE_FLAG_CALIB_DONE  (1 << 4)   // calibración OK
#define SLAVE_FLAG_CALIB_ERROR (1 << 5)   // calibración fallida
#define SLAVE_FLAG_NOT_CALIBRATED (1 << 6)   // ← añadir
// bits 5-7: modo de automatización (3 bits = 8 valores)
#define AUTOMODE_SHIFT  5
#define AUTOMODE_MASK   (0x07 << AUTOMODE_SHIFT)

// Valores de autoMode (extraer con: (flags & AUTOMODE_MASK) >> AUTOMODE_SHIFT)
enum AutoMode : uint8_t {
    AUTO_OFF    = 0,
    AUTO_READ   = 1,
    AUTO_WRITE  = 2,
    AUTO_TRIM   = 3,
    AUTO_TOUCH  = 4,
    AUTO_LATCH  = 5,
    // 6, 7 reservados
};

// Helper: insertar autoMode en flags
// Uso: flags = setAutoMode(flags, AUTO_READ)
inline uint8_t setAutoMode(uint8_t flags, AutoMode mode) {
    return (flags & ~AUTOMODE_MASK) | ((mode << AUTOMODE_SHIFT) & AUTOMODE_MASK);
}

// Helper: extraer autoMode de flags
inline AutoMode getAutoMode(uint8_t flags) {
    return (AutoMode)((flags & AUTOMODE_MASK) >> AUTOMODE_SHIFT);
}

// --- CRC8 ---
inline uint8_t rs485_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

// --- Master → Slave (16 bytes) ---
struct __attribute__((packed)) MasterPacket {
    uint8_t  header;        // 0xAA
    uint8_t  id;            // 1-17
    char     trackName[7];  // Mackie Scribble Strip (7 chars, sin null)
    uint8_t  flags;         // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT
    uint16_t faderTarget;   // Pitch Bend 14-bit: 0-16383
    uint8_t  vuLevel;       // 0-127
    uint8_t  vpotValue;     // ← NUEVO: raw CC byte (bit6=center, 5-4=modo, 3-0=pos)
    uint8_t  connected;     // 1=CONNECTED, 0=DISCONNECTED
    uint8_t  crc;
};
static_assert(sizeof(MasterPacket) == 16, "MasterPacket debe ser 16 bytes");

// --- Slave → Master (9 bytes) ---
struct __attribute__((packed)) SlavePacket {
    uint8_t  header;        // 0xBB
    uint8_t  id;            // MY_SLAVE_ID
    uint16_t faderPos;      // FaderADC 0-8191
    uint8_t  touchState;    // 0=libre 1=tocado
    uint8_t  buttons;       // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT
    int8_t   encoderDelta;  // rotación acumulada (-127..+127)
    uint8_t  encoderButton; // push encoder: 0/1
    uint8_t  crc;
};
static_assert(sizeof(SlavePacket) == 9, "SlavePacket debe ser 9 bytes");