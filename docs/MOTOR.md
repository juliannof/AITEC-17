# MOTOR — Control DRV8833 y Calibración (S2 Slave)

Documentación exhaustiva del subsistema de motor fader. Incluye hardware, máquina de calibración, control de posición, protección de topes y diagnóstico.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-19  
**Estado:** En producción (17 faders activos)

---

## 1. HARDWARE MOTOR

### 1.1 DRV8833 H-Bridge

| Parámetro | Valor |
|-----------|-------|
| **Chip** | DRV8833 Dual H-Bridge |
| **EN (nSLEEP)** | GPIO14 |
| **IN1** | GPIO18 |
| **IN2** | GPIO16 |
| **Control** | `analogWrite()` — NO LEDC |
| **Frecuencia PWM** | 5kHz (analogWrite predeterminado) |
| **Rango PWM** | 0-255 (8-bit) |
| **PWM operacional** | 100-160 (calibrado) |

**CRÍTICO — analogWrite vs LEDC:**
LovyanGFX backlight (GPIO3) agota los canales LEDC del ESP32-S2. El motor DEBE usar `analogWrite()` (API simple, sin conflictos de recursos). La migración a LEDC fue intentada y revertida (2026-05-10).

### 1.2 Sensor de Posición

**ADS1115 I2C ADC (16-bit, ±4.096V)**
- Rango físico real: ~44–26450 (varía por unidad)
- ISR-driven, 860 SPS
- Latencia: 0-2µs (no polling)
- Mejora: 6-15× vs ADC nativo

Ver **FADER.md** para especificación completa de ADS1115.

### 1.3 Rango ADC y Topes Físicos

El fader tiene topes mecánicos físicos. El ADC no llega a 0 ni a 32767 — los valores reales dependen del hardware individual:

| Parámetro | Valor típico | Notas |
|-----------|--------------|-------|
| `MOTOR_ADC_MIN` | 20 | Cota baja para rechazar ruido (config.h) |
| `MOTOR_ADC_MAX` | 27000 | Cota alta esperada (config.h) |
| Tope físico inferior | ~44 (varía) | ADC real cuando fader toca fondo |
| Tope físico superior | ~26450 (varía) | ADC real cuando fader toca techo |
| `_calibratedFaderMin` | bot + margen | Calculado en calibración |
| `_calibratedFaderMax` | top - margen | Calculado en calibración |

**MOTOR_ADC_MIN ≠ tope físico inferior.** MOTOR_ADC_MIN es solo un filtro de ruido. El tope físico real se determina en calibración (SETTLE_DOWN → `_calibratedFaderMin`).

Esta distinción causó el bug de motor caliente (2026-05-19): la condición `ADC <= MOTOR_ADC_MIN + 10 = 30` nunca se cumplía porque el fader físico no baja de ~44. El motor seguía empujando indefinidamente contra el tope.

### 1.4 Orden de Inicialización en setup() — CRÍTICO (2026-05-16)

**Líneas exactas de main.cpp:**

```c
setup() {
  // 1️⃣  LÍNEA 120-121: Configura GPIO14 (MOTOR_EN) ANTES de Motor::init()
  pinMode(MOTOR_EN, OUTPUT);
  digitalWrite(MOTOR_EN, LOW);      ← ⚠️ INMEDIATO (evita movement)
  
  // 2️⃣ LÍNEA 122: Safety delay
  delay(10);
  
  // 3️⃣ LÍNEA 123: Motor::init() — silencia motor (EN ya LOW)
  Motor::init();
  
  // LÍNEA 124: Serial.begin()
  Serial.begin(115200);
  
  // LÍNEA 132: Lee PWM range de NVS (guardado por SAT)
  Motor::initPWM();             ← ⚠️ SAT guarda pwmMin/pwmMax tras calibración
  
  // ... Display, Neopixels, Encoder, ButtonManager, SatMenu init ...
  
  // LÍNEA 187: ⚠️ CRÍTICO — ADC debe estar listo ANTES de Motor::update()
  faderADC.begin();
  
  // ... más hardware ...
  
  // LÍNEA 233: Motor baja a posición 0 ANTES de RS485
  Motor::goToMin();  // ← Baja fader, espera órdenes S3
  
  // LÍNEA 236: RS485 init — ÚLTIMO (Motor bajando, ADC listo)
  rs485.begin(slaveId);
}
```

**Estado final tras setup():**
- Motor EN (GPIO14) = activo, bajando hacia posición 0
- PWM range = cargado de NVS (si SAT calibró previamente)
- ADC = listo (FaderADC inicializado)
- Fader = bajando, llegará a 0 en loop() dentro de ~1-2 segundos
- RS485 = escuchando a S3
- Motor esperará orden de calibración (FLAG_CALIB) de S3

**¿Por qué este orden?**

1. **Motor EN LOW ANTES de Motor::init():** Evita pulso accidental en pines IN1/IN2 durante init
2. **Motor::init() ANTES de Serial:** Configura PWM sin debug output
3. **Motor::initPWM() DESPUÉS de Serial:** Lee NVS (requiere log output si hay error)
4. **faderADC.begin() ANTES de SAT init:** Motor necesita feedback ADC en loop()
5. **Motor::goToMin() ANTES de RS485:** Garantiza fader en posición 0 al recibir órdenes S3
6. **RS485 init ÚLTIMO:** Todos los módulos listos para procesar MasterPackets (incluyendo FLAG_CALIB)

---

## 2. ARQUITECTURA MOTOR v3 — PRIORIDADES DE CONTROL (2026-05-16 18:45)

### 2.0.1 Jerarquía de Prioridades — VINCULANTE

```
PRIORIDAD 1 (MÁXIMA):  Usuario mueve fader → Motor para INMEDIATAMENTE
PRIORIDAD 2:           Motor::goToMin() ejecuta SIEMPRE si no conectado a S3
PRIORIDAD 3:           S3 ordena posición → Motor se mueve SOLO si usuario NO toca
PRIORIDAD 4 (MÍNIMA):  Sin comando: Motor idle en posición actual
```

**Principio fundamental:** El usuario tiene control físico absoluto. S3 es esclavo que responde, no maestro que ordena.

### 2.0.2 Flujo Completo Operación (2026-05-16 18:45)

```
SETUP:
  Motor::init()         ← Configura pines
  Motor::initPWM()      ← Lee PWM de NVS (o fallback config.h)
  Motor::goToMin()      ← INMEDIATAMENTE baja a 0
  
LOOP (mientras motor bajando):
  Motor::update()       ← Máquina de estados baja a 0
  Motor::setADC()       ← Recibe posición ADC desde FaderADC
  Motor::setADCDelta()  ← Detecta movimiento usuario

CUANDO FADER LLEGA A 0:
  Motor en AT_TARGET (posición 0)
  Motor apagado
  Esperando órdenes

ESCENARIOS DURANTE OPERACIÓN:

  1️⃣  Calibración (S3 ordena, independiente de Logic):  (2026-05-16 19:24)
      ├─ S3 envía FLAG_CALIB al boot
      │  └─ Motor::requestCalibration()  ← procesado ANTES de desconexión
      │     ├─ Si ADC ≠ 0: Motor::goToMin() baja a 0
      │     └─ Si ADC = 0: startCalib() directo
      ├─ Motor transiciona: GOING_TO_MIN → CALIBRATING
      └─ BuildResponse() reporta CALIB_DONE cuando completa
      
  2️⃣  S3 conectado + usuario NO toca (post-calibración):
      └─ S3 envía setTarget(X):
         └─ Motor se mueve a X (si usuario NO toca)

  3️⃣  Usuario mueve fader:
      ├─ Motor::setADCDelta() detecta delta grande O FaderTouch activo
      ├─ Motor::stop() INMEDIATAMENTE
      ├─ Usuario es MASTER → ADC actual = nueva posición
      ├─ touchState=1 reportado a S3 vía RS485
      └─ S3 ignora targets mientras usuario toque (setTargetFromS3 rechaza)

  4️⃣  Usuario suelta fader:
      ├─ _motor_manualTouchDetected = false (después 200ms debounce)
      └─ S3 puede enviar nuevo target (Motor acepta)

  5️⃣  S3 desconectado (Logic no conectado):
      ├─ Motor::setConnected(false) ejecutado
      ├─ Motor::goToMin() SIEMPRE activo
      └─ Fader baja a 0 indefinidamente (IDLE loop)

  6️⃣  S3 conectado (Logic conectado):
      ├─ Motor::setConnected(true) ejecutado
      ├─ Motor NO baja automáticamente
      └─ Espera órdenes S3
```

### 2.0.3 Variables de Estado y Guards

```cpp
// Estado conexión S3
static bool _connected;                    ← setConnected() actualiza esto

// Detección movimiento usuario
static bool _motor_manualTouchDetected;    ← setADCDelta() actualiza
static uint32_t _motor_manualTouchStartTime;
static uint16_t _motor_lastADCForDelta;

// Máquina estados
static MotorState _motor_state;            ← IDLE, GOING_TO_MIN, CALIBRATING, MOVING_TO_TARGET, AT_TARGET

// Flags
static bool _pendingCalib;                 ← requestCalibration() pone en true
static bool _motor_goingToMin;             ← goToMin() pone en true

// Protección hardware (2026-05-19)
static bool _motor_hw_active;              ← true cuando _hwUp()/_hwDown() activos
```

---

## 2. CONTROL DE MOTOR

### 2.1 APIs Críticas (Motor.h)

```cpp
// Inicialización (setup)
Motor::init()              // Configura pines (EN→LOW, IN1/IN2 PWM 20kHz)
Motor::initPWM()           // Lee pwmMin/Max de NVS (SAT las guarda)
Motor::goToMin()           // Baja fader a posición 0 (MASTER, ejecuta SIEMPRE si !_connected)
Motor::off()               // Apaga motor (emergencia)

// Calibración (v3 — 2026-05-16)
Motor::requestCalibration() // FLAG_CALIB desde RS485 → baja a 0 si necesario, luego calibra
Motor::startCalib()        // Inicia máquina calibración (KICK_UP → DONE) — REQUIERE estar en 0
Motor::getCalibState()     // Estado actual (IDLE/CALIB_UP/CALIB_DOWN/DONE/ERROR)

// Control de usuario (MASTER — máxima prioridad)
Motor::setADCDelta(uint16_t currentADC) // Detecta movimiento usuario (delta > 500 O capacitivo)
                           // Si activo: Motor::stop(), usuario toma control, ADC = target

// Control desde S3 (ESCLAVO — solo si usuario NO toca)
Motor::setTargetFromS3(uint16_t adcTarget) // S3 ordena posición — RECHAZADO si usuario toca
Motor::setConnected(bool connected)        // Notifica estado conexión S3 (goToMin respeta esto)

// Control en loop (post-calibración)
Motor::setADC(uint16_t pos)  // Actualiza _motor_adcPos desde FaderADC (ejecutar SIEMPRE)
Motor::update()              // Máquina de estado (ejecutar SIEMPRE en main loop)

// Diagnóstico & Test
Motor::getRawADC()         // Lectura ADC actual (_motor_adcPos)
Motor::getPosition()       // Posición normalizada 0.0-1.0
Motor::getADCMin() / getADCMax() // Rango calibrado
Motor::getState()          // MotorState actual (IDLE, GOING_TO_MIN, etc.)
Motor::testUp(pwm) / testDown(pwm) / testOff()  // Test Manual SAT
```

### 2.2 Control de Posición (Post-calibración)

```cpp
Motor::setTarget(uint16_t target) {
    // target: 0-16383 (rango Logic, 14-bit MIDI completo)
    // _adcMin/_adcMax: valores calibrados
    
    // Mapear Logic range → ADC range
    uint16_t targetADC = _adcMin + (target * (_adcMax - _adcMin) / 16383);
    
    // Comparar con posición actual
    int16_t error = targetADC - _adcPos;  // -27000..+27000
    
    // Dead zone: si |error| < 50 → apagar motor
    if (abs(error) < DEAD_ZONE) {
        Motor::off();
        return;
    }
    
    // Dirección: arriba o abajo
    if (error > 0) {
        Motor::up(PWM_MIN);   // Movimiento suave
    } else {
        Motor::down(PWM_MIN);
    }
}
```

### 2.2.1 Motor::initPWM() y Persistencia NVS (2026-05-16)

**Flujo completo PWM Min/Max:**

```
Primer boot:
  Motor::initPWM() → lee NVS ("ptxx", pwmMin/Max) → no existe
  → _pwm_min=0, _pwm_max=0 (inválido)
  → Motor usa fallback PWM_MIN=100, PWM_MAX=160 (config.h)

Usuario entra SAT → edita PWM Min/Max:
  SatMenu::loadConfig() → lee NVS
  SatMenu::saveConfig() → guarda con:
    _prefs.putUChar("pwmMin", _cfg.pwmMin);
    _prefs.putUChar("pwmMax", _cfg.pwmMax);

Próximo boot:
  Motor::initPWM() → lee NVS
  → encuentra pwmMin=123, pwmMax=157 (valor guardado por SAT)
  → _pwm_min=123, _pwm_max=157 ← usa valores persistentes
```

### 2.3 Parámetros de Control (config.h)

```cpp
// Motor — control de posición
static constexpr uint8_t  PWM_MIN                  = 100;
static constexpr uint8_t  PWM_MAX                  = 160;

// Motor — dead zone
static constexpr uint16_t DEAD_ZONE                = 50;    // error < esto → apagar motor

// Motor — spike guard (rechaza cambios > este valor)
static constexpr uint16_t ADC_SPIKE_GUARD          = 500;   // cuentas entre lecturas
```

---

## 2.4 Comportamiento Inicial (Boot) — Motor::goToMin() (2026-05-16 10:45)

**Flujo de setup() → loop():**

```
setup() LÍNEA 233: Motor::goToMin()
  ↓
_motor_goingToMin = true
_hwDown(_pwm_max) → _motor_hw_active = true
log: "goToMin: bajando a posición 0..."
  ↓
setup() termina, entra a loop()
  ↓
loop() tick 1-N: Motor::update()
  ↓ PROTECCIÓN STALL (global, antes del switch):
    ADC se mueve → timer stall no corre
    ADC llega al tope físico (~44) → ADC se estabiliza
    400ms sin cambio → _hwOff() + _motor_hw_active=false
  
  ↓ GOING_TO_MIN (state machine):
    Condición alternativa: ADC <= MOTOR_ADC_MIN + 60 = 80 (threshold generoso)
    O stall propio (400ms) → hwOff + transición
    Si _pendingCalib → CALIBRATING
    Si no → AT_TARGET
  ↓
Fader en 0, motor apagado, esperando órdenes S3
```

**Duración:** ~1-2 segundos (solo baja, no mide ruido)

**Estado tras goToMin() DONE:**

| Estado | Valor | Notas |
|--------|-------|-------|
| Fader posición | ADC ≈ 44 (tope físico) | Físicamente abajo |
| Motor | Apagado (EN=LOW, `_motor_hw_active=false`) | No consume corriente |
| Fase motor | `CalibPhase::IDLE` | Listo para calibración |
| PWM_MIN/MAX | Cargado de NVS | Conoce su rango PWM |
| ADC | Listo (FaderADC activo) | Leyendo continuamente |

**Siguiente paso:** Espera FLAG_CALIB de S3 para calibración completa

---

## 2.5 Máquina de Estados Motor v3 (2026-05-16 / actualizado 2026-05-19)

### 2.5.1 Estados y Transiciones

```
┌─────────────────────────────────────────────────────────────┐
│ IDLE                                                        │
│ - Fader en 0, esperando órdenes S3 o usuario               │
│ - Si ADC > MIN+10 y !_connected: → GOING_TO_MIN            │
│ - Si ADC ≤ MIN+10 o connected: apagar motor                 │
└─────────────────────────────────────────────────────────────┘
    ↓ (ADC > MIN+10 y !connected)
┌─────────────────────────────────────────────────────────────┐
│ GOING_TO_MIN                                                │
│ - Motor baja con PWM_MAX                                    │
│ - Doble detección de llegada (2026-05-19):                  │
│   • Por threshold: ADC <= MOTOR_ADC_MIN + 60                │
│   • Por stall: ADC sin cambio 400ms (tope físico)           │
│ - Si _pendingCalib: → CALIBRATING                           │
│ - Si no: → AT_TARGET                                        │
│ - GLOBAL STALL PROTECT también activo (ver §2.6)            │
└─────────────────────────────────────────────────────────────┘
    ↓ (_pendingCalib = true)
┌─────────────────────────────────────────────────────────────┐
│ CALIBRATING                                                 │
│ - Máquina calibración en curso (KICK_UP → DONE)            │
│ - GLOBAL STALL PROTECT NO activo (CALIB tiene el suyo)     │
│ - Si DONE o ERROR: → IDLE                                   │
└─────────────────────────────────────────────────────────────┘
    ↓ (calibración completa)
└──────────→ IDLE (vuelve al inicio)

┌─────────────────────────────────────────────────────────────┐
│ MOVING_TO_TARGET (desde S3 setTarget)                      │
│ - Motor se mueve a posición S3                              │
│ - GLOBAL STALL PROTECT activo (2026-05-19)                 │
│ - Si error < DEAD_ZONE: → AT_TARGET                         │
└─────────────────────────────────────────────────────────────┘
    ↓ (error < DEAD_ZONE)
┌─────────────────────────────────────────────────────────────┐
│ AT_TARGET                                                   │
│ - Posición objetivo alcanzada, esperando nuevo comando S3   │
│ - Motor apagado (_motor_hw_active = false)                  │
└─────────────────────────────────────────────────────────────┘
```

### 2.5.2 Funciones Públicas (API v3)

```cpp
void requestCalibration();           // FLAG_CALIB desde RS485
void setTargetFromS3(uint16_t adc);  // setTarget ADC — GUARDED por usuario (TOUCH/TRIM/LATCH)
void setTargetForced(uint16_t adc);  // setTarget ADC — DAW absoluto, bypass guard (OFF/READ) (2026-05-30)
void setUserDropTarget(uint16_t adc); // Usuario soltó fader en ADC
void setConnected(bool connected);   // Notifica estado conexión S3
MotorState getState();               // Consulta estado motor
void setADCDelta(uint16_t currentADC); // Detecta movimiento usuario (delta OR capacitivo)
```

**Routing por AutoMode (2026-05-30):** la elección entre `setTargetFromS3()` y `setTargetForced()` vive en `RS485Handler::Internal::_applyFaderTarget()`. El Motor no conoce el AutoMode — solo expone las dos APIs. Ver **[AUTOMODE.md](AUTOMODE.md)** para detalles del routing.

---

## 2.6 PROTECCIÓN DE TOPES MECÁNICOS (2026-05-19)

### 2.6.1 Problema: Motor Caliente en Tope

**Evento:** Motor aplicando PWM_MAX contra tope físico sin detener → sobrecalentamiento DRV8833 y motor DC.

**Causa raíz del bug original (commit 06d9562):**
- `MOTOR_ADC_MIN = 20` (umbral de ruido, no tope físico)
- Condición de salida GOING_TO_MIN: `ADC <= MOTOR_ADC_MIN + 10 = 30`
- Tope físico real: ADC ≈ 44
- `44 <= 30` → NUNCA true → motor apretado indefinidamente

**Lección:** `MOTOR_ADC_MIN` es un filtro de ruido, NO el tope físico. Siempre usar detección dinámica (stall) para topes.

### 2.6.2 Solución 1 — Stall en GOING_TO_MIN (commit 06d9562, 2026-05-19)

Detección en el propio estado:

```cpp
case MotorState::GOING_TO_MIN: {
    bool arrived = (_motor_adcPos <= (MOTOR_ADC_MIN + 60)); // threshold generoso

    // Stall: ADC no cambia en GOTO_MIN_STALL_MS → fader en tope físico
    if (abs((int)_motor_adcPos - (int)_goToMinLastADC) > 15) {
        _goToMinLastADC    = _motor_adcPos;
        _goToMinStallStart = millis();
    } else if (_goToMinStallStart > 0 &&
               millis() - _goToMinStallStart > GOTO_MIN_STALL_MS) {
        arrived = true;
    }

    if (arrived) {
        _hwOff();
        // → CALIBRATING o AT_TARGET según _pendingCalib
    }
}
```

**Parámetros (config.h):**
```cpp
static constexpr uint32_t GOTO_MIN_STALL_MS  = 400;  // ms sin ADC change → llegó
static uint32_t           _goToMinStallStart = 0;
static uint16_t           _goToMinLastADC    = 0;
```

### 2.6.3 Solución 2 — Protección Global (commit actual, 2026-05-19)

Capa de seguridad adicional que actúa ANTES del switch de estados, protege TODOS los estados de movimiento excepto CALIBRATING:

```cpp
void update() {
    // ─── Protección global topes mecánicos ──────────────────────
    if (_motor_hw_active && _motor_state != MotorState::CALIBRATING) {
        if (abs((int)_motor_adcPos - (int)_stallProtectLastADC) > 10) {
            _stallProtectLastADC = _motor_adcPos;
            _stallProtectStart   = millis();
        } else if (_stallProtectStart > 0 &&
                   millis() - _stallProtectStart > STALL_PROTECT_MS) {
            _hwOff();  // _motor_hw_active = false → no refire
            log_e("[MOTOR] STALL — tope físico, motor apagado (adc=%d)", _motor_adcPos);
        }
    } else if (!_motor_hw_active) {
        _stallProtectStart   = 0;
        _stallProtectLastADC = _motor_adcPos;
    }
    // ─────────────────────────────────────────────────────────────

    switch (_motor_state) { ... }
}
```

**Parámetros (config.h):**
```cpp
static constexpr uint32_t STALL_PROTECT_MS     = 400;   // ms sin ADC change → apagar
static bool               _motor_hw_active     = false; // seteado por _hwUp/_hwDown/_hwOff
static uint32_t           _stallProtectStart   = 0;
static uint16_t           _stallProtectLastADC = 0;
```

**El flag `_motor_hw_active` es fuente de verdad del estado hardware:**

```cpp
static void _hwOff()         { ...; _motor_hw_active = false; }
static void _hwUp(uint8_t p) { ...; _motor_hw_active = true;  }
static void _hwDown(uint8_t p){ ...; _motor_hw_active = true; }
```

### 2.6.4 Cobertura de Protección

| Estado | GOING_TO_MIN stall | Global stall | Notas |
|--------|-------------------|--------------|-------|
| `GOING_TO_MIN` | ✅ 400ms | ✅ 400ms | Doble protección |
| `MOVING_TO_TARGET` | ❌ No tiene | ✅ 400ms | Global es única protección |
| `CALIBRATING` | ❌ | ❌ Excluido | Usa `CALIB_STUCK_TIMEOUT = 1000ms` propio |
| `IDLE` | N/A | ✅ (motor off en IDLE) | Motor normalmente apagado |
| `AT_TARGET` | N/A | ✅ (motor off) | Motor normalmente apagado |

**¿Por qué excluir CALIBRATING?**

La calibración deliberadamente empuja el motor contra los topes superior e inferior para medir los extremos físicos. Cada fase (KICK_UP, GOING_UP, KICK_DOWN, GOING_DOWN) tiene su propio `CALIB_STUCK_TIMEOUT = 1000ms` que detecta si el motor NO se mueve en 1 segundo y transiciona o reporta error. Aplicar el global (400ms) interferiría con estas fases legítimas.

### 2.6.5 Cómo se Re-arma el Stall Timer

Cuando el motor se apaga por stall (`_hwOff()` → `_motor_hw_active = false`):
1. En el siguiente tick de `update()`, `!_motor_hw_active` → rama `else if`
2. `_stallProtectStart = 0` y `_stallProtectLastADC = _motor_adcPos` se resetean
3. Si en el mismo ciclo se vuelve a llamar `_hwDown()` → `_motor_hw_active = true` otra vez
4. El timer arranca desde 0 nuevamente

No hay posibilidad de "re-fire en el mismo tick" porque `_hwOff()` → `_motor_hw_active=false` → rama `else if` en lugar de la rama `if`.

---

## 3. CALIBRACIÓN (Máquina de Estados No-Bloqueante)

### 3.1 Objetivo

Encontrar rango físico real del fader (min ADC, max ADC) midiendo movimiento y ruido sin bloqueos.

**Duración típica:** 3-5 segundos  
**Ejecución:** No-bloqueante (integrada en loop principal)

### 3.2 Diagrama de Estados

```
                ┌─────────────────────────────────┐
                │  IDLE (espera FLAG_CALIB)       │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  KICK_UP (PWM=175, 500ms)       │  ← Fuerza movimiento inicial
                │  Objetivo: alcanzar ADC > 26000 │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  GOING_UP (PWM=150 u 175)       │  ← Refinamiento
                │  Sigue subiendo hasta estable   │
                │  Detección: delta < 100/frame   │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  SETTLE_UP (PWM=0, 200ms)       │  ← Mide ruido tope
                │  Motor apagado, capta ruido     │
                │  Resultado: _adcTop ± _noise_top│
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  KICK_DOWN (PWM=175, 500ms)     │  ← Repite hacia abajo
                │  Objetivo: alcanzar ADC < 100   │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  GOING_DOWN (PWM=150 u 175)     │
                │  Sigue bajando hasta estable    │
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  SETTLE_DOWN (PWM=0, 200ms)     │  ← Mide ruido fondo
                │  Resultado: _adcBot ± _noise_bot│
                └────────────┬────────────────────┘
                             │
                             ▼
                ┌─────────────────────────────────┐
                │  VALIDATE & DONE                │
                │  MIN = bot + margin             │
                │  MAX = top - margin             │
                │  span = MAX - MIN               │
                └─────────────────────────────────┘
```

### 3.3 Inicio de Calibración (v3 — 2026-05-16 18:45)

**Boot automático (S3 ordena FLAG_CALIB vía RS485):**
```
S3 envía MasterPacket con FLAG_CALIB
  ↓
S2 RS485Handler::onMasterData() detecta FLAG_CALIB
  ↓
Motor::requestCalibration()
  ├─ Si fader EN 0:
  │   └─ Motor::startCalib() → KICK_UP inmediatamente
  └─ Si fader NO EN 0:
      ├─ _pendingCalib = true
      ├─ Motor::goToMin() baja (stall detection activo)
      └─ Al llegar: _pendingCalib → startCalib()
```

**Código requestCalibration():**
```cpp
void requestCalibration() {
    if (_motor_adcPos <= (MOTOR_ADC_MIN + 10)) {
        if (_motor_state != MotorState::CALIBRATING) {
            _motor_state = MotorState::CALIBRATING;
            startCalib();
        }
    } else {
        if (_motor_state != MotorState::GOING_TO_MIN) {
            _pendingCalib = true;
            _motor_state = MotorState::GOING_TO_MIN;
            goToMin();
        }
    }
}
```

### 3.4 Guard Cooldown (2026-05-16)

```cpp
Motor::startCalib() {
    // Guard: no reiniciar si completó hace <2s
    if (millis() - _motor_lastCalibDone < CALIB_COOLDOWN_MS) {
        log_w("[CALIB] Enfriamiento activo, rechazando FLAG_CALIB");
        return;
    }
    // ...
}
```

**config.h:**
```cpp
static constexpr uint32_t CALIB_COOLDOWN_MS        = 2000;  // ms espera mínima (2026-05-16)
static uint32_t           _motor_lastCalibDone      = 0;    // timestamp último finish
```

### 3.5 Parámetros de Calibración (config.h)

```cpp
static constexpr uint16_t ADC_STABILITY_THRESHOLD  = 100;    // cambio máximo para "estable"
static constexpr uint32_t CALIB_STABLE_TIME        = 500;    // ms para confirmar estable
static constexpr uint32_t CALIB_SETTLE_MS          = 200;    // ms para medir ruido
static constexpr uint32_t CALIB_MIN_TRAVEL_MS      = 300;    // ms mínimo de viaje
static constexpr uint32_t CALIB_TIMEOUT            = 6000;   // ms timeout total
static constexpr uint32_t CALIB_STUCK_TIMEOUT      = 1000;   // ms sin movimiento = atascado
static constexpr uint32_t CALIB_COOLDOWN_MS        = 2000;   // ms espera antes de reintentar
static constexpr uint8_t  PWM_SLEW                 = 5;      // cambio PWM máximo/tick
```

---

## 4. SAT (Sistema de Auto-Test) — Motor

Acceder: Encoder push >3 segundos en display normal

### 4.1 Opciones SAT

- **Motor Off** — Desactiva motor completamente
- **Motor On** — Activa motor
- **Motor Calibrar** — Inicia calibración automática
  - Se ejecuta en loop principal (Motor::update() SIEMPRE)
  - SAT solo dibuja estado (no controla)
  - Presionar REC: reinicia calibración si falla
- **Motor Drive** — Test PWM manual (slider 0-255)

### 4.2 Motor Calibración en SAT (2026-05-12 19:07)

- `main.cpp` ejecuta `Motor::update()` cada frame (incluso con SAT abierto)
- SAT **NO** ejecuta Motor::update() (evita race conditions)
- Motor::setADC() actualizado siempre en main.cpp
- SAT solo dibuja `Motor::getCalibState()`

**Nota:** La protección global de topes también está activa durante Test Mode SAT. Si se presiona REC/SOLO contra el tope físico más de 400ms, el motor se apagará automáticamente.

---

## 5. DIAGNÓSTICO Y DEBUGGING

### 5.1 Expected Logs — Boot Normal (con stall detection)

```
[MOTOR] goToMin: bajando a posición 0 (MASTER)...
[MOTOR-STATE] GOING_TO_MIN stall detectado (adc=44)    ← stall local GOING_TO_MIN
[MOTOR-STATE] GOING_TO_MIN → AT_TARGET (llegó a 0)
```

O si _pendingCalib:
```
[MOTOR-STATE] GOING_TO_MIN stall detectado (adc=44)
[MOTOR-STATE] GOING_TO_MIN → CALIBRATING
[CALIB] Iniciada
[CALIB] KICK_UP adc=44 ...
```

### 5.2 Expected Logs — Stall Global Activado

```
[MOTOR] STALL — tope físico, motor apagado (adc=44)
```
Este log indica que la protección global disparó (MOVING_TO_TARGET pegó contra un tope, o GOING_TO_MIN llegó antes del stall local).

### 5.3 Expected Logs — Calibración Exitosa

```
[CALIB] Iniciada
[CALIB] KICK_UP → GOING_UP (ADC=26200)
[CALIB] GOING_UP → SETTLE_UP (estable en 26400)
[CALIB] SETTLE_UP: ruido_top=±8 cuentas
[CALIB] SETTLE_UP → KICK_DOWN
[CALIB] KICK_DOWN → GOING_DOWN (ADC=500)
[CALIB] GOING_DOWN → SETTLE_DOWN (estable en 100)
[CALIB] SETTLE_DOWN: ruido_bot=±5 cuentas
[CALIB] OK  MIN=44 MAX=26448 span=26404
```

### 5.4 Checklist Troubleshooting

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Motor caliente / apretado contra tope | Stall no activo o MOTOR_ADC_MIN muy bajo | Ver §2.6 — protección global activa desde 2026-05-19 |
| `[MOTOR] STALL` en log pero motor no detiene | Flag `_motor_hw_active` no seteado | Verificar que `_hwDown/_hwUp` setean `_motor_hw_active=true` |
| Motor inmóvil | EN (GPIO14) no LOW en init() | Ver `Motor::init()` |
| Movimiento invertido | IN1/IN2 lógica invertida | Test manual con PWM → observar dirección |
| PWM insuficiente | PWM_MIN/MAX mal calibrados | Test Mode SAT |
| Calibración timeout | Motor atascado o sensor roto | Check ADS1115 lectura en Test Mode |
| Fader sube y se queda arriba con fuerza | ADC tope físico < 26000 — KICK_UP stuck | Fix aplicado 2026-05-27: stuck timeout 1000ms → pasa a GOING_UP. Log: `KICK_UP stuck pos=XXXX` |
| Reinicios infinitos | S3 envía FLAG_CALIB continuamente | Verificar cooldown guard (2000ms) |
| GOING_TO_MIN no transiciona | `_pendingCalib` no seteado | Verificar `requestCalibration()` setea `_pendingCalib=true` |
| **Sube perfecto, nunca baja (una unidad aislada, mismo firmware que el resto)** | **DRV8833 dañado — mitad `IN2`/`OUT2` del puente H** (motor DC de una bobina, sin canal físico separado por sentido) | SAT > Motor Test (SOLO=baja) para descartar lógica; si tampoco responde ahí, medir con multímetro el pin `MOTOR_IN2` durante el test — si el GPIO conmuta y el DRV8833 no reacciona, es el chip. No aplica fix de software (hardware locked) — decisión de reparación de hardware. Ver `CHANGELOG.md` sesión 2026-08-13. |

---

## 6. DETECCIÓN DE USUARIO — Master Control (2026-05-16 10:52)

### 6.1 Mecanismo: Sensor Capacitivo + Delta ADC

| Método | Fuente | Umbral | Ventaja |
|--------|--------|--------|---------|
| **Capacitivo** | FaderTouch::isTouched() | Contacto físico | Preciso, sin lag |
| **Delta ADC** | setADCDelta(currentADC) | > 500 cuentas/tick | Detecta velocidad rápida |

### 6.2 Flujo: Usuario Toma Control

```
Loop() — usuario mueve fader rápido:
  setADCDelta(currentADC) detecta delta > 500
    ↓
  _motor_manualTouchDetected = true
  Motor::stop()  ← Motor para INMEDIATAMENTE
  _motor_state = AT_TARGET  ← Usuario define posición
  _motor_targetADC = currentADC  ← Nuevo target aceptado
    ↓
  RS485Handler::buildResponse():
    touchState = FaderTouch::isTouched() ? 1 : 0
    → envía SlavePacket a S3
      ↓
  S3 recibe touchState=1 → Logic entiende "usuario movió fader"
    ↓
  Usuario suelta fader (después de MANUAL_TOUCH_DEBOUNCE_MS=200ms):
    _motor_manualTouchDetected = false
    S3 puede mandar nuevo target
```

### 6.3 Guardia en setTargetFromS3()

```cpp
void setTargetFromS3(uint16_t adcTarget) {
    if (_motor_manualTouchDetected || FaderTouch::isTouched()) {
        return;  // Usuario es master — S3 ignorado
    }
    _motor_targetADC = adcTarget;
    _motor_state = MotorState::MOVING_TO_TARGET;
}
```

---

## 7. HISTORIA DE FIXES

### 2026-08-13 (noche) — Rampa cuadrática + causa raíz real: falso touch por rebote al frenar

**Rampa cuadrática (`_positionTick()`):** la rampa lineal (sesión anterior, mismo día) mantenía demasiado PWM cerca del target — el fader empezó a rebotar sobre el target (overshoot→corrige→overshoot menor→asienta, confirmado por el usuario que se amortigua solo). Fix: `targetPWM = _pwm_min + (_pwm_max-_pwm_min) × (absErr/POSITION_CRUISE_ERR)²` — mismo empuje al entrar en la zona, decae mucho antes y más fuerte cerca del target.

**Causa raíz real — `setADCDelta()`, falso touch por rebote (línea ~651):** el síntoma que se venía persiguiendo todo el día ("fader se queda a medio camino") no era overshoot puro — era que cualquier rebote/inversión de dirección al frenar (más probable cuanto más rápido llega el motor) dispara el guard direccional existente de `setADCDelta()` (2026-05-24), que interpreta dirección opuesta al target como "usuario oponiéndose". En AUTO_OFF/READ el motor no se detiene, pero **sí reporta `touchState=1` a Logic** — y Logic, al verlo, abandona el fader ahí donde estaba, aunque el motor internamente sí llegara al target real. Confirmado con MIDI Monitor + `[S3-RX] touchState=1 slave=X faderPos=...` sostenido sin que nadie tocara nada.

**Fix:** una inversión de dirección dentro de la zona de frenado (`< POSITION_CRUISE_ERR` del target) ya no cuenta como touch — se asume asentamiento mecánico. Fuera de esa zona, la detección de oposición real del usuario no cambia — la garantía "usuario es master" se mantiene intacta.

**RIESGO ALTO, pendiente validar en banco** (orden: cerrar proyecto sin huérfanos → sujetar fader real sigue cediendo control al instante → automatización normal). Detalle completo: `CHANGELOG.md` sesión 2026-08-13, puntos 17-19.

### 2026-08-13 (noche, continuación) — spike guard de setADC() + hallazgo central: MANUAL_TOUCH_AT_TARGET_THRESHOLD dentro del ruido ADC real

**`setADC()`:** el spike guard del ADC (`ADC_SPIKE_GUARD`) se desactivaba también por falso touch (`_motor_manualTouchDetected`), no solo por calibración/goToMin real — justo en el instante de más ruido eléctrico (motor frenando fuerte). Quitado ese caso del bypass.

**Hallazgo central del día — `MANUAL_TOUCH_AT_TARGET_THRESHOLD`:** log decisivo (MIDI Monitor + serie S3) mostró `touchState=1` sostenido 3+ segundos en un slave recién conectado, con `faderPos` temblando ~28 cuentas sin que nadie tocara nada, **antes de que llegara el target real del proyecto**. El umbral estaba en `30` — dentro del ruido real medido en banco. Cada vez que el ruido lo superaba se refrescaba el timer de debounce (600ms), impidiendo soltar el touch nunca. Esto explica el patrón "todos los S2 se van mal al abrir Logic, solo se arreglan moviendo los faders en Logic" — es la sexta capa del mismo mecanismo tocada hoy. Fix: 30 → 70.

**Pendiente de diseño, sesión dedicada (no hoy):** separar un PWM de arranque/kick (fricción estática) de un PWM de posicionamiento fino (fricción dinámica) — el `pwmMin=125` real usado en banco (más alto que el default de fábrica) limita cuánto puede bajar el "suelo" de velocidad en el frenado, pese a las dos rampas ya ajustadas. Brief completo: `CHANGELOG.md` sesión 2026-08-13, al final (BRIEF DE DISEÑO PENDIENTE).

### 2026-08-13 (tarde) — Jitter en goToMin() + fix frenado fino _positionTick()

**Jitter en goToMin() (`Motor.cpp`, `config.h`):** el jitter de boot (arriba) solo cubría la autocalibración — `goToMin()` (MASTER ABSOLUTO si `!_connected`) seguía disparando sincronizado en TODAS las S2 a la vez, tanto en encendido simultáneo del rig como en desconexión de Logic en caliente. Confirmado en banco: correlaciona con ráfagas de `[RS485] ID MISMATCH`/`CRC ERROR` en S3 (ver `RS485.md`). Fix: `GOTOMIN_JITTER_MAX_MS=2000`, calculado en `Motor::init()` (boot) y en `setConnected()` (flanco conectado→desconectado). Los dos disparadores de `goToMin()` (`IDLE`, `AT_TARGET`) respetan `_goToMinJitterUntil` — la garantía "ejecuta SIEMPRE" no se compromete, solo se retrasa hasta 2s por unidad.

**Fix frenado fino — `_positionTick()` (línea ~357):** denominador equivocado (`_motor_adcSpan` en vez de `POSITION_CRUISE_ERR`) hacía que el PWM cayera casi a `_pwm_min` de golpe al entrar en la zona de frenado (2000 counts) y se quedara plano el resto — sin rampa real. Causaba overshoot hasta el tope físico en movimientos largos (llega con inercia, freno insuficiente) y trompicones en movimientos cortos (PWM casi mínimo desde el principio). Fix: denominador → `POSITION_CRUISE_ERR`, rampa lineal real de `PWM_MAX` a `PWM_MIN` a lo largo de la zona. Detalle completo, incluyendo la investigación previa (Bug B3 en S3, descartada la hipótesis de calibración no aplicada): `CHANGELOG.md` sesión 2026-08-13, puntos 13 y 16.

### 2026-08-13 — Jitter anti-cascada en boot + reintento automático en timeout de calibración

**Jitter (`main.cpp`, `config.h`):** todos los S2 arrancaban y disparaban `requestCalibration()` casi al mismo instante al energizar el rig (todos llegan a "10 lecturas ADC válidas" casi a la vez) → todos los motores subían juntos. Fix: nueva constante `CALIB_BOOT_JITTER_MAX_MS=2000`, cada S2 espera un retardo aleatorio 0-2s (hardware RNG del ESP32) antes de disparar la autocalibración de boot.

**Retry en timeout (`Motor.cpp:86-98`):** el fallo por `CALIB_TIMEOUT` (6s sin terminar) iba directo a `CalibPhase::ERROR` sin reintentar — a diferencia del fallo por "span corto", que ya reintentaba hasta `CALIB_MAX_RETRIES=3`. Fix: el timeout ahora reutiliza el mismo contador `_motor_calibRetries` y el mismo camino de reintento (`goToMin()` + `_pendingCalib=true` → vuelve a `startCalib()`). Solo cae en `ERROR` permanente tras agotar los 3 intentos combinados.

**Pendiente conocido:** `_motor_calibRetries` solo se resetea a 0 en éxito — un reintento manual tras `ERROR` (SAT REC o `FLAG_CALIB` por MIDI) sin power-cycle no obtiene 3 intentos frescos si el contador ya estaba agotado de un ciclo anterior.

**Auditoría PWM:** confirmado que toda la máquina de calibración (`_hwUp`/`_hwDown` en `KICK_UP`/`GOING_UP`/`GOING_DOWN`/etc.) usa siempre `_pwm_min`/`_pwm_max`, cargados de NVS en `Motor::initPWM()` — sin PWM hardcodeado. El fallback a `config.h` (100-160) solo aplica si NVS está vacío/inválido.

Detalle completo: `CHANGELOG.md` sesión 2026-08-13.

### 2026-05-27 — KICK_UP Stuck Detection (variación ADC entre unidades)

**Problema:** Fader subía durante calibración y se quedaba arriba con fuerza sin bajar.

**Causa raíz:** `KICK_UP` espera `pos >= 26000` para pasar a `GOING_UP`. Si el ADC real del tope físico es < 26000 (varía por unidad), la condición nunca se cumple. Motor empuja con `PWM_MAX` durante `CALIB_TIMEOUT = 6s` → `ERROR` → `IDLE` con `_connected=true` → fader queda arriba sin bajar.

No había timeout en `KICK_UP` (a diferencia de `GOING_UP` que tiene `CALIB_STUCK_TIMEOUT`).

**Fix:** Añadido stuck detection en `KICK_UP`. Si ADC estable `CALIB_STUCK_TIMEOUT = 1000ms` y `pos < 26000` → transiciona a `GOING_UP` igualmente (asume tope físico alcanzado).

**Log diagnóstico:** `[CALIB] KICK_UP stuck pos=XXXX (<26000) — tope físico detectado → GOING_UP`

**Lección:** El umbral `26000` en `KICK_UP` es teórico. El tope real varía por unidad, igual que `MOTOR_ADC_MIN` no es el tope real del mínimo (ver §2.6). Siempre preferir detección dinámica por stall sobre umbrales absolutos.

---

### 2026-05-19 — Protección Global Topes Mecánicos

**Problema:** Motor caliente al apretarse contra tope físico. GOING_TO_MIN nunca transicionaba porque `MOTOR_ADC_MIN + 10 = 30` era inalcanzable (tope físico real ≈ 44).

**Fix 1 (commit 06d9562):** Stall detection en GOING_TO_MIN — si ADC sin cambio 400ms → transición a CALIBRATING o AT_TARGET.

**Fix 2 (commit actual):** Protección global en `Motor::update()` — cubre todos los estados de movimiento excepto CALIBRATING. Flag `_motor_hw_active` como fuente de verdad del estado HW.

**Lección permanente:** MOTOR_ADC_MIN es filtro de ruido, NO tope físico. Los topes siempre se detectan dinámicamente por stall.

### 2026-05-18 — _pendingCalib: GOING_TO_MIN → CALIBRATING (commit a04e58f)

**Problema:** FLAG_CALIB one-shot de S3 → S2 bajaba a 0 pero no calibraba al llegar (sin `_pendingCalib`).

**Fix:** `requestCalibration()` setea `_pendingCalib=true` + GOING_TO_MIN. `update()` case GOING_TO_MIN verifica `_pendingCalib` → transiciona a CALIBRATING.

### 2026-05-16 08:29 — Guard Cooldown & Auto-Calib Disable

**Problema:** S3 continuamente enviaba FLAG_CALIB después de calibración → motor reiniciaba calibración infinitamente.

**Fix:** Cooldown 2000ms en `Motor::startCalib()`, auto-calibración S2 desactivada.

### 2026-05-16 10:52 — Usuario como Master (v2)

**Fix:** Variables `_connected`, `setTargetFromS3()` con guards usuario, `setADCDelta()` integra FaderTouch.

### 2026-05-09 23:50 — Lógica IN1/IN2 Invertida

**Fix:** UP=IN2 PWM, DOWN=IN1 PWM.

---

## Referencias

- **FADER.md** — Documentación ADS1115, calibración bidireccional, EMA filter
- **CLAUDE.md** — Directivas obligatorias (NUNCA compilar, orden init, etc.)
- **config.h (S2)** — Fuente de verdad para constantes motor
- **CHANGELOG.md** — Historial detallado de fixes y validaciones
