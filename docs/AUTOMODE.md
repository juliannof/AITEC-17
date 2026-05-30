# AUTOMODE — Routing del fader según modo de automatización (S2)

Documentación del sistema de **AutoMode awareness** del fader motorizado S2. Define cómo el handler RS485 enruta el target del DAW al motor en función del modo de automatización activo (OFF / READ / WRITE / TRIM / TOUCH / LATCH).

**Última actualización:** 2026-05-30 09:35
**Estado:** En implementación — pendiente validación hardware
**Aplica a:** S2 (Slave). S3 y P4 ya transmiten AutoMode en `MasterPacket.flags` (bits 5-7).

---

## 1. PRINCIPIOS DE DISEÑO

### 1.1 Punto único de decisión

Toda la lógica de AutoMode vive en **`RS485Handler::Internal::_applyFaderTarget()`** (`S2/S2_V1/src/RS485/RS485Handler.cpp`). El `Motor` es un **actuador puro** y no tiene conocimiento de AutoMode.

| Componente | Responsabilidad |
|------------|-----------------|
| `RS485Handler::onMasterData()` | Extrae `AutoMode`, detecta cambio de modo, dispatcha a `_applyFaderTarget()` |
| `RS485Handler::Internal::_applyFaderTarget()` | Decide qué API del motor llamar según el modo |
| `RS485Handler::buildResponse()` | Reporta `touchState` con debounce específico del modo activo |
| `Motor::setTargetForced()` | Aplica target ignorando guard usuario (AUTO_OFF/READ) |
| `Motor::setTargetFromS3()` | Aplica target respetando guard usuario (AUTO_TOUCH/TRIM/LATCH) |
| `Motor::isManualTouchDetected()` | Proxy de touch (ADC delta) — única fuente cruda de detección |

### 1.2 FaderTouch — no fiable actualmente

`FaderTouch::isTouched()` produce falsos positivos en topes mecánicos y no se usa para enrutar el motor ni para reportar `touchState`. La fuente de verdad actual es **`Motor::isManualTouchDetected()`** (delta ADC en ventana 80ms).

Cuando FaderTouch sea fiable, el cambio es de **una sola línea** en `buildResponse()`:

```cpp
// Actualmente:
bool rawTouch = Motor::isManualTouchDetected();
// Futuro:
bool rawTouch = FaderTouch::isTouched();
```

Hay un `TODO` marcado en el código en ese punto.

---

## 2. COMPORTAMIENTO POR MODO

### 2.1 Tabla resumen

| AutoMode | Valor | Motor llamado | Guard usuario | touchState debounce | LATCH frozen |
|----------|:-----:|---------------|---------------|---------------------|:------------:|
| `AUTO_OFF` | 0 | `setTargetForced()` | ❌ ignorado | 600ms | ❌ |
| `AUTO_READ` | 1 | `setTargetForced()` | ❌ ignorado | 600ms | ❌ |
| `AUTO_WRITE` | 2 | — (motor inhibido) | n/a | 600ms | ❌ |
| `AUTO_TRIM` | 3 | `setTargetFromS3()` | ✅ activo | **80ms** | ❌ |
| `AUTO_TOUCH` | 4 | `setTargetFromS3()` | ✅ activo | **80ms** | ❌ |
| `AUTO_LATCH` | 5 | `setTargetFromS3()` / inhibido | ✅ + freeze | **300ms** | ✅ |

### 2.2 AUTO_OFF / AUTO_READ — DAW absoluto

- DAW tiene autoridad total.
- Si el usuario empuja el fader, el motor **vuelve inmediatamente** al target sin esperar debounce.
- Si veníamos de LATCH con freeze activo, se descongela aquí.
- Internamente: `Motor::setTargetForced(target)` — bypass del guard `_motor_manualTouchDetected`.

**Caso típico:** Logic en READ reproduciendo automatización grabada. El fader sigue al DAW siempre.

### 2.3 AUTO_WRITE — usuario libre, motor inhibido

- Motor nunca recibe target. No se mueve por sí solo.
- Usuario mueve el fader libremente; Logic registra la posición física vía `SlavePacket.faderPos`.
- `touchState` se reporta normalmente (con debounce 600ms base) para que Logic sepa cuándo escribir.

**Caso típico:** grabación inicial de automatización con fader vacío.

### 2.4 AUTO_TOUCH / AUTO_TRIM — usuario gana mientras toca

- Mientras `Motor::isManualTouchDetected()` es true: motor inhibido por el guard interno de `setTargetFromS3()` (no necesitamos lógica adicional aquí).
- Al soltar, tras la ventana de debounce **80ms**, el motor reanuda seguimiento del target DAW.
- `touchState` mantiene `1` durante todo el debounce de 80ms — Logic recibe la ventana exacta para escribir o no.

**Caso típico:** ajuste fino en TOUCH; el usuario corrige un fader y suelta para que Logic siga la automatización original.

### 2.5 AUTO_LATCH — freeze hasta que Logic mueva mucho

Comportamiento de tres estados:

1. **Tocando:** primera detección → `_rsLatchFrozen = true`, `_rsLatchFrozenADC = ADC actual`. Motor inhibido.
2. **Frozen (sin tocar):** motor sigue inhibido. Cada paquete RS485 comprueba `abs(target - _rsLatchFrozenADC) > AUTOMODE_LATCH_UNFREEZE_ADC` (200 cuentas, ≈1.5% del rango).
3. **Descongelado:** Logic ha decidido un valor lejano del frozen → `_rsLatchFrozen = false`, motor sigue al target.

**Caso típico:** el usuario corrige un fader en LATCH y lo deja en una posición nueva. La automatización se mantiene en esa posición hasta que Logic envía un valor sustancialmente distinto (nueva rama de automatización).

`touchState` con debounce **300ms** — ventana más larga para que Logic capte la transición a "frozen".

---

## 3. ESTADO Y CONSTANTES

### 3.1 Variables en `config.h`

Siguiendo la directiva CLAUDE.md "NUNCA static de estado en .cpp", el estado vive en `config.h`:

```cpp
// Constantes
static constexpr uint32_t AUTOMODE_TOUCH_DEBOUNCE_MS  =  80;
static constexpr uint32_t AUTOMODE_LATCH_DEBOUNCE_MS  = 300;
static constexpr uint16_t AUTOMODE_LATCH_UNFREEZE_ADC = 200;

// Estado del handler
static AutoMode  _rsCurrentMode    = AUTO_OFF;
static bool      _rsLatchFrozen    = false;
static uint16_t  _rsLatchFrozenADC = 0;
static bool      _rsTouchActive    = false;
static uint32_t  _rsLastTouchTime  = 0;
```

| Variable | Propósito | Cuándo se activa | Cuándo se resetea |
|----------|-----------|------------------|-------------------|
| `_rsCurrentMode` | Último modo aplicado | Cada `onMasterData()` si cambia | Nunca (siempre tiene un valor) |
| `_rsLatchFrozen` | LATCH ha capturado pos | LATCH + touch detectado | OFF/READ entry, cambio de modo, target DAW lejano |
| `_rsLatchFrozenADC` | ADC capturado al freeze | Al activarse `_rsLatchFrozen` | Implícito (solo válido si frozen) |
| `_rsTouchActive` | Ventana de touch reportada | Touch crudo detectado | Tras `_touchDebounceForMode()` sin touch |
| `_rsLastTouchTime` | Timestamp último touch | Touch crudo detectado | Cambio de modo (reset total) |

### 3.2 Regla de reset total al cambiar de modo

Cuando `pktMode != _rsCurrentMode`:
- `_rsLatchFrozen = false`
- `_rsTouchActive = false`
- `_rsLastTouchTime = 0`

Razón: cada modo debe arrancar limpio. Si veníamos de LATCH frozen y pasamos a READ, el freeze previo no debe arrastrarse a una sesión nueva en otro modo.

---

## 4. FLUJO COMPLETO — `onMasterData()`

```text
Paquete RS485 RX
    │
    ▼
┌────────────────────────────────────────────────┐
│ 1. FLAG_CALIB → Motor::requestCalibration()    │  (independiente de modo)
├────────────────────────────────────────────────┤
│ 2. pkt.connected → setConnected(true/false)    │  (existente)
├────────────────────────────────────────────────┤
│ 3. trackName, botones, VU, encoder             │  (existente)
├────────────────────────────────────────────────┤
│ 4. pktMode = getAutoMode(pkt.flags)            │  ←── NUEVO
│    Si pktMode != _rsCurrentMode:               │
│      - log "cambio modo X → Y"                 │
│      - reset _rsLatchFrozen + _rsTouchActive   │
│      - _rsCurrentMode = pktMode                │
├────────────────────────────────────────────────┤
│ 5. Display: setAutoMode(uint8_t) si cambia     │  (existente, para color VPot)
├────────────────────────────────────────────────┤
│ 6. faderPositions = pkt.faderTarget/27000.0f   │  (solo display)
│    Internal::_applyFaderTarget(pktMode,target) │  ←── NUEVO (en CADA paquete)
├────────────────────────────────────────────────┤
│ 7. setVPotRaw(pkt.vpotValue)                   │  (existente)
└────────────────────────────────────────────────┘
```

**Diferencia clave vs versión anterior:** el target del fader se aplicaba solo si cambiaba el valor (`fabsf(faderPositions - newFader) > 0.001f`). Ahora se reevalúa en **cada paquete**, para que al soltar el fader en TOUCH/LATCH el motor vuelva al target sin esperar a que Logic reenvíe un valor distinto.

---

## 5. FLUJO DE `_applyFaderTarget()`

```text
_applyFaderTarget(mode, target):
    switch(mode):

        AUTO_OFF / AUTO_READ:
            if _rsLatchFrozen: log + clear
            Motor::setTargetForced(target)    ← bypass guard usuario

        AUTO_WRITE:
            return  (motor inhibido)

        AUTO_TOUCH / AUTO_TRIM:
            Motor::setTargetFromS3(target)    ← guard cooperativo

        AUTO_LATCH:
            if Motor::isManualTouchDetected():
                if !_rsLatchFrozen:
                    _rsLatchFrozen = true
                    _rsLatchFrozenADC = Motor::getRawADC()
                return  (no mover)
            if _rsLatchFrozen:
                if abs(target - _rsLatchFrozenADC) > UNFREEZE_ADC:
                    _rsLatchFrozen = false
                    Motor::setTargetFromS3(target)
                return  (mantener frozen)
            Motor::setTargetFromS3(target)    ← sin frozen, sin touch

        default:
            Motor::setTargetForced(target)    ← conservador (DAW manda)
```

---

## 6. `touchState` CON DEBOUNCE POR MODO

`buildResponse()` mantiene una ventana virtual `_rsTouchActive` que extiende o recorta el touch crudo según el debounce del modo:

```text
rawTouch = Motor::isManualTouchDetected()
now = millis()

if rawTouch:
    _rsTouchActive   = true
    _rsLastTouchTime = now
else if _rsTouchActive and (now - _rsLastTouchTime) > _touchDebounceForMode(_rsCurrentMode):
    _rsTouchActive = false

resp.touchState = (!motorCalibrating && _rsTouchActive) ? 1 : 0
```

| Modo | Debounce reportado |
|------|-------------------:|
| TOUCH / TRIM | 80ms |
| LATCH | 300ms |
| OFF / READ / WRITE | 600ms (base) |

**Por qué dos debounces distintos:**
- El de **Motor** (600ms, `MANUAL_TOUCH_DEBOUNCE_MS`) está optimizado para *autoridad de fader* — cuánto tiempo el motor ignora targets DAW tras un touch.
- El de **touchState reportado a S3** depende del modo Logic: TOUCH escribe inmediatamente cuando deja de tocar, así que ventanas largas le confunden.

---

## 7. INVARIANTES Y GARANTÍAS

| Invariante | Cómo se garantiza |
|------------|-------------------|
| Logic y fader físico siempre coinciden (READ/OFF) | `setTargetForced()` sin guard usuario — DAW siempre gana |
| Usuario tiene control libre (WRITE) | Motor nunca recibe target en WRITE |
| Motor regresa al target al soltar (TOUCH/TRIM) | `setTargetFromS3()` guard se libera con `_motor_manualTouchDetected` debounce (600ms motor) — el touchState reportado tiene su propio debounce 80ms para Logic |
| Frozen persiste hasta Logic mueva (LATCH) | `_rsLatchFrozen` solo se limpia con cambio de modo o `abs(target - frozenADC) > 200` |
| Cambio de modo no arrastra estado previo | Reset total `_rsLatchFrozen`, `_rsTouchActive`, `_rsLastTouchTime` |
| Calibración independiente de modo | `FLAG_CALIB` se procesa antes del routing de modo, llama directo a `Motor::requestCalibration()` |

---

## 8. EDGE CASES Y NOTAS

### 8.1 `setTargetFromS3()` sigue teniendo su propio guard

Aunque `_applyFaderTarget()` decide qué función llamar, **`setTargetFromS3()` mantiene** su comprobación interna `if (_motor_manualTouchDetected) return;`. Eso es intencional:

- Garantiza que TOUCH/TRIM/LATCH funcionan correctamente sin necesidad de duplicar lógica en RS485Handler.
- Si en el futuro otra ruta llama a `setTargetFromS3()` (p. ej. SAT manual), el guard sigue activo.
- `setTargetForced()` existe para los modos que **explícitamente** quieren ignorar el guard (OFF/READ).

### 8.2 Cambio de modo durante MOVING_TO_TARGET

Si Logic cambia de modo mientras el motor está moviéndose:
- El reset limpia `_rsTouchActive` y `_rsLatchFrozen`.
- El motor sigue en `MOVING_TO_TARGET` hasta llegar al target actual.
- El siguiente paquete RS485 aplica el nuevo modo con el nuevo target.

No hay condición de carrera porque `_motor_state` lo gestiona el `Motor::update()` desacoplado del handler.

### 8.3 LATCH + Calibración

Si Logic solicita calibración (`FLAG_CALIB`) mientras `_rsLatchFrozen` es true:
- `Motor::requestCalibration()` se procesa primero (línea 146 de `onMasterData()`).
- El motor entra en `GOING_TO_MIN` / `CALIBRATING` — `_motor_state` cambia.
- `_applyFaderTarget()` se ejecuta después, pero el motor ignorará órdenes durante calibración (guards internos del motor).
- Tras calibración: `_rsLatchFrozen` sigue activo (solo se limpia con cambio de modo o target lejano), pero `_rsLatchFrozenADC` ya no es coherente con el nuevo rango calibrado.

**Riesgo bajo:** Logic raramente recalibra en mitad de una sesión LATCH. Si ocurre, el primer target nuevo lejano descongelará. Se podría añadir un reset explícito de `_rsLatchFrozen` al detectar fin de calibración, pero está fuera del scope inicial.

---

## 9. ARCHIVOS AFECTADOS

| Archivo | Cambio |
|---------|--------|
| `S2/S2_V1/src/hardware/Motor/Motor.h` | + declaración `Motor::setTargetForced()` |
| `S2/S2_V1/src/hardware/Motor/Motor.cpp` | + implementación `Motor::setTargetForced()` |
| `S2/S2_V1/src/config.h` | + constantes `AUTOMODE_*` + estado `_rs*` |
| `S2/S2_V1/src/RS485/RS485Handler.h` | + `Internal` namespace con `_applyFaderTarget`, `_touchDebounceForMode` |
| `S2/S2_V1/src/RS485/RS485Handler.cpp` | + implementaciones `Internal::*`, refactor `onMasterData()` + `buildResponse()` |

**MCU no afectadas:** S3 (ya envía AutoMode), P4 (no toca fader directo).

---

## 10. VALIDACIÓN HARDWARE PENDIENTE

| Test | Estado |
|------|:------:|
| AUTO_OFF: usuario empuja fader → motor vuelve sin debounce | ⏳ |
| AUTO_READ: idem | ⏳ |
| AUTO_WRITE: motor nunca se mueve, posición física reportada a Logic | ⏳ |
| AUTO_TOUCH: tocar → motor para; soltar → motor vuelve tras 80ms reportado | ⏳ |
| AUTO_TRIM: igual que TOUCH | ⏳ |
| AUTO_LATCH: tocar → frozen; soltar lejos del DAW → mantiene posición | ⏳ |
| AUTO_LATCH: Logic mueve >200 cuentas → descongelar y seguir | ⏳ |
| Cambio TOUCH→READ con frozen: reset limpio | ⏳ |
| Calibración FLAG_CALIB en cualquier modo: prevalece | ⏳ |

**Regla CLAUDE.md:** sin validación hardware, no merge a producción de los faders desplegados.

---

## 11. REFERENCIAS CRUZADAS

- **[MOTOR.md](MOTOR.md)** — API del motor, máquina de estados, calibración
- **[RS485.md](RS485.md)** — Protocolo binario, MasterPacket/SlavePacket
- **[FADER.md](FADER.md)** — ADS1115, FaderTouch, mapping Logic↔ADC
- **[MIDI.md](MIDI.md)** — Cómo Logic envía AutoMode al master (Mackie MCU)
- **[CLAUDE.md](../CLAUDE.md)** — Directivas vinculantes, jerarquía Motor v3
