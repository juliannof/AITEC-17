// src/Elevator/Elevator.cpp
#include "Elevator.h"
#include "../config.h"
#include <Preferences.h>

// logicConnectionState ya está declarado extern en config.h — lo define main.cpp.

namespace {

enum class Phase {
    BOOT_HOMING,
    IDLE_UNCALIBRATED,
    IDLE_AUTO,
    PANEL_ACTIVE,
    PANEL_SAVING_RETRACT,
    AUTO_EXTENDING,
    AUTO_RETRACTING,
};

enum class PendingDir { NONE, EXTEND, RETRACT };

Phase _phase = Phase::BOOT_HOMING;

// Persistencia (namespace NVS "uimenu", compartido con UIMenu.cpp)
uint8_t  _elevatorPwm        = ELEVATOR_TEST_PWM_START;
uint32_t _elevatorExtendMs   = 0;
bool     _elevatorCalibrated = false;

// Estado runtime — Core0 escribe, Core1 solo lee (getters de abajo)
ElevatorMotorState _motorState  = ElevatorMotorState::IDLE;
int32_t  _netExtendMs        = 0;
uint32_t _lastTickMs         = 0;
uint32_t _phaseStartMs       = 0;
bool     _error               = false;
bool     _wasConnected        = false;
PendingDir _pendingDir        = PendingDir::NONE;
int32_t  _saveTExtendCandidate = 0;   // netExtendMs congelado al pulsar "Guardar"

// Panel: Core1 escribe intención, Core0 la consume y limpia
volatile bool   _panelOpen     = false;
volatile bool   _reqExtend     = false;
volatile bool   _reqRetract    = false;
volatile bool   _reqStop       = false;
volatile bool   _reqSave       = false;
volatile int16_t _reqPwmDelta  = 0;
uint8_t  _panelPwm            = ELEVATOR_TEST_PWM_START;
uint32_t _lastPanelActivityMs = 0;

// Resultados consume-once para que UIMenu pinte el toast
ElevatorSaveResult _saveResult        = {false, false, 0, 0};
bool               _safetyCutoffPending = false;

bool _switchActive() {
    return digitalRead(LIMIT_SWITCH_PIN) == LIMIT_SWITCH_ACTIVE_LEVEL;
}

void _stop() {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, 0);
    _motorState = ElevatorMotorState::IDLE;
}

void _extend(uint8_t pwm) {
    analogWrite(MOTOR_IN2, 0);
    analogWrite(MOTOR_IN1, pwm);
    _motorState = ElevatorMotorState::EXTENDING;
}

void _retract(uint8_t pwm) {
    analogWrite(MOTOR_IN1, 0);
    analogWrite(MOTOR_IN2, pwm);
    _motorState = ElevatorMotorState::RETRACTING;
}

void _enterPanelActive() {
    _stop();
    _pendingDir  = PendingDir::NONE;
    _reqExtend = _reqRetract = _reqStop = _reqSave = false;
    _reqPwmDelta = 0;
    _panelPwm = _elevatorCalibrated ? _elevatorPwm : ELEVATOR_TEST_PWM_START;
    _lastPanelActivityMs = millis();
    _phase = Phase::PANEL_ACTIVE;
}

} // namespace

void elevatorInit() {
    // Arranque seguro: motor cortado antes de cualquier init que pueda demorarse.
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);

    Preferences prefs;
    prefs.begin("uimenu", true);
    _elevatorPwm        = prefs.getUChar("elevatorPwm", ELEVATOR_TEST_PWM_START);
    _elevatorExtendMs   = prefs.getUInt("elevatorExtendMs", 0);
    _elevatorCalibrated = prefs.getBool("elevatorCalibrated", false);
    prefs.end();

    _panelPwm = _elevatorCalibrated ? _elevatorPwm : ELEVATOR_TEST_PWM_START;
    _lastTickMs   = millis();
    _phaseStartMs = _lastTickMs;

    if (_switchActive()) {
        _phase = _elevatorCalibrated ? Phase::IDLE_AUTO : Phase::IDLE_UNCALIBRATED;
        log_i("[ELEVATOR] Home ya activo al arrancar — sin homing");
    } else {
        uint8_t homingPwm = _elevatorCalibrated ? _elevatorPwm : ELEVATOR_HOMING_PWM_DEFAULT;
        log_i("[ELEVATOR] Homing de arranque: PWM=%u, timeout=%dms", homingPwm, ELEVATOR_HOMING_TIMEOUT_MS);
        _phase = Phase::BOOT_HOMING;
        _retract(homingPwm);
    }
}

void elevatorTick() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastTickMs;
    _lastTickMs  = now;

    bool switchActive = _switchActive();

    if (_motorState == ElevatorMotorState::EXTENDING) {
        _netExtendMs += (int32_t)dt;
    } else if (_motorState == ElevatorMotorState::RETRACTING) {
        _netExtendMs -= (int32_t)dt;
        if (switchActive) _netExtendMs = 0;  // autocorrección contra la posición física real
    }
    if (_netExtendMs < 0) _netExtendMs = 0;

    // Aislamiento de la medida: el panel abierto suspende el automatismo de
    // producción de inmediato, sea cual sea la fase en curso (salvo homing de
    // arranque, que es un paso único de seguridad y se deja terminar).
    if (_panelOpen && _phase != Phase::PANEL_ACTIVE &&
        _phase != Phase::PANEL_SAVING_RETRACT && _phase != Phase::BOOT_HOMING) {
        _enterPanelActive();
    }

    switch (_phase) {

    case Phase::BOOT_HOMING: {
        if (switchActive) {
            _stop();
            _netExtendMs = 0;
            _phase = _elevatorCalibrated ? Phase::IDLE_AUTO : Phase::IDLE_UNCALIBRATED;
            log_i("[ELEVATOR] Home OK");
        } else if (now - _phaseStartMs > ELEVATOR_HOMING_TIMEOUT_MS) {
            _stop();
            _error = true;
            _phase = Phase::IDLE_UNCALIBRATED;
            log_e("[ELEVATOR] TIMEOUT homing de arranque — motor cortado, sin reintento");
        }
        return;
    }

    case Phase::IDLE_UNCALIBRATED:
        return; // reposo total — solo sale vía elevatorPanelOpen() (guard de arriba)

    case Phase::IDLE_AUTO: {
        bool connected = (logicConnectionState == ConnectionState::CONNECTED);
        if (connected && !_wasConnected) {
            _stop();
            _phase = Phase::AUTO_EXTENDING;
            _phaseStartMs = now;
        } else if (!connected && _wasConnected) {
            _stop();
            _phase = Phase::AUTO_RETRACTING;
            _phaseStartMs = now;
        }
        _wasConnected = connected;
        return;
    }

    case Phase::AUTO_EXTENDING: {
        if (_motorState == ElevatorMotorState::IDLE) {
            _extend(_elevatorPwm);
        } else if (_netExtendMs >= (int32_t)_elevatorExtendMs) {
            _stop();
            _phase = Phase::IDLE_AUTO;
        } else if (now - _phaseStartMs > _elevatorExtendMs + ELEVATOR_HOMING_TIMEOUT_MS) {
            _stop();
            _error = true;
            _phase = Phase::IDLE_AUTO;
            log_e("[ELEVATOR] TIMEOUT despliegue automático");
        }
        return;
    }

    case Phase::AUTO_RETRACTING: {
        if (_motorState == ElevatorMotorState::IDLE) {
            _retract(_elevatorPwm);
        } else if (switchActive) {
            _stop();
            _phase = Phase::IDLE_AUTO;
        } else if (now - _phaseStartMs > ELEVATOR_HOMING_TIMEOUT_MS) {
            _stop();
            _error = true;
            _phase = Phase::IDLE_AUTO;
            log_e("[ELEVATOR] TIMEOUT retracción automática");
        }
        return;
    }

    case Phase::PANEL_ACTIVE: {
        if (!_panelOpen) {
            _stop();
            _pendingDir = PendingDir::NONE;
            _phase = _elevatorCalibrated ? Phase::IDLE_AUTO : Phase::IDLE_UNCALIBRATED;
            _wasConnected = (logicConnectionState == ConnectionState::CONNECTED);
            return;
        }

        // Aplica dirección pendiente tras el tick de frenado intermedio
        if (_pendingDir != PendingDir::NONE && _motorState == ElevatorMotorState::IDLE) {
            if (_pendingDir == PendingDir::EXTEND) _extend(_panelPwm);
            else                                    _retract(_panelPwm);
            _pendingDir = PendingDir::NONE;
        }

        if (_reqPwmDelta != 0) {
            int16_t newPwm = (int16_t)_panelPwm + _reqPwmDelta;
            _reqPwmDelta = 0;
            if (newPwm < ELEVATOR_TEST_PWM_MIN) newPwm = ELEVATOR_TEST_PWM_MIN;
            if (newPwm > ELEVATOR_TEST_PWM_MAX) newPwm = ELEVATOR_TEST_PWM_MAX;
            _panelPwm = (uint8_t)newPwm;
            if      (_motorState == ElevatorMotorState::EXTENDING)  _extend(_panelPwm);
            else if (_motorState == ElevatorMotorState::RETRACTING) _retract(_panelPwm);
            _lastPanelActivityMs = now;
        }

        if (_reqStop) {
            _reqStop = false;
            _pendingDir = PendingDir::NONE;
            _stop();
            _lastPanelActivityMs = now;
        } else if (_reqExtend) {
            _reqExtend = false;
            if (_motorState == ElevatorMotorState::RETRACTING) {
                _stop();                          // frena IN1=IN2=LOW primero
                _pendingDir = PendingDir::EXTEND;  // se aplica el tick siguiente
            } else if (_motorState == ElevatorMotorState::IDLE) {
                _extend(_panelPwm);
            }
            _lastPanelActivityMs = now;
        } else if (_reqRetract) {
            _reqRetract = false;
            if (_motorState == ElevatorMotorState::EXTENDING) {
                _stop();
                _pendingDir = PendingDir::RETRACT;
            } else if (_motorState == ElevatorMotorState::IDLE) {
                _retract(_panelPwm);
            }
            _lastPanelActivityMs = now;
        } else if (_reqSave) {
            _reqSave = false;
            if (_motorState == ElevatorMotorState::IDLE &&
                _pendingDir == PendingDir::NONE && _netExtendMs > 0) {
                _saveTExtendCandidate = _netExtendMs;
                _phase = Phase::PANEL_SAVING_RETRACT;
                _phaseStartMs = now;
                return;
            }
        }

        // Tope físico durante retracción manual
        if (_motorState == ElevatorMotorState::RETRACTING && switchActive) {
            _stop();
            _pendingDir = PendingDir::NONE;
        }

        // Corte de seguridad: PWM sostenido 10s sin ninguna interacción
        if (_motorState != ElevatorMotorState::IDLE &&
            now - _lastPanelActivityMs > ELEVATOR_PANEL_IDLE_TIMEOUT_MS) {
            _stop();
            _pendingDir = PendingDir::NONE;
            _safetyCutoffPending = true;
            log_w("[ELEVATOR] Corte de seguridad — PWM sostenido en panel");
        }
        return;
    }

    case Phase::PANEL_SAVING_RETRACT: {
        if (_motorState == ElevatorMotorState::IDLE) {
            _retract(_panelPwm);
        } else if (switchActive) {
            _stop();
            int32_t tRetract = (int32_t)(now - _phaseStartMs);
            int32_t tExtend  = _saveTExtendCandidate;
            int32_t diff = (tExtend > tRetract) ? (tExtend - tRetract) : (tRetract - tExtend);
            int32_t tolerance = (tExtend * ELEVATOR_CALIB_TOLERANCE_PCT) / 100;
            bool ok = diff <= tolerance;
            if (ok) {
                _elevatorPwm      = _panelPwm;
                _elevatorExtendMs = (uint32_t)tExtend;
                _elevatorCalibrated = true;
                Preferences prefs;
                prefs.begin("uimenu", false);
                prefs.putUChar("elevatorPwm", _elevatorPwm);
                prefs.putUInt("elevatorExtendMs", _elevatorExtendMs);
                prefs.putBool("elevatorCalibrated", true);
                prefs.end();
                log_i("[ELEVATOR] Calibración guardada: pwm=%u extend=%dms retract=%dms",
                      _elevatorPwm, (int)tExtend, (int)tRetract);
            } else {
                log_w("[ELEVATOR] Calibración RECHAZADA: extend=%dms retract=%dms (tolerancia %d%%)",
                      (int)tExtend, (int)tRetract, ELEVATOR_CALIB_TOLERANCE_PCT);
            }
            _saveResult = { true, ok, tExtend, tRetract };
            _netExtendMs = 0;
            _phase = _panelOpen ? Phase::PANEL_ACTIVE
                                 : (_elevatorCalibrated ? Phase::IDLE_AUTO : Phase::IDLE_UNCALIBRATED);
            if (!_panelOpen) _wasConnected = (logicConnectionState == ConnectionState::CONNECTED);
            _lastPanelActivityMs = now;
        } else if (now - _phaseStartMs > ELEVATOR_HOMING_TIMEOUT_MS) {
            _stop();
            _saveResult = { true, false, _saveTExtendCandidate, (int32_t)(now - _phaseStartMs) };
            _error = true;
            _phase = _panelOpen ? Phase::PANEL_ACTIVE
                                 : (_elevatorCalibrated ? Phase::IDLE_AUTO : Phase::IDLE_UNCALIBRATED);
            log_e("[ELEVATOR] TIMEOUT en verificación de guardado");
        }
        return;
    }

    } // switch
}

// ── API panel (Core1) — solo fija intención, nunca toca GPIO ──
void elevatorPanelOpen()  { _panelOpen = true; }
void elevatorPanelClose() { _panelOpen = false; }
void elevatorPanelExtend()  { _reqExtend = true; }
void elevatorPanelRetract() { _reqRetract = true; }
void elevatorPanelStop()    { _reqStop = true; }
void elevatorPanelAdjustPwm(int8_t stepSign) {
    _reqPwmDelta += (int16_t)(stepSign >= 0 ? ELEVATOR_TEST_PWM_STEP : -ELEVATOR_TEST_PWM_STEP);
}
void elevatorPanelSaveCalibration() { _reqSave = true; }

// ── Getters (Core1 lee) ──
bool elevatorIsCalibrated() { return _elevatorCalibrated; }
bool elevatorIsHoming()     { return _phase == Phase::BOOT_HOMING; }
bool elevatorIsError()      { return _error; }
ElevatorMotorState elevatorGetMotorState() { return _motorState; }
int32_t elevatorGetNetExtendMs() { return _netExtendMs; }
uint8_t elevatorGetPanelPwm()    { return _panelPwm; }

ElevatorSaveResult elevatorConsumeSaveResult() {
    ElevatorSaveResult r = _saveResult;
    _saveResult.pending = false;
    return r;
}

bool elevatorConsumeSafetyCutoff() {
    bool v = _safetyCutoffPending;
    _safetyCutoffPending = false;
    return v;
}
