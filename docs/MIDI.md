# MIDI.md — Protocolo Mackie MCU (iMakie S3 Extender)

**Fuente:** `src/midi/MIDIProcessor.cpp`  
**Última actualización:** 2026-05-18 18:10  
**Estado:** Producción

---

## 1. CONTEXTO

iMakie usa el protocolo **Mackie Control Universal (MCU)** sobre USB-MIDI. Logic Pro lo reconoce como una superficie de control física (familia `0x14`).

El S3 actúa como **esclavo MCU**: responde al handshake de Logic, traduce mensajes MIDI a comandos RS485 hacia los S2, y devuelve el estado de faders/botones/encoders a Logic.

---

## 2. ESTADOS DE CONEXIÓN

```
DISCONNECTED
    │
    │  Logic envía GoOnline (SysEx 0x21)
    ▼
CONNECTED  ◄──── estado operacional normal
    │
    │  Logic envía GoOffline (SysEx 0x0F)
    │  O: 9+ faders a 0 simultáneamente (detección automática)
    ▼
DISCONNECTED
```

> Los estados INITIALIZING, AWAITING_SESSION y MIDI_HANDSHAKE_COMPLETE existen en el enum pero actualmente no se usan como estados de tránsito activos. El salto es directo DISCONNECTED → CONNECTED.

---

## 3. HANDSHAKE — SECUENCIA DE CONEXIÓN

Logic sondea la superficie antes de identificarla. El handshake tiene dos fases.

### Fase 0 — Sondeo (cualquier familia)

Logic pregunta a todos los dispositivos conectados:

```
Logic → S3:   F0 00 00 66 XX 00 F7        (XX = cualquier familia)
S3 → Logic:   F0 00 00 66 14 01 00 00 00 01 00 00 00 00 F7
              └─ S3 se identifica como familia 0x14
```

```
Logic → S3:   F0 00 00 66 XX 13 F7        (petición de versión)
S3 → Logic:   F0 00 00 66 14 14 00 F7
```

A partir de aquí, Logic sabe que hay una superficie familia `0x14` y solo enviará comandos con ese identificador.

### Fase 1 — Conexión (familia 0x14)

```
Logic → S3:   F0 00 00 66 14 0C 00 F7     (tipo de superficie)
S3 → Logic:   F0 00 00 66 14 0C 00 F7     (eco inmediato)
S3 → Logic:   F0 00 00 66 14 10 00 F7     (suscripción a feedback)
              └─ Pide recibir Note On/Off, VU, etc. en tiempo real
```

### Fase 2 — GoOnline (crítico)

```
Logic → S3:   F0 00 00 66 14 21 01 F7
S3 → Logic:   F0 00 00 66 14 21 01 F7     (eco inmediato)
```

**Efecto en S3:**
- Estado → CONNECTED
- `g_logicConnected = 1` (RS485 task empieza a enviar paquetes a S2)
- Se dispara `tickCalibracion()` → FLAG_CALIB al primer S2

---

### 3.3 — Secuencia Completa de Arranque (MIDI monitor 2026-05-18)

Logic Pro emite **3 iteraciones GoOnline** en ~2.5 segundos. Solo la tercera contiene el estado real del proyecto.

```
t=0ms     Logic → S3:  Sondeo cmd 0x00 (familia 0x14)
          Logic → S3:  GoOnline #1 (cmd 0x21) + reset completo:
                         SysEx 0x20 ×8    — VPot rings a 0
                         SysEx 0x0A 01    — fader touch sense ON
                         SysEx 0x0E ×9    — automodos → Trim (modo 3)
                         SysEx 0x0C 00    — tipo superficie
                         SysEx 0x0B 0F    — button enable mask (REC/SOLO/MUTE/SEL)
                         SysEx 0x12       — nombres de canal (vacíos: "- ")
                         CC reset a 32    — VPots a centro
                         Note Off masivo  — todos los botones/LEDs a OFF
                         Note On selectivos — LEDs fijos del proyecto (LOOP, etc.)
                         SysEx 0x72       — VU meters a 7 (peak)
                         Pitch Wheel ×10  — -8192 (raw 0) ← TODOS LOS FADERS A MÍNIMO
                         CC VPots a 0

t=122ms   Logic → S3:  GoOnline #2 (cmd 0x21) + mismo reset completo
                         Pitch Wheel ×10  — -8192 ← de nuevo todos a mínimo

t=2471ms  Logic → S3:  GoOnline #3 (cmd 0x21) + estado REAL del proyecto:
                         SysEx 0x12       — volcado completo estado real: nombres en row1, valores en row2 (ej: "Pan", "PanSpr", "0", "111 o" si el proyecto estaba en modo Pan)
                         Note On reales   — botones/LEDs con estado real
                         SysEx 0x72       — VU con niveles reales
                         Pitch Wheel ×10  — valores reales: 6653, -951, -6755, 3733…
                         CC VPots reales

t=~4000ms Logic → S3:  SysEx 0x0E ×9    — automodos reales del proyecto (Trim por defecto)
```

> **⚠️ CRÍTICO — Por qué existe `CONNECT_GRACE_MS = 1500`:**
> Las iteraciones #1 y #2 mandan los 10 faders a -8192 (raw 0). Sin grace period, el sistema detectaría 9+ faders a 0 simultáneos y ejecutaría la desconexión automática. El grace period de 1500ms absorbe las dos primeras iteraciones y deja pasar la tercera con los valores reales.

---

## 4. COMANDOS LOGIC → S3

### 4.1 GoOffline (SysEx 0x0F)

```
Logic → S3:   F0 00 00 66 14 0F F7
```

Logic se está desconectando. S3:
- Estado → DISCONNECTED
- `g_logicConnected = 0`
- Inicia secuencia de desconexión: envía `connected=0` a todos los S2 por RS485
- El estado no cambia a offline en pantalla hasta que todos los S2 confirmen recepción

---

### 4.2 Scribble Strip (SysEx 0x12)

```
Logic → S3:   F0 00 00 66 14 12 <offset> <chars...> F7
```

**Layout del buffer:** 112 bytes totales — 2 filas × 8 canales × 7 chars.

| Rango offset | Nombre | Contenido según momento |
|-------------|--------|------------------------|
| 0–55 | **Row 1** (top) | GoOnline: nombres de pista. Modo Pan: etiquetas ("Pan    "). Post-GoOnline: vacía. |
| 56–111 | **Row 2** (bottom) | GoOnline: vacía o valores. Post-GoOnline: nombres de pista actualizados. |

Canal N en row 1 → offset `N×7` (N = 0–7). Canal N en row 2 → offset `56 + N×7`.  
**Ejemplo:** canal 3 row 1 → offset 21. Canal 3 row 2 → offset 77.

---

**⚠️ Comportamiento de Logic según momento — crítico para el parser S3:**

| Momento | Row 1 (offsets 0–55) | Row 2 (offsets 56–111) | S3 actual |
|---------|---------------------|----------------------|-----------|
| **GoOnline #3** (t≈2471ms) + cualquier actualización normal | **Nombres de pista** (7 chars, truncados) | Valores numéricos (fader, pan) | ✅ Procesa row 1 → S2 muestra nombres |
| **Modo Pan** (cualquier momento) | Etiquetas parámetro: "Pan    ", "PanSpr " | Valores: "0      ", "111 o  " | ✅ Row 1 capturada; Row 2 ignorada (correcto) |
| **Modo plugin/Atmos** (especial) | Vacía (56 × 0x20) | Nombres de parámetro del plugin ("Angle  ", "LFE    ", "Spread ") | ❌ Row 1 vacía → S3 borra nombre; Row 2 ignorada |
| **Borrado** | 56 espacios | Vacía | ✅ Borra nombres correctamente |

> **Bug B2 (2026-05-20):** Cuando Logic muestra parámetros de plugin en row 2 (modo Atmos, spatial audio, inserts), `if (offset >= 56) break;` en `MIDIProcessor.cpp` línea 373 impide que S3 los procese. Caso menos frecuente. Ver sección §7.

---

**SysEx capturados — GoOnline normal (2026-05-20 07:45:16):**

Logic envía a P4 y al Extender simultáneamente con 119 bytes cada uno (offset=0):

```
F0 00 00 66 14 12 00  [112 bytes: row1 + row2]  F7
```

**P4 Master — 8 canales (07:45:16.079):**

```
Row 1 — nombres (offsets 0–55):
  Ch1 (0–6):   41 75 64 6F 31 35 20  "Audo15 "    (Audio 15, truncado)
  Ch2 (7–13):  41 75 64 69 6F 54 20  "AudioT "
  Ch3 (14–20): 42 61 73 65 20 20 20  "Base   "
  Ch4 (21–27): 41 75 64 69 6F 32 20  "Audio2 "
  Ch5 (28–34): 4E 6F 20 20 20 20 20  "No     "
  Ch6 (35–41): 6E 61 74 68 6C 65 20  "nathle "    (Nathalie, truncado)
  Ch7 (42–48): 56 4F 5A 20 34 20 20  "VOZ 4  "
  Ch8 (49–55): 61 6C 65 78 20 20 20  "alex   "

Row 2 — valores fader/pan (offsets 56–111):
  Ch1 (56–62):  2B 36 33 20 20 20 20  "+63    "
  Ch2 (63–69):  30 20 20 20 20 20 20  "0      "
  Ch3 (70–76):  2B 34 20 20 20 20 20  "+4     "
  Ch4 (77–83):  2D 31 20 20 20 20 20  "-1     "
  Ch5 (84–90):  20 20 20 20 20 20 20  "       "    (sin valor)
  Ch6 (91–97):  2D 31 20 20 20 20 20  "-1     "
  Ch7 (98–104): 2D 34 39 20 20 20 20  "-49    "
  Ch8 (105–111):30 20 20 20 20 20 20  "0      "
```

**S3 Extender — 8 canales (07:45:16.115):**

```
Row 1 — nombres (offsets 0–55):
  Ch1: 53 6F 6C 65 72 20 20  "Soler  "
  Ch2: 4C 6F 70 65 72 20 20  "Loper  "
  Ch3: 54 72 6E 71 69 6C 20  "Trnqil "    (Tranquil, truncado)
  Ch4: 70 69 61 6E 6F 20 20  "piano  "
  Ch5: 41 75 64 6F 31 32 20  "Audo12 "
  Ch6: 54 72 54 65 42 65 20  "TrTeBe "
  Ch7: 44 57 59 53 6C 65 20  "DWYSle "
  Ch8: 49 6E 73 74 20 34 20  "Inst 4 "

Row 2 — valores (offsets 56–111):
  Ch1: 2B 32 35 20 20 20 20  "+25    "
  Ch2: 2B 33 20 20 20 20 20  "+3     "
  Ch3: 2D 33 34 20 20 20 20  "-34    "
  Ch4: 2D 36 34 20 20 20 20  "-64    "
  Ch5: 2B 36 33 20 20 20 20  "+63    "
  Ch6: 30 20 20 20 20 20 20  "0      "
  Ch7: 30 20 20 20 20 20 20  "0      "
  Ch8: 30 20 20 20 20 20 20  "0      "
```

> Los valores de row 2 son los niveles de fader en dB (ej: `+63` = canal muy activo, `-64` = casi a mínimo). Logic muestra estos valores en el scribble strip inferior de la superficie física.

---

**SysEx capturado — modo Atmos/plugin especial (2026-05-20 07:36:59, al Extender):**

```
F0 00 00 66 14 12 00  [116 bytes]  F7
```

Este SysEx tenía row 1 completamente vacía y row 2 con nombres de parámetros de audio espacial:
`"Angle  "`, `"Divers "`, `"LFE    "`, `"Spread "`, `""`, `" CStrip"`, `" Ang/Dv"` + extra `" X/Y"`.

Interpretación: Logic estaba en un modo de plugin (Atmos/spatial) donde los faders controlan parámetros espaciales. En ese modo, row 1 se vacía y row 2 muestra los nombres de los parámetros del plugin. S3 borra los nombres al recibir row 1 vacía.

> El parser S3 actual maneja correctamente el caso normal (GoOnline). El bug B2 solo afecta este modo especial.

> Los 4 bytes extra (`20 58 2F 59` = " X/Y") van más allá del buffer de 112 posiciones — Logic puede exceder el LCD físico; el parser debe ignorar offsets ≥ 112.

---

**Logic usa 0x12 para múltiples tipos de contenido** (confirmado MIDI Monitor 2026-05-20):

| Situación | Row usada | Ejemplo observado |
|-----------|-----------|-------------------|
| GoOnline — nombres Track | Row 1 | `"GUITAR "`, `"-      "` |
| Actualización post-GoOnline | Row 2 | `"Angle  "`, `"Divers "`, `"LFE    "` |
| Modo Pan — etiquetas | Row 1 | `"Pan    "`, `"PanSpr "`, `"-      "` |
| Modo Pan — valores | Row 2 | `"0      "`, `"111 o  "` |
| Mensaje de estado | Ambas rows | `"El modo"` `" Write "` `"borra v"` … |
| Borrado | Row 1 offset 0 | 56 × `0x20` |

---

**Volcado completo vs. actualización parcial:**

- **Volcado completo** (offset=0, hasta 112+ bytes): al GoOnline o cambio de modo. Logic envía ambas rows de una vez.
- **Actualización parcial** (offset específico, pocos bytes): al ajustar un parámetro o renombrar una pista.

```
# Volcado completo al entrar en modo Pan (119 bytes):
F0 00 00 66 14 12 00  <56 bytes row1>  <56 bytes row2>  F7

# Actualización parcial — solo canal 4 row1 (14 bytes):
F0 00 00 66 14 12 15  50 61 6E 53 70 72  F7
                  ^   "PanSpr" (6 chars)
                  offset=21 (canal 4, row 1)

# Actualización parcial — nombre canal 3 en row 2 (9 bytes):
F0 00 00 66 14 12 4D  4C 46 45 20 20 20 20  F7
                  ^   "LFE    "
                  offset=77 (canal 3, row 2: 56 + 3×7 = 77)
```

S3 reconstruye el nombre a partir de actualizaciones parciales y llama `rs485.setTrackName(canal, nombre)` para enviarlo al S2 correspondiente vía RS485.

---

**Regla de diseño — qué ve S2:**

> **S2 solo recibe y muestra nombres de pista (row 1).** Nunca valores de fader/pan (row 2), nunca etiquetas de parámetro de plugin.

La responsabilidad del parser S3 es filtrar el buffer 0x12 y extraer únicamente los nombres de row 1 antes de enviarlos por RS485. Row 2 se descarta siempre.

**Implementación actual (`MIDIProcessor.cpp` case 0x12):**

```cpp
for (int i = 0; i < text_len; i++) {
    byte offset = startOffset + i;
    if (offset >= 56) break;          // row 2 descartada — solo row 1 llega a S2
    nameBufs[offset / 7][offset % 7] = (char)payload[6 + i];
    nameChanged[offset / 7] = true;
}
// ...
rs485.setTrackName(t + 1, nameBufs[t]);  // solo row 1 va por RS485 a S2
```

El `break` en `offset >= 56` es correcto para esta regla. El problema de **bug B2** no es que los valores lleguen a S2 (no llegan), sino que en modo Atmos/plugin Logic envía row 1 vacía → S3 borra los nombres de S2. Fix pendiente: ignorar actualizaciones donde row 1 es todo espacios, conservar el nombre previo.

Caracteres ASCII estándar. Caracteres de timecode con `MACKIE_CHAR_MAP[64]` (sección §4.9).

---

### 4.3 Display de Asignación (SysEx 0x11)

```
Logic → S3:   F0 00 00 66 14 11 <char1> <char2> F7
```

2 caracteres que Logic muestra en el display de asignación de la superficie (ej: "PT", "EQ", "CH"). S3 lo almacena en `assignmentString`. Actualmente sin pantalla en S3 → variable guardada como stub.

---

### 4.4 AllFadersToMinimum (SysEx 0x61)

```
Logic → S3:   F0 00 00 66 14 61 F7
```

Logic pide que todos los faders bajen a 0. **No es GoOffline.** Se envía durante banco de canales o antes de ciertos cambios de modo.

> ⚠️ **BUG PENDIENTE (2026-05-18):** El handler actual hace `g_logicConnected = 0`, lo que para el RS485 y causa que S2 vuelva a pantalla de inicio. Fix aprobado pero no implementado aún: enviar `setFaderTarget(0)` a todos los slaves sin tocar `g_logicConnected`.

**Comportamiento correcto esperado:** S3 envía target = 0 a todos los S2, motores bajan, S3 permanece conectado.

---

### 4.5 VU Meters en Bloque (SysEx 0x72)

```
Logic → S3:   F0 00 00 66 14 72 <byte0>..<byte7> F7
```

8 bytes, uno por canal. Cada byte codifica canal + nivel:
- Bits 7-4 → índice de canal (0-7)
- Bits 3-0 → nivel de señal:
  - `0x00` a `0x0B` → -inf a 0 dBFS (12 escalones)
  - `0x0E` → clip activo
  - `0x0F` → limpiar clip

S3 normaliza y llama `rs485.setVuLevel(canal, valor_0-127)`.

---

### 4.6 Modo de Automatización (SysEx 0x0E)

```
Logic → S3:   F0 00 00 66 14 0E <canal> <modo> F7
```

| Modo | Valor |
|------|-------|
| Off | 0 |
| Read | 1 |
| Write | 2 |
| Trim | 3 |
| Touch | 4 |
| Latch | 5 |

S3 almacena en `g_channelAutoMode[]` y propaga al S2 vía flags RS485.

---

### 4.7 Faders — Pitch Bend (0xEx)

```
Logic → S3:   Ex <LSB> <MSB>      x = canal MIDI 0-8
```

| Canal | Función |
|-------|---------|
| 0-7 | Faders de canal 1-8 |
| 8 | Fader master (no se envía a S2) |

**Rango real de Logic (confirmado con MIDI monitor, 2026-05-18):**

| Posición | Valor signed (monitor) | Valor raw unsigned |
|----------|------------------------|-------------------|
| Mínimo (fondo) | -8192 | 0 |
| Máximo (tope) | 6653 | 14845 |

> Logic NO usa el rango MIDI completo 0–16383. Span real: 6653 − (−8192) = **14845** (`LOGIC_PITCHBEND_MAX` en `config.h`).

S3 pasa el valor raw directamente a `rs485.setFaderTarget(canal+1, valor)`. `setFaderTarget` gestiona internamente el mapeo a ADC calibrado del S2 (0–27000).

**Deadband:** ±80 cuentas (~0,5%). Solo se envía a S2 si el cambio supera el umbral. Evita retroalimentación de ruido ADC.

**Detección de desconexión automática:**
- Si 9 o más canales envían valor 0 simultáneamente (dentro de 150 ms) → S3 interpreta que Logic ha cerrado y pasa a DISCONNECTED
- Protección: los primeros 1500 ms tras conexión se ignoran (grace period)

---

### 4.8 VU Meters por Canal — Channel Pressure (0xDx)

**Formato MCU (canal 0):**
```
Logic → S3:   D0 <byte>
```
Byte = `(índice_canal << 4) | nivel_vu` — canal en bits superiores, nivel en inferiores.

**Formato alternativo (canales 1-7):**
```
Logic → S3:   Dx <valor_0-127>      x = canal 1-7
```
Valor normalizado directo.

S3 → `rs485.setVuLevel(canal, valor_normalizado_0-127)`.

---

### 4.9 VPots y Timecode — Control Change (0xBx)

**VPot (encoders de canal):** CC 48-55 en canal 0 ó 15

| CC | VPot |
|----|------|
| 48 | Canal 1 |
| ... | ... |
| 55 | Canal 8 |

Byte de valor: `bit6=centro, bits5-4=modo, bits3-0=posición`

S3 → `rs485.setVPotValue(canal, valor_raw)`.

**Timecode / Beats display:** CC 64-73 en canal 0 ó 15

- `digit_index = 73 - CC` (CC73=dígito 0, CC64=dígito 9)
- Bits 5-0 del valor = carácter (mapeado por `MACKIE_CHAR_MAP`)
- Bit 6 del valor = indicador de punto

S3 almacena en `timeCodeChars_clean[]` / `beatsChars_clean[]`. Sin pantalla activa en S3 → stub.

---

### 4.10 Botones de Canal — Note On/Off (0x9x / 0x8x)

```
Logic → S3:   9x <nota> <vel>      vel 127 = on, vel 0 = off
              8x <nota> <vel>      siempre off
```

**Botones de canal (notas 0-31):**

| Rango de notas | Función | Canal |
|----------------|---------|-------|
| 0-7 | REC arm | Canal 1-8 |
| 8-15 | SOLO | Canal 1-8 |
| 16-23 | MUTE | Canal 1-8 |
| 24-31 | SELECT | Canal 1-8 |

S3 acumula los estados REC+SOLO+MUTE+SELECT del canal afectado y llama `rs485.setFlags(slave, flags)`.

**Automatización (notas 74-79), solo si hay canal seleccionado:**

| Nota | Modo |
|------|------|
| 74 | Read |
| 75 | Write |
| 76 | Trim |
| 77 | Touch |
| 78 | Latch |
| 79 | Off |

S3 → `rs485.setAutoMode(canal_seleccionado, modo)`.

**Transport LEDs:** todas las notas pasan por `Transporte::setLedByNote()`. Si coinciden con notas de transporte (0x5B-0x5F), encienden/apagan el LED físico correspondiente. Ver `docs/Transport.md`.

---

### 4.11 Fader Touch Sense (SysEx 0x0A)

```
Logic → S3:   F0 00 00 66 14 0A 01 F7
```

Habilita el modo touch en los faders. Enviado en cada iteración GoOnline. S3 lo **ecoa inmediatamente** sin procesamiento adicional.

---

### 4.12 Button Enable Mask (SysEx 0x0B)

```
Logic → S3:   F0 00 00 66 14 0B 0F F7
```

`0x0F` = bits 0-3 activos → habilita los 4 botones por canal (REC, SOLO, MUTE, SELECT). Enviado en cada iteración GoOnline. S3 lo **ecoa inmediatamente**.

---

### 4.13 VPot Ring LEDs (SysEx 0x20)

```
Logic → S3:   F0 00 00 66 14 20 <canal> <valor> F7
```

Controla el anillo LED del encoder VPot de cada canal. Enviado en bloques de 8 (canales 0-7) en cada iteración GoOnline.

| Bits del valor | Significado |
|----------------|-------------|
| 7-6 | Modo: 0=single dot, 1=boost/cut, 2=fill left, 3=spread |
| 4-0 | Posición (0-11) |

S3 lo **ecoa inmediatamente**. Sin pantalla activa → stub.

> En las iteraciones #1 y #2 el valor es `0x07` (posición 7, modo single). En la iteración #3 llegan valores reales del proyecto.

---

## 5. MENSAJES S3 → LOGIC

### 5.1 Posición de Fader — Pitch Bend

```
S3 → Logic:   Ex <LSB> <MSB>      x = canal MIDI 0-7
```

**Origen:** `faderPos` de S2, suavizado con filtro EMA en RS485.  
**Condición de envío:**  
- `touchState = 1` (S2 detecta toque o movimiento)  
- El flag `SLAVE_FLAG_CALIB_SENDING` no está activo (durante calibración no se mandan valores a Logic)
- El valor ha cambiado respecto al último enviado (filtro send-only-on-change, array `lastSentPb[]`)

**Fórmula:** `pb = (faderPos × LOGIC_PITCHBEND_MAX) / 27000`  → `pb = (faderPos × 14845) / 27000`

---

### 5.2 Estado de Botones — Note On/Off

```
S3 → Logic:   90 <nota> 7F    (botón presionado)
S3 → Logic:   80 <nota> 00    (botón suelto)
```

Enviado cuando cambia `ch.buttons` respecto a `ch.prevButtons` en la respuesta RS485. Mismo mapeo de notas que la sección 4.10.

---

### 5.3 Encoder — Control Change

```
S3 → Logic:   B0 <CC> <valor>
```

| CC | Encoder |
|----|---------|
| 16 | Canal 1 |
| ... | ... |
| 23 | Canal 8 |

**Codificación de dirección:**
- Giro CW (+delta): valor 1-62 (número de ticks)
- Giro CCW (-delta): valor 64-127 (64 + número de ticks)

---

## 6. STUBS — IMPLEMENTADO PERO SIN PANTALLA

Estos mensajes se reciben y procesan correctamente, pero su efecto visual está pendiente de pantalla en S3:

| Mensaje | Variable | Pendiente |
|---------|----------|-----------|
| SysEx 0x12 | `trackNames[]` | Display scribble strip |
| SysEx 0x11 | `assignmentString` | Display asignación |
| CC 64-73 | `timeCodeChars_clean[]`, `beatsChars_clean[]` | Display timecode |
| Channel Pressure | `vuLevels[]`, `vuClipState[]` | Display VU meters |

---

## 7. BUGS CONOCIDOS / PENDIENTES

| # | Descripción | Fichero | Estado |
|---|-------------|---------|--------|
| B1 | SysEx 0x61 (`AllFadersToMinimum`) corta RS485 incorrectamente | `MIDIProcessor.cpp` case 0x61 | ⚠️ Pendiente |
| B2 | Modo plugin/Atmos: Logic envía row 1 vacía → S3 borra nombres de S2 | `MIDIProcessor.cpp` case 0x12 L371 | ⚠️ Pendiente — fix: ignorar update si row 1 es todo espacios |
| — | Detección desconexión requiere 9 canales a 0 (threshold hardcoded) | `MIDIProcessor.cpp` L27 | Revisar si aplica con <9 faders |

### B2 — Detalle: borrado de nombres en modo plugin (2026-05-20)

**Síntoma:** Al entrar en modo Atmos/plugin/spatial audio en Logic, los nombres de pista desaparecen de los displays S2.

**Causa:** Logic envía SysEx 0x12 con row 1 completamente vacía (56 × `0x20`) y parámetros del plugin en row 2. El parser S3 procesa row 1 → sobreescribe `trackNames[]` con cadenas vacías → `rs485.setTrackName()` envía string vacío → S2 limpia su display.

**Fix propuesto (pendiente):** En case 0x12, antes de llamar `rs485.setTrackName()`, comprobar si el nombre extraído es todo espacios. Si es vacío/espacios → no sobreescribir → S2 conserva el nombre anterior.

```cpp
// Guardia propuesta (pendiente implementar):
trimRight(nameBufs[t]);
if (nameBufs[t][0] == '\0') continue;   // ← ignorar si vacío → S2 conserva nombre
if (trackNames[t] == nameBufs[t]) continue;
trackNames[t] = String(nameBufs[t]);
rs485.setTrackName(t + 1, nameBufs[t]);
```

**Riesgo:** BAJO — cambio local en S3, no afecta RS485 ni Motor. Requiere validar que el borrado intencional (track eliminada) sigue funcionando (Logic enviaría `"-      "`, no espacios puros).

---

## 8. REFERENCIAS

- `docs/Transport.md` — Botones transporte (RW/FF/STOP/PLAY/REC), LEDs, handshake parcial
- `docs/RS485.md` — Protocolo binario S3↔S2, paquetes MasterPacket/SlavePacket
- `docs/FADER.md` — Rango ADC S2, calibración, mapeo
- `src/midi/MIDIProcessor.cpp` — Implementación completa
- `src/config.h` — `DEVICE_FAMILY`, `DISCONNECT_THRESHOLD`, `CONNECT_GRACE_MS`
