// ============================================================
//  RS485.cpp  –  Master P4 (integrado en iMakie)
// ============================================================
#include "RS485.h"

RS485Master rs485;

void RS485Master::begin(uint8_t numSlaves) {
    _numSlaves = numSlaves;

    pinMode(RS485_ENABLE_PIN, OUTPUT);
    digitalWrite(RS485_ENABLE_PIN, LOW);

    Serial1.setRxBufferSize(256);   // ← ANTES del begin (fix bug anterior)
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    _mutex = xSemaphoreCreateMutex();
    configASSERT(_mutex);

    for (uint8_t i = 1; i <= _numSlaves; i++) {
        snprintf(_ch[i].trackName, 8, "TRK-%02d", i);
        _ch[i].faderTarget = 8192;
        _ch[i].dirty       = true;
    }

    _cycleStart = millis();

    log_i("[RS485] Master init | slaves:%u baud:%u", _numSlaves, RS485_BAUD);
    // ← task ya NO se crea aquí
}

void RS485Master::startTask() {
    xTaskCreatePinnedToCore(
        RS485Master::taskEntry, "RS485",
        4096, this, 5, nullptr, 1
    );
    log_i("[RS485] Task iniciado.");
}

void RS485Master::setCalibrate(uint8_t id) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].calibrate = true;
        _ch[id].dirty     = true;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::setAutoMode(uint8_t id, AutoMode mode) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].autoMode = mode;
        _ch[id].dirty    = true;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::taskEntry(void* param) {
    static_cast<RS485Master*>(param)->runTask();
}

void RS485Master::runTask() {
    _stateTimer = micros();
    for (;;) {
        switch (_busState) {

            case BusState::SEND:
                _sendPacket(_currentId);
                _rxGot    = 0;
                _rxHeader = false;
                _busState = BusState::WAIT_RESP;
                _stateTimer = micros();
                break;

            case BusState::WAIT_RESP:
                if (_readResponse()) {
                    _handleResponse();
                    _busState   = BusState::GAP;
                    _stateTimer = micros();
                } else if (micros() - _stateTimer > RS485_RESP_TIMEOUT_US) {
                    _timeouts++;
                    log_v("[RS485] TIMEOUT slave %d (rx bytes=%d)", _currentId, Serial1.available());  // ← añade esto
                    if (xSemaphoreTake(_mutex, 0) == pdTRUE) {
                        _ch[_currentId].responded = false;
                        xSemaphoreGive(_mutex);
                    }
                    _busState   = BusState::GAP;
                    _stateTimer = micros();
                }
                break;

            case BusState::GAP:
                if (micros() - _stateTimer >= RS485_GAP_US) {
                    _nextSlave();
                    _busState = BusState::SEND;
                }
                break;
        }
        taskYIELD();
    }
}

void RS485Master::_sendPacket(uint8_t id) {
    MasterPacket pkt = {};

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        pkt.header      = RS485_START_BYTE;
        pkt.id          = id;
        memcpy(pkt.trackName, _ch[id].trackName, 7);
        pkt.flags       = _ch[id].flags;
        // ── autoMode en bits 5-7 ──
        pkt.flags = ::setAutoMode(pkt.flags, _ch[id].autoMode);
        // ── FLAG_CALIB one-shot ──
        if (_ch[id].calibrate) {
            pkt.flags |= FLAG_CALIB;
            _ch[id].calibrate = false;
        }
        pkt.faderTarget = _ch[id].faderTarget;
        pkt.vuLevel     = _ch[id].vuLevel;
        pkt.vpotValue  = _ch[id].vpotValue;   // ← NUEVO
        extern uint8_t g_logicConnected;
        pkt.connected   = g_logicConnected;
        _ch[id].dirty   = false;
        xSemaphoreGive(_mutex);
    } else {
        _busState   = BusState::GAP;
        _stateTimer = micros();
        return;
    }

    pkt.crc = rs485_crc8((const uint8_t*)&pkt, sizeof(MasterPacket) - 1);

    while (Serial1.available()) Serial1.read();

    digitalWrite(RS485_ENABLE_PIN, HIGH);
    delayMicroseconds(RS485_TX_ENABLE_US);
    Serial1.write((const uint8_t*)&pkt, sizeof(MasterPacket));
    Serial1.flush();
    delayMicroseconds(RS485_TX_DONE_US);
    digitalWrite(RS485_ENABLE_PIN, LOW);

    _txCount++;
}

bool RS485Master::_readResponse() {
    while (Serial1.available()) {
        uint8_t b = (uint8_t)Serial1.read();
        log_v("RX byte: 0x%02X", b);  // ← solo esta línea

        if (!_rxHeader) {
            if (b == RS485_RESP_BYTE) {
                _rxBuf[0] = b;
                _rxGot    = 1;
                _rxHeader = true;
            }
        } else {
            if (_rxGot < sizeof(SlavePacket))
                _rxBuf[_rxGot++] = b;
            if (_rxGot >= sizeof(SlavePacket))
                return true;
        }
    }
    return false;
}

//***************************************************************************************************
// Procesa la respuesta del esclavo: valida CRC, actualiza estado del canal, maneja calibración, etc.
//***************************************************************************************************

void RS485Master::_handleResponse() {
    const SlavePacket* resp = reinterpret_cast<const SlavePacket*>(_rxBuf);

    uint8_t crc = rs485_crc8(_rxBuf, sizeof(SlavePacket) - 1);
    if (crc != resp->crc) {
        _crcErrors++;
        log_e("[RS485] slave=%u CRC ERROR calc=0x%02X recv=0x%02X",
              _currentId, crc, resp->crc);
        return;
    }
    if (resp->id != _currentId) {
        log_e("[RS485] ID MISMATCH esperado=%u recibido=%u",
              _currentId, resp->id);
        return;
    }

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        _ch[_currentId].faderPos          = resp->faderPos;
        _ch[_currentId].touchState        = resp->touchState;
        _ch[_currentId].prevButtons       = _ch[_currentId].buttons;
        _ch[_currentId].buttons           = resp->buttons & 0x0F;
        _ch[_currentId].encoderDelta      = resp->encoderDelta;
        _ch[_currentId].prevEncoderButton = _ch[_currentId].encoderButton;
        _ch[_currentId].encoderButton     = resp->encoderButton;
        _ch[_currentId].responded         = true;

        bool calibDone     = resp->buttons & SLAVE_FLAG_CALIB_DONE;
        bool calibError    = resp->buttons & SLAVE_FLAG_CALIB_ERROR;
        bool notCalibrated = resp->buttons & SLAVE_FLAG_NOT_CALIBRATED;

        if (calibDone) {
            _ch[_currentId].calibrating = false;
            if (!_ch[_currentId].calibrated) {
                _ch[_currentId].calibrated = true;
                _ch[_currentId].dirty      = true;
                log_i("[RS485] Slave %d calibrado OK", _currentId);
            }
        }

        if (calibError) {
            _ch[_currentId].calibrating  = false;
            _ch[_currentId].calibRetries++;
            log_w("[RS485] Slave %d ERROR calibracion (intento %d)",
                  _currentId, _ch[_currentId].calibRetries);
        }

        if (notCalibrated && !_ch[_currentId].calibrating) {
            _ch[_currentId].calibrated   = false;
            _ch[_currentId].calibRetries = 0;
        }

        if (!_ch[_currentId].calibrated &&
            !_ch[_currentId].calibrating &&
            _ch[_currentId].calibRetries < 3) {
            _ch[_currentId].calibrate   = true;
            _ch[_currentId].dirty       = true;
            _ch[_currentId].calibrating = true;
            log_i("[RS485] Slave %d sin calibrar — disparando (intento %d)",
                  _currentId, _ch[_currentId].calibRetries + 1);
        }

        xSemaphoreGive(_mutex);
    }

    _rxCount++;
}




void RS485Master::_nextSlave() {
    _currentId++;
    if (_currentId > _numSlaves) {
        _currentId = 1;
        uint32_t elapsed = millis() - _cycleStart;
        if (elapsed < POLL_CYCLE_MS)
            vTaskDelay(pdMS_TO_TICKS(POLL_CYCLE_MS - elapsed));
        _cycleStart = millis();
    }
}

// --- API Core 0 ---

void RS485Master::setTrackName(uint8_t id, const char* name) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        strncpy(_ch[id].trackName, name, 7);
        _ch[id].trackName[7] = '\0';
        _ch[id].dirty = true;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::setFlags(uint8_t id, uint8_t flags) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].flags = flags & ~AUTOMODE_MASK;  // preservar autoMode separado
        _ch[id].dirty = true;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::setFaderTarget(uint8_t id, uint16_t value14bit) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].faderTarget = value14bit & 0x3FFF;
        _ch[id].dirty       = true;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::setVuLevel(uint8_t id, uint8_t value) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].vuLevel = value;
        xSemaphoreGive(_mutex);
    }
}

void RS485Master::setVPotValue(uint8_t id, uint8_t rawCC) {
    if (id < 1 || id > _numSlaves) return;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        _ch[id].vpotValue = rawCC & 0x7F;   // 7 bits útiles
        _ch[id].dirty     = true;
        xSemaphoreGive(_mutex);
    }
}

bool RS485Master::hasNewSlaveData(uint8_t id) {
    if (id < 1 || id > _numSlaves) return false;
    bool result = false;
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        result = _ch[id].responded;
        _ch[id].responded = false;
        xSemaphoreGive(_mutex);
    }
    return result;
}

const ChannelData& RS485Master::getChannel(uint8_t id) {
    static ChannelData empty;
    if (id < 1 || id > _numSlaves) return empty;
    return _ch[id];
}

void RS485Master::printStats() const {
    float rate = _txCount > 0 ? (float)_rxCount / _txCount * 100.0f : 0.0f;
    log_i("[RS485] TX:%u RX:%u TO:%u CRC_ERR:%u Exito:%.1f%%",
          _txCount, _rxCount, _timeouts, _crcErrors, rate);
}

void RS485Master::resetStats() {
    _txCount = _rxCount = _timeouts = _crcErrors = 0;
}