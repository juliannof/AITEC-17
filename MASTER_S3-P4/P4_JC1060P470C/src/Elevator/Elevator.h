// src/Elevator/Elevator.h
#pragma once
#include <Arduino.h>

// Elevador de pantalla P4 (motor N20 + DRV8833). Todo el GPIO del motor se
// escribe SOLO desde Core0 (elevatorTick(), llamado desde taskCore0). El panel
// de calibración en UIMenu.cpp (Core1) nunca toca IN1/IN2 directamente — solo
// deposita intención vía las funciones elevatorPanel*() de abajo.

void elevatorInit();   // llamar en setup(), paso 0, antes de cualquier init lenta
void elevatorTick();   // llamar en taskCore0, junto a tickCalibracion()

// ── Panel de calibración manual (llamado SOLO desde UIMenu.cpp / Core1) ──
void elevatorPanelOpen();
void elevatorPanelClose();
void elevatorPanelExtend();
void elevatorPanelRetract();
void elevatorPanelStop();
void elevatorPanelAdjustPwm(int8_t stepSign);   // +1 / -1 -> aplica ELEVATOR_TEST_PWM_STEP
void elevatorPanelSaveCalibration();

// ── Lecturas para pintar el panel (Core1 lee, Core0 escribe) ──
enum class ElevatorMotorState { IDLE, EXTENDING, RETRACTING };

bool elevatorIsCalibrated();
bool elevatorIsHoming();
bool elevatorIsError();
ElevatorMotorState elevatorGetMotorState();
int32_t elevatorGetNetExtendMs();
uint8_t elevatorGetPanelPwm();

// Resultado de la última verificación de guardado — se consume una sola vez
// (pending pasa a false tras leerlo) para que UIMenu sepa cuándo pintar el toast.
struct ElevatorSaveResult {
    bool    pending;
    bool    ok;
    int32_t tExtend;
    int32_t tRetract;
};
ElevatorSaveResult elevatorConsumeSaveResult();

// Resultado del corte de seguridad por PWM sostenido — mismo patrón consume-once.
bool elevatorConsumeSafetyCutoff();
