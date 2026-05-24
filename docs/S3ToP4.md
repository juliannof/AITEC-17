# S3→P4: Diferencias MIDI — Guía de sincronización

**Fecha:** 2026-05-24  
**Propósito:** Documentar las diferencias entre el MIDIProcessor de S3 (código actual / referencia) y el de P4 (más antiguo / divergido), para guiar la sincronización futura.  
**Acción requerida:** Solo documentación. Ningún cambio de código en esta sesión.

---

> ⚠️ **ADVERTENCIA PARA SESIONES FUTURAS (2026-05-24)**
>
> **Este documento es una foto fija del estado del código en la fecha arriba indicada.**
>
> En el momento de redactar esto, **S3 sigue en desarrollo activo** — se esperan cambios adicionales en `MIDIProcessor.cpp`, `RS485.cpp` y posiblemente `config.h` antes de que S3 esté estabilizado.
>
> **Antes de usar este documento para portar código a P4:**
> 1. Verificar el git log de S3 desde `2026-05-24` → `git log --oneline MASTER_S3-P4/S3/`
> 2. Releer `MIDIProcessor.cpp` de S3 en su estado actual — puede haber fixes nuevos no reflejados aquí
> 3. Actualizar este documento con las diferencias nuevas antes de tocar P4
>
> **Señales de que este doc está desactualizado:** nuevos commits en S3 tras `2026-05-24`, cambios en `processPitchBend` o `processMackieSysEx`, o bugs nuevos documentados en CHANGELOG que afecten S3 MIDI.

---

---

## Regla de referencia

**S3 = código actual y correcto.** Toda diferencia en P4 debe evaluarse contra S3. Las diferencias se clasifican en:

- 🔴 **Bug en P4** — ausencia de fix que S3 ya tiene
- 🟡 **P4 tiene más** — funcionalidad extra de P4 que S3 no necesita (Display LVGL, Timecode, Transporte diferente)
- 🔵 **Divergencia arquitectónica** — decisión diferente por diseño del MCU, no un error

---

## 1. config.h — Constantes y tipos

| Elemento | S3 | P4 | Clasificación |
|----------|----|----|---------------|
| `DEVICE_FAMILY` testing | `0x14` | `0x14` | ✅ Igual |
| `DEVICE_FAMILY` producción | `0x14` | `0x15` | 🔵 P4 es MCU master principal, familia distinta |
| `NUM_SLAVES` | `1` (testing) / `8` (prod) | `9` hardcoded | 🔵 P4 controla bus A (9 esclavos) |
| `LOGIC_PITCHBEND_MAX` | `14845` — rango real Logic | **Ausente** — usa `16383.0f` | 🔴 Bug: P4 normaliza con rango MIDI estándar, no el real de Logic |
| `ConnectionState` enum | `DISCONNECTED, HANDSHAKE, MIDI_HANDSHAKE_COMPLETE, CONNECTED` | Igual | ✅ Igual |
| `DisplayMode` | `enum class DisplayMode { BEATS, SMPTE }` | `enum DisplayMode { MODE_BEATS, MODE_SMPTE }` | 🟡 P4 usa enum plano + macros (compatible con LVGL) |

---

## 2. MIDIProcessor.h

| Elemento | S3 | P4 | Clasificación |
|----------|----|----|---------------|
| `#include "../config.h"` | Sí | **No** | 🔴 P4 no incluye config.h en el header — tipos como `ConnectionState` dependen de que main.cpp los incluya antes |
| Extern redraw flags (`needsTOTALRedraw`, `needsMainAreaRedraw`, etc.) | **No declarados aquí** — viven en main.cpp | **Declarados en MIDIProcessor.h** | 🟡 P4 necesita estos flags para el display LVGL |
| Extern estado MIDI (`logicConnectionState`, `trackNames[]`, etc.) | Declarados en MIDIProcessor.h | **No declarados** en MIDIProcessor.h | 🔵 Diferencia de encapsulación — funcional en ambos |

---

## 3. namespace anónimo — Variables internas

| Variable | S3 | P4 | Clasificación |
|----------|----|----|---------------|
| `PITCHBEND_DEADBAND = 80` | ✅ Presente | **Ausente** | 🔴 Bug: P4 envía `setFaderTarget` en cada PitchBend sin filtro — tráfico RS485 excesivo |
| `lastSentPitchBend[9]` | ✅ Presente | **Ausente** | 🔴 Bug: P4 no puede implementar deadband sin este array |

---

## 4. `processPitchBend` — Fader mapping

Esta función tiene las diferencias más críticas.

### 4.1 Clamping del valor

```cpp
// S3 (correcto):
int bendClamped = (bendValue < 0) ? 0 : bendValue;
rs485.setFaderTarget(channel + 1, (uint16_t)bendClamped);

// P4 (incorrecto):
uint16_t fader14bit = (uint16_t)bendValue;
rs485.setFaderTarget(channel + 1, fader14bit);
```

**Clasificación:** 🔴 Bug en P4. `bendValue` puede ser negativo (Logic envía signed). El cast a `uint16_t` sin clamp produce wrapping (ej: `-1` → `65535`).

### 4.2 Deadband antes de enviar RS485

```cpp
// S3 (correcto):
if (abs(bendClamped - (int)lastSentPitchBend[channel]) > PITCHBEND_DEADBAND) {
    rs485.setFaderTarget(channel + 1, (uint16_t)bendClamped);
    lastSentPitchBend[channel] = (int16_t)bendClamped;
}

// P4 (incorrecto):
// Sin deadband — setFaderTarget en cada PitchBend recibido
if (channel < 8) rs485.setFaderTarget(channel + 1, fader14bit);
```

**Clasificación:** 🔴 Bug en P4. Sin deadband, cada movimiento de fader genera cientos de setFaderTarget/s innecesarios.

### 4.3 Normalización para display

```cpp
// S3 (correcto):
float faderPositionNormalized = (float)bendClamped / (float)LOGIC_PITCHBEND_MAX;  // /14845

// P4 (incorrecto):
float faderPositionNormalized = (float)fader14bit / 16383.0f;  // rango MIDI estándar
```

**Clasificación:** 🔴 Bug en P4. Logic usa rango `0–14845`, no el estándar MIDI de `0–16383`. La barra de fader en display P4 nunca llega al 100%.

### 4.4 Log de desconexión

```cpp
// S3:
unsigned long elapsed = now - firstFaderMinTime;
log_d("[DISCONNECT] %d faders en 0 en %lums.", bitsSet, elapsed);

// P4:
// Sin log de desconexión, sin variable elapsed
```

**Clasificación:** 🟡 S3 tiene más logging — útil para debugging pero no funcional.

### 4.5 Transición `MIDI_HANDSHAKE_COMPLETE → CONNECTED`

```cpp
// S3 (presente):
if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
    logicConnectionState = ConnectionState::CONNECTED;
    g_logicConnected     = 1;
    connectedSinceTime   = millis();
    fadersAtMinMask      = 0;
    for (uint8_t i = 0; i < 8; i++) { /* reset selectStates + NoteOff */ }
    // _calibPendingFrom = 1;  // ELIMINADO en S3 (boot auto-calib)
    g_switchToPage3 = true;
}

// P4 (ausente):
// P4 no tiene este bloque — la transición ocurre en case 0x21 directamente
```

**Clasificación:** 🔵 Divergencia arquitectónica. En S3, el primer PitchBend confirma la conexión completa. En P4, el 0x21 ya hace todo. Ambos son válidos para sus contextos.

---

## 5. SysEx `0x0F` — GoOffline

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Reset estado básico | ✅ | ✅ |
| `rs485.beginDisconnectSequence()` | ✅ Presente | **Ausente** — el método no existe en RS485 de P4 |
| `memset` de todos los arrays de estado | **No** — solo flags mínimos | ✅ Presente (reset completo) |
| `rs485.setFlags(i, 0)` para todos los slaves | **No** | ✅ Presente |
| Reset `trackNames[]` | **No** | ✅ Presente |
| `g_switchToOffline = true` | ✅ | ✅ |

**Clasificación:**
- `beginDisconnectSequence()` — 🔵 S3 tiene lógica de desconexión gradual (notifica a slaves antes de cambiar UI). P4 necesita algo equivalente o el reset inmediato es suficiente.
- Reset completo de arrays — 🟡 P4 hace limpieza más completa al GoOffline. S3 es minimalista. Ambos son válidos.

---

## 6. SysEx `0x21` — GoOnline (HostConnected)

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Echo inmediato `0x21 0x01` | ✅ | ✅ |
| `fadersAtMinMask = 0` | **No** | ✅ |
| Reset `selectStates` + NoteOff | **No** (lo hace en processPitchBend) | ✅ |
| `needsTOTALRedraw = true` | **No** | ✅ |
| Dispara calibración (`_calibPendingFrom = 1`) | **No** — comentado (S3 calibra en boot) | ✅ Activo |
| `g_switchToPage3 = true` | **No** | ✅ |

**Clasificación:** 🔵 Divergencia arquitectónica esperada. S3 tiene calibración boot independiente de Logic (requisito de diseño). P4 dispara calibración al conectar Logic — pendiente revisar si esto sigue siendo correcto dado el esquema de calibración actualizado en S3.

**⚠️ Atención:** P4 calibra al conectar Logic. Si se porta el esquema de calibración cascada de S3 a P4, esta línea debería comentarse también.

---

## 7. SysEx `0x12` — LCD Write (Track Names)

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Guard contra row 1 vacía (`nameBufs[t][0] == '\0'`) | ✅ Fix B2 aplicado | **Ausente** |
| `needsMainAreaRedraw = true` | **No** | ✅ |
| `needsButtonsRedraw = true` | **No** | ✅ |

**Clasificación:**
- Guard row vacía — 🔴 **Bug B2 latente en P4.** En modo plugin/Atmos, Logic envía row 1 vacía → P4 borra los nombres de pista en todos los slaves. Fix está en S3 (`if (nameBufs[t][0] == '\0') continue;`).
- Redraw flags — 🟡 P4 necesita forzar redraw de display LVGL. S3 no tiene display propio.

---

## 8. SysEx `0x11` — Assignment Display

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Actualiza `assignmentString` | ✅ | ✅ |
| `needsHeaderRedraw = true` | **No** | ✅ |

**Clasificación:** 🟡 P4 actualiza display. S3 no tiene display.

---

## 9. SysEx `0x72` — VU Meter batch

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Procesa 8 canales de VU | ✅ | ✅ |
| `rs485.setVuLevel()` | ✅ | ✅ |
| `needsVUMetersRedraw = true` | **No** | ✅ |

**Clasificación:** 🟡 P4 actualiza display.

---

## 10. SysEx `0x0E` — Channel Auto Mode

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Actualiza `g_channelAutoMode[]` | ✅ | ✅ |
| `needsButtonsRedraw = true` | **No** | ✅ |

**Clasificación:** 🟡 P4 actualiza display.

---

## 11. `processNote` — Notas MIDI

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Notas 0–31 (REC/SOLO/MUTE/SELECT) | ✅ | ✅ |
| Notas 74–79 (Auto Mode) | ✅ | ✅ |
| Nota 113 → `MODE_SMPTE` | **No** | ✅ |
| Nota 114 → `MODE_BEATS` | **No** | ✅ |
| `Transporte::setLedByNote(note, is_on)` | ✅ | **No** |
| `needsMainAreaRedraw`, `needsButtonsRedraw` | **No** | ✅ |

**Clasificación:**
- Notas 113/114 — 🟡 P4 cambia modo timecode en display. S3 no tiene display.
- `Transporte::setLedByNote` — 🔵 S3 controla LEDs físicos de transporte. P4 tiene sus propios LEDs via NeoTrellis (pendiente implementar).

---

## 12. `processControlChange` — CC (VPot + Timecode)

| Comportamiento | S3 | P4 |
|---------------|----|----|
| CC 48–55: `rs485.setVPotValue()` | ✅ | ✅ |
| CC 48–55: `needsButtonsRedraw` | **No** | ✅ |
| CC 64–73: Actualiza `beatsChars_clean[]` / `timeCodeChars_clean[]` | **No** | ✅ |
| CC 64–73: `needsHeaderRedraw`, `needsTimecodeRedraw` | **No** | ✅ |

**Clasificación:** 🟡 P4 tiene display con timecode. S3 no necesita estas actualizaciones.

---

## 13. `processChannelPressure` — VU por canal

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Procesamiento canal 0 (MCU format) | ✅ | ✅ |
| Procesamiento canales 1–7 | ✅ | ✅ |
| `rs485.setVuLevel()` | ✅ | ✅ |
| `needsVUMetersRedraw = true` | **No** | ✅ |
| `log_v` inicial | **No** | ✅ |

**Clasificación:** 🟡 P4 actualiza display. Logging extra no es funcional.

---

## 14. `checkMidiTimeout`

| Comportamiento | S3 | P4 |
|---------------|----|----|
| Detecta timeout MIDI | ✅ | ✅ |
| `g_switchToOffline = true` | ✅ | ✅ |
| `needsTOTALRedraw = true` | **No** | ✅ |

**Clasificación:** 🟡 P4 actualiza display.

---

## 15. UIHeader — Bugs en display timecode SMPTE/BEATS (2026-05-24)

> Componente exclusivo de P4. No hay referencia en S3.

### 15.1 Ghost text distinto entre `create` y `update` — desplazamiento visual

En `uiHeaderCreate()` (línea 74-75), el texto inicial del ghost para BEATS es:
```cpp
"0. 0. 0. 000"   // 13 chars
```

En `uiHeaderUpdate()` (línea 118), el mismo ghost se sobreescribe con:
```cpp
"0. 0. 0.  000"  // 14 chars (espacio extra entre el tercer punto y los ceros)
```

**Efecto:** En el primer `uiHeaderUpdate()`, el label `s_tc_ghost` cambia de ancho. La posición y el pivot de rotación (90°) se calcularon en `uiHeaderCreate()` con el ancho de 13 chars. Al cambiar el texto a 14 chars, el label crece pero el pivot ya no coincide con el centro real — el texto queda visualmente desplazado o mal alineado en pantalla.

**Clasificación:** 🔴 Bug visual. Afecta solo modo BEATS al recibir el primer dato de timecode.

**Archivo:** `MASTER_S3-P4/P4/src/display/UIHeader.cpp` líneas 74 y 118.

---

### 15.2 Botón táctil BEAT/SMPT no envía MIDI a Logic

El callback del botón BEAT/SMPT (línea 63-71) hace:
```cpp
currentTimecodeMode = toggle;
lv_label_set_text(lbl, "BEAT" / "SMPT");
needsTimecodeRedraw = true;
// ← FALTA: sendMIDIBytes con Note 0x59 (Mackie SMPTE/BEATS toggle)
```

**Efecto:** La UI cambia el modo local de display, pero Logic **no sabe** que el usuario ha cambiado el modo. Logic continúa enviando CC 64-73 en su formato actual (BEATS o SMPTE según su propio estado). Poco después, cuando Logic envía Note 113 o Note 114, el callback de `processNote()` sobreescribe `currentTimecodeMode` y revierte el toggle del usuario. Además, mientras la UI muestra un modo y Logic envía datos del otro, `formatTimecodeString()` o `formatBeatString()` interpretan los datos con el separador equivocado (`:` vs `.`), produciendo texto garbled o incorrecto.

**Fix de referencia (Mackie MCU):** El botón SMPTE/BEATS de la superficie envía Note 0x59 (89) velocity 127 a Logic. Logic conmuta su propio display y responde con Note 113 o 114 para confirmar el nuevo modo.

**Clasificación:** 🔴 Bug funcional. El toggle es inoperativo — Logic siempre manda su modo a los ~100ms.

**Archivo:** `MASTER_S3-P4/P4/src/display/UIHeader.cpp` línea 63 (lambda del evento click).

---

### 15.3 Arrays `beatsChars_clean` y `timeCodeChars_clean` siempre idénticos

En `processControlChange()` (líneas 215-216 de `MIDIProcessor.cpp`):
```cpp
beatsChars_clean[digit_index]    = char_to_store;
timeCodeChars_clean[digit_index] = char_to_store;  // mismo dato
```

Logic envía **un único flujo** de CC 64-73, cuyo contenido ya refleja el modo activo (BEATS o SMPTE) según su estado interno. El controlador recibe ese flujo y lo escribe en ambos arrays simultáneamente, por lo que son siempre copias exactas. La intención de tener dos buffers separados (uno por modo) nunca se materializa.

**Consecuencia directa del Bug 15.2:** Cuando el usuario hace toggle manual y Logic no lo sabe, ambos arrays contienen datos del modo Logic (no del modo del display), y el formatter aplica los separadores incorrectos. El resultado es texto con `:` donde deberían ir `.` o viceversa.

**Clasificación:** 🟡 Diseño incompleto — no es un bug aislado, es la causa raíz de por qué el Bug 15.2 produce texto incorrecto. Si se arregla 15.2 (enviar Note 0x59), Logic sincroniza el modo y ambos arrays siempre tendrán los datos correctos para el modo activo.

**Archivo:** `MASTER_S3-P4/P4/src/midi/MIDIProcessor.cpp` líneas 215-216 + `UIHeader.cpp` línea 110.

---

### 15.4 Guard `hasDigit` comprueba el array incorrecto en modo BEATS

En `uiHeaderUpdate()` (línea 110):
```cpp
if (!hasDigit(timeCodeChars_clean)) return;  // siempre comprueba array SMPTE
```

En modo `MODE_BEATS`, el display usa `beatsChars_clean`. La guard comprueba `timeCodeChars_clean` independientemente del modo actual. Como ambos arrays son idénticos (Bug 15.3), no produce fallo en la práctica — pero si en el futuro se separan los arrays (para tener caché por modo), esta guard bloquearía el modo BEATS.

**Clasificación:** 🟡 Inconsistencia latente. No produce bug hoy, pero rompe si se corrige el diseño de arrays separados.

**Archivo:** `MASTER_S3-P4/P4/src/display/UIHeader.cpp` línea 110.

---

## 16. UIHeader — Touch no funciona en la columna del header (2026-05-24)

> Bug de interacción LVGL confirmado en hardware. Touch funciona en toda la pantalla excepto en la columna del header (`x: 410–480`).

### Layout del header

```
Pantalla física: 480×800 px (portrait)
Header: x=410, y=0, ancho=70px, alto=800px (columna derecha)
```

Objetos creados en `uiHeaderCreate()`, todos hijos de `parent`, en orden:

| Objeto | Posición (pre-rotación) | Tamaño | Rotación | Clickable por defecto (LVGL v9) |
|--------|------------------------|--------|----------|---------------------------------|
| `s_strip` | (410, 0) | 70×800 | No | **Sí** ← problema |
| `s_mode_lbl` | (415, 10) | 60×30 | 90° CW | Sí (explícito) |
| `s_tc_ghost` | calculado | variable | 90° CW | No |
| `s_timecode` | calculado | variable | 90° CW | No |

---

### Causa raíz — `s_strip` absorbe todos los toques del header

En LVGL v9, `lv_obj_create()` activa `LV_OBJ_FLAG_CLICKABLE` por defecto. El código elimina `LV_OBJ_FLAG_SCROLLABLE` pero **no elimina `LV_OBJ_FLAG_CLICKABLE`**:

```cpp
s_strip = lv_obj_create(parent);
// ...
lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_SCROLLABLE);
// ← FALTA: lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_CLICKABLE);
```

**Efecto:** `s_strip` cubre la columna completa (`x: 410–480, y: 0–800`). Cualquier toque dentro de esa área que NO caiga exactamente en el hit-test de `s_mode_lbl` (`y: 10–40`) es absorbido por `s_strip`. Como `s_strip` no tiene event handler, el toque se consume sin efecto visible.

**Por qué `s_mode_lbl` no salva la situación:** En LVGL, el hit-test de un objeto usa su bounding box **pre-transformación**, no su posición visual después de `transform_rotation`. Después de rotar `s_mode_lbl` 90° CW con pivot en su centro `(445, 25)`, el botón visualmente aparece en el centro de la columna del header (aprox. `x: 430–460, y: 0–55`). Su área de hit-test real sigue siendo el rectángulo original `(415, 10) → (475, 40)`. Hay solapamiento parcial con la posición visual, pero:

- Toques en `y: 0–10` o `y: 40–800` de la columna → `s_strip` los captura (sin handler)
- Toques en `y: 10–40` de la columna → hit-test de `s_mode_lbl` coincide parcialmente; sin embargo, `s_strip` tiene Z-order MENOR (fue creado antes), así que `s_mode_lbl` debería ganar en esa franja concreta

**Resultado neto:** El botón BEAT/SMPT solo responde si el usuario toca exactamente en `y: 10–40` de la columna derecha (franja muy estrecha de 30px en 800px de alto). Fuera de esa franja, `s_strip` absorbe el toque. El usuario percibe que el header entero no responde al táctil.

---

### Causa secundaria — Hit-test no sigue `transform_rotation` en LVGL v9

LVGL v9 calcula el hit-test a partir del bounding box sin transformar. Objetos rotados con `lv_obj_set_style_transform_rotation()` tienen su área táctil en la posición pre-rotación, no en su posición visual. Esto afecta a `s_mode_lbl`, `s_tc_ghost` y `s_timecode`.

**Implicación para el fix:** Eliminar `LV_OBJ_FLAG_CLICKABLE` de `s_strip` es condición necesaria pero no suficiente. Si se quiere que el botón BEAT/SMPT sea táctil en su posición visual (centro del header), el hit-test de `s_mode_lbl` debe redefinirse — o bien usando `lv_obj_set_ext_click_area()`, o bien replanteando el enfoque de rotación (usar `lv_obj_set_style_transform_rotation` en el padre versus rotar el contenido via LVGL `transform_pivot`).

---

### Comportamiento observado en hardware (2026-05-24)

- Botón BEAT/SMPT aparece visualmente cerca del centro superior del header (en landscape)
- El botón responde táctil solo si se toca "muy arriba/izquierda" — exactamente en la posición pre-rotación (`portrait y=10..40` = `landscape top-left`)
- El resto de la columna del header no responde en absoluto
- El resto de la pantalla (páginas 1, 2, 3, 3B) funciona correctamente

Esto confirma: (a) `s_strip` absorbe toques fuera de la franja y=10..40, (b) la hit-test pre-rotación de `s_mode_lbl` está en la esquina superior-izquierda del landscape, no en su posición visual.

---

### ⚠️ Nota de diseño — Coordenadas portrait en código, landscape en hardware

**El código LVGL trabaja siempre en portrait (480×800).** La rotación a landscape la hace el driver MIPI-DSI a nivel de hardware — LVGL no sabe nada de ello. Esto es correcto e intencional.

Para orientar elementos en la vista landscape, el código usa `transform_rotation=900` en LVGL: los elementos se posicionan en portrait y se rotan visualmente 90° para que el usuario los vea correctamente en landscape.

**El problema:** `lv_obj_set_style_transform_rotation()` en LVGL v9 rota la representación visual pero **NO rota el área de hit-test**. El hit-test sigue siendo el bounding box en coordenadas portrait pre-rotación.

Correspondencia portrait → landscape cuando el driver rota 90° CW:
```
Portrait x=0   → borde izquierdo landscape
Portrait x=480 → borde derecho landscape
Portrait y=0   → borde superior landscape  (← aquí va el header visual)
Portrait y=800 → borde inferior landscape
```

- Header en portrait: columna derecha `x=410..480`
- En landscape: esto aparece como la franja derecha vertical
- `s_mode_lbl` posición portrait `(415, 10) → (475, 40)` → en landscape queda arriba-izquierda
- El usuario toca donde VE el botón rotado, no donde está su bounding box portrait

**Regla para futuro trabajo en UIHeader:**
> Al colocar objetos táctiles en el header, la posición portrait del bounding box debe coincidir con la zona que el usuario tocará en landscape. Si el elemento está rotado visualmente con `transform_rotation`, el hit-test queda en la posición portrait original — hay que usar `lv_obj_set_ext_click_area()` para ampliar/desplazar el área táctil al lugar correcto, o bien reconsiderar el layout para que el hit-test nativo coincida con la posición visual.

---

### Clasificación

| Bug | Causa | Gravedad | Archivo |
|-----|-------|----------|---------|
| `s_strip` clickable → absorbe todos los toques del header | `LV_OBJ_FLAG_CLICKABLE` no eliminado | 🔴 Crítico — header entero no responde | `UIHeader.cpp` línea 31 |
| Hit-test portrait desalineado con posición visual landscape | `transform_rotation` no arrastra el hit-test en LVGL v9 | 🔴 Crítico — botón responde solo en bounding box portrait original | `UIHeader.cpp` líneas 59–61, 94–101 |

---

## Resumen de bugs críticos en P4

| # | Bug | Archivo P4 | Fix de referencia (S3) |
|---|-----|-----------|----------------------|
| **B-P4-1** | `LOGIC_PITCHBEND_MAX` ausente → normalización errónea de fader en display (usa 16383 en lugar de 14845) | `config.h` | Añadir `#define LOGIC_PITCHBEND_MAX 14845` |
| **B-P4-2** | Sin clamp de `bendValue` a 0 → wrapping a 65535 con valores negativos | `MIDIProcessor.cpp::processPitchBend` | `int bendClamped = (bendValue < 0) ? 0 : bendValue;` |
| **B-P4-3** | Sin `PITCHBEND_DEADBAND` ni `lastSentPitchBend[]` → tráfico RS485 excesivo | `MIDIProcessor.cpp` namespace + `processPitchBend` | Añadir ambas variables + guard |
| **B-P4-4** | Bug B2 latente: sin guard row vacía en 0x12 → modo plugin/Atmos borra nombres de pista | `MIDIProcessor.cpp::processMackieSysEx` case 0x12 | `if (nameBufs[t][0] == '\0') continue;` |
| **B-P4-5** | Ghost text 13 chars en create vs 14 chars en update → pivot de rotación desalineado en modo BEATS | `UIHeader.cpp` líneas 74 y 118 | Igualar el texto ghost en ambos sitios |
| **B-P4-6** | Botón BEAT/SMPT no envía Note 0x59 a Logic → toggle inoperativo, Logic lo revierte en ~100ms | `UIHeader.cpp` callback click línea 63 | Añadir `sendMIDIBytes` con Note On 0x59 vel 127 |
| **B-P4-7** | `hasDigit` comprueba `timeCodeChars_clean` en modo BEATS — inconsistencia latente | `UIHeader.cpp` línea 110 | En modo BEATS, comprobar `beatsChars_clean` |
| **B-P4-8** | `s_strip` clickable por defecto → absorbe todos los toques de la columna header | `UIHeader.cpp` línea 31 | `lv_obj_clear_flag(s_strip, LV_OBJ_FLAG_CLICKABLE)` |
| **B-P4-9** | Hit-test de objetos rotados (s_mode_lbl, s_timecode) no sigue `transform_rotation` → área táctil desalineada con posición visual | `UIHeader.cpp` líneas 59–61, 94–101 | Redefinir hit-test con `lv_obj_set_ext_click_area()` o replantear enfoque de rotación |

---

## Divergencias arquitectónicas pendientes de decisión

| Tema | S3 | P4 | Decisión pendiente |
|------|----|----|-------------------|
| Calibración al conectar Logic | No (boot auto-calib) | Sí (`_calibPendingFrom=1` en 0x21) | ¿P4 mantiene calibración-on-connect o adopta boot auto-calib de S3? |
| `beginDisconnectSequence()` | Sí (en RS485) | No existe en RS485 P4 | ¿P4 necesita notificación gradual a slaves o reset inmediato es suficiente? |
| LEDs de transporte | `Transporte::setLedByNote()` | Sin implementar | P4 necesita equivalente via NeoTrellis (pendiente backlog) |
| Timecode en display | No aplica | `MODE_SMPTE`/`MODE_BEATS` via notas 113/114 | ✅ Correcto para P4 — no portar a S3 |

---

## Feature: Agregación de 16 pistas en P4 — VU global (2026-05-24)

### Motivación

Observado en sesión 2026-05-24 mediante captura MIDI Monitor (SysEx 0x12, cmd LCD Write):

| Destino | Pistas recibidas | Tracks Logic |
|---------|-----------------|-------------|
| iMakie-P4-Master | 8 tracks (ch 1–8) | AudioT, Base, Audio2, No, nathle, VOZ 4, alex, Inst 4 |
| iMakie-Extender (S3) | 8 tracks (ch 1–8 del extender) | Soler, Loper, Trnqil, piano, Audo12, TrTeBe, DWYSle, Audo15 |
| **Total proyecto** | **16 pistas** | — |

Logic Pro envía cada banco de 8 canales a su dispositivo Mackie MCU correspondiente de forma independiente. P4 y S3 reciben MIDI por separado, sin visibilidad mutua del banco del otro.

**Objetivo:** P4 agrega los datos de las 16 pistas (VU meters, nombres, fader positions) y los muestra en su display 480×800 LVGL — P4 tiene la potencia para ello (dual-core 400MHz, 32MB PSRAM).

---

### Arquitectura actual vs objetivo

```
ACTUAL:
Logic ──USB-MIDI──► P4  (8 tracks, ch 1–8)    ► display P4 muestra 8ch
Logic ──USB-MIDI──► S3  (8 tracks, ch 9–16)   ► S3 sin display

OBJETIVO:
Logic ──USB-MIDI──► P4  (8 tracks, ch 1–8)    ─┐
Logic ──USB-MIDI──► S3  (8 tracks, ch 9–16)   ─┤► P4 agrega 16ch en display
                         S3 ──► P4  (reenvío)  ─┘
```

S3 debe reenviar a P4 los datos que recibe de Logic (VU, nombres de pista, posiciones de fader). P4 los almacena en sus propios arrays de 16 canales y los muestra en el display.

---

### Datos a sincronizar S3 → P4

| Tipo de dato | Mensaje MIDI de origen | Frecuencia | Prioridad |
|-------------|----------------------|-----------|-----------|
| **VU meters** | Channel Pressure (0xD0) | Alta — ~10-20 Hz por canal | 🔴 Alta — feedback visual en tiempo real |
| **Nombres de pista** | SysEx 0x12 row 0 | Baja — solo al cambio | 🟡 Media |
| **Posición de fader** | PitchBend (0xE0) | Media — al mover fader | 🟡 Media |
| **Estado botones** (rec/mute/solo/select) | Note On/Off | Media — al cambiar | 🟡 Media |
| **Auto mode** | SysEx 0x0E | Baja | 🔵 Baja |

VU meters son la prioridad absoluta: son los únicos datos con frecuencia alta y efecto visual directo.

---

### Canal de comunicación S3 → P4

**WiFi descartado** — no se enciende WiFi en este proyecto. Descartado por diseño.

**Solución elegida: MIDI por UART directo (31250 baud)**

S3 y P4 ya tienen implementado el parser MIDI completo (`processMidiByte()`). El protocolo de comunicación ya existe — solo hace falta un cable UART entre las placas.

```
Logic ──USB-MIDI──► S3  (ch 1–8, extender)
                    S3 ──UART MIDI 31250──► P4  (re-emite como ch 9–16)
                                            P4 parsea con processMidiByte() existente
```

S3 re-emite a P4 lo que recibe de Logic, remapeando canales 1–8 → 9–16. P4 ya sabe parsear Channel Pressure, SysEx 0x12, PitchBend — sin código nuevo en el parser.

**Carga UART a 31250 baud:**
- Channel Pressure: 2 bytes × 8 canales × 20 Hz = 320 bytes/s
- Capacidad UART MIDI: ~3125 bytes/s
- Ocupación: ~10% — completamente holgado

**Ventajas:**
- Sin radio, sin WiFi, sin ESP-NOW
- Sin riesgo de interferencia con RS485 (UART es hardware independiente)
- Protocolo reutilizado — cero código nuevo en el parser de P4
- S3 solo añade un re-emit de los mensajes que ya procesa

**Requisito hardware:** 1 cable TX→RX entre S3 y P4. Ambas placas tienen UARTs libres (RS485 usa uno, quedan al menos 2 en cada MCU).

**Mensajes que S3 debe re-emitir a P4:**

| Mensaje Logic→S3 | Re-emit S3→P4 | Propósito en P4 |
|-----------------|---------------|-----------------|
| Channel Pressure (ch 1–8) | Channel Pressure (ch 9–16) | VU meters canales 9–16 |
| SysEx 0x12 (track names) | SysEx 0x12 (ídem, offset +56) | Nombres pistas 9–16 en display |
| PitchBend (ch 1–8) | PitchBend (ch 9–16) | Posición faders 9–16 |
| Note On/Off (rec/mute/solo/select) | Note On/Off (remapeado) | Estados botones 9–16 |

---

### ⚠️ P4 ya recibe datos del extender — pero incorrectamente (2026-05-24)

Logic Pro, en arquitectura master+extender, envía Channel Pressure con dos encodings distintos:

| Destino | Mensaje | Encoding |
|---------|---------|----------|
| Master (P4) | `0xD0 ch=0`, value=`(track<<4)\|level` | 4 bits nivel, 0–11 por pista |
| Extender (S3) | `0xD0 ch=1..7`, un canal MIDI por pista | Nivel 0–127 directo |

**El problema:** Logic envía los mensajes del extender (`ch 1..7`) también al master (P4) como broadcast. P4 los procesa en `processChannelPressure` bloque `ch >= 1 && ch <= 7` (línea 281), los mapea a sus propios `targetChannel 1-7` con escala 0–127, y los propaga a sus S2 slaves.

Consecuencias actuales:
- S2 slaves 2–8 reciben VU **dos veces**: una correcta (ch=0, 4-bit) y una incorrecta (ch=1-7, 0-127 mal escalada) 
- Display P4 muestra VU solapados de ambas fuentes para los mismos canales
- El bloque `ch 1..7` en P4 está resolviendo un problema real (Logic envía esos mensajes) pero con la escala equivocada

**Implicación para la feature de 16 pistas:**
Cuando S3 re-emita sus VU por UART a P4 (ch 9–16), P4 ya estará recibiendo una versión corrupta de esos mismos datos directamente de Logic (ch=1..7 → mapeados a targets 1-7). Hay que decidir:
1. ¿P4 ignora el bloque `ch 1..7` de Logic (esos datos son del extender, no del master)?
2. ¿O P4 los redirige correctamente a slots 9–16 con escala 0–127?

**Opción correcta:** ignorar `ch 1..7` de Logic en P4 (o filtrarlos) y usar exclusivamente el relay UART de S3 como fuente para los canales 9–16. Así no hay doble fuente ni solapamiento.

---

### Cambios necesarios en cada MCU

| MCU | Cambio necesario |
|-----|-----------------|
| **S3** | Añadir emisor ESP-NOW/UART que reenvíe datos de los 8 canales a P4 al recibirlos de Logic |
| **P4** | Añadir receptor ESP-NOW/UART; ampliar arrays de estado a 16 canales; nueva página LVGL con 16 VU |
| **S2** | Sin cambios — no tiene visibilidad de esta feature |

---

### Display P4 — Página 16ch VU

P4 tiene 480×800 px LVGL (landscape: 800×480). La UIPage3 actual muestra 8 canales de VU. Una nueva página UIPage3_16ch mostraría los 16 canales:

```
Landscape 800×410 px (zona de contenido, excluyendo header 70px):
16 canales × ~48px ancho = 768px (caben en 800px con márgenes)
Altura VU: 400px (suficiente para 12 segmentos visibles)
```

No hay restricción de procesamiento — P4 tiene capacidad para gestionar 16 VU en LVGL a 30fps con margen.

---

### Estado (2026-05-24)

- [ ] Canal de comunicación P4↔S3 — **pendiente decidir** (ESP-NOW vs UART)
- [ ] Protocolo S3VUPacket — propuesta, **pendiente validar**
- [ ] S3: emisor de datos — **no implementado**
- [ ] P4: receptor + arrays 16ch — **no implementado**
- [ ] P4: UIPage3_16ch — **no implementado**

**Prioridad:** Feature de backlog. No bloquea funcionamiento actual. Implementar tras estabilizar S3 y P4 en 8 canales.

---

## Archivos relevantes

| Archivo | Ruta |
|---------|------|
| S3 MIDIProcessor.cpp | `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/midi/MIDIProcessor.cpp` |
| S3 MIDIProcessor.h | `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/midi/MIDIProcessor.h` |
| S3 config.h | `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h` |
| P4 MIDIProcessor.cpp | `MASTER_S3-P4/P4/src/midi/MIDIProcessor.cpp` |
| P4 MIDIProcessor.h | `MASTER_S3-P4/P4/src/midi/MIDIProcessor.h` |
| P4 config.h | `MASTER_S3-P4/P4/src/config.h` |
