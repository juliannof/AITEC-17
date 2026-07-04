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

El color del texto y de los dots sigue el modo activo (`COL_MODE_KAOSS` rojo por defecto).

### Botones
| Botón | Función | Estado |
|---|---|---|
| **HOLD** | Congela XY al soltar — la nota suena hasta nuevo toque | ON / OFF |
| **TAP** | Tap tempo *(v2)* | — |
| **SCALE** | Cicla escala: MAJOR → MINOR → PENTA → CHROM | Nombre activo |
| **PANIC** | All Notes Off (CC 123) | — |

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
| 3 | L2 | 0 | 2 | SCALE |
| 4 | L3 | 0 | 3 | — |
| 5 | L4 | 1 | 0 | SYNTH |
| 6 | L5 | 1 | 1 | — |
| 7 | L6 | 1 | 2 | — |
| 8 | L7 | 1 | 3 | — |
| 9 | L8 | 2 | 0 | — |
| 10 | L9 | 2 | 1 | — |
| 11 | L10 | 2 | 2 | — |
| 12 | L11 | 2 | 3 | — |
| 13 | L12 | 3 | 0 | — |
| 14 | L13 | 3 | 1 | — |
| 15 | L14 | 3 | 2 | — |
| 16 | L15 | 3 | 3 | — |

**Panel derecho — 0x2E (botones 17-32):**

| Nº físico | Índice interno | Fila | Columna | Función Kaoss actual |
|---|---|---|---|---|
| 17 | R0 | 0 | 4 | Preset 0 |
| 18 | R1 | 0 | 5 | Preset 1 |
| 19 | R2 | 0 | 6 | Preset 2 |
| 20 | R3 | 0 | 7 | Preset 3 |
| 21 | R4 | 1 | 4 | — |
| 22 | R5 | 1 | 5 | — |
| 23 | R6 | 1 | 6 | — |
| 24 | R7 | 1 | 7 | — |
| 25 | R8 | 2 | 4 | — |
| 26 | R9 | 2 | 5 | — |
| 27 | R10 | 2 | 6 | — |
| 28 | R11 | 2 | 7 | — |
| 29 | R12 | 3 | 4 | — |
| 30 | R13 | 3 | 5 | — |
| 31 | R14 | 3 | 6 | — |
| 32 | R15 | 3 | 7 | — |

### Modo Kaoss (por defecto, Bank cerrado)

| Fila | Panel izquierdo (col 0-3) | Panel derecho (col 4-7) |
|---|---|---|
| 0 | **HOLD**(k0) · **PANIC**(k1) · **SCALE**(k2) · — | **Preset 0-3** (k0-k3) |
| 1 | **SYNTH**(k4, tap=cicla sintetizador / long-press ≥600ms=abre Bank) · — · — · — | — · — · — · — |
| 2-3 | apagadas | apagadas |

- **HOLD** — toggle congelar XY del pad
- **PANIC** — All Notes Off (CC 123)
- **SCALE** — cicla preset de escala (`kaoss.nextPreset()`)
- **SYNTH** — tap cicla sintetizador activo (`g_currentSynth`); long-press ≥600ms abre `UIBank`
- **Preset 0-3** (panel derecho, fila 0) — selección directa de preset (`kaoss.setPreset()`)

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

| Mensaje | Valor | Condición |
|---|---|---|
| Note On/Off | Canal `MIDI_CH` (def. 1) | Al tocar / soltar pad |
| CC 74 (`CC_X`) | 0–127 (izq → der) | Mientras se toca |
| CC 71 (`CC_Y`) | 0–127 (abajo → arriba) | Mientras se toca |
| CC 123 (All Notes Off) | 0 | Botón PANIC |

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

```c
#define MIDI_CH        1    // Canal MIDI
#define CC_X          74    // CC eje horizontal
#define CC_Y          71    // CC eje vertical
#define NOTE_VELOCITY 100

#define SCALE_MAJOR    0
#define OCTAVE_DEFAULT 4
#define BOOT_SCREEN_MS   3000  // duración splash (ms)
#define SCROLL_STEP_MS     80  // velocidad scroll grilla: 1 col cada 80 ms (~5.4 s texto completo)
#define SCROLL_IDLE_TICKS  60  // 60 × 50 ms = 3 s de reposo antes de scroll automático
```

---

## Estructura de código

```
src/
├── main.cpp                  — Setup + tareas Core0/Core1
├── config.h                  — Configuración global
├── midi/
│   └── MIDIOut.h/cpp         — sendCC, sendNote, sendAllNotesOff
├── kaoss/
│   └── KaossPad.h/cpp        — Lógica XY: escalas, CC mapping, notas
├── display/
│   ├── Display.h/cpp         — Init LCD + LVGL + rotación landscape
│   ├── UIBoot.h/cpp          — Splash 3s (AITEC / ExPressif)
│   └── UIKaoss.h/cpp         — Pad XY + 4 botones + grilla 8×8 dots + scroll "ExPressive"
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
