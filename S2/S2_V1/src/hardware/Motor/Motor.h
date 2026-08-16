#pragma once
#include <Arduino.h>

namespace Motor {

    // Máquina de estados Motor v2 (2026-05-16 10:45)
    enum class MotorState {
        IDLE,                  // Fader en 0, esperando órdenes S3
        GOING_TO_MIN,          // Bajando a 0 (usuario soltó o pre-calib)
        WAITING_FOR_CALIB,     // En 0, esperando que FLAG_CALIB inicie calibración
        CALIBRATING,           // Máquina calibración en curso
        MOVING_TO_TARGET,      // Moviéndose a posición S3
        AT_TARGET              // En posición objetivo, esperando nuevo comando S3
    };

    enum class CalibState {
        IDLE,
        CALIB_UP,
        CALIB_DOWN,
        DONE,
        ERROR
    };

    // Ciclo de vida
    void init();        // configurar pines + PWM — llamar ANTES de Serial.begin()
    void initPWM();     // leer pwmMin/Max de NVS — si inválido, pwmMin=pwmMax=0 (2026-05-10 20:20)
    void update();      // tick de calibración + control de posición

    // Entrada ADC (desde FaderADC)
    void setADC(uint16_t raw);

    // Control
    void setTarget(uint16_t midiPB14);  // 0-16383, mapea internamente a ADC
    void setConnected(bool connected);  // notifica estado de conexión S3 (2026-05-16 10:52)
    void off();                         // para motor + desactiva control
    void stop();                        // frena motor, mantiene target

    // Consultas
    uint16_t   getRawADC();
    float      getPosition();  // 0.0–1.0
    uint16_t   getADCMin();
    uint16_t   getADCMax();
    bool       isManualTouchDetected();  // SIEMPRE false desde 2026-08-13 (delta ADC eliminado) — gancho reservado para reactivar el capacitivo

    // Calibración
    void       startCalib();
    void       goToMin();  // Baja motor a posición 0 (mínimo), espera órdenes de S3
    CalibState getCalibState();
    bool       isCalibrated();
    bool       isPendingCalib();  // true solo si GOING_TO_MIN es por recalibración pedida, no por desconexión (2026-07-20)

    // Máquina de estados v2 (2026-05-16) — S3 commands
    void       requestCalibration();          // FLAG_CALIB desde S3 → inicia calib o goToMin si necesario
    void       setTargetFromS3(uint16_t adc); // setTarget desde S3 (ADC ya mapeado, respeta guard usuario)
    void       setTargetForced(uint16_t adc); // setTarget DAW absoluto — bypass guard usuario (AUTO_OFF/READ) (2026-05-30 09:35)
    void       setUserDropTarget(uint16_t adc); // Usuario soltó fader en posición ADC
    void       setDawAbsolute(bool on);         // AUTO_OFF/READ: DAW manda en absoluto, usuario no puede fijar posición (2026-07-20)
    MotorState getState();                    // Consulta estado actual

    // Test mode — control directo (2026-05-10 19:54)
    void testUp(uint8_t pwm);
    void testDown(uint8_t pwm);
    void testOff();

    // PWM range (lee de NVS, 0=inválido) (2026-05-10 20:20)
    uint8_t getPWMMin();
    uint8_t getPWMMax();

} // namespace Motor
