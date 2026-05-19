// ============================================================
//  RS485.cpp  –  Master S3 (integrado en iMakie)
// ============================================================
#include "RS485.h"
#include "Profiler.h"
#include <Adafruit_NeoPixel.h>

RS485Master rs485;
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void RS485Master::begin(uint8_t numSlaves) {
    _numSlaves = numSlaves;

    pinMode(RS485_ENABLE_PIN, OUTPUT);
    digitalWrite(RS485_ENABLE_PIN, LOW);

    Serial1.setRxBufferSize(256);   // ← ANTES del begin (fix bug anterior)
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Inicializar NeoPixel (2026-05-16 19:40)
    pixels.begin();
    pixels.setBrightness(NEOPIXEL_BRIGHTNESS);
    pixels.setPixelColor(0, pixels.Color(0, 0, 255));  // Azul inicial (esperando)
    pixels.show();

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
        _ch[id].calibrate  = true;
        _ch[id].calibrating = true;  // FIX (2026-05-14): evita retries infinitos — Core0 verifica !calibrating
        _ch[id].dirty      = true;
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
    uint32_t _cycleCount = 0;
    for (;;) {
        switch (_busState) {

            case BusState::SEND:
                rs485prof.markTxStart(_currentId);
                _sendPacket(_currentId);
                _rxGot    = 0;
                _rxHeader = false;
                _busState = BusState::WAIT_RESP;
                _stateTimer = micros();
                rs485prof.markRxWaitStart();
                break;

            case BusState::WAIT_RESP:
                if (_readResponse()) {
                    rs485prof.markRxSuccess();
                    _handleResponse();
                    _consecutiveTimeouts = 0;
                    _busState   = BusState::GAP;
                    _stateTimer = micros();
                } else if (micros() - _stateTimer > RS485_RESP_TIMEOUT_US) {
                    rs485prof.markTimeout();
                    _timeouts++;
                    _consecutiveTimeouts++;
                    if (_consecutiveTimeouts <= 3 || _consecutiveTimeouts % 10 == 0)
                        log_w("[RS485] TIMEOUT slave %d (#%u consecuciones)",
                              _currentId, _consecutiveTimeouts);

                    // ── Límite de reintentos (2026-05-16 19:25) ──
                    // HALT solo si calibración ACTIVA — no durante operación normal (2026-05-19)
                    if (_ch[_currentId].calibrated && _ch[_currentId].calibrating && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES) {
                        // ✗ FALLO CRÍTICO: Calibración falló después de máx reintentos (2026-05-16 19:40)
                        pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Rojo puro
                        pixels.show();
                        if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
                            _ch[_currentId].calibrating = false;
                            _ch[_currentId].responded = false;
                            log_e("[CALIB] ✗ FALLO CRÍTICO Slave %d — comunicación perdida. Sistema DETENIDO.",
                                  _currentId);
                            xSemaphoreGive(_mutex);
                        }
                        // SISTEMA DETENIDO: loop infinito (requiere reset manual)
                        while(1) {
                            delay(1000);  // Espera infinita
                        }
                    } else {
                        // Continuar esperando
                        if (xSemaphoreTake(_mutex, 0) == pdTRUE) {
                            _ch[_currentId].responded = false;
                            xSemaphoreGive(_mutex);
                        }
                        _busState   = BusState::GAP;
                        _stateTimer = micros();
                    }
                }
                break;

            case BusState::GAP:
                if (micros() - _stateTimer >= RS485_GAP_US) {
                    rs485prof.markGapEnd();
                    _cycleCount++;
                    rs485prof.reportIfNeeded(_cycleCount, 1000, false);
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
        rs485prof.recordCrcError(_currentId);  // registrar en profiler
        log_e("[RS485] slave=%u CRC ERROR calc=0x%02X recv=0x%02X",
              _currentId, crc, resp->crc);
        return;
    }
    if (resp->id != _currentId) {
        rs485prof.recordIdMismatch(_currentId, resp->id);  // registrar en profiler
        log_e("[RS485] ID MISMATCH esperado=%u recibido=%u",
              _currentId, resp->id);
        return;
    }

    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        // ── Capturar calibración (min/max) si slave está enviando ──
        if (resp->buttons & SLAVE_FLAG_CALIB_SENDING) {
            if (resp->buttons & SLAVE_FLAG_CALIB_IS_MIN) {
                _ch[_currentId].calibratedMin = resp->faderPos;
                log_i("[RS485] Slave %d: calibratedMin=%d", _currentId, resp->faderPos);
            } else {
                _ch[_currentId].calibratedMax = resp->faderPos;
                log_i("[RS485] Slave %d: calibratedMax=%d ✓", _currentId, resp->faderPos);
            }
        } else {
            if (resp->touchState) {
                // Usuario tocando: posición directa, sin filtro — feedback inmediato a Logic (2026-05-19)
                _filteredFaderPos[_currentId] = resp->faderPos;
                _ch[_currentId].faderPos      = resp->faderPos;
            } else {
                // Motor siguiendo target: EMA para suavizar ruido EMI
                const float FADER_EMA_ALPHA = 0.15f;
                _filteredFaderPos[_currentId] = _filteredFaderPos[_currentId] +
                    (int16_t)((int32_t)resp->faderPos - _filteredFaderPos[_currentId]) * FADER_EMA_ALPHA;
                _ch[_currentId].faderPos = _filteredFaderPos[_currentId];
            }
        }

        if (resp->touchState) log_w("[S3-RX] touchState=1 slave=%d faderPos=%d", _currentId, resp->faderPos);
        _ch[_currentId].touchState        = resp->touchState;
        _ch[_currentId].prevButtons       = _ch[_currentId].buttons;
        _ch[_currentId].buttons           = resp->buttons;  // FIX: guardar todos los bits (incluyendo CALIB_*) para que Logic no vea valores calibración
        _ch[_currentId].encoderDelta      = resp->encoderDelta;
        _ch[_currentId].prevEncoderButton = _ch[_currentId].encoderButton;
        _ch[_currentId].encoderButton     = resp->encoderButton;
        // Auto-calibración al primer contacto (boot pre-Logic)
        if (!_ch[_currentId].responded && !_ch[_currentId].calibrated && !_ch[_currentId].calibrating) {
            _ch[_currentId].calibrate   = true;
            _ch[_currentId].calibrating = true;
            _ch[_currentId].dirty       = true;
            log_i("[CALIB] Slave %d primer contacto — calibración automática", _currentId);
        }
        _ch[_currentId].responded         = true;

        // ════════════════════════════════════════════════════════════════════
        // CALIBRACIÓN — DESACTIVADA TEMPORALMENTE
        // ════════════════════════════════════════════════════════════════════
        //
        // RAZÓN: Los motores DRV8833 en los slaves S2 están desactivados en la
        // PCB actual (líneas de control no conectadas/alimentadas). Sin motor,
        // la calibración siempre falla → ERROR_CALIBRACION → retry automático.
        // Los retries generan tráfico innecesario RS485 → timeouts artificiales.
        //
        // CUANDO REACTIVAR:
        // - Una vez que los motores estén presentes en hardware
        // - Cambiar _ch[_currentId].calibrated = true (abajo) a false
        // - Descomentar bloque de lógica de calibración (ver comentario ANTIGUO)
        // - Probar con Motor::init() funcionando en setup() S2
        //
        // ESTADO ACTUAL:
        // - Se ignoran flags CALIB_DONE, CALIB_ERROR, NOT_CALIBRATED
        // - Todos los slaves se marcan como calibrated=true (bypass)
        // - Faders responden correctamente a targets del master
        // - Botones/encoders funcionan sin depender de calibración
        // ════════════════════════════════════════════════════════════════════

        bool calibDone     = resp->buttons & SLAVE_FLAG_CALIB_DONE;
        bool calibError    = resp->buttons & SLAVE_FLAG_CALIB_ERROR;
        // notCalibrated no se usa en S3 (solo en S2)

        // ── Lógica de calibración — S3 MASTER (2026-05-16 19:30) ──
        if (calibDone) {
            _ch[_currentId].calibrating = false;
            if (!_ch[_currentId].calibrated) {
                _ch[_currentId].calibrated = true;
                _ch[_currentId].dirty      = true;
                log_i("[CALIB] Slave %d ✓ CALIBRADO OK: MIN=%d MAX=%d",
                      _currentId, _ch[_currentId].calibratedMin, _ch[_currentId].calibratedMax);
            }
        } else if (calibError) {
            _ch[_currentId].calibrating  = false;
            _ch[_currentId].calibRetries++;
            log_e("[CALIB] Slave %d ✗ ERROR calibración (reintento %d)",
                  _currentId, _ch[_currentId].calibRetries);
        } else {
            // S2 en tránsito — calibrating solo lo limpia CALIB_DONE o CALIB_ERROR
        }

        xSemaphoreGive(_mutex);
    }

    // Si estamos en desconexión y este era el último slave, limpiar flag
    if (_disconnecting && _currentId == _disconnectLastId) {
        _disconnecting = false;
        log_i("[RS485] DISCONNECT SEQUENCE completada — todos los slaves notificados");
    }

    _rxCount++;
}




void RS485Master::_nextSlave() {
    _currentId++;

    // Durante desconexión: NO reiniciar, dejar que currentId siga > numSlaves
    // para que isDisconnectComplete() retorne true cuando se complete el ciclo
    if (_disconnecting) {
        // currentId seguirá incrementando hasta pasar numSlaves
        // sin esperar cycle time (prioridad: apagar rápido)
        return;
    }

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
        // Mapeo: Logic 0-14848 → rango calibrado real de slave
        uint16_t faderTarget;
        if (_ch[id].calibratedMax > _ch[id].calibratedMin) {
            // Slave calibrado: mapear a rango real
            uint16_t span = _ch[id].calibratedMax - _ch[id].calibratedMin;
            faderTarget = _ch[id].calibratedMin + ((uint32_t)value14bit * span / LOGIC_PITCHBEND_MAX);
        } else {
            // Slave no calibrado aún: usar rango teórico (0-27000)
            faderTarget = (uint32_t)value14bit * 27000 / LOGIC_PITCHBEND_MAX;
        }
        _ch[id].faderTarget = faderTarget;
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
    log_i("[RS485] ═════════════════════════════════════");
    log_i("[RS485] TX:%u  RX:%u  TIMEOUT:%u  CRC_ERR:%u", _txCount, _rxCount, _timeouts, _crcErrors);
    log_i("[RS485] Tasa éxito: %.1f%%  (RX/TX)", rate);
    for (uint8_t i = 1; i <= _numSlaves; i++) {
        if (xSemaphoreTake((SemaphoreHandle_t)_mutex, pdMS_TO_TICKS(2)) == pdTRUE) {
            const char* status = _ch[i].calibrated ? "OK" : _ch[i].calibrating ? "CAL" : "---";
            log_i("[RS485] Slave %d: %-3s  responded:%s", i, status, _ch[i].responded ? "Y" : "N");
            xSemaphoreGive((SemaphoreHandle_t)_mutex);
        }
    }
    log_i("[RS485] ═════════════════════════════════════");
}

void RS485Master::resetStats() {
    _txCount = _rxCount = _timeouts = _crcErrors = 0;
}

// ═════════════════════════════════════════════════════════════════════
//  beginDisconnectSequence() — Inicia envío de DISCONNECTED a todos
// ═════════════════════════════════════════════════════════════════════
// Cuando Logic se desconecta (GoOffline), este método garantiza que
// TODOS los slaves reciban connected=0 antes de cambiar a offline.
// Itera sobre slave 1..numSlaves, esperando respuesta de cada uno.
// ═════════════════════════════════════════════════════════════════════
void RS485Master::beginDisconnectSequence() {
    _disconnecting = true;
    _disconnectStartId = 1;
    _disconnectLastId = _numSlaves;
    _disconnectStartTime = millis();
    _currentId = _disconnectStartId;  // Reinicia al primer slave
    log_i("[RS485] DISCONNECT SEQUENCE iniciada para slaves 1..%d", _numSlaves);
}

// ═════════════════════════════════════════════════════════════════════
//  isDisconnectComplete() — Comprueba si ya se notificó a todos
// ═════════════════════════════════════════════════════════════════════
// Retorna true si:
// - Se completó el ciclo (currentId pasó el último)
// - O timeout de seguridad se alcanzó (5s máx)
// ═════════════════════════════════════════════════════════════════════
bool RS485Master::isDisconnectComplete() const {
    if (!_disconnecting) return true;  // Si no está en modo desconexión, está completo

    // Timeout de seguridad: si tarda >5s, fuerza completación
    if (millis() - _disconnectStartTime > 5000) {
        log_w("[RS485] Timeout desconexión (5s) — forzando completación");
        return true;
    }

    // Completado cuando currentId ha pasado el último slave
    return _currentId > _disconnectLastId;
}