# Transport — Controles y Feedback (iMakie S3 Extender)

Documentación exhaustiva del subsistema de transporte. Incluye botones físicos (RW/FF/STOP/PLAY/REC), LEDs de feedback, mapeo MIDI, comportamiento por estado de conexión, y handshake Mackie MCU.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-27  
**Estado:** En producción (5 botones, 5 LEDs, MIDI feedback)

---

## 1. HARDWARE TRANSPORT

### 1.1 Pinout Transport (S3)

| Función | Nota MIDI | GPIO BTN | GPIO LED | Lógica LED | Vel MIDI |
|---------|-----------|----------|----------|------------|----------|
| **RW** (Rewind) | 0x5B (91) | BTN_RW | LED_RW | Ánodo común 5V, sink — LOW=ON | 127 on / 0 off |
| **FF** (Fast Forward) | 0x5C (92) | BTN_FF | LED_FF | Ánodo común 5V, sink — LOW=ON | 127 on / 0 off |
| **STOP** | 0x5D (93) | BTN_STOP | LED_STOP | Ánodo común 5V, sink — LOW=ON | 127 on / 0 off |
| **PLAY** | 0x5E (94) | BTN_PLAY | LED_PLAY | Ánodo común 5V, sink — LOW=ON | 127 on / 0 off |
| **REC** (Record) | 0x5F (95) | BTN_REC | LED_REC | Ánodo común 5V, sink — LOW=ON | 127 on / 0 off |

> Pines GPIO definidos en `config.h` S3. Lógica invertida: `LOW = encendido`, `HIGH = apagado`.

**Hardware:**
- **Botones:** Switches mecánicos, gestión con Button2 library
- **LEDs:** PWM 8-bit vía `analogWrite()` — brillo ajustable, ver §1.2 (2026-08-16)
- **Polaridad:** Ánodo común a 5V → sink a GND para encender

### 1.2 Brillo PWM (2026-08-16 22:05)

Los 5 LEDs pasaron de `digitalWrite` on/off a `analogWrite` con brillo configurable:

```cpp
// config.h
#define TRANSPORT_LED_BRIGHTNESS 50   // 0-255: 0=apagado, 255=brillo máximo

// Transporte.cpp
void setLed(uint8_t pin, bool on) {
    // PWM invertido: ánodo común 5V, sink por GPIO — más tiempo en LOW = más brillo.
    // analogWrite(valor) = % tiempo en HIGH, por eso 255-brillo
    analogWrite(pin, on ? (255 - TRANSPORT_LED_BRIGHTNESS) : 255);
}
```

**Por qué invertido:** con ánodo común, el GPIO enciende el LED en `LOW` (sink). `analogWrite(pin, valor)` en el core Arduino-ESP32 interpreta `valor` como % de tiempo en `HIGH` — así que para que el brillo suba con `TRANSPORT_LED_BRIGHTNESS` hay que restar a 255, no pasar el valor directo.

Sin conflicto de canales LEDC en S3: no hay display/backlight ni otro uso previo de PWM (el NeoPixel de estado usa RMT, no LEDC).

---

## 2. MAPEO MIDI TRANSPORT

### 2.1 Notas MIDI (Mackie MCU familia 0x14)

```cpp
// MCU_TRANSPORT_NOTES[] en Transporte.cpp
0x5F  // REC   (95)
0x5E  // PLAY  (94)
0x5C  // FF    (92)
0x5D  // STOP  (93)
0x5B  // RW    (91)
```

**Canal:** MIDI 1 (omnidireccional en Mackie MCU)

**Velocidad:**
- **127** = encendido (Logic confirma estado activo, LED on)
- **0** = apagado (Logic confirma estado inactivo, LED off)

### 2.2 PLAY y STOP — combo, sin nota 93 (2026-08-16 22:25, tras dos intentos fallidos)

**Intento 1 (22:15, revertido):** STOP separado en `case 93` propio, asumiendo que Logic la manda de forma fiable — no es así, STOP dejó de apagarse nunca.

**Intento 2 (22:18, revertido):** se restauró el combo (`case 94`) y se mantuvo `case 93` como vía secundaria — resultado: STOP dejaba de **encenderse** al pulsarlo. Sospecha: Logic manda un Note Off de la nota 93 como flash momentáneo del botón físico que pisa el `true` que el combo ya había puesto correctamente.

**Diseño final:** `case 93` eliminado por completo — no hay evidencia fiable (sin captura de MIDI Monitor) de cómo Logic usa esa nota, y en dos intentos distintos causó una regresión distinta cada vez. STOP depende **únicamente** del combo con PLAY (mecanismo original, validado antes de tocar nada) + el apagado explícito al activar FF/RW:
```cpp
case 94:  // PLAY/STOP combo — relación inversa, la única vía validada
    setLed(LED_PLAY, on);
    setLed(LED_STOP, !on);
    break;
case 92:  // FAST FWD
    setLed(LED_FF, on);
    if (on) setLed(LED_STOP, false);   // Logic no apaga STOP al hacer FF por su cuenta
    break;
case 91:  // REWIND
    setLed(LED_RW, on);
    if (on) setLed(LED_STOP, false);   // mismo motivo que FF
    break;
```
**Confirmado con captura real de MIDI Monitor (2026-08-16 22:28):** Logic **nunca** manda feedback de la nota 93 al pulsar el botón físico STOP del S3 — en toda una sesión de prueba con RW/FF/STOP pulsados varias veces, `A5` (nota 93) como mensaje `To iMakie-Extender` solo apareció una vez, dentro de la ráfaga de reset de conexión (§3.3 en `MIDI.md`), nunca como respuesta a una pulsación real. Sí se confirmó que Logic **responde con eco fiable a FF y RW** (`G♯5`→92 y `G5`→91 respectivamente), así que el problema es específico de la nota 93, no de la ruta de recepción MIDI en general.

**Fix final (2026-08-16 22:30) — STOP encendido localmente:** ya que no hay ninguna señal fiable de Logic para "transporte parado", `onButtonPressed()` (`Transporte.cpp`) enciende `LED_STOP` de forma **local e inmediata** al pulsar el botón físico STOP, sin esperar feedback:
```cpp
static void onButtonPressed(Button2& b) {
    for (uint8_t i = 0; i < N; i++) {
        if (&buttons[i] == &b) {
            sendNoteOn(MCU_TRANSPORT_NOTES[i]);
            if (LEDS[i] == LED_STOP) setLed(LED_STOP, true);
            return;
        }
    }
}
```
PLAY/FF/RW siguen apagando STOP como antes (combo `case 94` + `case 91`/`92`), y REC/PLAY/FF/RW se siguen rigiendo por feedback real de Logic (confirmado fiable para esas 4 notas).

---

## 3. COMPORTAMIENTO POR ESTADO DE CONEXIÓN (2026-05-27)

### 3.1 Tabla de estados

| Estado Logic | LEDs Transport | Botones | Descripción |
|-------------|----------------|---------|-------------|
| **DISCONNECTED** (boot) | Todos apagados | No envían MIDI | S3 no tiene Logic activo |
| **GoOnline en curso** | Apagados | Funcionales | Handshake SysEx en progreso |
| **CONNECTED** | Controlados por Logic | Envían MIDI | Logic dicta estado de cada LED |
| **GoOffline (`0x0F`)** | Todos apagados | No envían MIDI efectivo | `setAllLedsOff()` inmediato |
| **Disconnect PitchBend** | Todos apagados | No envían MIDI efectivo | `setAllLedsOff()` inmediato |

### 3.2 Regla de comportamiento

> Los LEDs de transporte **solo se encienden si Logic los enciende** (vía nota MIDI).  
> Al desconectar por cualquier vía, `setAllLedsOff()` apaga todos instantáneamente.  
> No hay estado "tenue siempre encendido" — es on/off según Logic.

### 3.3 Implementación — `setAllLedsOff()`

```cpp
// Transporte.cpp
void setAllLedsOff() {
    for (uint8_t i = 0; i < N; i++)
        setLed(LEDS[i], false);
}
```

Llamado desde `MIDIProcessor.cpp` en dos puntos:
1. `case 0x0F:` — GoOffline recibido de Logic
2. Bloque disconnect por detección de 9 faders a 0 en `processPitchBend()`

---

## 4. HANDSHAKE MACKIE MCU (FAMILIA 0x14)

### 4.1 Secuencia GoOnline completa

```
Logic → S3:  F0 00 00 66 <any> 00 F7          (sondeo — cualquier familia)
S3 → Logic:  F0 00 00 66 14 01 00..00 F7       (responde familia 0x14)

Logic → S3:  F0 00 00 66 14 13 00 F7           (solicita versión firmware)
S3 → Logic:  F0 00 00 66 14 14 00 F7           (responde versión)

Logic → S3:  F0 00 00 66 14 21 F7              (GoOnline — solicita conexión)
S3 → Logic:  F0 00 00 66 14 21 01 F7           (confirma CONNECTED)
             → g_logicConnected = 1
             → logicConnectionState = CONNECTED

Logic → S3:  F0 00 00 66 14 61 F7              (AllFadersToMinimum — inicialización)
             → S3 pone faderTarget=0 en todos los slaves
             → NO cambia g_logicConnected (ver §4.2)

Logic → S3:  F0 00 00 66 14 0C 00 F7           (tipo superficie = Master)
S3 → Logic:  F0 00 00 66 14 0C 00 F7           (confirma)
S3 → Logic:  F0 00 00 66 14 10 00 F7           (suscripción a feedback de notas)

→ Estado final: CONNECTED, LEDs responden a Logic
```

### 4.2 Bug crítico resuelto — SysEx 0x61 (2026-05-27)

**Problema:** El handler de `0x61` (AllFadersToMinimum) contenía `g_logicConnected = 0`. Como Logic envía `0x61` **después** de `0x21` en la secuencia GoOnline, esto anulaba inmediatamente la conexión: los slaves S2 recibían `pkt.connected=0` y sus pantallas quedaban oscuras siempre.

```
ANTES (incorrecto):          DESPUÉS (correcto):
0x21 → g_logicConnected=1   0x21 → g_logicConnected=1
0x61 → g_logicConnected=0   0x61 → faderTarget=0 a todos los slaves
         ↑ BUG                       (g_logicConnected intacto)
S2s: pantallas oscuras       S2s: se activan con Logic
```

**Fix en `MIDIProcessor.cpp` case 0x61:**
```cpp
case 0x61: {
    // AllFadersToMinimum — Logic inicializa faders al conectar (parte de GoOnline)
    // NO cambiar g_logicConnected — S2s deben quedarse CONNECTED (2026-05-27)
    for (uint8_t i = 1; i <= NUM_SLAVES; i++)
        rs485.setFaderTarget(i, 0);
    log_i("[MCU] AllFaderstoMinimum — faders a 0");
    break;
}
```

### 4.3 Secuencia GoOffline

```
Logic → S3:  F0 00 00 66 14 0F F7   (GoOffline)
             → logicConnectionState = DISCONNECTED
             → g_logicConnected = 0
             → Transporte::setAllLedsOff()
             → rs485.beginDisconnectSequence()
               (todos los slaves reciben pkt.connected=0)
             → g_switchToOffline = true

S2 recibe pkt.connected=0:
             → Motor::setConnected(false)
             → Motor::off()
             → setScreenBrightness(0)
             → neoWaitingHandshake = true (LEDs azul)
```

---

## 5. ARQUITECTURA SOFTWARE

### 5.1 Archivos relevantes

| Archivo | Responsabilidad |
|---------|----------------|
| `Transporte.cpp` | LEDs, botones, `setAllLedsOff()`, `setLedByNote()` |
| `Transporte.h` | Declaraciones públicas |
| `MIDIProcessor.cpp` | Recibe notas MIDI, llama `setLedByNote()`, gestiona conexión |

### 5.2 Implementación real

**`Transporte::setLed()`** — PWM invertido, ánodo común (ver §1.2):
```cpp
void setLed(uint8_t pin, bool on) {
    analogWrite(pin, on ? (255 - TRANSPORT_LED_BRIGHTNESS) : 255);
}
```

**`Transporte::setLedByNote()`** — convierte nota MIDI en control LED. PLAY/STOP en combo (relación inversa, validada), RW/FF apagan STOP al activarse. Nota 93 (STOP explícita) deliberadamente NO manejada — ver §2.2, dos intentos con ella causaron regresiones distintas:
```cpp
void setLedByNote(uint8_t note, bool on) {
    switch (note) {
        case 94:  // PLAY/STOP combo
            setLed(LED_PLAY, on);
            setLed(LED_STOP, !on);
            break;
        case 95:  // RECORD
            setLed(LED_REC, on);
            break;
        case 92:  // FAST FWD (nota MCU real 0x5C)
            setLed(LED_FF, on);
            if (on) setLed(LED_STOP, false);
            break;
        case 91:  // REWIND
            setLed(LED_RW, on);
            if (on) setLed(LED_STOP, false);
            break;
    }
}
```

**`Transporte::setAllLedsOff()`** — apaga todos los 5 LEDs (2026-05-27):
```cpp
void setAllLedsOff() {
    for (uint8_t i = 0; i < N; i++)
        setLed(LEDS[i], false);
}
```

**`Transporte::begin()`** — secuencia de test al boot:
```cpp
void begin() {
    for (uint8_t i = 0; i < N; i++) {
        pinMode(LEDS[i], OUTPUT);
        setLed(LEDS[i], false);          // Off por defecto
        buttons[i].setPressedHandler(onButtonPressed);
        buttons[i].setReleasedHandler(onButtonReleased);
    }
    // Test visual: enciende cada LED 150ms
    for (uint8_t i = 0; i < N; i++) {
        setLed(LEDS[i], true);
        delay(150);
        setLed(LEDS[i], false);
    }
    // → Tras begin(), todos los LEDs apagados
}
```

### 5.3 Flujo completo S3 Botón → Logic → LED

```
Usuario presiona BTN (ej. PLAY)
     ↓
Button2::loop() detecta flanco
     ↓
onButtonPressed() → sendNoteOn(0x5E)
     │  byte msg[] = {0x90, 0x5E, 0x7F}
     ↓
Logic Pro recibe Note On 0x5E vel 127
     │  → Inicia reproducción
     │  → Envía feedback: Note On 0x5E vel 127
     ↓
S3 processMidiByte() → processNote()
     ↓
Transporte::setLedByNote(94, true)
     │  setLed(LED_PLAY, true)
     │  setLed(LED_STOP, false)
     ↓
LED PLAY enciende, LED STOP apaga (combo, ver §2.2)

Latencia total típica: < 60ms
```

---

## 6. TROUBLESHOOTING

### 6.1 Síntomas Comunes

| Síntoma | Causa probable | Solución |
|---------|---------------|----------|
| LEDs siempre apagados aunque Logic conectado | Bug 0x61 (pre-2026-05-27): `g_logicConnected=0` | Actualizar firmware S3 (fix commit `01dae66`) |
| LED no enciende al pulsar | GPIO LED abierto o lógica invertida | Verificar `LOW=ON` en hardware real |
| LED lag >100ms | Loop taskCore1 bloqueante | Verificar `vTaskDelay(10)` en taskCore1 |
| Handshake falla (Logic no conecta) | Familia 0x14 no respondida en 0x00 | Verificar `DEVICE_FAMILY = 0x14` en config.h |
| LEDs no se apagan al desconectar | `setAllLedsOff()` no llamado | Verificar en case 0x0F y bloque PitchBend disconnect |
| Botón no envía MIDI | MIDI buffer lleno o task bloqueada | Verificar `tud_midi_stream_read()` en taskCore0 |
| Brillo tenue "fantasma" en LED (típicamente REC) al arrancar/conectar, sin acción del usuario ni de Logic | GPIO en alta impedancia (flotante) entre power-on y el primer `pinMode(OUTPUT)` — fuga vía diodo ESD interno, microamperios (2026-08-16) | Resuelto: `Transporte::initPins()` como primera instrucción de `setup()` (antes de Serial/USB), + brillo PWM bajo (`TRANSPORT_LED_BRIGHTNESS=50`) reduce el contraste. Confirmado en banco por el usuario. |
| Botonera de transporte se nota lenta al pulsar | `taskCore1` (botones) compartía core 1 con el task RS485 (prioridad 5, busy-loop `taskYIELD()`) que lo dejaba sin CPU | Resuelto (2026-08-16): `taskCore1` movido a core 0 (`main.cpp:330`) — core 1 queda dedicado en exclusiva al RS485. Ver CHANGELOG sesión 2026-08-16 22:05. |

### 6.2 Logs de Referencia

**GoOnline exitoso:**
```
[I][MIDIProcessor.cpp:447] processMackieSysEx(): [MCU] 0x21 — CONNECTED
[I][Transporte.cpp:83] setLedByNote(): [TRANSP] PLAY=0 STOP=1
```

**GoOffline:**
```
[I][MIDIProcessor.cpp:346] processMackieSysEx(): [MCU] GoOffline recibido — iniciando DISCONNECT SEQUENCE
[I][RS485.cpp:514] beginDisconnectSequence(): [RS485] DISCONNECT SEQUENCE iniciada para slaves 1..8
[I][main.cpp:184] taskCore0(): [MAIN] Desconexión completada — todos los slaves en DISCONNECTED
```

---

## 7. REFERENCIAS

- **RS485.md** — Protocolo, pkt.connected, calibración cascada
- **MIDI.md** — Protocolo Mackie MCU completo, SysEx, handshake
- **BUTTONS.md** — ButtonManager S2, debounce (no transport pero similar)
- **MASTER_S3-P4/S3/.../src/hardware/Transporte.cpp** — Implementación
- **MASTER_S3-P4/S3/.../src/midi/MIDIProcessor.cpp** — Integración MIDI

---

## Últimas Actualizaciones

- **(2026-08-16 22:30)** §2.2 Confirmado con MIDI Monitor real: Logic nunca manda feedback de la nota 93 al pulsar Stop. Fix final: `onButtonPressed()` enciende LED_STOP localmente al pulsar el botón físico, sin esperar a Logic. Ver CHANGELOG sesión 2026-08-16 22:05, Hallazgo 5 (cerrado)
- **(2026-08-16 22:25)** §2.2/§5.2 `case 93` (STOP explícito) eliminado por completo tras dos intentos fallidos distintos — STOP depende solo del combo con PLAY + apagado explícito por FF/RW. Ver CHANGELOG sesión 2026-08-16 22:05, Hallazgo 5 (actualizado)
- **(2026-08-16 22:10)** §5.2 `setLedByNote()` — LEDs RW/FF corregidos (`case 97`→`92`, nuevo `case 91`), nunca recibían feedback real de Logic. Ver CHANGELOG sesión 2026-08-16 22:05, Hallazgo 4
- **(2026-08-16 22:05)** §1.2 Brillo PWM ajustable en los 5 LEDs (`TRANSPORT_LED_BRIGHTNESS`, invertido por ánodo común) — antes on/off binario
- **(2026-08-16 22:05)** §6.1 Fantasma REC (GPIO flotante en boot) y botonera lenta (contención de core FreeRTOS con RS485) — ambos diagnosticados y resueltos, ver CHANGELOG sesión 2026-08-16 22:05
- **(2026-05-27)** §3 Comportamiento por estado de conexión — tabla completa conectado/desconectado
- **(2026-05-27)** §4.2 Bug 0x61 documentado y fix explicado — causa: S2s siempre oscuros al conectar
- **(2026-05-27)** §4.3 Secuencia GoOffline completa con `setAllLedsOff()`
- **(2026-05-27)** §5.3 Flujo completo botón → Logic → LED con latencias
- **(2026-05-27)** §6 Troubleshooting actualizado con nuevos síntomas
- **(2026-05-16)** Creado Transport.md como documento exhaustivo, extraído de S3 README
