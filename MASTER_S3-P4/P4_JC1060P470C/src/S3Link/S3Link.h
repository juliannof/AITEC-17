#pragma once
#include <Arduino.h>

// ============================================================
//  S3Link.h — Master (ESP32-P4)
//  Recibe del S3 el estado de sus 8 canales (nombre/REC/MUTE/
//  SOLO/SELECT/VU) por Serial2 y lo escribe directamente en los
//  arrays de UI (índices 0-7, reservados por P4_CH_OFFSET, ver
//  config.h). Envía PING periódico y marca g_s3Connected=false
//  si no llega PONG dentro de S3LINK_TIMEOUT_MS. (2026-08-16)
// ============================================================

class S3Link {
public:
    void begin();
    void update();  // llamar en loop: procesa RX + heartbeat saliente
    void notifyGoOffline();  // avisa al S3 de que el P4 detectó desconexión de Logic (2026-08-23)

private:
    uint8_t  _rxBuf[16] = {};
    uint8_t  _rxLen      = 0;
    uint8_t  _rxExpected = 0;

    uint32_t _lastPingSentMs = 0;
    uint32_t _lastRxMs       = 0;

    // Diagnóstico heartbeat (2026-08-16) — log periódico en update()
    uint32_t _pingSentCount  = 0;
    uint32_t _pongRecvCount  = 0;
    uint32_t _lastLogMs      = 0;

    void _feed(uint8_t b);
    void _processFrame();
    void _applyChannelFrame(const uint8_t* buf);
    void _sendPing();
};

extern S3Link s3Link;
extern volatile bool g_s3Connected;
