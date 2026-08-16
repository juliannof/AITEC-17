#pragma once
#include <Arduino.h>

// ============================================================
//  S3Link.h — Extender (ESP32-S3)
//  Envía al P4 el estado de los 8 canales (nombre/REC/MUTE/
//  SOLO/SELECT/VU) por Serial2 y responde PONG a los PING
//  periódicos del P4. (2026-08-16)
// ============================================================

class S3Link {
public:
    void begin();
    void update();  // llamar en loop: procesa PING entrante del P4

    void setTrackName(uint8_t ch, const char* name);  // ch 0-7, name sin null (7 chars)
    void setFlags    (uint8_t ch, uint8_t flags);      // bits REC/SOLO/MUTE/SELECT (protocol.h)
    void setVuLevel  (uint8_t ch, uint8_t value);      // 0-127

private:
    struct ChCache {
        char    name[7] = {' ',' ',' ',' ',' ',' ',' '};
        uint8_t flags   = 0;
        uint8_t vu      = 0;
    };
    ChCache  _ch[8];
    uint32_t _lastSendMs[8] = {};

    uint8_t  _rxBuf[16] = {};
    uint8_t  _rxLen     = 0;
    uint8_t  _rxExpected = 0;

    void _feed(uint8_t b);
    void _processFrame();
    void _sendChannel(uint8_t ch, bool force);
    void _sendPong();
};

extern S3Link s3Link;
