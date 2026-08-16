#include "S3Link.h"
#include "../config.h"
#include "../protocol.h"
#include "../s3_link_protocol.h"

S3Link s3Link;

static const uint32_t S3LINK_VU_THROTTLE_MS = 30;  // separación mínima entre envíos disparados por VU

void S3Link::begin() {
    Serial2.begin(S3LINK_BAUD, SERIAL_8N1, S3LINK_RX_PIN, S3LINK_TX_PIN);
}

void S3Link::update() {
    while (Serial2.available()) {
        _feed((uint8_t)Serial2.read());
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
        _rxExpected = (b == S3LINK_TYPE_PING) ? sizeof(S3LinkPingPongFrame)
                                                : sizeof(S3LinkChannelFrame);
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

    if (type == S3LINK_TYPE_PING) {
        _sendPong();
    }
    // S3LINK_TYPE_CHANNEL/PONG no se esperan en este sentido (P4→S3) — se ignoran
}

void S3Link::_sendPong() {
    S3LinkPingPongFrame f;
    f.start = S3LINK_START;
    f.type  = S3LINK_TYPE_PONG;
    f.crc   = s3link_crc8(&f.type, 1);
    Serial2.write((uint8_t*)&f, sizeof(f));
}

void S3Link::setTrackName(uint8_t ch, const char* name) {
    if (ch >= 8) return;
    strncpy(_ch[ch].name, name, 7);
    _sendChannel(ch, true);
}

void S3Link::setFlags(uint8_t ch, uint8_t flags) {
    if (ch >= 8) return;
    _ch[ch].flags = flags & (FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT);
    _sendChannel(ch, true);
}

void S3Link::setVuLevel(uint8_t ch, uint8_t value) {
    if (ch >= 8) return;
    _ch[ch].vu = value;
    _sendChannel(ch, false);
}

void S3Link::_sendChannel(uint8_t ch, bool force) {
    uint32_t now = millis();
    if (!force && (now - _lastSendMs[ch] < S3LINK_VU_THROTTLE_MS)) return;
    _lastSendMs[ch] = now;

    S3LinkChannelFrame f;
    f.start   = S3LINK_START;
    f.type    = S3LINK_TYPE_CHANNEL;
    f.channel = ch;
    memcpy(f.trackName, _ch[ch].name, 7);
    f.flags   = _ch[ch].flags;
    f.vuLevel = _ch[ch].vu;
    f.crc     = s3link_crc8(&f.type, sizeof(f) - 2);  // CRC sobre [type..vuLevel]
    Serial2.write((uint8_t*)&f, sizeof(f));
}
