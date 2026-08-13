# ENCODER — Rotario Infinito VPot (iMakie S2)

Documentación exhaustiva del subsistema de encoder rotario. Incluye hardware, firmware ISR, sequenciamiento, debounce, integración SAT, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (ISR-driven, resolución comprobada)

---

## 1. HARDWARE ENCODER

### 1.1 Pinout Encoder

| Señal | GPIO | Función | Lógica |
|-------|------|---------|--------|
| **A** | 12 | Canal A | Gray code |
| **B** | 13 | Canal B | Gray code |
| **Push** | 11 | Botón encoder | Activo LOW |

**Tipo:** Rotario infinito (sin límite de giros), Gray code (error detection)

### 1.2 Configuración Hardware

```cpp
// config.h (S2)
#define ENCODER_PIN_A       13  // Gray code A
#define ENCODER_PIN_B       12  // Gray code B
#define ENCODER_SW_PIN      11  // Switch (botón)
#define ENCODER_DEBOUNCE_DELAY_MS 5
```

```cpp
// Hardware init
pinMode(ENCODER_PIN_A, INPUT);
pinMode(ENCODER_PIN_B, INPUT);
pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
```

### 1.3 Especificación Gray Code

```
Estado A-B:
  00 = posición 0
  01 = posición 1
  11 = posición 2
  10 = posición 3
  00 = posición 0 (siguiente ciclo)

Rotación derecha: 00→01→11→10→00 (+1 por ciclo)
Rotación izquierda: 00→10→11→01→00 (-1 por ciclo)
```

---

## 2. FIRMWARE ISR

### 2.1 Interrupt Service Routine

```cpp
// Encoder.cpp - ISR basada en cambio (CHANGE)
static volatile int _count = 0;
static uint8_t _lastState = 0;

void IRAM_ATTR onEncoderChange() {
    uint8_t currentState = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
    
    // Gray code decoding
    if (currentState != _lastState) {
        // Derecha: 00→01→11→10
        if ((_lastState == 0b00 && currentState == 0b01) ||
            (_lastState == 0b01 && currentState == 0b11) ||
            (_lastState == 0b11 && currentState == 0b10) ||
            (_lastState == 0b10 && currentState == 0b00)) {
            _count++;  // Derecha = +1
        }
        // Izquierda: 00→10→11→01
        else if ((_lastState == 0b00 && currentState == 0b10) ||
                 (_lastState == 0b10 && currentState == 0b11) ||
                 (_lastState == 0b11 && currentState == 0b01) ||
                 (_lastState == 0b01 && currentState == 0b00)) {
            _count--;  // Izquierda = -1
        }
        
        _lastState = currentState;
    }
}
```

**Ejecución:** <5µs (sin bloqueos)

### 2.2 Inicialización ISR

```cpp
// Encoder::begin()
void Encoder::begin() {
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);
    
    _lastState = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
    
    // Attachar ISR en ambos pines (CHANGE = cualquier flanco)
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), onEncoderChange, CHANGE);
}
```

---

## 3. APIs Y CONTROL

### 3.1 Funciones Públicas

```cpp
class Encoder {
public:
    static void begin();
    static void update();              // Llama en loop() para no-ISR tasks
    static bool hasChanged();
    static int getCount();             // Contador acumulado
    static void reset();               // Limpiar contador
    static bool isPushed();            // Botón presionado
    
    // Display
    static int currentVPotLevel;       // -7..+7 (mapeado a display)
};
```

### 3.2 Rango VPot

```
getCount() → currentVPotLevel

getCount() / 4 = nivel VPot
  -28..(-8) → -7 (limit)
  -8..0     → -7..0
  0..+8     → 0..+2
  +28..     → +7 (limit)

Rango final: -7 a +7 (15 posiciones + centro)
```

---

## 4. SEQUENCIAMIENTO EN MAIN LOOP

### 4.1 Flujo Correcto (2026-04-28)

```cpp
// main.cpp loop()

// 1. RS485 first - captura delta para enviar
if (rs485.hasNewData()) {
    SlavePacket resp = RS485Handler::buildResponse(...);
    // buildResponse() lee Encoder::getCount() aquí
    rs485.sendResponse(resp);
    
    // NO resetear encoder aquí (BUG anterior)
}

// 2. Procesar VPot con delta actualizado
if (!satMenu->isEncoderConsumed()) {
    Encoder::update();
    
    if (Encoder::hasChanged()) {
        int newLevel = constrain((int)(Encoder::getCount() / 4), -7, 7);
        
        if (newLevel != Encoder::currentVPotLevel) {
            Encoder::currentVPotLevel = newLevel;
            needsVPotRedraw = true;
        }
    }
}

// 3. RESET al final (post-VPot, pre-display)
Encoder::reset();

// 4. Actualizar display
updateDisplay();
```

**Critical:** `Encoder::reset()` debe ser **post-VPot, pre-display** (línea 242 en main.cpp)

### 4.2 Bug Anterior (RESUELTO 2026-04-28)

**Problema:**
- `Encoder::reset()` estaba post-RS485, pre-VPot processing
- RS485 capturaba delta, se enviaba al master
- Luego se reseteaba contador → getCount() = 0 al procesar VPot
- VPot nunca cambiaba en Logic

**Síntoma:**
- REC/SOLO/MUTE/SELECT funcionaban vía RS485
- VPot ring mudo (no respondía a encoder en Logic)
- SAT funcionaba (no tenía reset intermedio)

**Fix:**
- Mover `Encoder::reset()` a post-VPot processing
- Ahora RS485 y Display usan mismo delta ✅

---

## 5. INTEGRACIÓN SAT

### 5.1 SAT Encoder Push

```cpp
// SatMenu::update() - detecta long press encoder button
if (Encoder::isPushed() && millis() - pushTime > 3000) {
    openMenu();  // Abre SAT menu
}
```

**No resetea contador:** SAT sigue siendo autoridad del display, encoder sigue acumulando

### 5.2 SAT Consuma Encoder

```cpp
// Cuando SAT abierto
if (satMenu->isOpen()) {
    // SatMenu controla encoder (navegación menú)
    satMenu->handleEncoder();  // Consume getCount()
    
    // main.cpp NOT procesa VPot
    // (detectado por isEncoderConsumed())
}
```

**Critical:** Cuando SAT abierto:
- SAT lee encoder (navegación)
- main.cpp **NO** actualiza VPot
- Reset ocurre en SAT::close() o sync

---

## 6. BOTÓN ENCODER (PUSH)

### 6.1 Detección Push

```cpp
// Encoder::update() - en loop
void Encoder::update() {
    static bool lastPushState = HIGH;
    bool currentPushState = digitalRead(ENCODER_SW_PIN);
    
    // Debounce 5ms
    if (currentPushState != lastPushState) {
        delayMicroseconds(5000);  // 5ms
        
        if (digitalRead(ENCODER_SW_PIN) != lastPushState) {
            lastPushState = currentPushState;
            
            if (currentPushState == LOW) {
                _isPushed = true;  // Release (flanco ascendente)
            }
        }
    }
}
```

### 6.2 SAT Open Trigger

```
Condición: Encoder::isPushed() && sostenido >3s
     ↓
SAT::open()
     ↓
Display menú SAT
     ↓
Usuario navega encoder
     ↓
Usuario presiona encoder nuevamente
     ↓
SAT::close()
     ↓
Encoder::reset() en sync
     ↓
Vuelve a display normal
```

---

## 7. TROUBLESHOOTING

### 7.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Encoder no responde | ISR no attachada o GPIO no INPUT | Check `attachInterrupt()` en begin() |
| Dirección invertida | Gray code logic invertida | Test manual: girar derecha, debe contar +1 |
| Contador salta | Ruido en contactos | Verificar cable encoder (twisted pair) |
| SAT no abre | Push button no funciona | Test GPIO11 con Serial.println() |
| VPot sigue viejo | reset() en lugar equivocado | Verificar orden main.cpp líneas 240-246 |
| Lag en SAT navegación | Encoder procesado en main.cpp | Verificar isEncoderConsumed() en main |

### 7.2 Debugging

**Log esperado — rotación derecha:**
```
[ENCODER] ISR triggered (A change)
[ENCODER] State: 0b00 → 0b01 (derecha)
[ENCODER] count++ (now: 1)
[ENCODER] ISR triggered (B change)
[ENCODER] State: 0b01 → 0b11 (derecha)
[ENCODER] count++ (now: 2)
```

**Test manual:**
```cpp
// En loop() durante boot
Serial.printf("[DEBUG] Encoder count: %d, level: %d\n", 
              Encoder::getCount(), Encoder::currentVPotLevel);
```

---

## 8. HISTORIA BUGS

### 8.1 Sequenciamiento Encoder (RESUELTO 2026-04-28)

**Bug:** `Encoder::reset()` post-RS485, pre-VPot → VPot ring mudo

**Fix:** Mover reset() post-VPot, pre-display

**Commit:** Historial CHANGELOG.md

**Status:** ✅ Resuelto

---

## 9. REFERENCIAS

- **BUTTONS.md** — SAT menu abierto con push >3s
- **DISPLAY.md** — VPot ring redibujado con needsVPotRedraw
- **RS485.md** — encoderDelta en SlavePacket
- **MOTOR.md** — Encoder no interfiere con calibración motor

---

## Últimas Actualizaciones

- **(2026-08-13)** Fix signo invertido en `ENC_TABLE`/`_counter` (`Encoder.cpp:24`) — derecha ahora es +1, coincide con §1.3. Fix runaway y doble-conteo en `SatMenu::_readBtn()`. Fix `encoderDelta` enviado ×4 de más en `RS485Handler.cpp:343`. Detalle: `CHANGELOG.md` sesión 2026-08-13.
- **(2026-05-16)** Creado ENCODER.md como documento exhaustivo, trasladado contenido de CLAUDE.md
