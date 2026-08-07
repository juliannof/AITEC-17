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

static bool _dawAbsolute = false;  // true en AUTO_OFF/READ — usuario no puede fijar posición (2026-07-20)

// Suelo de la máquina "fader a 0 sin Logic": el mínimo real calibrado si existe,
// si no la constante. Sin esto, los umbrales fijos (MOTOR_ADC_MIN) contra un fondo
// físico real de ~115-135 cuentas hacen que la llegada por threshold NUNCA se cumpla
// → llegada por stall → re-disparo → bucle de pulsos contra el tope. (2026-07-20)
static uint16_t _goToMinRestADC = 0;  // latch: dónde descansó el fader tras stall en el tope
static uint16_t _floorADC() {
    return (_motor_phase == CalibPhase::DONE) ? _calibratedFaderMin : (uint16_t)MOTOR_ADC_MIN;
}

// ─── Funciones HW (privadas) ──────────────────────────────────
static void _hwOff() {
    digitalWrite(MOTOR_EN, LOW);   // EN primero: corta driver antes de cambiar PWM
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
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
        log_d("[CALIB] KICK_UP adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
        if (pos >= 26000) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_UP;
            _hwUp(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_currentPWM  = _pwm_min;  // Sincronizar estado (2026-05-12 20:15)
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_i("[CALIB] → GOING_UP");
        } else {
            // Stuck detection: ADC < 26000 pero fader en tope físico (variación HW entre unidades)
            if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
                now = millis();
                _motor_phase       = CalibPhase::GOING_UP;
                _hwUp(_pwm_min);
                _motor_currentPWM  = _pwm_min;
                _motor_stableRef   = pos;
                _motor_stableStart = now;
                log_w("[CALIB] KICK_UP stuck pos=%d (<26000) — tope físico detectado → GOING_UP", pos);
            }
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
            // Motor no avanzó en GOING_UP → fader en tope físico, registrar como top (2026-05-27)
            log_w("[CALIB] GOING_UP stall pos=%d → tope físico → SETTLE_UP", pos);
            now = millis();
            _motor_adcTop     = pos;
            _motor_phase      = CalibPhase::SETTLE_UP;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
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
            _motor_adcTop       = _motor_settleMax;  // máximo medido durante settle = tope real (2026-05-27)
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
        log_d("[CALIB] KICK_DOWN adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
        if (pos <= 200) {
            now = millis();  // Recapturar timestamp fresco
            _motor_phase       = CalibPhase::GOING_DOWN;
            _hwDown(_pwm_min);  // Movimiento controlado hacia el tope
            _motor_currentPWM  = _pwm_min;  // Sincronizar estado (2026-05-12 20:15)
            _motor_stableRef   = pos;
            _motor_stableStart = now;
            log_i("[CALIB] → GOING_DOWN  pwm=%d", _pwm_min);
        } else {
            // Stuck detection: tope físico inferior por encima del umbral 200 (variación HW) (2026-05-27)
            if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
                _motor_stableRef   = pos;
                _motor_stableStart = now;
            } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
                now = millis();
                _motor_phase       = CalibPhase::GOING_DOWN;
                _hwDown(_pwm_min);
                _motor_currentPWM  = _pwm_min;
                _motor_stableRef   = pos;
                _motor_stableStart = now;
                log_w("[CALIB] KICK_DOWN stall pos=%d (>200) — tope físico detectado → GOING_DOWN", pos);
            }
        }
        break;

    case CalibPhase::GOING_DOWN: {
        if (now < _motor_calibMinDetect) break;

        // PWM adaptativo: MAX hasta 200, luego MIN para refinamiento (2026-05-12 00:30)
        // FIX 2026-07-22: el umbral de aplicación real (1000) no coincidía con el umbral
        // de cálculo (200) — el motor perdía fuerza (PWM_MIN) desde 1000 cuentas de
        // distancia, mucho antes de acercarse al fondo real, y se atascaba sin llegar al
        // tope físico verdadero (detectaba "stuck" muy por encima del mínimo real).
        uint8_t pwmDown = (pos > 200) ? _pwm_max : _pwm_min;
        if (_motor_currentPWM != pwmDown) {
            if (pos > 200) _hwDown(_pwm_max);
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
            // Motor no avanzó en GOING_DOWN → fader en tope físico, registrar como bottom (2026-05-27)
            log_w("[CALIB] GOING_DOWN stall pos=%d → tope físico → SETTLE_DOWN", pos);
            now = millis();
            _motor_phase      = CalibPhase::SETTLE_DOWN;
            _hwOff();
            _motor_settleMin  = 27000;
            _motor_settleMax  = 0;
            _motor_phaseStart = now;
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

        uint16_t adcBot        = _motor_settleMin;  // mínimo medido durante settle = fondo real (2026-05-27)
        _motor_noiseBottomSpan = _motor_settleMax - _motor_settleMin;

        uint16_t marginBot = 20;  // margen fijo — DEAD_ZONE protege tope físico (2026-05-27)
        uint16_t marginTop = 20;  // antes: noiseSpan*2 dejaba fader corto en extremos
        uint16_t minGapRequired = marginBot + marginTop;

        log_i("[CALIB] Tope inferior: %d  noise_span=%d  margin=%d", adcBot, _motor_noiseBottomSpan, marginBot);
        log_i("[CALIB] Tope superior: noise_span=%d  margin=%d", _motor_noiseTopSpan, marginTop);
        log_i("[CALIB] Gap requerido: %d (ruidos: top=%d bot=%d)", minGapRequired, _motor_noiseTopSpan, _motor_noiseBottomSpan);

        uint16_t _tentativeSpan = (_motor_adcTop > adcBot) ? (_motor_adcTop - adcBot) : 0;
        if (_motor_adcTop > adcBot + minGapRequired && _tentativeSpan >= CALIB_MIN_SPAN) {
            _calibratedFaderMin    = adcBot + marginBot;
            _calibratedFaderMax    = _motor_adcTop - marginTop;
            _motor_adcSpan   = _calibratedFaderMax - _calibratedFaderMin;
            // Quedarse en la posición actual (fader recién asentado en el fondo tras
            // SETTLE_DOWN). El "flanco de calibración" en RS485Handler.cpp:286-297 aplica
            // el target real de Logic en el siguiente paquete RS485, ya acotado a
            // [_calibratedFaderMin,_calibratedFaderMax] por _pbToADC(). No hace falta
            // precalcular un target aquí. (2026-08-07)
            _motor_targetADC = _motor_adcPos;
            faderADC.setCalibration(_calibratedFaderMin, _calibratedFaderMax);
            _motor_lastCalibDone = millis();  // Registrar timestamp (2026-05-16 07:48)
            _motor_phase     = CalibPhase::DONE;
            _motor_calibRetries = 0;
            log_i("[CALIB] OK  MIN=%d MAX=%d span=%d target=%d",
                  _calibratedFaderMin, _calibratedFaderMax, _motor_adcSpan, _motor_targetADC);
        } else {
            // Span demasiado corto = calibración amputada (stall a mitad de recorrido
            // tratado como tope, o stream ADC congelado). Reintentar localmente. (2026-07-20)
            _motor_calibRetries++;
            if (_motor_calibRetries < CALIB_MAX_RETRIES) {
                log_w("[CALIB] span corto (%d < %d) top=%d bot=%d — reintento %d/%d",
                      _tentativeSpan, CALIB_MIN_SPAN, _motor_adcTop, adcBot,
                      _motor_calibRetries, CALIB_MAX_RETRIES);
                _pendingCalib = true;
                _motor_phase  = CalibPhase::IDLE;
                _motor_state  = MotorState::GOING_TO_MIN;
                Motor::goToMin();
            } else {
                _motor_phase = CalibPhase::ERROR;
                log_e("[CALIB] ERROR — amputada tras %d intentos  top=%d bot=%d span=%d",
                      _motor_calibRetries, _motor_adcTop, adcBot, _tentativeSpan);
            }
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
        log_i("[POS] ON  pos=%d target=%d err=%d span=%d", pos, (int)_motor_targetADC, err, _motor_adcSpan);
    }

    // Crucero a PWM_MAX mientras el error sea grande — igual que _calibUpdate() en
    // GOING_UP/GOING_DOWN. El proporcional puro nunca llegaba a PWM_MAX salvo que
    // el target estuviera en el extremo opuesto exacto, y con errores grandes pero
    // no máximos el PWM resultante no bastaba para vencer fricción localizada cerca
    // de los extremos → bucle STALL→cooldown→reintento parcial. (2026-07-22)
    int targetPWM;
    if (absErr > POSITION_CRUISE_ERR) {
        targetPWM = _pwm_max;
    } else {
        targetPWM = _pwm_min + (min((uint16_t)absErr, _motor_adcSpan) * (_pwm_max - _pwm_min)) / _motor_adcSpan;
        targetPWM = constrain(targetPWM, _pwm_min, _pwm_max);
    }

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
        _pwm_min = PWM_MIN;  // fallback config.h — NVS vacío o inválido (2026-05-19)
        _pwm_max = PWM_MAX;
        log_w("[MOTOR] PWM no en NVS, usando defaults: min=%u max=%u", _pwm_min, _pwm_max);
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
        } else if (_stallProtectStart == 0) {
            // Motor recién activado contra una posición que no se mueve (2026-08-07):
            // sin esto, _stallProtectStart nunca se arma si el fader ya está pegado
            // al tope físico desde el primer tick — el corte de seguridad por timeout
            // (rama siguiente) nunca dispara porque su guarda "> 0" es siempre falsa.
            // Arrancar la cuenta aquí garantiza que STALL_PROTECT_MS después SIEMPRE
            // se evalúa, se haya movido el fader o no.
            _stallProtectStart = millis();
        } else if (millis() - _stallProtectStart > STALL_PROTECT_MS) {
            _hwOff();  // _motor_hw_active = false → no refire inmediato
            log_e("[MOTOR] STALL — tope físico, motor apagado (adc=%d)", _motor_adcPos);
            _stallProtectStart  = 0;  // evitar refire inmediato al reactivarse
            _stallCooldownUntil = millis() + STALL_COOLDOWN_MS;  // bloquear re-armado (2026-07-20)
            // En DAW absoluto (OFF/READ) cualquier STALL se asume obstrucción del usuario
            // aunque sujete el fader QUIETO (sin delta ADC no hay touch): así soltar cancela
            // el cooldown en vez de pulsar a ciegas cada 2s. (2026-07-20)
            _stallCooldownFromTouch = _motor_manualTouchDetected || _dawAbsolute;
            if (_motor_state == MotorState::MOVING_TO_TARGET) {
                _motor_state       = MotorState::AT_TARGET;
                _atTargetStartTime = millis();
            }
        }
    } else if (!_motor_hw_active) {
        _stallProtectStart   = 0;
        _stallProtectLastADC = _motor_adcPos;
    }
    // ─────────────────────────────────────────────────────────────

    switch (_motor_state) {

    case MotorState::IDLE:
        // Esperando órdenes S3 o usuario.
        if (!_connected && _motor_adcPos > (_floorADC() + 10) &&
            (_goToMinRestADC == 0 || _motor_adcPos > _goToMinRestADC + 40) &&
            millis() >= _stallCooldownUntil && !_motor_manualTouchDetected) {
            // Sin S3: fader debe estar en 0 — bajar (boot o desconexión S3)
            _goToMinStallStart = 0;
            _goToMinLastADC    = _motor_adcPos;
            _motor_state = MotorState::GOING_TO_MIN;
            _hwDown(_pwm_max);
            log_i("[MOTOR-STATE] IDLE → GOING_TO_MIN (adc=%d)", _motor_adcPos);
        } else {
            // CONNECTED o ya en 0: motor quieto — solo apagar si estaba activo
            if (_motor_hw_active) _hwOff();
        }
        break;

    case MotorState::GOING_TO_MIN: {
        // Llegada por threshold o por stall (fader en tope físico, ADC no baja más)
        bool arrived = (_motor_adcPos <= (_floorADC() + 60));

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
            _goToMinRestADC    = _motor_adcPos;   // latch: no re-disparar hasta que el fader suba de verdad (2026-07-20)
            _motor_targetADC   = _motor_adcPos;
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
        // En posición — fricción mecánica mantiene el fader, motor completamente apagado
        if (_motor_hw_active) _hwOff();
        // Umbral 80 > arrival threshold 60 — evita bucle AT_TARGET↔GOING_TO_MIN en tope físico
        if (!_connected && _motor_adcPos > (_floorADC() + 80) &&
            (_goToMinRestADC == 0 || _motor_adcPos > _goToMinRestADC + 40) &&
            millis() >= _stallCooldownUntil && !_motor_manualTouchDetected) {
            _goToMinStallStart = 0;
            _goToMinLastADC    = _motor_adcPos;
            _motor_state       = MotorState::GOING_TO_MIN;
            _hwDown(_pwm_max);
            log_i("[MOTOR-STATE] AT_TARGET → GOING_TO_MIN (desconectado, adc=%d)", _motor_adcPos);
        }
        break;

    default:
        _hwOff();
        break;
    }
    // └────────────────────────────────────────────────────────────
}

void setADC(uint16_t v) {
    // Saturar, NO rechazar: rechazar congela _motor_adcPos y fabrica topes falsos en
    // calibración (la última lectura válida se toma como tope físico). (2026-07-20)
    if (v < MOTOR_ADC_MIN) v = MOTOR_ADC_MIN;
    if (v > MOTOR_ADC_MAX) v = MOTOR_ADC_MAX;
    // Bypass spike guard durante: calibración, bajando a mínimo, O usuario moviendo fader (2026-05-19)
    bool inCalibFlow = _isCalibrating() ||
                       _motor_state == MotorState::GOING_TO_MIN ||
                       _motor_state == MotorState::WAITING_FOR_CALIB ||
                       _motor_manualTouchDetected;  // setADCDelta() ya lo activó en el mismo loop
    if (!inCalibFlow && _motor_adcPos > 0 &&
        abs((int)v - (int)_motor_adcPos) > ADC_SPIKE_GUARD) return;
    _motor_adcPos = v;
}

void setADCDelta(uint16_t currentADC) {
    // Detecta movimiento manual del fader: delta ADC rápido
    // FaderTouch::isTouched() DESACTIVADO — falsos positivos en tope mecánico (2026-05-19)
    // TODO: reactivar cuando FaderTouch sea fiable en todo el recorrido

    // Inicialización en primera llamada (evitar falsa detección en boot)
    if (_motor_lastADCForDelta == 0) {
        _motor_lastADCForDelta = currentADC;
        return;  // Solo calibrar, no detectar delta en boot
    }

    // No detectar usuario durante calibración ni goToMin
    static bool _prevInCalibFlow = false;
    bool inCalibFlow = _isCalibrating() ||
                       _motor_state == MotorState::GOING_TO_MIN ||
                       _motor_state == MotorState::CALIBRATING;
    if (inCalibFlow) {
        _prevInCalibFlow       = true;
        _motor_lastADCForDelta = currentADC;
        _motor_manualTouchDetected = false;  // motor bajo control automático — resetear flag usuario (2026-05-27)
        return;
    }

    // Primer ciclo tras salir de calibFlow: resetear ventana delta para evitar falso touch (2026-06-14)
    // _deltaWindowRef queda con el ADC de antes del goToMin → delta enorme → falsa detección
    if (_prevInCalibFlow) {
        _prevInCalibFlow       = false;
        _deltaWindowRef        = currentADC;
        _deltaWindowStart      = millis();
        _motor_lastADCForDelta = currentADC;
        return;
    }

    // MOVING_TO_TARGET: suprimir solo si ADC va en la misma dirección que el target (2026-05-24)
    // Motor rápido genera delta > threshold en su propia dirección → falsa detección
    // Si el ADC va en dirección CONTRARIA al target → usuario oponiéndose → dejar pasar
    if (_motor_state == MotorState::MOVING_TO_TARGET) {
        bool motorGoingUp = (_motor_targetADC > _motor_lastADCForDelta);
        bool adcGoingUp   = ((int)currentADC >= (int)_motor_lastADCForDelta);
        if (motorGoingUp == adcGoingUp) {
            _motor_lastADCForDelta = currentADC;
            return;  // Motor en su dirección — no es el usuario
        }
        // Dirección opuesta: usuario oponiéndose → detección normal
    }

    // Spike eléctrico: raw ADC salta más de ADC_SPIKE_GUARD desde posición filtrada
    // setADC() ya rechaza el spike para la posición, pero sin este guard setADCDelta()
    // re-dispara "usuario master" justo tras el debounce → bloqueo indefinido de targets S3
    if (!_motor_manualTouchDetected && _motor_adcPos > 0 &&
        abs((int)currentADC - (int)_motor_adcPos) > ADC_SPIKE_GUARD) {
        return;  // No actualizar referencia — mantiene base válida para siguiente lectura
    }

    // Delta acumulado en ventana de tiempo — captura movimientos lentos (2026-05-27)
    uint32_t nowMs = millis();
    if (nowMs - _deltaWindowStart >= TOUCH_DELTA_WINDOW_MS) {
        _deltaWindowRef   = currentADC;
        _deltaWindowStart = nowMs;
    }
    uint16_t delta = abs((int)currentADC - (int)_deltaWindowRef);
    _motor_lastADCForDelta = currentADC;

    // Threshold adaptativo: bajo cuando motor off (AT_TARGET/IDLE), alto cuando motor activo (2026-05-27)
    // En AT_TARGET el motor está apagado → todo delta es del usuario, no del motor
    uint16_t touchThreshold = (_motor_state == MotorState::AT_TARGET ||
                               _motor_state == MotorState::IDLE)
        ? MANUAL_TOUCH_AT_TARGET_THRESHOLD
        : MANUAL_TOUCH_THRESHOLD;
    bool userTouch = (delta > touchThreshold);

    // Periodo de gracia tras entrar en AT_TARGET: el crucero a PWM_MAX (POSITION_CRUISE_ERR)
    // da al motor más inercia al llegar — el overshoot/asentamiento mecánico puede superar
    // el umbral sensible de AT_TARGET sin que el usuario haya tocado nada. (2026-07-22)
    if (userTouch && _motor_state == MotorState::AT_TARGET &&
        (millis() - _atTargetStartTime) < AT_TARGET_TOUCH_GRACE_MS) {
        userTouch = false;
    }

    if (userTouch) {
        _motor_manualTouchStartTime = millis();  // Refresh en cada movimiento — evita reset prematuro (2026-05-19)
        if (!_motor_manualTouchDetected) {
            // Primera detección: usuario toma control — INMEDIATO
            _motor_manualTouchDetected = true;
            // DAW absoluto (AUTO_OFF/READ): reportamos touch a Logic pero el motor
            // NO cede — sigue persiguiendo el target y devuelve el fader. (2026-07-20)
            if (!_dawAbsolute) {
                Motor::stop();
                _motor_state = MotorState::AT_TARGET;  // Motor cede control
                _userDropTarget = currentADC;
                _motor_targetADC = currentADC;         // ADC actual = nueva posición aceptada
            }
            log_w("[MOTOR] Usuario touch: adc=%d delta=%d dawAbs=%d", currentADC, delta, _dawAbsolute);
        }
    } else if (_motor_manualTouchDetected) {
        // Sin delta: reset tras DEBOUNCE_MS de inactividad — FaderTouch eliminado (2026-05-19)
        if (millis() - _motor_manualTouchStartTime > MANUAL_TOUCH_DEBOUNCE_MS) {
            _motor_manualTouchDetected = false;
            // Cancelar cooldown STALL si su origen fue esta misma sujeción —
            // ya no hay obstáculo, el motor puede perseguir el target ya (2026-07-20)
            if (_stallCooldownFromTouch) {
                _stallCooldownUntil    = 0;
                _stallCooldownFromTouch = false;
            }
            log_i("[MOTOR] Usuario soltó fader en adc=%d", currentADC);
        }
    }
}

// setTarget — ancla el target ADC directamente (dominio ADC, no PitchBend).
// WHAT: fija _motor_targetADC = target sin remapear. Usado por RS485Handler al
//       desconectar/timeout para anclar el target a la posición ADC actual
//       (Motor::getRawADC()), evitando que el motor persiga un target viejo.
// WHY:  antes esta función también guardaba el valor en _motor_lastMidiTarget
//       para remapearlo como si fuera PitchBend 0-16383 al completar una
//       calibración posterior — pero el llamador siempre pasa un ADC crudo
//       (0-27000), nunca un PitchBend real. Ese remapeo producía un target
//       muy por encima del tope físico vía map() sin clamp. Eliminado. (2026-08-07)
// ASSUMES: solo tiene efecto si ya hay calibración (_motor_phase == DONE);
//          antes de calibrar, el dominio ADC del target no tiene sentido.
void setTarget(uint16_t target) {
    if (_motor_phase != CalibPhase::DONE) return;
    _motor_targetADC = target;
    log_d("[TARGET] adc=%d", target);
}

void off() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
    // Resetear estado — sin esto, si estaba en MOVING_TO_TARGET, el siguiente
    // update() reengancha _positionTick() persiguiendo el target viejo de
    // Logic en vez de respetar la desconexión (goToMin master absoluto). (2026-07-20)
    _motor_state = MotorState::IDLE;
}

void stop() {
    _hwOff();
    _motor_active = false;
    _motor_currentPWM = 0;
}

void setConnected(bool connected) {
    _connected = connected;  // Notificar estado de conexión S3 (2026-05-16 10:52)
    if (connected) _goToMinRestADC = 0;  // limpiar latch anti-rebote al reconectar (2026-07-20)
}

uint16_t getRawADC() {
    return _motor_adcPos;
}

bool isManualTouchDetected() {
    return _motor_manualTouchDetected;
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

    // No interrumpir calibración activa — S3 llama cada 10ms, guard idempotente (2026-05-23)
    if (_isCalibrating() || _motor_state == MotorState::CALIBRATING) return;

    if (_motor_adcPos <= (MOTOR_ADC_MIN + 10)) {
        // Fader EN 0 → calibrar directamente
        if (_motor_state != MotorState::CALIBRATING) {
            _motor_state = MotorState::CALIBRATING;
            startCalib();
            log_i("[MOTOR] requestCalibration: EN 0, iniciando startCalib()");
        }
    } else {
        // Fader ≠ 0 → bajar a 0, luego calibrar automáticamente
        if (!_pendingCalib) {
            _pendingCalib = true;
            _motor_state = MotorState::GOING_TO_MIN;
            goToMin();
            log_i("[MOTOR] requestCalibration: ≠ 0, goToMin() bajando (pendingCalib armado)...");
        }
    }
}

void setTargetFromS3(uint16_t adcTarget) {
    // S3 ordena posición → solo si usuario NO está tocando (usuario es master)
    _s3Target = adcTarget;
    if (_motor_phase != CalibPhase::DONE) {
        static unsigned long _lastNoCal = 0;
        if (millis() - _lastNoCal > 2000) {
            log_w("[MOTOR] setTargetFromS3: no calibrado, ignorando target=%d phase=%d", adcTarget, (int)_motor_phase);
            _lastNoCal = millis();
        }
        return;
    }
    if (_motor_manualTouchDetected) {
        static unsigned long _lastTouch = 0;
        if (millis() - _lastTouch > 1000) {
            log_w("[MOTOR] setTargetFromS3: usuario master, ignorando target=%d", adcTarget);
            _lastTouch = millis();
        }
        return;
    }
    if (millis() < _stallCooldownUntil) {
        static unsigned long _lastCooldownLog = 0;
        if (millis() - _lastCooldownLog > 1000) {
            log_w("[MOTOR] setTargetFromS3: cooldown STALL activo, ignorando target=%d (%lu ms restantes)",
                  adcTarget, _stallCooldownUntil - millis());
            _lastCooldownLog = millis();
        }
        return;
    }
    _motor_targetADC = adcTarget;

    // No reactivar si ya estamos en posición: fricción mantiene el fader sin motor
    if (_motor_state == MotorState::AT_TARGET &&
        abs((int)_motor_adcPos - (int)adcTarget) < DEAD_ZONE) {
        return;
    }

    if (_motor_targetADC != adcTarget || _motor_state != MotorState::MOVING_TO_TARGET) {
        log_i("[MOTOR] → MOVING_TO_TARGET adc=%d target=%d span=%d", _motor_adcPos, adcTarget, _motor_adcSpan);
    }
    _motor_state = MotorState::MOVING_TO_TARGET;
}

// ─── setTargetForced — AUTO_OFF/READ: DAW absoluto (2026-05-30 09:35) ──
// WHAT: Aplica un target ADC sin respetar _motor_manualTouchDetected.
// WHY:  En AUTO_OFF y AUTO_READ el DAW manda en absoluto. Si el usuario
//       empuja el fader, el motor debe volver al target sin esperar
//       debounce. Esta función es la ruta "forced" que usa el handler
//       RS485 cuando AutoMode == OFF/READ.
// ASSUMES: motor calibrado (CalibPhase::DONE). Si no, ignora.
void setTargetForced(uint16_t adcTarget) {
    _s3Target = adcTarget;
    if (_motor_phase != CalibPhase::DONE) {
        static unsigned long _lastNoCalForced = 0;
        if (millis() - _lastNoCalForced > 2000) {
            log_w("[MOTOR] setTargetForced: no calibrado, ignorando target=%d", adcTarget);
            _lastNoCalForced = millis();
        }
        return;
    }
    if (millis() < _stallCooldownUntil) {
        static unsigned long _lastCooldownLogForced = 0;
        if (millis() - _lastCooldownLogForced > 1000) {
            log_w("[MOTOR] setTargetForced: cooldown STALL activo, ignorando target=%d (%lu ms restantes)",
                  adcTarget, _stallCooldownUntil - millis());
            _lastCooldownLogForced = millis();
        }
        return;
    }
    // NO guard _motor_manualTouchDetected — DAW absoluto en OFF/READ
    _motor_targetADC = adcTarget;

    // No reactivar si ya estamos en posición: fricción mantiene el fader
    if (_motor_state == MotorState::AT_TARGET &&
        abs((int)_motor_adcPos - (int)adcTarget) < DEAD_ZONE) {
        return;
    }

    if (_motor_state != MotorState::MOVING_TO_TARGET) {
        static unsigned long _lastForcedLog = 0;
        if (millis() - _lastForcedLog > 2000) {
            log_i("[MOTOR] FORCED → MOVING_TO_TARGET adc=%d target=%d", _motor_adcPos, adcTarget);
            _lastForcedLog = millis();
        }
    }
    _motor_state = MotorState::MOVING_TO_TARGET;
}

void setDawAbsolute(bool on) {
    _dawAbsolute = on;
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

bool isPendingCalib() {
    return _pendingCalib;
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
