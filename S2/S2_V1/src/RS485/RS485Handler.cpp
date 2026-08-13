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
//  Internal — AutoMode routing helpers (2026-05-30 09:35)
// =============================================================
namespace Internal {

// ─── _touchDebounceForMode ───────────────────────────────────
// WHAT: devuelve el debounce (ms) que aplica al reporte de touchState
//       según el AutoMode activo.
// WHY:  cada modo tiene una ventana distinta antes de "soltar" el
//       touchState a S3. Centralizar aquí evita números mágicos
//       repetidos por el resto del handler.
// ASSUMES: nada — pura función.
uint32_t _touchDebounceForMode(AutoMode mode) {
    switch (mode) {
        case AUTO_TOUCH:
        case AUTO_TRIM:   return AUTOMODE_TOUCH_DEBOUNCE_MS;   //  80ms (TRIM tratado como TOUCH)
        case AUTO_LATCH:  return AUTOMODE_LATCH_DEBOUNCE_MS;   // 300ms
        default:          return MANUAL_TOUCH_DEBOUNCE_MS;     // 600ms (OFF/READ/WRITE base)
    }
}

// ─── _applyFaderTarget ───────────────────────────────────────
// WHAT: enruta el faderTarget del DAW al Motor según AutoMode.
// WHY:  punto único de decisión para AutoMode. Motor no sabe de modos.
//       Cada rama llama a la API de Motor apropiada (setTargetForced
//       para autoridad absoluta, setTargetFromS3 para autoridad
//       cooperativa, o no llama para inhibición total).
// ASSUMES: Motor calibrado. pkt validado por onMasterData() antes.
//          Si cambio de modo: caller ya hizo reset de _rsLatchFrozen
//          y _rsTouchActive (regla "reset total al cambiar modo").
// Cache del último PitchBend de Logic y flanco de calibración (2026-07-20).
// El S2 ahora mapea PB(0-16383)→ADC localmente con su rango calibrado.
static uint16_t _rsLastPBTarget   = 0;
static bool     _rsPBTargetValid  = false;
static bool     _rsWasCalibrated  = false;
static float    _rsFaderPosEMA    = 0.0f;

static uint16_t _pbToADC(uint16_t pb) {
    if (pb > 16383) pb = 16383;
    uint16_t lo = Motor::getADCMin();
    uint16_t hi = Motor::getADCMax();
    if (hi <= lo) return lo;
    return lo + (uint16_t)(((uint32_t)pb * (hi - lo)) / 16383);
}

void _applyFaderTarget(AutoMode mode, uint16_t target) {
    switch (mode) {

        // ── OFF / READ ─ DAW absoluto ──────────────────────
        // Usuario empuja → motor regresa al target sin esperar debounce.
        // Cualquier estado frozen previo se descarta aquí.
        case AUTO_OFF:
        case AUTO_READ:
            if (_rsLatchFrozen) {
                log_i("[AUTOMODE] OFF/READ: descongelando LATCH (frozen=%d → target=%d)",
                      _rsLatchFrozenADC, target);
                _rsLatchFrozen = false;
            }
            Motor::setTargetForced(target);
            break;

        // ── WRITE ─ motor inhibido ─────────────────────────
        // Usuario libre. Logic registra la posición física vía SlavePacket.
        // No se llama nunca al motor en este modo.
        case AUTO_WRITE:
            // intencionalmente vacío
            break;

        // ── TOUCH / TRIM ─ usuario gana mientras toca ──────
        // Motor::setTargetFromS3() ya implementa el guard de touch.
        // El debounce de "qué reportar a S3" es distinto y vive en
        // buildResponse() (_rsTouchActive con _touchDebounceForMode).
        case AUTO_TOUCH:
        case AUTO_TRIM:
            Motor::setTargetFromS3(target);
            break;

        // ── LATCH ─ usuario congela hasta que Logic mueva mucho ──
        // Lógica:
        //   1. Si usuario toca AHORA → congelar en posición actual.
        //   2. Si ya está frozen → comparar target con ADC congelado:
        //      delta > AUTOMODE_LATCH_UNFREEZE_ADC → Logic ha tomado
        //      decisión nueva, descongelamos y seguimos target.
        //   3. Si no está frozen y no toca → seguimiento normal.
        case AUTO_LATCH: {
            bool touching = Motor::isManualTouchDetected();
            if (touching) {
                if (!_rsLatchFrozen) {
                    _rsLatchFrozen    = true;
                    _rsLatchFrozenADC = Motor::getRawADC();
                    log_i("[AUTOMODE] LATCH: frozen en adc=%d (usuario tocando)", _rsLatchFrozenADC);
                }
                // No mover motor mientras usuario toca
                break;
            }
            if (_rsLatchFrozen) {
                int delta = abs((int)target - (int)_rsLatchFrozenADC);
                if (delta > AUTOMODE_LATCH_UNFREEZE_ADC) {
                    log_i("[AUTOMODE] LATCH: descongelando — Logic movió target frozen=%d target=%d delta=%d",
                          _rsLatchFrozenADC, target, delta);
                    _rsLatchFrozen = false;
                    Motor::setTargetFromS3(target);
                }
                // Si delta no supera umbral: mantener frozen, no mover motor.
                break;
            }
            // No frozen, no tocando: seguimiento normal con guard cooperativo.
            Motor::setTargetFromS3(target);
            break;
        }

        // ── Reservados / desconocidos ──────────────────────
        // Comportamiento conservador: trato como AUTO_READ (DAW manda).
        default:
            Motor::setTargetForced(target);
            break;
    }
}

} // namespace Internal

// =============================================================
//  onMasterData
// =============================================================
void onMasterData(const MasterPacket& pkt) {
    // Log fader cada 500ms — target de Logic vs posición ADC actual
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 500) {
        log_d("[FADER] target=%d adc=%d diff=%d mode=%d conn=%d",
              pkt.faderTarget, Motor::getRawADC(),
              (int)pkt.faderTarget - (int)Motor::getRawADC(),
              (int)getAutoMode(pkt.flags), pkt.connected);
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
        setScreenBrightness(BRIGHTNESS_SELECTED);   // Restaurar brillo al conectar (2026-05-27)
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
        // Unificado con arranque/timeout (2026-08-07): sin esto, la pantalla se
        // quedaba mostrando el último estado (nombre de pista, VU) y el brillo no
        // volvía a SPLASH — inconsistente con el timeout (checkTimeout más abajo)
        // y con el arranque (main.cpp), que sí hacen ambas cosas.
        drawSplashScreen();
        setScreenBrightness(BRIGHTNESS_SPLASH);
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
    // vuLastUpdateTime SOLO se actualiza cuando VU realmente sube.
    // Si se resetea en cada paquete RS485 (~10ms), el decay de 100ms nunca dispara. (2026-05-26)
    float newVu = pkt.vuLevel / 127.0f;
    if (newVu > vuLevels + 0.01f) {                   // solo subir desde RS485
        vuLastUpdateTime = millis();
        vuLevels = newVu;
        if (vuLevels > vuPeakLevels) {
            vuPeakLevels         = vuLevels;
            vuPeakLastUpdateTime = millis();
        }
        needsVUMetersRedraw = true;
    }

    // ── Modo de automatización (bits 5-7) ─────────────────────
    // CRÍTICO: extraer AutoMode ANTES de enrutar el fader. (2026-05-30 09:35)
    // El modo decide cómo se aplica el target — debe procesarse primero.
    AutoMode pktMode = getAutoMode(pkt.flags);

    // TRIM no debe sacar al S2 de READ (petición explícita, 2026-07-30).
    // Se reescribe pktMode ANTES del resto del flujo — así el paquete se procesa
    // como si TRIM nunca hubiese llegado (sin duplicar el filtro en ningún otro punto).
    if (_rsCurrentMode == AUTO_READ && pktMode == AUTO_TRIM) {
        pktMode = AUTO_READ;
    }

    // Reset total al cambiar de modo (regla: modo nuevo arranca limpio)
    // WHY: si veníamos de LATCH frozen y pasamos a READ, el freeze
    //      previo no debe arrastrarse. Lo mismo con _rsTouchActive.
    if (pktMode != _rsCurrentMode) {
        log_i("[AUTOMODE] cambio modo %d → %d (reset freeze+touch)", _rsCurrentMode, pktMode);
        _rsCurrentMode    = pktMode;
        _rsLatchFrozen    = false;
        _rsTouchActive    = false;
        _rsLastTouchTime  = 0;
    }
    // DAW absoluto: en OFF/READ el usuario no puede fijar la posición. (2026-07-20)
    Motor::setDawAbsolute(pktMode == AUTO_OFF || pktMode == AUTO_READ);

    // Comparación type-safe: ambos lados AutoMode. setAutoMode() acepta uint8_t. (2026-06-14)
    if (pktMode != currentAutoMode) {
        setAutoMode((uint8_t)pktMode);
        needsVPotRedraw   = true;
        needsHeaderRedraw = true;
    }

    // ── Fader / Motor ─────────────────────────────────────────
    // Reevaluación en CADA paquete (regla: re-aplicar modo aunque target no cambie). (2026-05-30 09:35)
    // WHY: si entras en TOUCH y sueltas el fader, motor debe volver al target
    //      sin esperar a que Logic reenvíe un valor distinto.
    // faderTarget ahora es PitchBend crudo 0-16383 (el S3 ya no mapea). (2026-07-20)
    Internal::_rsLastPBTarget  = pkt.faderTarget;
    Internal::_rsPBTargetValid = true;

    float newFader = pkt.faderTarget / 16383.0f;
    if (fabsf(faderPositions - newFader) > 0.001f) {
        faderPositions = newFader;  // solo redibujar display si cambia visualmente
    }

    // Flanco de calibración: en cuanto el motor pasa a calibrado, aplicar de inmediato
    // el último valor de Logic — así el fader salta al valor correcto tras calibrar,
    // sin depender de que Logic reenvíe nada. (2026-07-20)
    bool calibNow = Motor::isCalibrated();
    if (calibNow && !Internal::_rsWasCalibrated && Internal::_rsPBTargetValid) {
        log_i("[CALIB] DONE → aplicando último PB=%d", Internal::_rsLastPBTarget);
    }
    Internal::_rsWasCalibrated = calibNow;

    if (calibNow) {
        Internal::_applyFaderTarget(pktMode, Internal::_pbToADC(pkt.faderTarget));  // enrutado por modo
    }
    // Si no está calibrado: no se enruta target (el motor lo ignoraría igual). El PB
    // queda cacheado y se aplicará en el flanco de calibración.

    // ── VPot ──────────────────────────────────────────────────
    setVPotRaw(pkt.vpotValue);
}

// =============================================================
//  buildResponse
// =============================================================
SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu) {
    SlavePacket resp = {};

    // Durante calibración/goToMin el motor mueve el fader — nunca es toque de usuario (2026-05-27)
    Motor::MotorState mstate = Motor::getState();
    bool motorCalibrating = (mstate == Motor::MotorState::CALIBRATING ||
                             mstate == Motor::MotorState::GOING_TO_MIN);

    // ─── touchState con debounce por AutoMode (2026-05-30 09:35) ─
    // WHAT: ventana de reporte de touchState que respeta el debounce del modo activo.
    // WHY:  el "touch crudo" (Motor::isManualTouchDetected) tiene su propio debounce
    //       (600ms) optimizado para autoridad de fader, pero S3/Logic esperan ventanas
    //       distintas según AutoMode (TOUCH=80ms, LATCH=300ms). Reportamos una ventana
    //       virtual que extiende/recorta la cruda según _touchDebounceForMode().
    // TODO: cuando FaderTouch::isTouched() sea fiable en todo el recorrido,
    //       sustituir `rawTouch` por `FaderTouch::isTouched()` (cambio de UNA línea).
    bool rawTouch = Motor::isManualTouchDetected();
    uint32_t now  = millis();
    if (rawTouch) {
        _rsTouchActive   = true;
        _rsLastTouchTime = now;
    } else if (_rsTouchActive &&
               (now - _rsLastTouchTime) > Internal::_touchDebounceForMode(_rsCurrentMode)) {
        _rsTouchActive = false;
    }
    resp.touchState = (!motorCalibrating && _rsTouchActive) ? 1 : 0;
    if (resp.touchState) log_w("[S2-RESP] touchState=1 faderPos=%d mode=%d", Motor::getRawADC(), _rsCurrentMode);
    resp.buttons    = ButtonManager::getButtonFlags();

    // SELECT gestionado en S3 desde touchState — no enviar FLAG_SELECT desde S2 (2026-05-27)
    resp.encoderDelta  = (int8_t)constrain(Encoder::getCount() / 4, -127, 127);  // crudo→muescas: 1 clic = 1 unidad (2026-08-13 09:08)
    resp.encoderButton = ButtonManager::getEncoderButton();

    Motor::CalibState cs = Motor::getCalibState();

    // faderPos ahora se reporta en PitchBend 0-16383, mapeado localmente con el rango
    // calibrado. El S3 ya no gestiona rangos ADC. (2026-07-20)
    if (Motor::isCalibrated()) {
        uint16_t lo  = Motor::getADCMin();
        uint16_t hi  = Motor::getADCMax();
        uint16_t raw = Motor::getRawADC();
        if (raw < lo) raw = lo;
        if (raw > hi) raw = hi;
        uint16_t pb  = (hi > lo) ? (uint16_t)(((uint32_t)(raw - lo) * 16383) / (hi - lo)) : 0;

        if (resp.touchState) {
            // Usuario tocando: sin filtro — feedback inmediato a Logic.
            Internal::_rsFaderPosEMA = (float)pb;
            resp.faderPos  = pb;
        } else {
            Internal::_rsFaderPosEMA = Internal::_rsFaderPosEMA + ((float)pb - Internal::_rsFaderPosEMA) * FADER_EMA_ALPHA;
            resp.faderPos  = (uint16_t)Internal::_rsFaderPosEMA;
        }
        resp.buttons |= SLAVE_FLAG_CALIB_DONE;   // bit de estado: calibrado y operativo
    } else {
        resp.faderPos = 0;
    }

    if (cs == Motor::CalibState::ERROR) resp.buttons |= SLAVE_FLAG_CALIB_ERROR;

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
        drawSplashScreen();
        setScreenBrightness(BRIGHTNESS_SPLASH);
        neoWaitingHandshake = true;
        // Volver a azul en timeout
        // ¡CRÍTICO! NO actualizar neopixels aquí — timeout podría estar en path RS485
        // Los neopixels se actualizarán en el siguiente ciclo main.cpp
        needsTOTALRedraw = true;
    }
}

} // namespace RS485Handler