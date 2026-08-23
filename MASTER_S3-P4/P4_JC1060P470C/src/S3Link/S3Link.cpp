#include "S3Link.h"
#include "../config.h"
#include "../protocol.h"
#include "../s3_link_protocol.h"

S3Link s3Link;
volatile bool g_s3Connected = false;

void S3Link::begin() {
    Serial2.begin(S3LINK_BAUD, SERIAL_8N1, S3LINK_RX_PIN, S3LINK_TX_PIN);
    _lastRxMs = millis();  // evita marcar desconectado en el primer tramo tras boot
}

void S3Link::update() {
    while (Serial2.available()) {
        _feed((uint8_t)Serial2.read());
    }

    uint32_t now = millis();
    if (logicConnectionState == ConnectionState::CONNECTED &&
        now - _lastPingSentMs >= S3LINK_HEARTBEAT_MS) {
        _lastPingSentMs = now;
        _sendPing();
        _pingSentCount++;
    }
    g_s3Connected = (now - _lastRxMs) < S3LINK_TIMEOUT_MS;

    // Diagnóstico heartbeat (2026-08-16) — intervalo ampliado a 30s (2026-08-19)
    if (now - _lastLogMs >= 30000) {
        _lastLogMs = now;
        log_d("[S3LINK] connected=%d ping=%lu pong=%lu lastRx=%lums",
              g_s3Connected, (unsigned long)_pingSentCount,
              (unsigned long)_pongRecvCount, (unsigned long)(now - _lastRxMs));
    }
}

void S3Link::_feed(uint8_t b) {
    if (_rxLen == 0) {
        if (b != S3LINK_START) return;  // resync: descarta hasta encontrar inicio de trama
        _rxBuf[_rxLen++] = b;
        return;
    }
    if (_rxLen == 1) {
        _rxBuf[_rxLen++] = b;
        _rxExpected = (b == S3LINK_TYPE_CHANNEL) ? sizeof(S3LinkChannelFrame)
                                                   : sizeof(S3LinkPingPongFrame);
        return;
    }
    if (_rxLen < sizeof(_rxBuf)) _rxBuf[_rxLen++] = b;
    if (_rxLen >= _rxExpected) {
        _processFrame();
        _rxLen = 0;
    }
}

void S3Link::_processFrame() {
    uint8_t type = _rxBuf[1];
    uint8_t len  = _rxExpected;
    uint8_t crcCalc = s3link_crc8(&_rxBuf[1], len - 2);
    if (crcCalc != _rxBuf[len - 1]) return;  // trama corrupta, descartar

    _lastRxMs = millis();  // cualquier trama válida del S3 confirma que sigue vivo

    if (type == S3LINK_TYPE_CHANNEL) {
        _applyChannelFrame(_rxBuf);
    } else if (type == S3LINK_TYPE_PONG) {
        _pongRecvCount++;  // diagnóstico heartbeat (2026-08-16)
    }
    // PING no se espera en este sentido (S3→P4) — se ignora
}

void S3Link::_applyChannelFrame(const uint8_t* buf) {
    const S3LinkChannelFrame* f = (const S3LinkChannelFrame*)buf;
    uint8_t ch = f->channel;
    if (ch >= 8) return;

    char nameBuf[8] = {};
    memcpy(nameBuf, f->trackName, 7);
    String newName = String(nameBuf);
    if (trackNames[ch] != newName) {
        trackNames[ch] = newName;
        needsButtonsRedraw = true;
    }

    bool rec    = f->flags & FLAG_REC;
    bool solo   = f->flags & FLAG_SOLO;
    bool mute   = f->flags & FLAG_MUTE;
    bool select = f->flags & FLAG_SELECT;
    if (recStates[ch] != rec || soloStates[ch] != solo ||
        muteStates[ch] != mute || selectStates[ch] != select) {
        recStates[ch]    = rec;
        soloStates[ch]   = solo;
        muteStates[ch]   = mute;
        selectStates[ch] = select;
        needsButtonsRedraw = true;
    }

    float normalized = f->vuLevel / 127.0f;
    if (normalized > 0.0f) vuLastUpdateTime[ch] = millis();
    if (normalized != vuLevels[ch]) {
        vuLevels[ch] = normalized;
        needsVUMetersRedraw = true;
        vuDirty[ch] = true;
    }
    if (normalized > vuPeakLevels[ch]) {
        vuPeakLevels[ch]         = normalized;
        vuPeakLastUpdateTime[ch] = millis();
        needsVUMetersRedraw = true;
        vuDirty[ch] = true;
    }
}

void S3Link::_sendPing() {
    S3LinkPingPongFrame f;
    f.start = S3LINK_START;
    f.type  = S3LINK_TYPE_PING;
    f.crc   = s3link_crc8(&f.type, 1);
    Serial2.write((uint8_t*)&f, sizeof(f));
}

// Reenvía al S3 la desconexión de Logic que el P4 acaba de detectar (2026-08-23) —
// ver hooks en MIDIProcessor.cpp (case 0x0F, heurística de faders, checkUsbLink()).
void S3Link::notifyGoOffline() {
    S3LinkPingPongFrame f;
    f.start = S3LINK_START;
    f.type  = S3LINK_TYPE_GOOFFLINE;
    f.crc   = s3link_crc8(&f.type, 1);
    Serial2.write((uint8_t*)&f, sizeof(f));
}
