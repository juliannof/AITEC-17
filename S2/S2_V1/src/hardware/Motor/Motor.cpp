#include "Motor.h"
#include "../../config.h"
#include <Preferences.h>
#include "../fader/FaderADC.h"
#include "../fader/FaderTouch.h"

extern FaderADC faderADC;

using Motor::MotorState;

// ────────────────────────────────────────────────────────────────
// Motor Control — iMakie PTxx Track S2
//
// Autoría: Julian Fuentes + Claude Haiku 4.5
// Rewrite completo: 2026-05-10
//
// Hardware: DRV8833 H-bridge (EN=GPIO14, IN1=GPIO18, IN2=GPIO16)
// Control: Calibración no-bloqueante + posición dead-zone + slew-rate
//
// Principios de diseño:
// - Orden crítico: configurar pins ANTES de habilitar EN
// - Estado SIEMPRE configurado ANTES de cambios HW
// - Todas las variables static en config.h (fuente única de verdad)
// - API pública limpia, no-bloqueante, con timeout global
// ────────────────────────────────────────────────────────────────

// Variables de estado en config.h (extern disponibles aquí via config.h)

// ─── PWM Range (lee de NVS, 0=inválido) ───────────────────────
static uint8_t _pwm_min = 0;
static uint8_t _pwm_max = 0;

// ─── Máquina de estados Motor v2 (2026-05-16 10:52) ──────────
// Arquitectura: S3 es master, usuario puede soltar fader
// Variables declaradas en config.h (fuente única de verdad)
// Estados: IDLE → GOING_TO_MIN → WAITING_FOR_CALIB → CALIBRATING → IDLE
//          IDLE → GOING_TO_MIN → AT_TARGET (usuario soltó)
//          Cualquiera → MOVING_TO_TARGET (S3 ordena setTarget)
static MotorState _motor_state = MotorState::IDLE;

// ─── Funciones HW (privadas) ──────────────────────────────────
static void _hwOff() {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_EN, LOW);
    _motor_hw_active = false;
}

static void _hwUp(uint8_t pwm) {
    analogWrite(MOTOR_IN1, pwm);
    analogWrite(MOTOR_IN2, 0);
    digitalWrite(MOTOR_EN, HIGH);
    _motor_hw_active = true;
}

static void _hwDown(uint8_t pwm) {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
    digitalWrite(MOTOR_EN, HIGH);
    _motor_hw_active = true;
}

// ─── Helper ───────────────────────────────────────────────────
static bool _isCalibrating() {
    return _motor_phase != CalibPhase::DONE  &&
           _motor_phase != CalibPhase::IDLE  &&
           _motor_phase != CalibPhase::ERROR;
}

// ─── Calibración no-bloqueante ────────────────────────────────
static void _calibUpdate() {
    uint32_t now = millis();
    int      pos = (int)_motor_adcPos;

    if (now - _motor_calibStart > CALIB_TIMEOUT) {
        _motor_phase = CalibPhase::ERROR;
        _hwOff();
        log_e("[CALIB] TIMEOUT");
        return;
    }

    switch (_motor_phase) {

    case CalibPhase::KICK_UP:
        log_i("[CALIB] KICK_UP adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
        if (pos >= 26000) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_UP;
            _hwUp(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_currentPWM  = _pwm_min;  // Sincronizar estado (2026-05-12 20:15)
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_i("[CALIB] → GOING_UP");
        }
        break;

    case CalibPhase::GOING_UP: {
        if (now < _motor_calibMinDetect) break;

        // PWM adaptativo: MAX hasta 26000, luego MIN para refinamiento
        uint8_t pwmGoing = (pos < 26000) ? _pwm_max : _pwm_min;
        if (_motor_currentPWM != pwmGoing) {
            if (pos < 26000) _hwUp(_pwm_max);
            else _hwUp(_pwm_min);
            _motor_currentPWM = pwmGoing;
        }

        if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
            if (pos >= MOTOR_ADC_MIN && pos <= MOTOR_ADC_MAX) {
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            }
        } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
            // Motor no se movió UP → intentar DOWN inmediatamente
            log_e("[CALIB] Sin movimiento en GOING_UP (pos=%d) → intentando DOWN", pos);
            now = millis();  // Recapturar timestamp fresco
            _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _motor_stableRef      = (int)_motor_adcPos;  // usar posición actual
            _motor_stableStart    = now;
            _motor_settleMin      = 27000;
            _motor_settleMax      = 0;
            _motor_phase          = CalibPhase::KICK_DOWN;
            _hwDown(_pwm_max);
            _motor_phaseStart     = now;
            log_i("[CALIB] KICK_DOWN iniciado desde GOING_UP");
        } else if (now - _motor_stableStart >= CALIB_STABLE_TIME) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase      = CalibPhase::SETTLE_UP;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
            log_i("[CALIB] → SETTLE_UP  pos=%d", pos);
        }
        }
        break;

    case CalibPhase::SETTLE_UP:
        if (_motor_adcPos < _motor_settleMin) _motor_settleMin = _motor_adcPos;
        if (_motor_adcPos > _motor_settleMax) _motor_settleMax = _motor_adcPos;

        if (now - _motor_phaseStart >= CALIB_SETTLE_MS) {
            _motor_adcTop       = _motor_adcPos;
            _motor_noiseTopSpan = _motor_settleMax - _motor_settleMin;
            log_i("[CALIB] TOP=%d noise_span=%d", _motor_adcTop, _motor_noiseTopSpan);
            log_i("[CALIB] Tope superior: %d  noise_span=%d", _motor_adcTop, _motor_noiseTopSpan);

            now = millis();  // Recapturar timestamp fresco
            _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
            _motor_stableRef      = (int)_motor_adcTop;
            _motor_stableStart    = now;
            _motor_settleMin      = 27000;
            _motor_settleMax      = 0;
            _motor_phase          = CalibPhase::KICK_DOWN;
            _hwDown(_pwm_max);
            _motor_phaseStart     = now;
        }
        break;

    case CalibPhase::KICK_DOWN:
        log_i("[CALIB] KICK_DOWN adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
        if (pos <= 200) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_DOWN;
            _hwDown(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_currentPWM  = _pwm_min;  // Sincronizar estado (2026-05-12 20:15)
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_i("[CALIB] → GOING_DOWN  pwm=%d", _pwm_min);
        }
        break;

    case CalibPhase::GOING_DOWN: {
        if (now < _motor_calibMinDetect) break;

        // PWM adaptativo: MAX hasta 200, luego MIN para refinamiento (2026-05-12 00:30)
        uint8_t pwmDown = (pos > 200) ? _pwm_max : _pwm_min;
        if (_motor_currentPWM != pwmDown) {
            if (pos > 1000) _hwDown(_pwm_max);
            else _hwDown(_pwm_min);
            _motor_currentPWM = pwmDown;
        }

        if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
            if (pos >= MOTOR_ADC_MIN && pos <= MOTOR_ADC_MAX) {
                log_d("[CALIB] GOING_DOWN movimiento detectado: pos=%d (ref=%d)", pos, _motor_stableRef);
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            }
        } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
            // Motor no se movió en tiempo máximo → ATASCADO
            _motor_phase = CalibPhase::ERROR;
            log_e("[CALIB] MOTOR BLOQUEADO — sin movimiento en GOING_DOWN (pos=%d)", pos);
            _hwOff();
        } else if (now - _motor_stableStart >= CALIB_STABLE_TIME) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase      = CalibPhase::SETTLE_DOWN;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
            log_i("[CALIB] → SETTLE_DOWN  pos=%d", pos);
        }
        }
        break;

    case CalibPhase::SETTLE_DOWN: {
        if (_motor_adcPos < _motor_settleMin) _motor_settleMin = _motor_adcPos;
        if (_motor_adcPos > _motor_settleMax) _motor_settleMax = _motor_adcPos;

        now = millis();  // Recapturar timestamp fresco
        if (now - _motor_phaseStart < CALIB_SETTLE_MS) break;

        uint16_t adcBot       = _motor_adcPos;
        _motor_noiseBottomSpan = _motor_settleMax - _motor_settleMin;

        uint16_t marginBot = max((uint16_t)(_motor_noiseBottomSpan * 2), (uint16_t)20);
        uint16_t marginTop = max((uint16_t)(_motor_noiseTopSpan * 2), (uint16_t)20);
        uint16_t minGapRequired = marginBot + marginTop;

        log_i("[CALIB] Tope inferior: %d  noise_span=%d  margin=%d", adcBot, _motor_noiseBottomSpan, marginBot);
        log_i("[CALIB] Tope superior: noise_span=%d  margin=%d", _motor_noiseTopSpan, marginTop);
        log_i("[CALIB] Gap requerido: %d (ruidos: top=%d bot=%d)", minGapRequired, _motor_noiseTopSpan, _motor_noiseBottomSpan);

        if (_motor_adcTop > adcBot + minGapRequired) {
            _calibratedFaderMin    = adcBot + marginBot;
            _calibratedFaderMax    = _motor_adcTop - marginTop;
            _motor_adcSpan   = _calibratedFaderMax - _calibratedFaderMin;
            _motor_targetADC = (uint16_t)map((long)_motor_lastMidiTarget,
                                        0, MIDI_PB_MAX, _calibratedFaderMin, _calibratedFaderMax);
            faderADC.setCalibration(_calibratedFaderMin, _calibratedFaderMax);
            _motor_lastCalibDone = millis();  // Registrar timestamp (2026-05-16 07:48)
            _motor_phase     = CalibPhase::DONE;
            log_i("[CALIB] OK  MIN=%d MAX=%d span=%d target=%d",
                  _calibratedFaderMin, _calibratedFaderMax, _motor_adcSpan, _motor_targetADC);
        } else {
            _motor_phase = CalibPhase::ERROR;
            log_e("[CALIB] ERROR — rango inválido  top=%d bot=%d", _motor_adcTop, adcBot);
        }
        break;
    }

    default: break;
    }
}

// ─── Control de posición ──────────────────────────────────────
static void _positionTick() {
    if (_motor_adcSpan == 0) {
        _hwOff();
        return;  // No calibrado
    }

    int pos    = (int)_motor_adcPos;
    int err    = (int)_motor_targetADC - pos;
    int absErr = abs(err);

    if (absErr < DEAD_ZONE) {
        if (_motor_active) {
            _motor_active = false;
            _hwOff();
            _motor_currentPWM = 0;
            log_d("[POS] OFF  pos=%d err=%d", pos, err);
        }
        return;
    }

    if (!_motor_active) {
        _motor_active = true;
        _motor_currentPWM  = 0;
        log_d("[POS] ON  pos=%d err=%d", pos, err);
    }

    int targetPWM = _pwm_min + (min((uint16_t)absErr, _motor_adcSpan) * (_pwm_max - _pwm_min)) / _motor_adcSpan;
    targetPWM = constrain(targetPWM, _pwm_min, _pwm_max);

    _motor_currentPWM = constrain(
        _motor_currentPWM + constrain(targetPWM - _motor_currentPWM, -PWM_SLEW, PWM_SLEW),
        0, _pwm_max);

    if (err > 0) _hwUp((uint8_t)_motor_currentPWM);
    else         _hwDown((uint8_t)_motor_currentPWM);
}

// ─── API pública ──────────────────────────────────────────────
namespace Motor {

void init() {
    log_i("[MOTOR] init(): configurando pines y PWM");

    pinMode(MOTOR_EN, OUTPUT);
    digitalWrite(MOTOR_EN, LOW);
    log_i("[MOTOR] GPIO%d (EN) LOW", MOTOR_EN);

    pinMode(MOTOR_IN1, OUTPUT);
    analogWriteFrequency(MOTOR_IN1, 20000);
    analogWriteResolution(MOTOR_IN1, 8);
    analogWrite(MOTOR_IN1, 0);
    log_i("[MOTOR] GPIO%d (IN1) 20kHz 8-bit", MOTOR_IN1);

    pinMode(MOTOR_IN2, OUTPUT);
    analogWriteFrequency(MOTOR_IN2, 20000);
    analogWriteResolution(MOTOR_IN2, 8);
    analogWrite(MOTOR_IN2, 0);
    log_i("[MOTOR] GPIO%d (IN2) 20kHz 8-bit", MOTOR_IN2);

    _hwOff();
    log_i("[MOTOR] init COMPLETE");
}

void initPWM() {
    // Lee pwmMin y pwmMax de NVS (2026-05-10 20:20)
    // Si no existen o son inválidos (==0), _pwm_min y _pwm_max quedan en 0 (error)
    Preferences prefs;
    prefs.begin("ptxx", true);
    uint8_t pwmMin = prefs.getUChar("pwmMin", 0);
    uint8_t pwmMax = prefs.getUChar("pwmMax", 0);
    prefs.end();

    if (pwmMin > 0 && pwmMax > 0 && pwmMin < pwmMax) {
        _pwm_min = pwmMin;
        _pwm_max = pwmMax;
        log_i("[MOTOR] PWM range: %u-%u (NVS)", _pwm_min, _pwm_max);
    } else {
        _pwm_min = 0;
        _pwm_max = 0;
        log_e("[MOTOR] PWM invalido en NVS: min=%u max=%u", pwmMin, pwmMax);
    }
}

void update() {
    // ┌─ Máquina de estados Motor v3 (2026-05-16 18:55) ──────────────────────────────────
    // PRIORIDAD ABSOLUTA:
    //   1. Usuario mueve/toca → Motor::stop() inmediato (setADCDelta)
    //   2. Motor::goToMin() MASTER → baja fader a 0 sin excepciones
    //   3. S3 ordena → Motor se mueve SOLO si usuario NO toca (setTargetFromS3)
    //
    // FLUJO CALIBRACIÓN (requestCalibration):
    //   S3 FLAG_CALIB → requestCalibration()
    //     ├─ Si fader EN 0: startCalib() inmediato
    //     └─ Si fader ≠ 0:
    //         ├─ _motor_state = GOING_TO_MIN
    //         ├─ _pendingCalib = true
    //         └─ goToMin() baja (MASTER sin guard _connected)
    //
    //   Motor::update() procesa:
    //     GOING_TO_MIN: cuando llega a 0 → WAITING_FOR_CALIB (si _pendingCalib)
    //     WAITING_FOR_CALIB: si _pendingCalib → startCalib() → CALIBRATING
    //
    // FLUJO NORMAL (S3 setTarget):
    //   S3 envía faderTarget → setTargetFromS3() (guarded por usuario)
    //     → MOVING_TO_TARGET → Motor mueve a target
    //   Llega a target → AT_TARGET → espera nuevo comando S3
    //
    // FLUJO USUARIO MASTER:
    //   Usuario mueve fader → setADCDelta() detecta delta OR FaderTouch
    //     → Motor::stop() INMEDIATO
    //     → AT_TARGET (usuario define posición)
    //   Usuario suelta (200ms debounce) → S3 puede controlar de nuevo
    //
    // FLUJO DESCONEXIÓN S3:
    //   Motor::setConnected(false) → IDLE con !_connected
    //     → IDLE → GOING_TO_MIN (baja a 0)
    //     → Motor::goToMin() loop indefinido (MASTER)
    // └────────────────────────────────────────────────────────────────────────────────
    // ─── Protección global topes mecánicos (2026-05-19) ──────────
    // Motor HW activo + ADC sin cambio en STALL_PROTECT_MS → fader en tope → apagar
    // CALIBRATING excluido: usa CALIB_STUCK_TIMEOUT propio por fase
    if (_motor_hw_active && _motor_state != MotorState::CALIBRATING) {
        if (abs((int)_motor_adcPos - (int)_stallProtectLastADC) > 10) {
            _stallProtectLastADC = _motor_adcPos;
            _stallProtectStart   = millis();
        } else if (_stallProtectStart > 0 &&
                   millis() - _stallProtectStart > STALL_PROTECT_MS) {
            _hwOff();  // _motor_hw_active = false → no refire inmediato
            log_e("[MOTOR] STALL — tope físico, motor apagado (adc=%d)", _motor_adcPos);
        }
    } else if (!_motor_hw_active) {
        _stallProtectStart   = 0;
        _stallProtectLastADC = _motor_adcPos;
    }
    // ─────────────────────────────────────────────────────────────

    switch (_motor_state) {

    case MotorState::IDLE:
        // Esperando órdenes S3 o usuario.
        if (!_connected && _motor_adcPos > (MOTOR_ADC_MIN + 10)) {
            // Sin S3: fader debe estar en 0 — bajar
            _motor_state = MotorState::GOING_TO_MIN;
            _hwDown(_pwm_max);
            log_d("[MOTOR-STATE] IDLE → GOING_TO_MIN");
        } else {
            // CONNECTED o ya en 0: motor quieto
            _hwOff();
        }
        break;

    case MotorState::GOING_TO_MIN: {
        // Llegada por threshold o por stall (fader en tope físico, ADC no baja más)
        bool arrived = (_motor_adcPos <= (MOTOR_ADC_MIN + 60));

        if (abs((int)_motor_adcPos - (int)_goToMinLastADC) > 15) {
            _goToMinLastADC    = _motor_adcPos;
            _goToMinStallStart = millis();
        } else if (_goToMinStallStart > 0 &&
                   millis() - _goToMinStallStart > GOTO_MIN_STALL_MS) {
            arrived = true;
            log_d("[MOTOR-STATE] GOING_TO_MIN stall detectado (adc=%d)", _motor_adcPos);
        }

        if (arrived) {
            _hwOff();
            _goToMinStallStart = 0;
            _goToMinLastADC    = 0;
            if (_pendingCalib) {
                _pendingCalib = false;
                _motor_state  = MotorState::CALIBRATING;
                startCalib();
                log_d("[MOTOR-STATE] GOING_TO_MIN → CALIBRATING");
            } else {
                _motor_state       = MotorState::AT_TARGET;
                _atTargetStartTime = millis();
                log_d("[MOTOR-STATE] GOING_TO_MIN → AT_TARGET (llegó a 0)");
            }
        }
        break;
    }

    case MotorState::CALIBRATING:
        // Máquina calibración en curso
        _calibUpdate();
        if (_motor_phase == CalibPhase::DONE || _motor_phase == CalibPhase::ERROR) {
            _motor_state = MotorState::IDLE;
            log_d("[MOTOR-STATE] CALIBRATING → IDLE");
        }
        break;

    case MotorState::MOVING_TO_TARGET:
        // Moviéndose a posición S3
        _positionTick();
        if (abs((int)_motor_adcPos - (int)_motor_targetADC) < DEAD_ZONE) {
            // Llegó a target
            _hwOff();
            _motor_state = MotorState::AT_TARGET;
            _atTargetStartTime = millis();
            log_d("[MOTOR-STATE] MOVING_TO_TARGET → AT_TARGET");
        }
        break;

    case MotorState::AT_TARGET:
        // En posición objetivo, esperando comando S3
        // Si usuario suelta en nueva posición, cambiar a GOING_TO_MIN
        _hwOff();
        // Timeout: si pasa tiempo sin comando S3, volver a IDLE (goToMin)
        // Configurar en config.h: MOTOR_AT_TARGET_TIMEOUT (ej: 30000 ms)
        break;

    default:
        _hwOff();
        break;
    }
    // └────────────────────────────────────────────────────────────
}

void setADC(uint16_t v) {
    if (v < MOTOR_ADC_MIN || v > MOTOR_ADC_MAX) return;  // Rechazar fuera de rango esperado
    // Permitir cambios grandes DURANTE calibración O cuando bajando a mínimo (para iniciar calibración)
    bool inCalibFlow = _isCalibrating() ||
                       _motor_state == MotorState::GOING_TO_MIN ||
                       _motor_state == MotorState::WAITING_FOR_CALIB;
    if (!inCalibFlow && _motor_adcPos > 0 &&
        abs((int)v - (int)_motor_adcPos) > ADC_SPIKE_GUARD) return;
    _motor_adcPos = v;
}

void setADCDelta(uint16_t currentADC) {
    // Detecta movimiento manual del fader: delta ADC rápido O sensor capacitivo
    // Usuario toma control absoluto del motor — S3 no puede overridear

    // Inicialización en primera llamada (evitar falsa detección en boot)
    if (_motor_lastADCForDelta == 0) {
        _motor_lastADCForDelta = currentADC;
        return;  // Solo calibrar, no detectar delta en boot
    }

    uint16_t delta = abs((int)currentADC - (int)_motor_lastADCForDelta);
    _motor_lastADCForDelta = currentADC;

    bool userTouch = (delta > MANUAL_TOUCH_THRESHOLD) || FaderTouch::isTouched();

    if (userTouch && !_motor_manualTouchDetected) {
        // Primera detección: usuario toma control — INMEDIATO
        _motor_manualTouchDetected = true;
        _motor_manualTouchStartTime = millis();
        Motor::stop();
        _motor_state = MotorState::AT_TARGET;      // Motor cede control
        _userDropTarget = currentADC;
        _motor_targetADC = currentADC;             // ADC actual = nueva posición aceptada
        log_w("[MOTOR] Usuario master: adc=%d delta=%d", currentADC, delta);
    } else if (_motor_manualTouchDetected && !FaderTouch::isTouched()) {
        // Usuario todavía moviendo pero sensor capacitivo ya lo perdió
        // Esperar a que se estabilice por debounce temporal
        if (millis() - _motor_manualTouchStartTime > MANUAL_TOUCH_DEBOUNCE_MS) {
            _motor_manualTouchDetected = false;
            log_i("[MOTOR] Usuario soltó fader en adc=%d", currentADC);
        }
    }
}

void setTarget(uint16_t target) {
    _motor_lastMidiTarget = target;
    if (_motor_phase != CalibPhase::DONE) return;
    _motor_targetADC = target;  // S3 ya mapeó al rango calibrado — usar directamente
    log_d("[TARGET] %d → adc=%d", target, _motor_targetADC);
}

void off() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
}

void stop() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
}

void setConnected(bool connected) {
    _connected = connected;  // Notificar estado de conexión S3 (2026-05-16 10:52)
}

uint16_t getRawADC() {
    return _motor_adcPos;
}

float getPosition() {
    if (_motor_adcSpan == 0) return 0.0f;
    int pos = (int)_motor_adcPos - (int)_calibratedFaderMin;
    return constrain((float)pos / (float)_motor_adcSpan, 0.0f, 1.0f);
}

uint16_t getADCMin() {
    return _calibratedFaderMin;
}

uint16_t getADCMax() {
    return _calibratedFaderMax;
}

void startCalib() {
    // Guard 1: no reiniciar si ya está calibrando (2026-05-16 07:48)
    if (_isCalibrating()) {
        return;
    }

    // Guard 2: cooldown — no reiniciar si completó hace poco (2026-05-16 07:48)
    uint32_t now = millis();
    if (_motor_lastCalibDone > 0 && (now - _motor_lastCalibDone) < CALIB_COOLDOWN_MS) {
        log_w("[CALIB] Enfriamiento activo — se completó hace %ld ms (esperar %ld ms)",
              now - _motor_lastCalibDone, CALIB_COOLDOWN_MS);
        return;
    }

    if (_motor_adcPos < MOTOR_ADC_MIN || _motor_adcPos > MOTOR_ADC_MAX) {
        log_e("[CALIB] Lectura ADC inválida: %d (esperado %d-%d)", _motor_adcPos, MOTOR_ADC_MIN, MOTOR_ADC_MAX);
        return;
    }
    _motor_active    = false;
    _motor_currentPWM     = 0;
    _motor_settleMin      = 27000;
    _motor_settleMax      = 0;
    _motor_noiseTopSpan   = 0;
    _motor_calibStart     = now;
    _motor_calibMinDetect = now + CALIB_MIN_TRAVEL_MS;
    _motor_stableRef      = (int)_motor_adcPos;
    _motor_stableStart    = now;
    _motor_phaseStart     = now;
    _motor_phase          = CalibPhase::KICK_UP;
    _hwUp(_pwm_max);
    log_i("[CALIB] Iniciada");
}

void goToMin() {
    // MASTER ABSOLUTO — ejecuta SIEMPRE sin excepciones (2026-05-16 18:55)
    // Prioridad máxima: Usuario > GoToMin > S3
    // No usar si S3 está conectado — requestCalibration() maneja eso

    // Guard: no reiniciar si ya está en movimiento
    if (_isCalibrating()) {
        return;
    }

    // ADC debe tener lectura válida
    if (_motor_adcPos < MOTOR_ADC_MIN || _motor_adcPos > MOTOR_ADC_MAX) {
        log_e("[MOTOR] goToMin: ADC inválido: %d", _motor_adcPos);
        return;
    }

    // Iniciar movimiento hacia abajo
    _motor_goingToMin = true;
    _hwDown(_pwm_max);  // Motor baja con PWM máximo
    log_i("[MOTOR] goToMin: bajando a posición 0 (MASTER)...");
}

// ─── Máquina de estados v2 (2026-05-16) ──────────────────────
void requestCalibration() {
    // S3 MASTER de calibración (2026-05-16 19:30)
    // Flujo: S3 envía FLAG_CALIB cada ciclo → S2 evalúa estado ACTUAL → ejecuta acción necesaria
    // S2 reporta CALIB_DONE vía SlavePacket → S3 detecta y pasa a siguiente slave
    // SIN estado pendiente, SIN bloqueos, evaluación PURA de estado actual

    if (_motor_adcPos <= (MOTOR_ADC_MIN + 10)) {
        // Fader EN 0 → calibrar directamente
        if (_motor_state != MotorState::CALIBRATING) {
            _motor_state = MotorState::CALIBRATING;
            startCalib();
            log_i("[MOTOR] requestCalibration: EN 0, iniciando startCalib()");
        }
    } else {
        // Fader ≠ 0 → bajar a 0, luego calibrar automáticamente
        if (_motor_state != MotorState::GOING_TO_MIN) {
            _pendingCalib = true;
            _motor_state = MotorState::GOING_TO_MIN;
            goToMin();
            log_i("[MOTOR] requestCalibration: ≠ 0, goToMin() bajando...");
        }
    }
}

void setTargetFromS3(uint16_t adcTarget) {
    // S3 ordena posición → solo si usuario NO está tocando (usuario es master)
    _s3Target = adcTarget;
    if (_motor_phase != CalibPhase::DONE) {
        log_w("[MOTOR] setTargetFromS3: no calibrado, ignorando target=%d", adcTarget);
        return;
    }
    if (_motor_manualTouchDetected || FaderTouch::isTouched()) {
        log_i("[MOTOR] setTargetFromS3: usuario master, ignorando target=%d", adcTarget);
        return;  // Usuario es master — ignora orden de S3
    }
    _motor_targetADC = adcTarget;
    _motor_state = MotorState::MOVING_TO_TARGET;
    log_d("[MOTOR] setTargetFromS3: target=%d", adcTarget);
}

void setUserDropTarget(uint16_t adcValue) {
    // Usuario soltó fader en posición adcValue → motor va allá
    _motor_state = MotorState::GOING_TO_MIN;  // Reutiliza lógica goToMin
    _userDropTarget = adcValue;
    _motor_targetADC = adcValue;
    _motor_phase = CalibPhase::DONE;
    _motor_goingToMin = false;  // Flag goToMin no aplica aquí
    log_i("[MOTOR] setUserDropTarget: ADC=%d", adcValue);
}

MotorState getState() {
    return _motor_state;
}

CalibState getCalibState() {
    switch (_motor_phase) {
        case CalibPhase::KICK_UP:
        case CalibPhase::GOING_UP:
        case CalibPhase::SETTLE_UP:
            return CalibState::CALIB_UP;
        case CalibPhase::KICK_DOWN:
        case CalibPhase::GOING_DOWN:
        case CalibPhase::SETTLE_DOWN:
            return CalibState::CALIB_DOWN;
        case CalibPhase::DONE:
            return CalibState::DONE;
        case CalibPhase::ERROR:
            return CalibState::ERROR;
        default:
            return CalibState::IDLE;
    }
}

bool isCalibrated() {
    return _motor_phase == CalibPhase::DONE && _motor_adcSpan > 100;
}

void testUp(uint8_t pwm) {
    _hwUp(pwm);
    log_d("[MOTOR-TEST] UP pwm=%d", pwm);
}

void testDown(uint8_t pwm) {
    _hwDown(pwm);
    log_d("[MOTOR-TEST] DOWN pwm=%d", pwm);
}

void testOff() {
    _hwOff();
    log_d("[MOTOR-TEST] OFF");
}

uint8_t getPWMMin() {
    return _pwm_min;
}

uint8_t getPWMMax() {
    return _pwm_max;
}

} // namespace Motor
