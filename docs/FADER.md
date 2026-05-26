# FADER — Calibración y Control (S2 Slave)

Documentación exhaustiva del subsistema de faders en iMakie. Incluye calibración, feedback ADC, control motor, mapping Logic → hardware, y diagnóstico.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (S2 ×17 activos)

---

## 1. ARQUITECTURA FADER

```
Logic Pro (macOS)
    │ PitchBend 0-14848 (signed 14-bit: -8192..+8191)
    ▼
S3 MidiProcessor::processPitchBend()
    │ Mapea: -8192..+8191 → 0..27000 (ADC range)
    ▼
S3 RS485Master::setFaderTarget()
    │ Envía MasterPacket.faderTarget (0-27000) O MasterPacket.flags |= FLAG_CALIB
    ▼
RS485 500kbaud (20ms ciclo, ~2 paquetes/s por slave)
    ▼
S2 Slave (×1 canal)
    │ Recibe faderTarget (0-27000) O FLAG_CALIB
    │
    ├─→ FaderADC (ADS1115)
    │   └─→ ISR-driven (860 SPS)
    │       └─→ Buffer circular 256 muestras
    │           └─→ Rango: 24-26476 (raw 16-bit)
    │
    ├─→ FaderTouch (GPIO1 capacitivo)
    │   └─→ Baseline IIR + sostenimiento (120ms)
    │       └─→ Detección contacto humano
    │
    ├─→ Motor (DRV8833 H-bridge) — v3 USUARIO MASTER (2026-05-16 18:45)
    │   ├─→ Prioridad: Usuario > GoToMin > S3
    │   ├─→ setADCDelta(): Usuario mueve → Motor stop
    │   ├─→ goToMin(): MASTER si !_connected
    │   ├─→ requestCalibration(): FLAG_CALIB → baja a 0, luego calibra
    │   └─→ setTargetFromS3(): S3 ordena SOLO si usuario NO toca
    │
    └─→ Display feedback (ST7789V3)
        └─→ Fader visual 0-100%

    ▼
S2 responde: SlavePacket.faderPos (0-27000) + touchState + flags

    ▼
S3 mapea: faderPos * 14848 / 27000 → PitchBend 0-14848

    ▼
Logic recibe PitchBend → sube/baja fader gráfico
```

---

## 1.1 INICIALIZACIÓN Y CALIBRACIÓN (v3 — 2026-05-16 18:45)

### Flujo Boot → Operación Normal

```
setup():
  Motor::init()
  Motor::initPWM()  ← Lee PWM de NVS
  Motor::goToMin()  ← INMEDIATAMENTE baja a 0 (MASTER control)
  
loop():
  FaderADC::update()        ← Lee posición ADC
  FaderTouch::update()      ← Detecta toque capacitivo
  Motor::setADC(pos)        ← Motor recibe ADC actual (SIEMPRE)
  Motor::setADCDelta(pos)   ← Detecta usuario moviendo (MASTER)
  RS485.update()            ← Recibe MasterPacket de S3
  
  Si S3 manda FLAG_CALIB:
    RS485Handler::onMasterData()
      ↓
    Motor::requestCalibration()
      ├─ Si fader EN 0: startCalib() inmediato
      └─ Si fader ≠ 0: goToMin() primero, luego startCalib()
      
  Si usuario mueve:
    setADCDelta() detecta delta > 500 O capacitivo activo
      ↓
    Motor::stop()
      ↓
    Motor en AT_TARGET (usuario es MASTER)
      ↓
    S3 targets ignorados hasta que usuario suelte (debounce 200ms)
    
  Si usuario suelta:
    _motor_manualTouchDetected = false (después 200ms)
      ↓
    setTargetFromS3() acepta órdenes de S3
    
  Si S3 desconectado:
    Motor::setConnected(false)
      ↓
    Motor::goToMin() ejecuta SIEMPRE
      ↓
    Fader baja a 0 indefinidamente (loop IDLE)
```

### Guardia de Usuario (2026-05-16 18:45)

```cpp
setTargetFromS3(uint16_t adcTarget) {
    if (_motor_manualTouchDetected || FaderTouch::isTouched()) {
        log_i("[MOTOR] Usuario MASTER, ignorando S3 target");
        return;  ← Usuario controla, S3 es esclavo
    }
    // S3 puede mover motor
    _motor_state = MotorState::MOVING_TO_TARGET;
    _motor_targetADC = adcTarget;
}
```

---

## 2. HARDWARE FADER

### 2.1 ADS1115 (I2C ADC externo)

**Razón de uso:** ADC nativo S2 es muy ruidoso (±30 cuentas, 13-bit). ADS1115 es 16-bit, ISR-driven, mejora 6-15×.

| Parámetro | Valor |
|-----------|-------|
| Chip | Adafruit ADS1115 (16-bit, ±4.096V) |
| Dirección I2C | 0x48 (SDA=GPIO21, SCL=GPIO34) |
| Rango capturado | 24–26476 (raw 16-bit) |
| Resolución de ruido | ~2-5 cuentas RMS (vs ±30 nativo) |
| Sampling rate | 860 SPS (internamente, ISR dispara en 1.16ms) |
| Pin Alert/RDY | GPIO17 (ISR FALLING) |
| Buffer circular | 256 muestras con timestamp |
| Latencia update | 0-2µs (ISR-driven, no polling) |
| API | FaderADC::update(), FaderADC::getFaderPos() |

**Inicialización:**
```cpp
FaderADC::begin()
  1. Configurar ADS1115: GAIN_ONE (±4.096V), 860 SPS
  2. Attachar ISR en GPIO17 (FALLING edge)
  3. Lanzar conversión inicial
  4. Retorna inmediatamente (no bloquea)
```

**ISR (Interrupt Service Routine):**
```cpp
// Corre en ISR context, <1µs
void IRAM_ATTR onAdsAlert() {
    uint16_t raw = ads.getLastConversionResults();  // Lectura rápida
    _buffer[_head] = raw;
    _timestamp[_head] = micros();
    _head = (_head + 1) & 0xFF;  // Circular
}
```

**Salida (update loop):**
```cpp
FaderADC::update() {
    // Lee última muestra del buffer sin bloquear
    if (newData) {
        _faderPos = _buffer[_tail];
        _tail = (_tail + 1) & 0xFF;
    }
}

uint16_t FaderADC::getFaderPos() {
    return _faderPos;  // 0-8191 (13-bit compatible) o 24-26476 (16-bit real)
}
```

### 2.2 Sensor Táctil Capacitivo (GPIO1 / T1)

**Hardware:** Pista de cobre del fader PCB conectada a GPIO1 (entrada capacitiva S2).

**Firmware:** FaderTouch.cpp implementa detección por sostenimiento (sin IRQ, polling en main loop).

```cpp
FaderTouch::update() {
    // Lectura capacitiva
    uint16_t raw = touchRead(FADER_TOUCH_PIN);  // 0-1023 típicamente
    
    // Baseline IIR (α=1/16, actualiza siempre)
    baseline = baseline + (raw - baseline) / 16;
    
    // Umbral de toque: baseline × 1.015
    uint16_t threshold = baseline * 1.015;
    
    // Sostenimiento: si raw > threshold durante 6 frames (120ms) → TOQUE
    if (raw > threshold) {
        touchFrames++;
        if (touchFrames >= 6) {
            isTouched = true;
        }
    } else {
        touchFrames = 0;
        isTouched = false;
    }
}
```

**Impacto en Motor:**
- Si `FaderTouch::isTouched()` = true → `Motor::stop()` inmediato (evita conflicto)
- Si usuario mueve fader manualmente → motor se apaga
- Si usuario suelta → motor puede retomar posición desde master

**Calibración capacitiva:** No hay. El baseline se actualiza en tiempo real.

---

## 3. MOTOR CONTROL (DRV8833 H-bridge)

### 3.1 Pinout Motor

| Señal | GPIO | Función | Notas |
|-------|------|---------|-------|
| EN (nSLEEP) | 14 | Enable motor | **DEBE estar LOW en init()** |
| IN1 (control) | 18 | PWM o digital | analogWrite() |
| IN2 (control) | 16 | PWM o digital | analogWrite() |
| Sensor (feedback) | Fader ADC | Posición actual | 24-26476 |

### 3.2 Calibración (Máquina de estados no-bloqueante)

**Objetivo:** Encontrar rango físico real del fader (min ADC, max ADC) con ruido medido.

**Duración típica:** 3-5 segundos (sin bloqueos).

**Estados:**

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

### 3.3 Control Posición (Post-calibración)

```cpp
Motor::setTarget(uint16_t target) {
    // target: 0-14848 (rango Logic)
    // _adcMin/_adcMax: valores calibrados
    
    // Mapear Logic range → ADC range
    uint16_t targetADC = _adcMin + (target * (_adcMax - _adcMin) / 14848);
    
    // Comparar con posición actual
    int16_t error = targetADC - _adcPos;  // -27000..+27000
    
    // Dead zone: si |error| < 50 → apagar motor
    if (abs(error) < DEAD_ZONE) {
        Motor::off();
        return;
    }
    
    // Dirección: arriba o abajo
    if (error > 0) {
        Motor::up(PWM_MIN);  // Movimiento suave
    } else {
        Motor::down(PWM_MIN);
    }
}
```

**EMA Filter en S2:**
```cpp
// Suaviza ruido ADC sin crear dead zones
filteredPos = filteredPos + (rawPos - filteredPos) * FADER_EMA_ALPHA;
// FADER_EMA_ALPHA = 0.20f (75% peso histórico, 25% peso nuevo)
```

### 3.4 Problemas Conocidos

| Problema | Síntoma | Causa | Fix | Estado |
|----------|---------|-------|-----|--------|
| Motor caliente / apretado en tope | Motor sin parar en posición 0 | `MOTOR_ADC_MIN+10` inalcanzable (tope físico ≠ ADC mín teórico) | Stall detection + protección global (2026-05-19) | ✅ Fijo |
| Motor no se mueve | Inmóvil en calibración | GPIO14 (EN) no LOW en init | Usar `digitalWrite(MOTOR_EN, LOW)` ANTES de analogWrite | **CRÍTICO** |
| Movimiento invertido | Sube cuando debe bajar | _hwUp/_hwDown invertidos | IN1/IN2 lógica invertida | ✅ Fijo |
| PWM insuficiente | Motor lento o sin movimiento | PWM_MIN/MAX mal calibrados | Test en bench: PWM_MIN=100, PWM_MAX=160 | ✅ Fijo |
| Conflicto LEDC | Motor jitter o falla | LovyanGFX backlight agota canales LEDC | Usar `analogWrite()` (no ledcWrite) | ✅ Fijo |

**Nota sobre topes mecánicos:** El fader tiene topes físicos de goma/plástico que limitan el recorrido. El ADC en el tope inferior es ~44, no 0. Cualquier lógica que espere `ADC == 0` fallará. Siempre usar detección por stall (ADC estable > Nms) para detectar llegada a tope. Ver **MOTOR.md §2.6** para implementación completa.

---

## 4. CALIBRACIÓN: FLUJO DETALLADO

### 4.1 Inicio (Boot automático o SAT)

**Boot:**
```
setup() {
  1. Motor::init()      ← ANTES de Serial
  2. RS485.begin()
  3. Esperar Master
  4. Master ordena FLAG_CALIB
}
```

**SAT Manual:**
```
Usuario: Encoder push >3s → SAT menu
         Motor → Calibración
         → Motor::startCalib()
```

### 4.2 Orden de Ejecución

```cpp
Motor::startCalib() {
    // Guard: no reiniciar si completó hace <2s
    if (millis() - _motor_lastCalibDone < CALIB_COOLDOWN_MS) {
        log_w("[CALIB] Enfriamiento activo");
        return;
    }
    
    // Inicializar máquina
    _motor_phase = CalibPhase::KICK_UP;
    _motor_calibStart = millis();
    _motor_phaseStart = millis();
    _motor_adcTop = 0;
    _motor_settleMin = 27000;  // inverso para min()
    _motor_settleMax = 0;       // inverso para max()
    
    _hwUp(_pwm_max);  // Inicio abrupto (KICK)
    log_i("[CALIB] Iniciada");
}

Motor::_calibUpdate() {
    // Máquina de estados no-bloqueante
    // Se ejecuta en main.cpp loop() cada ~5ms
    
    switch (_motor_phase) {
    case KICK_UP:
        // 500ms de PWM máximo
        if (now - _motor_phaseStart > 500) {
            if (pos >= 26000) {
                _motor_phase = GOING_UP;
                _hwUp(_pwm_min);
                log_i("[CALIB] → GOING_UP");
            }
        }
        break;
        
    case GOING_UP:
        // Refinamiento: sigue subiendo hasta estable
        if (delta < 100 && millis since last_move > 300) {
            _motor_phase = SETTLE_UP;
            _hwOff();
            _motor_phaseStart = now;
            log_i("[CALIB] → SETTLE_UP pos=%d", pos);
        }
        break;
        
    case SETTLE_UP:
        // 200ms muestreo ruido con motor apagado
        _motor_settleMin = min(_motor_settleMin, pos);
        _motor_settleMax = max(_motor_settleMax, pos);
        if (now - _motor_phaseStart > 200) {
            uint16_t noise = _motor_settleMax - _motor_settleMin;
            log_i("[CALIB] TOP=%d noise=%d", _motor_settleMax, noise);
            _motor_noiseTopSpan = noise;
            _motor_phase = KICK_DOWN;  // Repite hacia abajo
            _motor_phaseStart = now;
            _hwDown(_pwm_max);
        }
        break;
        
    // ... KICK_DOWN, GOING_DOWN, SETTLE_DOWN simétricos ...
    
    case SETTLE_DOWN:
        if (now - _motor_phaseStart > 200) {
            uint16_t noise = _motor_settleMax - _motor_settleMin;
            uint16_t adcBot = _motor_settleMin;
            uint16_t adcTop = _motor_adcTop;
            
            // Validar rango
            uint16_t minGapRequired = (_motor_noiseTopSpan + noise) * 2;
            if (adcTop > adcBot + minGapRequired) {
                // Aplicar márgenes de seguridad
                uint16_t marginTop = (_motor_noiseTopSpan + 10) / 2;
                uint16_t marginBot = (noise + 10) / 2;
                
                _calibratedFaderMin = adcBot + marginBot;
                _calibratedFaderMax = adcTop - marginTop;
                _motor_adcSpan = _calibratedFaderMax - _calibratedFaderMin;
                
                _motor_lastCalibDone = millis();  // Registra timestamp
                _motor_phase = DONE;
                
                log_i("[CALIB] OK  MIN=%d MAX=%d span=%d",
                      _calibratedFaderMin, _calibratedFaderMax, _motor_adcSpan);
            } else {
                _motor_phase = ERROR;
                log_e("[CALIB] ERROR — rango insuficiente");
            }
        }
        break;
    }
}
```

### 4.3 Logs Esperados (Boot + Master ordena calibración)

```
[   997][I][main.cpp:148] taskCore0(): [CALIB] Slave 1 (iniciando calibración)...
[  1009][I][main.cpp:219] setup(): === S3-02 Extender ACTIVO. Slaves: 1 ===
[  1012][I][RS485.cpp:47] setCalibrate(): [RS485] → Slave 1: ordena calibración

[  1234][I][Motor.cpp:71] _calibUpdate(): [CALIB] KICK_UP adc=24 (t=2 ms) pwm=175
[  1456][I][Motor.cpp:71] _calibUpdate(): [CALIB] KICK_UP adc=26211 (t=255 ms) pwm=175
[  1458][I][Motor.cpp:79] _calibUpdate(): [CALIB] → GOING_UP

[  2000][I][Motor.cpp:119] _calibUpdate(): [CALIB] → SETTLE_UP  pos=26470
[  2200][I][Motor.cpp:131] _calibUpdate(): [CALIB] TOP=26470 noise_span=9

[  2456][I][Motor.cpp:147] _calibUpdate(): [CALIB] KICK_DOWN adc=26470 (t=2 ms) pwm=175
[  2700][I][Motor.cpp:147] _calibUpdate(): [CALIB] KICK_DOWN adc=35 (t=264 ms) pwm=175
[  2702][I][Motor.cpp:155] _calibUpdate(): [CALIB] → GOING_DOWN  pwm=150

[  3200][I][Motor.cpp:188] _calibUpdate(): [CALIB] → SETTLE_DOWN  pos=30
[  3400][I][Motor.cpp:207] _calibUpdate(): [CALIB] Tope inferior: 24  noise_span=3  margin=20
[  3401][I][Motor.cpp:208] _calibUpdate(): [CALIB] Tope superior: 26470  noise_span=9  margin=20
[  3402][I][Motor.cpp:209] _calibUpdate(): [CALIB] Gap requerido: 40 (ruidos: top=9 bot=3)
[  3403][I][Motor.cpp:219] _calibUpdate(): [CALIB] OK  MIN=44 MAX=26443 span=26399 target=38334
```

### 4.4 Validación Éxito

✅ **Calibración EXITOSA:**
- Último log contiene `OK` (no `ERROR` o `TIMEOUT`)
- MIN > 20 (debe estar arriba del ruido de fondo)
- MAX < 27000 (debe estar dentro del rango ADC)
- span > 100 (debe haber recorrido suficiente)
- Secuencia completa: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN → OK

❌ **Calibración FALLIDA:**
- `[CALIB] ERROR — rango insuficiente` → motor bloqueado (no se mueve)
- `[CALIB] TIMEOUT` → motor atascado >1s sin movimiento
- Falta de logs de transición → proceso interrumpido (crash o RS485 timeout)

---

## 5. MAPEO LOGIC → HARDWARE

### 5.1 PitchBend Logic

**Entrada (Logic Pro):**
- Rango: Signed 14-bit (-8192..+8191)
- **Mínimo:** -8192 (Logic GoOffline, AllFadersToMinimum)
- **Máximo:** +8191 (posición física máxima)
- **Rango útil:** -8180..+6362 (valores reales observados)

**Problema histórico:** S3 asumía unsigned 0-16383 → overflow en negativos → ADC inválido → FLAG_CALIB automático.

**Fix (2026-05-16 08:05):**
```cpp
void processPitchBend(byte channel, int bendValue) {
    // bendValue: -8192..+8191 (signed 14-bit desde Logic)
    
    // Clipear negativos a 0 (fondo del fader)
    if (bendValue < 0) bendValue = 0;
    
    // Mapear: Logic 0..8191 → ADC 0..27000
    uint16_t fader_adc = ((uint32_t)bendValue * 27000 / 8191);
    
    // Enviar a S2 vía RS485
    rs485.setFaderTarget(channel + 1, fader_adc);
    
    // Normalizar para display (-7..+7 VPot)
    float faderPositionNormalized = (float)fader_adc / 27000.0f;
    faderPositions[channel] = faderPositionNormalized;
}
```

### 5.2 Mapeo Bidireccional S2 → S3 → Logic

**Dirección 1: Logic → S3 → S2**
```
Logic PitchBend: -8192..+8191 (14-bit)
    ↓ [clipear negativos]
Logic PitchBend: 0..+8191
    ↓ [mapear × 27000 / 8191]
S3 fader_adc: 0..27000
    ↓ [RS485 MasterPacket.faderTarget]
S2 recibe: 0..27000 (destino motor)
    ↓ [Motor::setTarget()]
Motor posiciona fader
```

**Dirección 2: S2 → S3 → Logic**
```
S2 Fader ADC: 24..26476 (raw 16-bit)
    ↓ [S2 envía SlavePacket.faderPos]
S3 recibe: faderPos (0-27000)
    ↓ [EMA filter + mapeo: × 14848 / 27000]
S3 envia MIDI: PitchBend 0..14848
    ↓ [USB-MIDI]
Logic recibe PitchBend → fader sube/baja
```

### 5.3 EMA Filter (S3)

```cpp
// En RS485::_handleResponse(), RS485.cpp línea ~221
// Suaviza oscilaciones residuales ±8000 → ±3 cuentas

filteredFaderPos[id] = filteredFaderPos[id] + 
                       (rawFaderPos - filteredFaderPos[id]) * 0.15f;

// Resultado:
// - Sin crear "zonas muertas"
// - Responsividad a movimientos reales
// - Mejora ruido 2700× (±8000 → ±3)
```

---

## 6. RS485 SINCRONIZACIÓN

### 6.1 Ciclo de Comunicación (20ms aprox)

```
S3 → S2:  MasterPacket (16 bytes)
    │ Contiene: trackName, faderTarget, vuLevel, flags
    │ Si flags & FLAG_CALIB → S2 inicia calibración
    │ Si flags & FLAG_CALIB_DONE → S2 envía min/max en 2 paquetes

S2 → S3:  SlavePacket (9 bytes)
    │ Contiene: faderPos (16-bit ADC), buttons, encoderDelta, flags
    │ Si flags & CALIB_DONE → faderPos = min o max (2 paquetes)

S3 timeout: Si no recibe en 3000µs → marca timeout, retenta
```

### 6.2 Guard Cooldown (S2 Motor)

```cpp
// Motor.cpp::startCalib()
if (millis() - _motor_lastCalibDone < CALIB_COOLDOWN_MS) {
    log_w("[CALIB] Enfriamiento activo — se completó hace %ld ms",
          millis() - _motor_lastCalibDone);
    return;  // Rechaza reinicio <2s
}
```

**Razón:** Master (S3) puede enviar FLAG_CALIB múltiples veces en corto plazo. Guard evita loop infinito.

---

## 7. DEBUGGING Y DIAGNOSIS

### 7.1 Test Mode (SAT)

Acceso: Encoder push >3s → SAT menu → Motor → Test Mode

```
[MOTOR-TEST] Controles:
  REC button   → UP (PWM_MAX)
  SOLO button  → DOWN (PWM_MAX)
  MUTE button  → OFF
  
Display muestra: ADC actual, estado botones, PWM activo
Logs: [MOTOR-TEST] UP pwm=160 / [MOTOR-TEST] DOWN pwm=160
```

### 7.2 Diagnóstico ADC

```cpp
// En config.h
void dumpAdsLog() {
    // Vacía buffer circular en formato CSV
    // timestamp(µs), raw(16-bit), filtered(EMA)
    for (int i = 0; i < 256; i++) {
        Serial.printf("%ld,%d\n", _timestamp[i], _buffer[i]);
    }
}

// Uso: SAT → Diagnóstico → ADS Dump → analizar en Excel/Python
```

**Interpretación:**
- Si raw es estable ±2 cuentas → ADC correcto
- Si raw oscila ±30 cuentas → ruido acoplado (revisar cable, alimentación)
- Si filtrado está plano → EMA funcionando

### 7.3 Debugging Motor No Calibra

| Síntoma | Diagnóstico | Fix |
|---------|-------------|-----|
| Logs dicen "Iniciada" pero sin KICK_UP | Motor no responde a PWM | Verificar GPIO14 (EN) LOW en init() |
| KICK_UP llega a 26000 pero no baja | Motor pegado arriba | Mover manualmente o revisar fricción |
| ERROR "rango insuficiente" | Span <100 | Medir voltaje motor, revisar DRV8833 |
| Timeout 1000ms sin movimiento | Motor stalled | Test Mode: presionar REC/SOLO para forzar |

### 7.4 Logs Críticos a Revisar

```
[RS485] TIMEOUT slave 1 (#10 consecuciones)
  → S3 pierde comunicación con S2. Causas: cable, baurate, glitch
  
[CALIB] ERROR — rango insuficiente
  → Motor no tiene recorrido suficiente. Gap <40 cuentas.
  
[CALIB] Enfriamiento activo
  → S3 intenta recalibrar <2s después de completada. Normal en boot.
  
[ADS] raw=X pos=X motor=X  (cada 500ms)
  → ADC no actualiza (siempre X) = ISR no funciona (GPIO17)
```

---

## 8. VALIDACIÓN EN HARDWARE

### 8.1 Checklist Boot

```
[ ] S3 arranca → logs sin [RS485] TIMEOUT
[ ] S2 responde a FLAG_CALIB
[ ] S2 calibra:
    [ ] KICK_UP llega a ADC > 26000 (255ms)
    [ ] KICK_DOWN llega a ADC < 100 (264ms)
    [ ] OK con MIN > 20, MAX < 27000, span > 100
[ ] Fader responde a Logic:
    [ ] PitchBend mínimo → fader baja
    [ ] PitchBend máximo → fader sube
    [ ] Movimiento suave (sin saltos)
[ ] Encoder VPot ring responde
[ ] LEDs RGB encienden (estado OK)
```

### 8.2 Checklist Motion

```
[ ] Fader 0% → motor baja completamente
[ ] Fader 50% → motor en medio
[ ] Fader 100% → motor sube completamente
[ ] Toque fader → motor para inmediatamente
[ ] Soltar toque → motor puede seguir target
[ ] Sin jitter en posición intermedia
[ ] Sin lag visual (respuesta <100ms)
```

### 8.3 Checksum Validación

```
S2 SlavePacket:
  [ ] CRC8 coincide
  [ ] ID coincide (1-8)
  [ ] faderPos en rango 0-27000
  [ ] buttons bits 0-3 válidos
  [ ] flags coherentes (no CALIB_SENDING sin CALIB_DONE)

S3 respuesta:
  [ ] Captura calibratedMin en paquete 1
  [ ] Captura calibratedMax en paquete 2
  [ ] Calcula span = MAX - MIN
  [ ] Mapea target Logic → ADC
```

---

## 9. PERFORMANCE SPECS

| Métrica | Medida | Nota |
|---------|--------|------|
| **Calibración** | 3-5s | No-bloqueante, ejecución en loop |
| **Motor latencia** | <100ms | Desde PitchBend Logic a movimiento |
| **ADC ruido** | ±3 cuentas | Post-EMA (vs ±30 nativo) |
| **ADC latencia** | 0-2µs | ISR-driven (vs 24ms polling) |
| **RS485 ciclo** | 20ms/8slaves | 2.5ms/slave |
| **Fader resolución** | 0.5% (27000 → 27 cuentas) | EMA suaviza a ±3 cuentas |
| **Dead zone** | 50 cuentas | Motor apagado si error < umbral |
| **Touch latencia** | 120ms | Sostenimiento 6 frames |
| **Display refresh** | 33ms (30 FPS) | SPI3 5MHz |

---

## 10. VUMETER — Arquitectura Completa (2026-05-26)

### 10.1 Qué es y para qué sirve

El VUMeter del S2 muestra el nivel de audio del canal asignado. Los datos vienen de Logic Pro vía MIDI → S3 → RS485 → S2 → Display. Es un indicador **pasivo**: S2 recibe el nivel ya calculado por Logic, no procesa audio.

---

### 10.2 Flujo de datos completo

```
Logic Pro
  │  Channel Pressure (0xD0)
  │  value = (trackId << 4) | mcu_level
  │  mcu_level 0x00-0x0B = nivel  0x0E = clip  0x0F = clear_clip
  │  Frecuencia: ~30 Hz (cada ~33ms)
  ▼
S3 MIDIProcessor::processChannelPressure()
  │  Decodifica trackId y mcu_level
  │  Normaliza: normalizedLevel = mcu_level / 11.0f
  │  Convierte: vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f)
  │  Llama: rs485.setVuLevel(trackId + 1, vuLevel7bit)
  ▼
S3 RS485Master — _ch[id].vuLevel = value
  │  Incluido en MasterPacket.vuLevel (0-127) en cada ciclo RS485
  │  Frecuencia: ~50 Hz (ciclo RS485 ~20ms con 1 slave)
  ▼
S2 RS485Handler::onMasterData()
  │  float newVu = pkt.vuLevel / 127.0f  →  0.0-1.0
  │  Attack instantáneo: solo sube desde RS485
  │  vuLastUpdateTime = millis() si newVu > 0.01
  │  needsVUMetersRedraw = true si nivel subió
  ▼
S2 handleVUMeterDecay()  ← llamada cada loop()
  │  Decay: -1/12 segmento cada 100ms si no llega señal
  │  Peak hold: 2000ms, luego salta al nivel actual
  ▼
S2 drawVUMeters()  ← llamada desde updateDisplay()
  │  Dibujo diferencial: solo segmentos que cambiaron
  │  Directo en tft (sin sprite intermedio)
  ▼
Display ST7789V3 — 12 segmentos RGB565
```

---

### 10.3 Encoding Mackie MCU — Channel Pressure

Logic Pro envía el nivel VU como **Channel Pressure** (MIDI 0xD0) en canal MIDI 1.

```
Byte value = (trackId << 4) | mcu_level

trackId:   bits 7-4  → qué canal del Extender (0-7)
mcu_level: bits 3-0  → nivel en 4 bits:
  0x00           = silencio
  0x01 .. 0x0B   = escala logarítmica (1/11 .. 11/11)
  0x0C, 0x0D     = alto / muy alto  → normalizedLevel = 1.0
  0x0E           = clip             → vuLevel7bit = 127
  0x0F           = clear clip       → mantiene nivel actual
```

**Datos reales capturados con MIDI Monitor (2026-05-26):**

```
Frecuencia:  ~30 Hz (grupos de 4-5 mensajes cada ~33ms)
Niveles observados: mcu_level 5, 6, 7, 8 (oscilación normal de audio)
Patrón por ciclo:
  value=6   → track 0, level 6  ← VU real del canal activo
  value=31  → track 1, clear_clip (0x1F)
  value=47  → track 2, clear_clip (0x2F)
  value=79  → track 4, clear_clip (0x4F)

mcu_level 5 → vuLevel7bit = 58  →  newVu = 0.457 →  5-6/12 segmentos
mcu_level 6 → vuLevel7bit = 69  →  newVu = 0.543 →  6-7/12 segmentos
mcu_level 7 → vuLevel7bit = 81  →  newVu = 0.638 →  7-8/12 segmentos
mcu_level 8 → vuLevel7bit = 92  →  newVu = 0.724 →  8-9/12 segmentos
```

**Diagnóstico del chisporreo (2026-05-26):**
El VUMeter chisporroteaba porque la señal de audio real fluctúa entre mcu_level 5-9 (~33ms), generando cambios de 4 segmentos visibles sin ningún smoothing. No había VU=0 intermitentes — era oscilación normal del audio sin decay visual.

---

### 10.4 Conversión en S3 — processChannelPressure()

```cpp
// MIDIProcessor.cpp
void processChannelPressure(byte channel, byte value) {
    // channel == 0 → encoding Mackie compacto (trackId + level en 1 byte)
    if (channel == 0) {
        int targetChannel = (value >> 4) & 0x0F;   // track 0-7
        byte mcu_level    = value & 0x0F;

        switch (mcu_level) {
            case 0x0F:  // clear clip — mantiene nivel, solo borra indicador
                clearClip = true;
                normalizedLevel = vuLevels[targetChannel];
                vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f);  // ← NO enviar 0
                break;
            case 0x0E:  // clip
                newClipState = true;
                normalizedLevel = 1.0f;
                vuLevel7bit = 127;
                break;
            case 0x0C: case 0x0D:  // muy alto
                normalizedLevel = 1.0f;
                vuLevel7bit = 120;
                break;
            default:   // 0x00-0x0B → escala normal
                normalizedLevel = (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
                vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f);
                break;
        }
        rs485.setVuLevel(targetChannel + 1, vuLevel7bit);
    }
}
```

⚠️ **Bug conocido en clear_clip (no crítico con NUM_SLAVES=1):**
El case `0x0F` original dejaba `vuLevel7bit = 0` → enviaba VU=0 al slave en cada clear_clip. Con NUM_SLAVES=1 el único slave activo es track 0, que nunca recibe clear_clip, así que no afecta. Con más slaves sí causaría chisporreo en los que reciben clear_clip con audio activo.

---

### 10.5 Lógica Attack/Decay en S2

**Antes (bug):** `vuLevels` se actualizaba en cualquier dirección (subida y bajada) desde RS485, y `handleVUMeterDecay()` nunca se llamaba → el VU reflejaba directamente el valor crudo de cada paquete → chisporreo.

**Después (fix 2026-05-26):**

```
Attack (RS485Handler::onMasterData):
  ├── Si newVu > 0.01 → actualiza vuLastUpdateTime (previene decay)
  └── Si newVu > vuLevels + 0.01 → sube vuLevels instantáneamente

Decay (handleVUMeterDecay, llamada en loop()):
  ├── Si vuLevels > 0 y now - vuLastUpdateTime > 100ms → baja 1/12 por tick
  ├── Peak hold: 2000ms, luego peak salta al nivel actual
  └── Seguridad: peak nunca < nivel actual
```

**Comportamiento resultante:**
- Audio sube de 0 a 9 segmentos → VU sube instantáneamente
- Audio baja momentáneamente → `vuLastUpdateTime` se mantiene activo → decay NO actúa
- Audio deja de llegar (silencio real) → tras 100ms, decay baja 1 segmento cada 100ms
- Pico → línea gris se mantiene 2s en el máximo alcanzado

---

### 10.6 Arquitectura de Dibujo — Diferencial sin Sprite

#### Antes (sprite completo):
```
drawVUMeters() {
  vuSprite.fillSprite(gris)          ← borra 60×230px en PSRAM  (~27KB escritura)
  for i in 0..11:
    vuSprite.fillRoundRect(...)      ← 12 primitivas en PSRAM
  vuSprite.pushSprite(x, y)          ← SPI: 60×230×2 = 27.6KB transferidos
}
```
**Coste total:** ~27KB por SPI + 12 fillRoundRect en PSRAM, siempre.  
**PSRAM consumida:** 27.6KB (liberados con el nuevo diseño).

#### Después (diferencial directo en tft):
```
drawVUMeters() {
  Calcular: active, peak, showPeak, clip

  Si lastActive < 0:                 ← primera vez o tras fillScreen total
    tft.fillRect(área VU, gris)      ← fondo una sola vez
    for i in 0..11: vuDrawSeg(i)     ← 12 segmentos

  Si no:                             ← diferencial
    for i in 0..11:
      si color cambió: vuDrawSeg(i)  ← solo los que cambiaron (1-3 típico)
}
```
**Coste típico (1 segmento cambia):** 1 fillRoundRect = 42×17px × 2B = ~1.4KB.  
**Ratio vs anterior:** ~20× menos datos SPI en el caso normal.

#### Geometría del VU (coordenadas absolutas en pantalla):

```
namespace VU {
    X      = MAINAREA_WIDTH + 3    // pixel X inicio en pantalla
    Y_TOP  = HEADER_HEIGHT  + 4    // pixel Y inicio en pantalla
    W      = 42                    // ancho del VU en pixels
    H      = MAINAREA_HEIGHT - 10  // alto total del VU en pixels
    SEGS   = 12                    // número de segmentos
    PAD    = 2                     // separación entre segmentos (px)
    CORNER = 2                     // radio de esquinas redondeadas (px)
    SEG_H  = (H - PAD*(SEGS-1)) / SEGS   // alto de cada segmento ≈ 17px
}
```

#### Colores por segmento:

| Segmentos | Estado ON | Estado OFF | Peak borde |
|-----------|-----------|------------|------------|
| 0-7 (bajo) | `VU_GREEN_ON` (verde) | `VU_GREEN_OFF` (verde oscuro) | `VU_PEAK_COLOR` (gris) |
| 8-9 (medio) | `VU_YELLOW_ON` (amarillo) | `VU_YELLOW_OFF` (amarillo oscuro) | `VU_PEAK_COLOR` |
| 10-11 (alto) | `VU_RED_ON` (rojo) | `VU_RED_OFF` (rojo oscuro) | `VU_PEAK_COLOR` |
| 11 (clip) | `VU_RED_ON` (siempre ON si clip) | — | — |

#### Lógica de selección de color:

```cpp
static uint16_t vuSegColor(int i, int active, int peak, bool clip) {
    if (i == VU::SEGS - 1 && clip) return VU_RED_ON;           // clip override
    if (peak >= 0 && i == peak)    return offColor(i);          // peak: fondo apagado
    if (i < active)                return onColor(i);           // activo: encendido
    return                                offColor(i);           // inactivo: apagado
}
// Peak borde doble: drawRoundRect exterior + interior (efecto 3D)
```

#### Estado diferencial guardado:

```cpp
namespace VU {
    int8_t lastActive = -1;   // -1 = no dibujado → fuerza fondo + todos los segs
    int8_t lastPeak   = -1;   // índice del segmento pico anterior (-1 = sin pico)
    bool   lastClip   = false;
}
// Reset a -1 en cada needsTOTALRedraw (tras fillScreen) para forzar redibujado completo
```

---

### 10.7 handleVUMeterDecay() — Implementación

```cpp
// Display.cpp (S2) y UIPage3.cpp (P4) — misma lógica
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 100;    // 1 segmento cada 100ms
    const unsigned long PEAK_HOLD_TIME_MS = 2000;   // peak se mantiene 2s
    const float         DECAY_AMOUNT      = 1.0f / 12.0f;  // 1 segmento por tick

    unsigned long now = millis();
    bool changed = false;

    // 1. Decay del nivel: baja si no llega señal nueva
    if (vuLevels > 0 && now - vuLastUpdateTime > DECAY_INTERVAL_MS) {
        vuLevels -= DECAY_AMOUNT;
        if (vuLevels < 0.01f) vuLevels = 0.0f;
        vuLastUpdateTime = now;   // ← resetea para que el siguiente tick espere 100ms más
        changed = true;
    }

    // 2. Decay del peak: tras 2s, peak salta al nivel actual
    if (vuPeakLevels > 0 && now - vuPeakLastUpdateTime > PEAK_HOLD_TIME_MS) {
        if (vuPeakLevels > vuLevels) {
            vuPeakLevels = vuLevels;
            vuPeakLastUpdateTime = now;
            changed = true;
        }
    }

    // 3. Seguridad: peak nunca puede ser menor que el nivel
    if (vuPeakLevels < vuLevels) {
        vuPeakLevels = vuLevels;
        vuPeakLastUpdateTime = now;
        changed = true;
    }

    if (changed) needsVUMetersRedraw = true;
}
```

**Dónde se llama:**
- **S2:** `loop()` en `main.cpp`, antes de `updateDisplay()`
- **P4:** `uiPage3Update()` en `UIPage3.cpp`, línea ~169

**Tiempo de decay de nivel máximo a 0:** 12 ticks × 100ms = **1.2 segundos**

---

### 10.8 Integración en loop() S2

```cpp
// main.cpp — orden en loop()
updateButtons();
handleVUMeterDecay();    // ← decay temporal (100ms tick)
updateDisplay();          // ← drawVUMeters() si needsVUMetersRedraw
updateAllNeopixels();
```

`handleVUMeterDecay()` se llama en cada iteración del loop (sin throttle propio en la llamada — el throttle está internamente con `DECAY_INTERVAL_MS`). El coste es mínimo: 3 comparaciones de `millis()` + 1 resta si hay que decaer.

---

### 10.9 Diferencias S2 vs P4

| Aspecto | S2 (Slave) | P4 (Master) |
|---------|-----------|-------------|
| **Librería display** | LovyanGFX (tft directo) | LVGL v9 (objetos) |
| **Dibujo** | `tft.fillRoundRect()` directo | `lv_obj_set_style_bg_color()` |
| **Canales VU** | 1 (canal propio) | 9 (todos los slaves del bus A) |
| **Estado previo** | `VU::lastActive`, `VU::lastPeak` | LVGL mantiene estado de objetos |
| **Decay** | `handleVUMeterDecay()` en loop() | `handleVUMeterDecay()` en uiPage3Update() |
| **PSRAM para VU** | 0 (eliminado sprite) | N/A (LVGL heap interno) |

---

### 10.10 Performance VUMeter

| Métrica | Valor | Notas |
|---------|-------|-------|
| **Frecuencia Logic → S3** | ~30 Hz (33ms) | Channel Pressure cada frame de audio |
| **Frecuencia S3 → S2** | ~50 Hz (20ms) | Ciclo RS485 con 1 slave |
| **Attack (subida)** | instantáneo | RS485Handler actualiza sin delay |
| **Decay (bajada)** | 100ms/segmento | 1.2s de máx a 0 |
| **Peak hold** | 2000ms | Luego salta al nivel actual |
| **Segmentos cambiados/update** | 1-3 (típico) | Audio ±1-2 segmentos entre frames |
| **Datos SPI/update** | ~1.4KB (1 seg) | vs 27.6KB antes (sprite completo) |
| **Mejora SPI** | ~20× | Caso típico 1 segmento |
| **PSRAM liberada** | 27.6KB | vuSprite eliminado |

---

## 11. HISTORIA DE CAMBIOS

### 2026-05-16 08:05 — PitchBend signed 14-bit fix

**Problema:** Logic envía signed 14-bit (-8192..+8191), S3 asumía unsigned 0-16383 → overflow en negativos.

**Síntoma:** Cada vez que Logic desconecta (PB -8192), S3 detecta "no calibrado" → FLAG_CALIB automático → S2 calibra innecesariamente.

**Fix:** Clipear negativos a 0, mapear 0..8191 → 0..27000.

### 2026-05-19 — Protección topes mecánicos (stall detection)

**Problema:** Motor se calentó al apretarse contra el tope físico inferior. La condición `ADC <= MOTOR_ADC_MIN + 10 = 30` nunca se cumplía porque el tope físico real del fader entrega ADC ≈ 44.

**Causa raíz:** `MOTOR_ADC_MIN = 20` es un umbral de filtro de ruido, no el valor ADC del tope físico. Confundir ambos causó que la condición de salida de GOING_TO_MIN nunca se activara.

**Lección permanente:** Los topes mecánicos siempre se detectan por **stall** (ADC estable varios cientos de ms), nunca por valor absoluto de ADC.

**Fixes:**
1. GOING_TO_MIN: stall local 400ms → transiciona a CALIBRATING o AT_TARGET
2. Global en `Motor::update()`: protege GOING_TO_MIN y MOVING_TO_TARGET — si `_motor_hw_active` y ADC sin cambio 400ms → apaga motor

**Commits:** `06d9562` (stall GOING_TO_MIN), `xxxxxxx` (protección global)

### 2026-05-16 07:48 — Guard cooldown S2 Motor

**Problema:** Calibración en bucle infinito: completaba → siguiente RS485 paquete con FLAG_CALIB → reiniciaba.

**Fix:** Guard cooldown 2000ms en `Motor::startCalib()`.

### 2026-05-14 17:06 — EMA Filter S3

**Problema:** faderPos oscilaba ±8000 cuentas en posición estática.

**Fix:** EMA alpha=0.15 en recepción RS485 → ±3 cuentas post-filter.

### 2026-05-13 00:30 — Mapeo bidireccional

**Arquitectura:** S3 mapea ambas direcciones (entrada Logic, salida ADC). S2 recibe valor final, sin cálculos (O(1), compatible single-core).

---

## 11. REFERENCIAS

- **CLAUDE.md** — Instrucciones obligatorias, init order, hardware safety
- **STATUS.md** — Bugs conocidos, RS485 spec, timing auditoria
- **CHANGELOG.md** — Historial detallado de fixes y validaciones
- **Motor.cpp** — Máquina de estados calibración (líneas 57-229)
- **FaderADC.cpp** — ISR ADC, buffer circular (líneas 40-120)
- **MidiProcessor.cpp** — Mapeo Logic PitchBend (líneas 599-612)
- **RS485.cpp** — Comunicación master, min/max capture (líneas 189-280)

---

**Responsable de documentación:** iMakie Development Team  
**Fecha de creación:** 2026-05-16 08:15  
**Última actualización:** 2026-05-16 08:15  
**Estado de validación:** ✅ Hardware (17× S2 activos)
