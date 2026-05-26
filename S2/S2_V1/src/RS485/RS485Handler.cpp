// src/RS485/RS485Handler.cpp
#include "RS485Handler.h"
#include "RS485.h"
#include "../display/Display.h"
#include "../hardware/Hardware.h"
#include "../hardware/Neopixels/Neopixel.h"   // ← neoWaitingHandshake, updateAllNeopixels, showNeopixels
#include "../hardware/Motor/Motor.h"
#include "../hardware/encoder/Encoder.h"
#include "../hardware/fader/FaderTouch.h"
#include "../hardware/button/ButtonManager.h"
#include "../config.h"

// ─── Externs de estado global (definidos en main.cpp) ────────
extern String trackName;
extern bool   recStates, soloStates, muteStates, selectStates;
extern bool   vuClipState;
extern float  vuPeakLevels, faderPositions, vuLevels;
extern unsigned long vuLastUpdateTime, vuPeakLastUpdateTime;

// ─── handleButtonLedState definida en Hardware.cpp ───────────
extern void handleButtonLedState(ButtonId id);

namespace RS485Handler {

// =============================================================
//  onMasterData
// =============================================================
void onMasterData(const MasterPacket& pkt) {
    // Log cada 5s — diagnóstico RS485 recepción (reducido 2026-05-19)
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
        log_d("[RS485 RX] id=%d target=%d connected=%d touch=%d", pkt.id, pkt.faderTarget, pkt.connected, Motor::isManualTouchDetected());
        lastLog = millis();
    }

    // ── Calibración — ANTES de desconexión (2026-05-16 19:20) ──
    // CRÍTICO: Procesar FLAG_CALIB ANTES de Motor::off() para que motor pueda calibrar
    // S3 envía calibración secuencial al boot, independiente de Logic
    // Motor debe estar activo cuando requestCalibration() lo ordene
    if (pkt.flags & FLAG_CALIB) {
        Motor::requestCalibration();  // Motor puede calibrar aunque _connected vaya a cambiar
    }

    // ── Conexión ──────────────────────────────────────────────
    ConnectionState newState = pkt.connected ?
        ConnectionState::CONNECTED : ConnectionState::DISCONNECTED;
    if (newState != logicConnectionState) {
    logicConnectionState = newState;
    needsTOTALRedraw = true;

    if (newState == ConnectionState::CONNECTED) {
        Motor::setConnected(true);  // Notificar motor que S3 conectó (2026-05-16 10:52)
        neoWaitingHandshake = false;
        // Cambio de azul a colores tenues
        // ¡CRÍTICO! NO llamar updateAllNeopixels() aquí — retarda RS485 response
        // Neopixels se actualizan en main.cpp DESPUÉS de sendResponse()
    } else {
        // ── Desconexión limpia ────────────────────────────
        Motor::setConnected(false);  // Notificar motor que S3 desconectó (2026-05-16 10:52)
        Motor::off();
        Motor::setTarget(Motor::getRawADC());
        recStates = soloStates = muteStates = selectStates = false;
        vuLevels = vuPeakLevels = 0.0f;
        //setScreenBrightness(0);
        neoWaitingHandshake = true;
        // Cambio a azul
        // ¡CRÍTICO! NO llamar updateAllNeopixels() aquí — retarda RS485 response
        // Neopixels se actualizan en main.cpp DESPUÉS de sendResponse()
    }
}

    if (logicConnectionState != ConnectionState::CONNECTED) return;

    // ── Nombre de pista ───────────────────────────────────────
    char nameBuf[8] = {};
    memcpy(nameBuf, pkt.trackName, 7);
    nameBuf[7] = '\0';
    if (trackName != nameBuf) {
        trackName = String(nameBuf);
        needsHeaderRedraw = true;
    }

    // ── Flags de botones ──────────────────────────────────────
    bool nr = (pkt.flags & FLAG_REC)    != 0;
    bool ns = (pkt.flags & FLAG_SOLO)   != 0;
    bool nm = (pkt.flags & FLAG_MUTE)   != 0;
    bool nq = (pkt.flags & FLAG_SELECT) != 0;

    if (recStates    != nr) { recStates    = nr; handleButtonLedState(ButtonId::REC);    needsMainAreaRedraw = true; }
    if (soloStates   != ns) { soloStates   = ns; handleButtonLedState(ButtonId::SOLO);   needsMainAreaRedraw = true; }
    if (muteStates   != nm) { muteStates   = nm; handleButtonLedState(ButtonId::MUTE);   needsMainAreaRedraw = true; }
    if (selectStates != nq) {
        selectStates = nq;
        handleButtonLedState(ButtonId::SELECT);
        handleButtonLedState(ButtonId::REC);
        handleButtonLedState(ButtonId::SOLO);
        handleButtonLedState(ButtonId::MUTE);
        //showNeopixels();
        needsHeaderRedraw = true;
           }

    // ── VU meter — attack instantáneo, decay por handleVUMeterDecay() ──────
    float newVu = pkt.vuLevel / 127.0f;
    if (newVu > 0.01f) vuLastUpdateTime = millis();   // previene decay mientras llega señal
    if (newVu > vuLevels + 0.01f) {                   // solo subir desde RS485
        vuLevels = newVu;
        if (vuLevels > vuPeakLevels) {
            vuPeakLevels         = vuLevels;
            vuPeakLastUpdateTime = millis();
        }
        needsVUMetersRedraw = true;
    }

    // ── Fader / Motor ─────────────────────────────────────────
    float newFader = pkt.faderTarget / 27000.0f;
    if (fabsf(faderPositions - newFader) > 0.001f) {
        faderPositions = newFader;
        Motor::setTargetFromS3(pkt.faderTarget);  // User can override (master) (2026-05-16 10:52)
    }

    // ── Modo de automatización (bits 5-7) ─────────────────────
    uint8_t newAutoMode = (pkt.flags >> 5) & 0x07;
    if (newAutoMode != currentAutoMode) {
        setAutoMode(newAutoMode);
        needsVPotRedraw   = true;
        needsHeaderRedraw = true;
    }

    // ── VPot ──────────────────────────────────────────────────
    setVPotRaw(pkt.vpotValue);
}

// =============================================================
//  buildResponse
// =============================================================
SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu) {
    static uint8_t _calib_send_state = 0;  // 0=normal, 1=enviando min, 2=enviando max
    static Motor::CalibState _last_cs = Motor::CalibState::IDLE;

    SlavePacket resp = {};
    resp.touchState    = Motor::isManualTouchDetected() ? 1 : 0;  // FaderTouch reemplazado — delta-based (2026-05-19)
    if (resp.touchState) log_w("[S2-RESP] touchState=1 faderPos=%d", Motor::getRawADC());
    resp.buttons       = ButtonManager::getButtonFlags();
    resp.encoderDelta  = (int8_t)constrain(Encoder::getCount(), -127, 127);
    resp.encoderButton = ButtonManager::getEncoderButton();

    Motor::CalibState cs = Motor::getCalibState();

    // Detección: si volvemos a calibración desde DONE, resetear estado de envío
    if (cs != Motor::CalibState::DONE && _last_cs == Motor::CalibState::DONE) {
        _calib_send_state = 0;  // Reset para próxima calibración
    }
    _last_cs = cs;

    // Máquina de estado: enviar min/max tras calibración
    if (cs == Motor::CalibState::DONE && _calib_send_state < 2) {
        if (_calib_send_state == 0) {
            // Paquete 1: enviar MIN — sin CALIB_DONE (S3 espera MAX antes de declarar OK)
            resp.faderPos = Motor::getADCMin();
            resp.buttons |= SLAVE_FLAG_CALIB_SENDING | SLAVE_FLAG_CALIB_IS_MIN;
            _calib_send_state = 1;
        } else if (_calib_send_state == 1) {
            // Paquete 2: enviar MAX
            resp.faderPos = Motor::getADCMax();
            resp.buttons |= SLAVE_FLAG_CALIB_DONE | SLAVE_FLAG_CALIB_SENDING;  // sin IS_MIN = es MAX
            _calib_send_state = 2;
        }
    } else {
        // Normal: enviar posición actual
        resp.faderPos = Motor::getRawADC();
        if (cs == Motor::CalibState::DONE)  resp.buttons |= SLAVE_FLAG_CALIB_DONE;
    }

    if (cs == Motor::CalibState::ERROR) resp.buttons |= SLAVE_FLAG_CALIB_ERROR;
    // NOT_CALIBRATED no se necesita — CALIB_DONE lo indica suficientemente

    return resp;
}

// =============================================================
//  checkTimeout
// =============================================================
void checkTimeout(unsigned long lastRxTime) {
    if (millis() - lastRxTime <= 500) return;

    if (vuLevels > 0.0f) {
        vuLevels = 0.0f;
        vuPeakLevels = 0.0f;
        needsVUMetersRedraw = true;
    }
    if (logicConnectionState != ConnectionState::DISCONNECTED) {
        logicConnectionState = ConnectionState::DISCONNECTED;
        Motor::off();
        Motor::setTarget(Motor::getRawADC());
        recStates = soloStates = muteStates = selectStates = false;
        setScreenBrightness(0);
        neoWaitingHandshake = true;
        // Volver a azul en timeout
        // ¡CRÍTICO! NO actualizar neopixels aquí — timeout podría estar en path RS485
        // Los neopixels se actualizarán en el siguiente ciclo main.cpp
        needsTOTALRedraw = true;
    }
}

} // namespace RS485Handler