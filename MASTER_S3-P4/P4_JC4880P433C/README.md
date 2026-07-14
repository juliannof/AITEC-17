# ExPressif — MIDI XY Controller

**Fabricante:** AITEC  
**Plataforma:** ESP32-P4 · USB MIDI Class-Compliant  
**Directorio:** `MASTER_S3-P4/P4_JC4880P433C/`

---

## Concepto

Controlador MIDI de performance inspirado en el Korg Kaoss Pad. El área táctil central de 480×480 px mapea coordenadas XY a dos parámetros MIDI continuos (CC). El toque sobre el pad dispara notas en la escala activa.

---

## Hardware (GUITION JC4880P433C)

| Componente | Especificación |
|---|---|
| MCU | ESP32-P4 dual-core 400 MHz |
| Flash | 16 MB |
| PSRAM | 32 MB OPI |
| Display | IPS 4.3" 480×800 · MIPI-DSI 2-lane · ST7701S |
| Touch | GT911 capacitivo I2C (GPIO 7/8) |
| NeoTrellis | 2× seesaw 4×4 RGB (I2C 0x2F/0x2E, GPIO 33/31) |
| USB | USB-OTG · MIDI Class-Compliant |

---

## Pantalla — Layout landscape 800×480

```
┌─────────┬──────────────────────────────┬─────────┐
│  HOLD   │                              │  SCALE  │
│  [OFF]  │       PAD  480×480           │ [MAJOR] │
│─────────│                              │─────────│
│   TAP   │                              │  PANIC  │
│         │                              │ ALL OFF │
└─────────┴──────────────────────────────┴─────────┘
  160px               480px               160px
```

> El display físico es portrait (480×800). LVGL lo rota 90° por software — todas las coordenadas de UI son landscape (800×480).

---

## Controles

### Pad XY
- **Touch** → Note On en la escala activa (nota determinada por posición Y)
- **Drag** → CC_X (eje horizontal, def. CC74) + CC_Y (eje vertical, def. CC71)
- **Release** → Note Off (salvo HOLD activo)
- **Grilla 8×8 de dots** → cada dot iluminado al tocar; radio de brillo decae ~700ms tras soltar

### Animación LED (grilla 8×8)
La grilla funciona como un display de matriz LED gigante (cada "pixel" ≈ 58×60 px en pantalla real):

| Momento | Comportamiento |
|---|---|
| **Arranque** | "ExPressive" scrollea derecha → izquierda (~5.4 s) en font 5×7 pixel |
| **Reposo (3 s sin toque)** | Scroll automático indefinido |
| **Toque** | Cancela scroll, activa modo normal (brillo radial en posición XY) |

El color del texto y de los dots sigue el **sintetizador activo** (`COL_SYNTH_*`, `config.h` — 2026-07-14, antes seguía el modo `COL_MODE_KAOSS`/`GRID`/`ARP`). Mismo esquema de color que ya usaban las teclas de selección de synth del NeoTrellis (L8-L10, L12-L14) — ahora extendido también a los 20 presets del NeoTrellis y a la franja del botón PRESET.

### Botones
| Botón | Función | Estado |
|---|---|---|
| **HOLD** | Congela XY al soltar — la nota suena hasta nuevo toque | ON / OFF |
| **TAP** | Tap tempo *(v2)* | — |
| **PRESET** *(antes SCALE, 2026-07-14)* | Muestra el nombre de los parámetros X/Y de la memoria activa. Al tocarlo abre el editor en pantalla (ver [Sistema de memorias Kaos](#sistema-de-memorias-kaos-parámetros-configurables-2026-07-14)) | Nombre parámetro X / Y |
| **PANIC** | All Notes Off (CC 123) — o CC64/64 (centro) en los ejes de la memoria activa, si hay una configurada | — |

---

## NeoTrellis — Matriz 4×8 (2026-07-01)

2× Adafruit seesaw 4×4 RGB — panel izquierdo (I2C `0x2F`, columnas 0-3) + panel derecho (I2C `0x2E`, columnas 4-7). Numeración interna de cada panel: `key = fila×4 + col` (0-15).

### Mapa físico de índices (Lx = panel izquierdo 0x2F, Rx = panel derecho 0x2E)

| Fila | Col0 | Col1 | Col2 | Col3 | Col4 | Col5 | Col6 | Col7 |
|---|---|---|---|---|---|---|---|---|
| 0 | L0 | L1 | L2 | L3 | R0 | R1 | R2 | R3 |
| 1 | L4 | L5 | L6 | L7 | R4 | R5 | R6 | R7 |
| 2 | L8 | L9 | L10 | L11 | R8 | R9 | R10 | R11 |
| 3 | L12 | L13 | L14 | L15 | R12 | R13 | R14 | R15 |

### Referencia secuencial — numeración física 1-32 (izquierda→derecha, arriba→abajo)

> Numeración derivada de `key = fila×4 + col` (convención de la librería Adafruit NeoTrellis,
> documentada en el comentario de cabecera de `NeoTrellis.cpp`) — **no verificada contra
> serigrafía física de la placa**, solo contra el código fuente.

**Panel izquierdo — 0x2F (botones 1-16):**

| Nº físico | Índice interno | Fila | Columna | Función Kaoss actual |
|---|---|---|---|---|
| 1 | L0 | 0 | 0 | HOLD |
| 2 | L1 | 0 | 1 | PANIC |
| 3 | L2 | 0 | 2 | **Brillo +** (2026-07-14, antes SCALE/cicla) |
| 4 | L3 | 0 | 3 | **Preset 0** (directo) |
| 5 | L4 | 1 | 0 | SYNTH |
| 6 | L5 | 1 | 1 | **Metrónomo visual** (2026-07-14, parpadea con MIDI Clock) |
| 7 | L6 | 1 | 2 | **Brillo −** (2026-07-14) |
| 8 | L7 | 1 | 3 | **Preset 5** (directo) |
| 9 | L8 | 2 | 0 | Selección directa synth (JV-2080) |
| 10 | L9 | 2 | 1 | Selección directa synth (TRITON) |
| 11 | L10 | 2 | 2 | Selección directa synth (TG55) |
| 12 | L11 | 2 | 3 | **Preset 10** (directo) |
| 13 | L12 | 3 | 0 | Selección directa synth (D-110) |
| 14 | L13 | 3 | 1 | Selección directa synth (WAVE) |
| 15 | L14 | 3 | 2 | Selección directa synth (MOTIF) |
| 16 | L15 | 3 | 3 | **Preset 15** (directo) |

**Panel derecho — 0x2E (botones 17-32) — TODAS son preset directo desde 2026-07-14:**

| Nº físico | Índice interno | Fila | Columna | Preset |
|---|---|---|---|---|
| 17 | R0 | 0 | 4 | Preset 1 |
| 18 | R1 | 0 | 5 | Preset 2 |
| 19 | R2 | 0 | 6 | Preset 3 |
| 20 | R3 | 0 | 7 | Preset 4 |
| 21 | R4 | 1 | 4 | Preset 6 |
| 22 | R5 | 1 | 5 | Preset 7 |
| 23 | R6 | 1 | 6 | Preset 8 |
| 24 | R7 | 1 | 7 | Preset 9 |
| 25 | R8 | 2 | 4 | Preset 11 |
| 26 | R9 | 2 | 5 | Preset 12 |
| 27 | R10 | 2 | 6 | Preset 13 |
| 28 | R11 | 2 | 7 | Preset 14 |
| 29 | R12 | 3 | 4 | Preset 16 |
| 30 | R13 | 3 | 5 | Preset 17 |
| 31 | R14 | 3 | 6 | Preset 18 |
| 32 | R15 | 3 | 7 | Preset 19 |

> Numeración de preset aquí es 0-based (índice interno, `kaoss.getPreset()`). En
> pantalla (botón PRESET) y en el editor se muestra 1-based ("P01".."P20").

### Modo Kaoss (por defecto, Bank cerrado) — 20 memorias, selección directa (2026-07-14)

Sustituye el sistema anterior (SCALE cicla + 4 presets directos R0-R3, único
desde 2026-06-29). Ya no hay "ciclar" — cada una de las 20 teclas asigna su
memoria de forma directa e inmediata, sin estado intermedio. Fórmula física →
índice de preset (`presetIndexLeft`/`presetIndexRight`, `NeoTrellis.cpp`):

```
Panel izquierdo (solo columna 3): preset = fila × 5
Panel derecho   (las 16 teclas):  preset = fila × 5 + 1 + columna
```

| Fila | Panel izquierdo (col 0-3) | Panel derecho (col 4-7) |
|---|---|---|
| 0 | **HOLD**(k0) · **PANIC**(k1) · —(k2) · **Preset 0**(k3) | **Preset 1-4** (k0-k3) |
| 1 | **SYNTH**(k4) · —(k5) · —(k6) · **Preset 5**(k7) | **Preset 6-9** (k0-k3) |
| 2 | **Synth JV-2080**(k8) · **Synth TRITON**(k9) · **Synth TG55**(k10) · **Preset 10**(k11) | **Preset 11-14** (k0-k3) |
| 3 | **Synth D-110**(k12) · **Synth WAVE**(k13) · **Synth MOTIF**(k14) · **Preset 15**(k15) | **Preset 16-19** (k0-k3) |

- **HOLD** — toggle congelar XY del pad
- **PANIC** — All Notes Off (CC 123 — pendiente, ver Roadmap) o reset a centro (CC64/64) en los ejes de la memoria activa, por el canal MIDI guardado en esa memoria
- **SYNTH** — tap cicla sintetizador activo; long-press ≥600ms abre `UIBank`
- **L8-L10, L12-L14** — selección directa de sintetizador (sin ciclar), solo activo en modo Kaoss (`g_bankOpen==false`)
- **L3, L7, L11, L15, R0-R15** — preset directo 0-19 (`g_trellis_setPreset`), tecla se ilumina con el color del **sintetizador activo** (`COL_SYNTH_*`) si es el preset seleccionado — mismo color que las teclas de selección de synth y que la rejilla de dots del pad táctil (2026-07-14)
- La tecla NeoTrellis **solo selecciona** qué par de parámetros usa el pad táctil — no manda MIDI por sí sola (el CC sale al tocar el pad XY)

### Modo Bank (UIBank abierto) — diseño por columnas + tríos (2026-07-04)

Sustituye el diseño por filas de 2026-07-01 (nunca implementado). Objetivo:
aligerar la interfaz para touch operativo — grid 2 cols × 4 filas = 8
ítems/página en pantalla (antes 16/10), y cada ítem corresponde a un **trío
de 3 teclas contiguas** del NeoTrellis (botón físico grande, más fácil de
acertar). Mientras `g_bankOpen=true`, las funciones Kaoss (HOLD/PANIC/SCALE/
SYNTH-tap/preset) quedan **suspendidas** en las teclas reutilizadas.

| Columna | Teclas | Función |
|---|---|---|
| **0** (izquierda) | L0, L4, L8, L12 | **Página anterior** (cualquiera de las 4) |
| **1-3** (panel izq.) | L1-3, L5-7, L9-11, L13-15 | Tríos de selección directa — **lado izquierdo** de cada fila |
| **4-6** (panel der.) | R0-2, R4-6, R8-10, R12-14 | Tríos de selección directa — **lado derecho** de cada fila |
| **7** (derecha) | R3, R7, R11, R15 | **Página siguiente** (cualquiera de las 4) |

**Mapa de slot (trío = 1 sonido; cualquier tecla del trío lo selecciona):**

| Fila | Trío izquierdo (panel izq.) | Trío derecho (panel der.) |
|---|---|---|
| 0 | L1,L2,L3 → **slot 0** | R0,R1,R2 → **slot 1** |
| 1 | L5,L6,L7 → **slot 2** | R4,R5,R6 → **slot 3** |
| 2 | L9,L10,L11 → **slot 4** | R8,R9,R10 → **slot 5** |
| 3 | L13,L14,L15 → **slot 6** | R12,R13,R14 → **slot 7** |

- Las 3 LEDs de un trío se encienden **al unísono con el mismo color**, como si fueran un único botón ancho: **azul** = sonido seleccionado, **naranja** = favorito, tenue = vacío/normal. En Favoritos, todo slot con contenido ya es favorito por definición (solo azul/tenue).
- Columnas 0 y 7 (página) se mantienen tenues mientras Bank está abierto, sin estado azul/naranja.
- `k==4` (SYNTH) conserva su comportamiento dual: long-press ≥600ms abre/cierra Bank **siempre**; tap corto cicla sintetizador si Bank está cerrado, o página anterior si Bank está abierto (columna 0 reutiliza esta tecla).
- Implementado en `NeoTrellis.cpp` (`leftTripletSlot`/`rightTripletSlot`, `neotrellisBankShowPage`) y `UIBank.cpp` (`uiBankNeoKey`/`uiBankNeoPage`).

---

## MIDI Output

⚠️ **2026-07-14 — el canal ya NO es el fijo `MIDI_CH`.** Cada **synth** (no
cada memoria — corrección 2026-07-14: "el canal midi es unico para el sinte,
no por preset") guarda su propio canal MIDI (1-16, editable desde el editor
en pantalla). El pad envía por `kaoss.getChannel()`, no por la constante
`MIDI_CH` de `config.h` (que queda **sin uso**, ver
[Configuración](#configuración-srcconfigh)).

| Mensaje | Valor | Condición |
|---|---|---|
| CC X (según memoria activa) | 0–127, lineal por posición del pad (izq → der) | Mientras se toca el pad — canal = `kaoss.getChannel()` (del synth activo) |
| CC Y (según memoria activa) | 0–127, lineal por posición del pad (abajo → arriba) | Mientras se toca el pad — canal = `kaoss.getChannel()` |
| CC X/Y → 64 | reset a centro | Botón PANIC (táctil o NeoTrellis `k1`) — solo si la memoria activa tiene parámetros configurados (`kaoss.hasPreset()`) |

Ver [Sistema de memorias Kaos](#sistema-de-memorias-kaos-parámetros-configurables-2026-07-14) para el origen de CC X/Y/canal.

---

## Sistema de memorias Kaos — parámetros configurables (2026-07-14)

Reemplaza el sistema original de 4 presets fijos en flash (`MOD`/`JOY`/`FILT`/`FX`,
CC1/2·16/17·74/71·91/93, iguales para todos los sintetizadores). Diseño y
decisiones de arquitectura: `aitec_kaos_brief_2026-07-13.md` (§2 modelo NVS,
§4 catálogo verificado por synth) + aclaraciones del usuario en sesión
2026-07-14 (20 memorias, parámetros sueltos configurables, canal único por
synth — varias correcciones sobre la marcha respecto al brief original, ver
CHANGELOG.md para el detalle completo de la iteración).

### Por qué — memorias por synth, no globales

Cada sintetizador del rack tiene parámetros MIDI distintos (CCs, rangos,
convenciones) y escucha en un canal MIDI propio. Antes, el Kaos pad mandaba
siempre CC1/16/74/91 por el canal fijo `MIDI_CH=1` (pensado para el
Wavestation VST) sin importar qué synth estuviera activo en Bank — en
JV-2080/TRITON/etc. la mayoría de esas memorias no hacían nada útil. Ahora el
Kaos lee el mismo `g_currentSynth` que usa `UIBank`: cada synth tiene su
propio banco de 20 memorias de parámetros **y** su propio canal MIDI.

### Modelo de datos

**Catálogo de parámetros nombrados** (`kaoss/KaosParams.h/.cpp`) — lista fija
en flash, por synth, de `{cc, nombre}` individuales (no parejas). El usuario
compone la pareja X/Y libremente desde el editor:

| Synth | Parámetros verificados | Fuente |
|---|---|---|
| JV-2080 | Cutoff(74), Resonance(71), Tone1 Lvl(80), Tone3 Lvl(82), Reverb Snd(91), Chorus Snd(93) | brief §4 — Sends solo funciona en modo Performance |
| TRITON Rack | Cutoff(74), Resonance(71), Filter EG(79), Release(72), LFO1 Speed(76), LFO1 Depth(77) | brief §4 Grupo A (Grupo B vía D-mod bloqueado, pendiente decisión `:Src` preconfigurado vs SysEx en cada selección) |
| MOTIF-RACK | Cutoff(74), Resonance(71) | brief §4 — resto sin explorar |
| TG55 / D-110 | *(vacío)* | Sin CC de fábrica verificado — requeriría SysEx por frame, no CC (brief §4) |
| WAVE (Wavestation VST) | Mod Wheel(1), Breath(2), Joy X(16), Joy Y(17), Cutoff(74), Resonance(71), Reverb Snd(91), Chorus Snd(93) | Presets originales del proyecto (2026-06-29), fuera del catálogo del brief (destino VST, no rack) |

**Almacén NVS** (`nvs/KaosStore.h/.cpp`, namespace `"kaos"`, mismo patrón que
`FavStore.cpp`):
- `kaos_slot[synth_id][slot 0-19] = {ccX, ccY, configured}` — clave `"s<synth>_<slot>"`. `configured==0` = slot vacío/sin datos.
- `kaos_channel[synth_id] = ch` (1-16) — clave `"c<synth>"`, **un valor por synth**, no por slot. `kaosLoadChannel()` devuelve 1 si nunca se guardó (no hay "vacío" para el canal).

Se siembra el catálogo de parámetros (no el canal, que ya tiene default=1) una
vez, la primera vez que arranca (`kaosInit()`, flag `"seeded"` en NVS) —
después es completamente editable sin reflashear.

**`KaossPad`** (`kaoss/KaossPad.h/.cpp`) ya no contiene catálogos `const` —
cachea en RAM el slot activo (`_current`, recargado en `setPreset()`) y el
canal del synth activo (`_channel`, recargado en `syncToSynth()`/`reload()` —
**no** en `setPreset()`, ya que el canal no depende del slot). `mapXtoCC`/
`mapYtoCC` no cambiaron — un CC MIDI siempre es un byte 0-127 lineal según
posición del pad, sea cual sea el synth.

### Editor en pantalla

Botón **PRESET** (antes SCALE) → tap abre `display/UIKaosEdit.h/.cpp`, overlay
a pantalla completa (mismo patrón show/hide que `UIBank`): dos listas
("EJE X"/"EJE Y") con los parámetros nombrados del synth activo — tocar uno
asigna ese eje — más un selector de canal MIDI (+/-, 1-16, **justo debajo del
título**, ya que es un ajuste del synth, no del slot) y botones GUARDAR/
CANCELAR. Al Guardar: el par ccX/ccY se escribe en el slot que se estaba
editando, pero **el canal se aplica a los 20 slots del synth activo**, no
solo a ese slot. Si el synth activo no tiene catálogo verificado (TG55/D-110),
muestra "Sin parámetros verificados" sin botón Guardar.

Geometría (verificada contra el botón cerrar de `UIBank.cpp`, ya probado en
hardware, y corregida en vivo tras dos rondas de feedback del usuario
2026-07-14 — "canal sale a la izquierda"/"todo pegado abajo"):
- `x` grande → arriba físico · `x` pequeño → abajo físico
- `y` pequeño → izquierda física · `y` grande → derecha física
- Título arriba-izquierda · canal (−/valor/+) en fila horizontal justo debajo
  · EJE X (izquierda) / EJE Y (centro-derecha) por debajo de eso · Guardar/
  Cancelar en el borde derecho, bajo el botón cerrar

---

## Color por sintetizador — esquema extendido a toda la UI (2026-07-14)

El color por synth (`COL_SYNTH_*`, `config.h`) ya se usaba correctamente en
las teclas de selección de synth del NeoTrellis (L8-L10, L12-L14). Petición
del usuario: extenderlo a todo lo demás que antes usaba un color fijo o de
"modo":

| Elemento | Antes | Ahora |
|---|---|---|
| 20 teclas de preset (NeoTrellis, L3/L7/L11/L15+R0-R15) | `modeColor()` (rojo KAOSS_XY fijo) | `synthColor()` — `modeColor()` eliminado de `NeoTrellis.cpp`, sin uso |
| Rejilla 8×8 de dots + scroll "ExPressive" (`UIKaoss.cpp`) | `mode_color()` (mismo rojo fijo) | `synth_color_rgb()` |
| Franja del botón PRESET (pantalla) | Verde fijo `0x00AA44` | `synthColor()`, incluso en reposo (`darken()`, 20% brillo) — antes solo el botón SYNTH cambiaba color al pulsar, PRESET quedaba fijo |
| Franja blanca con nombre de synth (`UIBank.cpp`, tabs Sonidos/Performances/Favoritos) | Blanco fijo `0xFFFFFF` | `synthColorHex()` (duplicado local, mismo patrón que `synthLabelText()`) |

Todos se refrescan en `uiKaossUpdateSynth()`/`uiBankSynthChanged()`, ya
llamados al cambiar de synth por cualquier vía (tap, long-press Bank,
selección directa NeoTrellis).

---

## Brillo de pantalla — L2/L6 + popup (2026-07-14)

Las dos teclas NeoTrellis que quedaron sin función tras retirar SCALE (L2) y
sin usar desde el principio (L6) ahora controlan el brillo del backlight:

| Tecla | Función |
|---|---|
| **L2** | Brillo −10% (mínimo 10%) |
| **L6** | Brillo +10% (máximo 100%) |

Al pulsar, `displaySetBrightness()` aplica el nuevo valor (`g_displayBrightness`,
`config.h`, persiste solo en RAM — no en NVS, vuelve a 80% en cada arranque) y
aparece un popup centrado en pantalla (`display/UIBrightnessPopup.h/.cpp`,
overlay independiente, topmost) con el número, que se oculta solo tras ~1.2s.
L2/L6 se iluminan tenues en reposo (`COL_ACCENT`) y brillan al pulsar.

---

## Metrónomo visual — L5 (2026-07-14)

L5 (última tecla NeoTrellis sin función) parpadea sincronizado al **MIDI
Clock entrante por USB** (24 PPQN, mensajes realtime `0xF8` Clock/`0xFA`
Start/`0xFB` Continue/`0xFC` Stop) — no hay BPM propio en este firmware, seguir
el reloj que manda Logic/el DAW por USB era el requisito. Confirmado con
captura real de MIDI Monitor mostrando Clock llegando a "ExPressif V1".

- `midi/MIDIClock.h/.cpp` — único punto del firmware que lee `MIDI.readPacket()`
  (hasta ahora el proyecto solo enviaba MIDI, nunca recibía). Se sondea en
  `taskCore0` (`main.cpp`), cada ~20ms, drenando todos los paquetes pendientes.
- Cuenta Clock 0-23 (24 por negra); en el pulso 0 marca un "beat" pendiente.
  Start/Continue realinean la fase a 0 inmediatamente; Stop apaga el LED pero
  el conteo de Clock sigue vivo (no se pierde la fase si vuelve a sonar sin
  Start explícito).
- `neotrellis/NeoTrellis.cpp::metronomeUpdate()` consume el beat, enciende L5
  a brillo pleno (`COL_ACCENT`) y decae en ~80ms (mismo patrón de decay que
  `s_glow_t` del pad XY en `UIKaoss.cpp`).

---

## Escalas

| ID | Nombre | Notas |
|---|---|---|
| 0 | MAJOR | C D E F G A B |
| 1 | MINOR | C D Eb F G Ab Bb |
| 2 | PENTA | C D E G A |
| 3 | CHROM | Cromática completa |

El pad abarca **2 octavas** en el eje Y. La raíz es C4 por defecto.

---

## Configuración (`src/config.h`)

> ⚠️ `MIDI_CH`, `CC_X`, `CC_Y` quedaron **sin uso** en el Kaos pad desde
> 2026-07-14 — el canal y los CC salen del slot activo (`KaosStore`), no de
> estas constantes. Se dejan definidas por si se usan en otro sitio.

```c
#define MIDI_CH        1    // Canal MIDI — SIN USO en Kaos desde 2026-07-14
#define CC_X          74    // CC eje horizontal — SIN USO en Kaos desde 2026-07-14
#define CC_Y          71    // CC eje vertical — SIN USO en Kaos desde 2026-07-14
#define NOTE_VELOCITY 100

#define SCALE_MAJOR    0
#define OCTAVE_DEFAULT 4
#define BOOT_SCREEN_MS   3000  // duración splash (ms)
#define SCROLL_STEP_MS     80  // velocidad scroll grilla: 1 col cada 80 ms (~5.4 s texto completo)
#define SCROLL_IDLE_TICKS  60  // 60 × 50 ms = 3 s de reposo antes de scroll automático
```

---

## Estructura de código

> Árbol no exhaustivo — no incluye `UIBank.cpp`, `TritonPatches.h/cpp`,
> `JVPatches.h/cpp`, `NeoTrellis.h/cpp`, `FavStore.h/cpp` ni otros módulos
> previos a esta nota (documentación desactualizada por sesiones anteriores).
> Añadidos de la sesión 2026-07-14 marcados abajo.

```
src/
├── main.cpp                  — Setup + tareas Core0/Core1
├── config.h                  — Configuración global
├── midi/
│   └── MIDIOut.h/cpp         — sendCC, sendNote, sendAllNotesOff
├── kaoss/
│   ├── KaossPad.h/cpp        — Lógica XY: mapeo CC, cache RAM del slot activo
│   └── KaosParams.h/cpp      — 🆕 2026-07-14: catálogo de parámetros nombrados por synth
├── nvs/
│   └── KaosStore.h/cpp       — 🆕 2026-07-14: kaos_slot[synth][0-19] en NVS ("kaos")
├── display/
│   ├── Display.h/cpp         — Init LCD + LVGL + rotación landscape
│   ├── UIBoot.h/cpp          — Splash 3s (AITEC / ExPressif)
│   ├── UIKaoss.h/cpp         — Pad XY + 4 botones + grilla 8×8 dots + scroll "ExPressive"
│   └── UIKaosEdit.h/cpp      — 🆕 2026-07-14: editor de memoria (parámetros X/Y + canal)
└── lcd/ touch/               — Drivers ST7701S + GT911 (no modificar)
```

---

## Roadmap

- **v1.0** — Pad XY + MIDI CC + Note + 4 botones táctiles *(este firmware)*
- **v2.0** — NeoTrellis: selección de escala/root/octava/modo por matriz 4×8
- **v3.0** — Modo Note Grid (cuadrícula isomórfica en pantalla)
- **v4.0** — Arpeggiador + tap tempo

---

*ExPressif — expressif · Espressif · ESP32*
