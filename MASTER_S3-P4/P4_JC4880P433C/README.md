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
- **Crosshair** → sigue el dedo; color varía de azul (arriba) a naranja (abajo)

### Botones
| Botón | Función | Estado |
|---|---|---|
| **HOLD** | Congela XY al soltar — la nota suena hasta nuevo toque | ON / OFF |
| **TAP** | Tap tempo *(v2)* | — |
| **SCALE** | Cicla escala: MAJOR → MINOR → PENTA → CHROM | Nombre activo |
| **PANIC** | All Notes Off (CC 123) | — |

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
#define BOOT_SCREEN_MS 3000
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
│   └── UIKaoss.h/cpp         — Pad XY + 4 botones laterales
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
