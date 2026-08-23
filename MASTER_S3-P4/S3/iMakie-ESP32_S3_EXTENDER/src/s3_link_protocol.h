#pragma once
#include <Arduino.h>

// ============================================================
//  s3_link_protocol.h — Enlace serie punto a punto S3 ↔ P4
//  UART dedicado (Serial2), independiente de los buses RS485
//  propios de cada MCU. (2026-08-16)
//
//  S3 → P4: estado de los 8 canales del S3 (nombre/REC/MUTE/
//           SOLO/SELECT/VU) para que el P4 los muestre en su
//           pantalla junto a sus propios 9 canales.
//  P4 → S3: heartbeat (PING), el S3 responde PONG — el P4 usa
//           la ausencia de PONG para detectar que el S3 no
//           responde.
// ============================================================

#define S3LINK_START         0xA5

#define S3LINK_TYPE_CHANNEL   0x01   // S3 → P4
#define S3LINK_TYPE_PING      0x02   // P4 → S3
#define S3LINK_TYPE_PONG      0x03   // S3 → P4
#define S3LINK_TYPE_GOOFFLINE 0x04   // P4 → S3: P4 detectó desconexión de Logic (2026-08-23)

// --- CRC8 (mismo polinomio que rs485_crc8 en protocol.h) ---
inline uint8_t s3link_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

// --- S3 → P4: estado de un canal (13 bytes) ---
struct __attribute__((packed)) S3LinkChannelFrame {
    uint8_t  start;        // S3LINK_START
    uint8_t  type;         // S3LINK_TYPE_CHANNEL
    uint8_t  channel;      // 0-7
    char     trackName[7]; // Mackie Scribble Strip, sin null terminator (igual que MasterPacket)
    uint8_t  flags;        // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT (ver protocol.h)
    uint8_t  vuLevel;      // 0-127
    uint8_t  crc;          // CRC8 sobre [type..vuLevel]
};
static_assert(sizeof(S3LinkChannelFrame) == 13, "S3LinkChannelFrame debe ser 13 bytes");

// --- PING (P4→S3) / PONG (S3→P4) — 3 bytes ---
struct __attribute__((packed)) S3LinkPingPongFrame {
    uint8_t start;  // S3LINK_START
    uint8_t type;   // S3LINK_TYPE_PING o S3LINK_TYPE_PONG
    uint8_t crc;    // CRC8 sobre [type]
};
static_assert(sizeof(S3LinkPingPongFrame) == 3, "S3LinkPingPongFrame debe ser 3 bytes");
