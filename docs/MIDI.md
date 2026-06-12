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

---

### 3.4 — Secuencia arranque dual P4 + S3 con MIDI Monitor (2026-05-24 12:35:38)

**Captura real** de Logic Pro conectando simultáneamente con `iMakie-P4-Master` y `iMakie-Extender` (S3). Esta sección es la fuente más completa y fiable del protocolo real que Logic emite.

---

#### 3.4.1 Sondeo multi-familia — Logic prueba 5 familias Mackie

Logic no sabe qué tipo de hardware está conectado. Sondea **5 familias Mackie distintas** en ambos dispositivos:

| Familia | Dispositivo Mackie | Cmd enviado | Esperado |
|---------|-------------------|-------------|---------|
| `0x10` | Logic Control (Apple original) | `0x00` (probe) | `0x01` response |
| `0x11` | Logic Control XT | `0x00` | `0x01` response |
| `0x14` | Mackie Control Universal (MCU) | `0x00` + `0x13` | `0x01` + `0x14` response |
| `0x15` | Mackie Control Universal XT | `0x00` + `0x13` | `0x01` + `0x14` response |
| `0x17` | Mackie C4 (plugin controller) | `0x00` + `0x13` | no response esperada |

**Mensajes observados (a ambos dispositivos simultáneamente):**
```
F0 00 00 66 10 00 F7   ← probe familia 0x10
F0 00 00 66 11 00 F7   ← probe familia 0x11
F0 00 00 66 17 00 F7   ← probe familia 0x17
F0 00 00 66 17 13 00 F7  ← versión familia 0x17
F0 00 00 66 14 00 F7   ← probe familia 0x14  ← responde P4 y S3
F0 00 00 66 14 13 00 F7  ← versión familia 0x14  ← responde P4 y S3
F0 00 00 66 15 00 F7   ← probe familia 0x15
F0 00 00 66 15 13 00 F7  ← versión familia 0x15
```

Esta secuencia se repite **3 veces** — Logic reintenta si no recibe respuesta satisfactoria en el tiempo esperado.

**⚠️ Bug crítico — P4 y S3 responden a CUALQUIER familia:**

El código actual en `processMackieSysEx()` responde a cmd `0x00` y `0x13` ANTES de comprobar la familia:

```cpp
if (command == 0x00) { sendMIDIBytes(reply, ...); return; }  // ← responde a familia 0x10, 0x11, 0x15, 0x17
if (command == 0x13) { sendMIDIBytes(reply, ...); return; }
if (device_family != 0x14) return;
```

Cuando Logic envía `F0 00 00 66 10 00 F7` (probe a familia 0x10), P4 y S3 responden identificándose como familia `0x14`. Logic recibe respuestas inesperadas a sus probes de otras familias, lo que puede causar confusión y contribuye al **loop de reintento del handshake** (ver 3.4.3).

**Fix:** Mover el guard `if (device_family != 0x14) return;` AL INICIO de `processMackieSysEx()`, antes de manejar `0x00` y `0x13`.

---

#### 3.4.2 Secuencia de inicialización completa (por dispositivo, tras GoOnline exitoso)

Tras reconocer el dispositivo, Logic envía esta secuencia en ráfaga:

```
F0 00 00 66 14 21 01 F7                 ← GoOnline (cmd 0x21, data=0x01)
F0 00 00 66 14 20 00 07 F7              ← cmd 0x20, ch=0, val=7
F0 00 00 66 14 20 01 07 F7              ← cmd 0x20, ch=1, val=7
... (×8 para ch=0..7)
F0 00 00 66 14 0A 01 F7                 ← cmd 0x0A, val=0x01
F0 00 00 66 14 0E 00 03 F7              ← cmd 0x0E, ch=0, val=0x03
F0 00 00 66 14 0E 01 03 F7              ← cmd 0x0E, ch=1, val=0x03
... (×9 para ch=0..8, incluye canal master)
F0 00 00 66 14 0C 00 F7                 ← cmd 0x0C, val=0x00
F0 00 00 66 14 0B 0F F7                 ← cmd 0x0B, val=0x0F
F0 00 00 66 14 12 00 [111×0x20] F7      ← LCD Write: borra display completo (espacios)
F0 00 00 66 14 72 07 07 07 07 07 07 07 07 F7  ← VU Meter batch: init a 0x07
```

**Decodificación de cada comando:**

| Cmd | Formato | Valor típico | Significado | Estado en P4 |
|-----|---------|-------------|-------------|-------------|
| `0x21` | `21 01` | — | GoOnline — Logic conectado, surface operativa | ✅ Procesado |
| `0x20` | `20 [ch] [val]` | ch=0..7, val=07 | **Fader Touch Sensitivity** — sensibilidad táctil del fader, 0=mínima, 7=máxima | ❌ No procesado en P4 |
| `0x0A` | `0A [val]` | val=01 | **Touch sense habilitado** (01=on) — activa detección de toque en faders | ❌ No procesado en P4 |
| `0x0E` | `0E [ch] [val]` | ch=0..8, val=03 | **Channel Auto Mode** — 0x03=Touch. **9 canales, incluye master (ch=8)** | ✅ Procesado (pero P4 ignora ch=8) |
| `0x0C` | `0C [val]` | val=00 | **Meter mode** — modo de los VU meters (0=peak) | ❌ No procesado en P4 |
| `0x0B` | `0B [val]` | val=0F | **Button enable mask** — máscara de botones activos (0x0F = REC/SOLO/MUTE/SEL habilitados) | ❌ No procesado en P4 |
| `0x12` | `12 [offset] [data]` | offset=0, 111 bytes 0x20 | LCD Write — limpia display completo con espacios antes del volcado real | ✅ Procesado |
| `0x72` | `72 [8 bytes]` | todos=0x07 (init) | VU Meter batch — al init todos a 0x07 (ver nota), luego valores reales | ✅ Procesado |

**Nota sobre `0x20` (Fader Touch Sensitivity):**
`F0 00 00 66 14 20 [ch] [sensitivity_0_7] F7` — inicializa la sensibilidad táctil de cada fader. Value=7 = máxima sensibilidad (toque más ligero registrado). P4 no procesa este comando actualmente — afecta a cómo Logic calcula el "touch" del fader en su lógica de automación.

**Nota sobre `0x0E` ch=8 (master fader auto mode):**
Logic envía auto mode para 9 canales (0-8), siendo ch=8 el fader master. El código P4 solo procesa ch=0..7:
```cpp
g_channelAutoMode[ch] = value;  // arrays de 8 elementos — ch=8 sería out of bounds o ignorado
```
El modo de grabación del master fader (ch=8) no se captura. Comportamiento deseado actual: ✅ P4 muestra el auto mode de la pista activa en display (confirmado en hardware 2026-05-24).

**Nota sobre `0x72` init a 0x07:**
En el arranque, todos los VU se inicializan a `0x07`. En el contexto de VU Meter batch (4 bits por canal, 0-11=niveles, 0xC-0xD=sobre, 0xE=clip, 0xF=clear clip), `0x07` = nivel 7/11 (~64%). Esto es un reset genérico a valor central antes del volcado de niveles reales. No confundir con el estado operacional donde `0x07` en Channel Pressure tiene otro significado.

---

#### 3.4.3 Bug S3 — Loop de reintento del handshake (3 intentos fallidos en 120ms)

**Observado en captura 2026-05-24.** P4 completa el handshake en el primer intento. S3 falla 3 veces antes de estabilizarse, con un total de ~3 segundos hasta la conexión exitosa.

**Timeline completo:**

```
12:35:38.664  Logic → P4 + S3:  probe 0x00 (familia 0x14)
12:35:38.666  Logic → P4:       GoOnline #1 + init completo ← P4 OK en primer intento
12:35:38.666  Logic → S3:       GoOnline #1 + init completo
12:35:38.749  Logic → S3:       cmd 0x13 ×17 veces (!)  ← Logic retrying firmware query
12:35:38.750  Logic → S3:       0x0A/0x0C/0x0B ×4 ciclos (garbage init)
12:35:38.750  Logic → S3:       GoOnline #2 + init completo ← segundo intento fallido
12:35:38.782  Logic → S3:       0x0A/0x0C/0x0B ×5 ciclos más
12:35:38.783  Logic → S3:       GoOnline #3 + init completo ← tercer intento fallido
              [pausa ~600ms]
12:35:39.382  Logic → S3:       0x12 (LCD clear)
12:35:39.683  Logic → P4:       0x0E ×9 (auto mode update)
12:35:39.782  Logic → S3:       0x0E ×9 (auto mode update)
12:35:40.351  Logic → P4 + S3:  0x12 "LogicPro Trial -" ← broadcast global
12:35:41.641  Logic → P4 + S3:  0x12 [16 espacios] ← clear parcial broadcast
12:35:41.659  Logic → P4:       GoOnline FINAL + init + nombres reales ← OK
12:35:41.698  Logic → S3:       GoOnline FINAL + init + nombres reales ← OK (~3s después)
```

**Causa probable:** S3 no responde al GoOnline (0x21) con suficiente rapidez. La respuesta requerida es el echo `F0 00 00 66 14 21 01 F7`. Si S3 está bloqueado en la tarea RS485 en ese momento, el echo llega tarde, Logic hace timeout y reintenta.

**Agravante — Bug de familia:** S3 responde a los probes `0x00` y `0x13` de otras familias (0x10, 0x15…), lo que genera respuestas inesperadas que Logic interpreta como señales de nuevos dispositivos y reinicia el sondeo.

**Resultado visible:** Durante ~3 segundos tras conectar, la pantalla de S3 está sin datos. Los 17× `cmd 0x13` consecutivos son Logic en estado de confusión, preguntando repetidamente la versión de firmware al S3.

**Impacto en P4:** P4 también se reinicializa al final (GoOnline a 12:35:41.659 es ~3 segundos después del primero). La segunda inicialización es la que contiene los nombres reales de pista.

---

#### 3.4.4 Broadcast global "LogicPro Trial -"

```
12:35:40.351  → P4:  F0 00 00 66 14 12 00 4C 6F 67 69 63 50 72 6F 20 54 72 69 61 6C 20 2D F7
12:35:40.351  → S3:  F0 00 00 66 14 12 00 4C 6F 67 69 63 50 72 6F 20 54 72 69 61 6C 20 2D F7
```

Decodificado: `"LogicPro Trial -"` (16 bytes, offset 0 = inicio de LCD row 0).

Logic envía su versión/licencia a **ambos dispositivos simultáneamente** via SysEx `0x12` (LCD Write). Esto es un **mensaje global broadcast** — el único tipo que Logic envía a master y extender a la vez sin diferenciación de banco.

**Implicación:** Si P4 muestra este texto en el display durante 1-2 segundos al arrancar, es comportamiento correcto de Logic. El LCD se borrará cuando llegue el volcado real de nombres de pista.

---

#### 3.4.5 Actualizaciones LCD parciales (SysEx 0x12 con offset variable)

Tras la inicialización completa, Logic puede enviar `0x12` con **offset distinto de 0** para actualizar solo parte del LCD:

```
12:35:43.138  → S3:  F0 00 00 66 14 12 00 56 6F 6C 75 6D 65 6E 20 20 20 20 20 F7
                     offset=0x00, data="Volumen    " (12 bytes) — solo ch1 row0

12:35:43.147  → S3:  F0 00 00 66 14 12 38 2B 30 2C 30 20 64 42 20 F7
                     offset=0x38=56, data="+0,0 dB " (8 bytes) — ch1 row1 (valor fader)
```

El LCD Mackie MCU tiene 112 posiciones (2 filas × 56 chars). El offset indica la posición de inicio en ese buffer lineal:
- offset `0x00..0x37` (0-55) = row 0 (nombres), posición dentro de esa fila
- offset `0x38..0x6F` (56-111) = row 1 (valores), posición dentro de esa fila

P4 ya soporta esto — `processMackieSysEx case 0x12` usa el offset para escribir en la posición correcta del array `trackNames`.

```
12:35:45.383  → S3:  F0 00 00 66 14 12 00 53 6F 6C 65 72 20 20 4C 6F 70 65 72 F7
                     "Soler  Loper" — actualiza ch1+ch2 row0

12:35:45.395  → S3:  F0 00 00 66 14 12 38 2D 34 32 20 20 20 20 30 F7
                     "-42    0" — actualiza ch1+ch2 row1
```

---

#### 3.4.6 Auto Mode `0x0E` — Confirmación en hardware P4 (2026-05-24)

El comando `0x0E [ch] [mode]` llega a P4 cada vez que Logic cambia el modo de automación de una pista. Modos observados:

| Valor | Nombre Mackie | Equivalente Logic Pro |
|-------|--------------|----------------------|
| 0x00 | Off | Off |
| 0x01 | Read | Read |
| 0x02 | Write | Write |
| 0x03 | Touch | Touch ← valor al arranque |
| 0x04 | Latch | Latch |
| 0x05 | Trim | Trim |

**Confirmado en hardware:** P4 muestra en pantalla el modo de grabación de la pista activa (✅ comportamiento deseado). El display de P4 reacciona correctamente a `0x0E`. Esto se gestiona en `MIDIProcessor.cpp` vía `g_channelAutoMode[ch] = value` y `needsButtonsRedraw = true`.

**Nota:** Logic envía `0x0E` para 9 canales (ch=0..8) en cada actualización. El ch=8 es el fader master — P4 actualmente lo ignora (no hay slot 8 en los arrays de 8 elementos). No es un bug funcional hoy, pero hay que tenerlo en cuenta cuando se amplíe a 16 canales.

---

#### 3.4.7 Resumen: qué mensajes llegan a P4 vs S3

| Mensaje | → P4 | → S3 | Notas |
|---------|------|------|-------|
| Probe `0x00` (todas familias) | ✅ | ✅ | Broadcast — ambos lo reciben |
| Versión `0x13` (todas familias) | ✅ | ✅ | Broadcast — ambos lo reciben |
| GoOnline `0x21 01` | ✅ | ✅ | Separado por dispositivo |
| Fader Touch Sensitivity `0x20` | ✅ | ✅ | Por dispositivo, 8 canales |
| Touch Enable `0x0A 01` | ✅ | ✅ | Por dispositivo |
| Auto Mode `0x0E` | ✅ | ✅ | Por dispositivo, 9 canales |
| Meter Mode `0x0C` | ✅ | ✅ | Por dispositivo |
| Button Mask `0x0B` | ✅ | ✅ | Por dispositivo |
| LCD Clear `0x12` (espacios) | ✅ | ✅ | Por dispositivo |
| VU Init `0x72` | ✅ | ✅ | Por dispositivo |
| "LogicPro Trial -" `0x12` | ✅ | ✅ | **Broadcast global** — idéntico a ambos. P4 lo escribe en `trackNames[0]` y lo reenvía a S2 slave 1 via RS485. El filtrado de este string basura **ocurre en S2, no en P4** — S2 decide si muestra el nombre recibido. Idealmente P4 debería filtrarlo antes de forwarding. |
| LCD Clear parcial `0x12` 16 spaces | ✅ | ✅ | **Broadcast global** — misma situación: P4 reenvía a S2 sin filtrar. S2 filtra. |
| Nombres reales `0x12` 119 bytes | ✅ tracks 1-8 | ✅ tracks 9-16 | **Datos distintos** por banco |
| VU real `0x72` | ✅ | ✅ | Datos distintos por banco |
| GoOffline `0x0F` | ✅ | ✅ | Por dispositivo |

---

#### 3.4.8 Capture completo MIDI Monitor (2026-05-24 12:35:38)

Captura raw sin modificar. Referencia canónica para auditoría futura del protocolo.

```
12:35:38.664    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.664    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.666    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 01 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 02 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 03 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 04 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 05 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 06 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 07 07 F7
12:35:38.666    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 01 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 02 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 03 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 04 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 05 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 06 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 07 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 08 03 F7
12:35:38.666    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.666    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:38.666    To iMakie-P4-Master    SysEx    119 bytes   F0 00 00 66 14 12 00 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 F7
12:35:38.666    To iMakie-P4-Master    SysEx    15 bytes    F0 00 00 66 14 72 07 07 07 07 07 07 07 07 F7
12:35:38.666    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 01 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 02 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 03 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 04 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 05 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 06 07 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 07 07 F7
12:35:38.666    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 01 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 02 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 03 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 04 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 05 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 06 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 07 03 F7
12:35:38.666    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 08 03 F7
12:35:38.666    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.666    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:38.666    To iMakie-Extender     SysEx    119 bytes   F0 00 00 66 14 12 00 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 F7
12:35:38.666    To iMakie-Extender     SysEx    15 bytes    F0 00 00 66 14 72 07 07 07 07 07 07 07 07 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 10 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 10 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 11 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 11 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 17 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 17 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 17 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 17 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 14 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 13 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-Extender     SysEx    7 bytes     F0 00 00 66 15 00 F7
12:35:38.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.667    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 15 13 00 F7
12:35:38.749    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 13 00 F7  [×17]
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7  [0x0A/0C/0B ×4 ciclos]
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:38.750    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7  [×8]
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:38.750    To iMakie-Extender     SysEx    119 bytes   F0 00 00 66 14 12 00 20[×111] F7
12:35:38.750    To iMakie-Extender     SysEx    15 bytes    F0 00 00 66 14 72 07 07 07 07 07 07 07 07 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:38.750    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7  [×8]
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.750    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:38.782    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7  [0x0A/0C/0B ×5 ciclos]
12:35:38.783    To iMakie-Extender     SysEx    15 bytes    F0 00 00 66 14 72 07 07 07 07 07 07 07 07 F7
12:35:38.783    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:38.783    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7  [×8]
12:35:38.783    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:38.783    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:38.783    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:39.382    To iMakie-Extender     SysEx    119 bytes   F0 00 00 66 14 12 00 20[×111] F7
12:35:39.683    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:39.782    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:40.351    To iMakie-P4-Master    SysEx    24 bytes    F0 00 00 66 14 12 00 4C 6F 67 69 63 50 72 6F 20 54 72 69 61 6C 20 2D F7
12:35:40.351    To iMakie-Extender     SysEx    24 bytes    F0 00 00 66 14 12 00 4C 6F 67 69 63 50 72 6F 20 54 72 69 61 6C 20 2D F7
12:35:41.641    To iMakie-P4-Master    SysEx    24 bytes    F0 00 00 66 14 12 00 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 F7
12:35:41.641    To iMakie-Extender     SysEx    24 bytes    F0 00 00 66 14 12 00 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 F7
12:35:41.659    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:41.659    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7  [×8]
12:35:41.664    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:41.664    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:41.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:41.667    To iMakie-P4-Master    SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:41.667    To iMakie-P4-Master    SysEx    119 bytes   F0 00 00 66 14 12 00 41 75 64 69 6F 54 20 42 61 73 65 20 20 20 41 75 64 69 6F 32 20 4E 6F 20 20 20 20 20 6E 61 74 68 6C 65 20 56 4F 5A 20 34 20 20 61 6C 65 78 20 20 20 49 6E 73 74 20 34 20 30 20 20 20 20 20 20 2B 34 20 20 20 20 20 2D 31 20 20 20 20 20 20 20 20 20 20 20 20 2D 31 20 20 20 20 20 2D 34 39 20 20 20 20 30 20 20 20 20 20 20 30 20 20 20 20 20 F7
12:35:41.692    To iMakie-P4-Master    SysEx    15 bytes    F0 00 00 66 14 72 04 02 04 02 04 04 04 02 F7
12:35:41.698    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 21 01 F7
12:35:41.698    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 20 00 07 F7  [×8]
12:35:41.702    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0A 01 F7
12:35:41.703    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:41.705    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0C 00 F7
12:35:41.706    To iMakie-Extender     SysEx    8 bytes     F0 00 00 66 14 0B 0F F7
12:35:41.706    To iMakie-Extender     SysEx    119 bytes   F0 00 00 66 14 12 00 53 6F 6C 65 72 20 20 4C 6F 70 65 72 20 20 54 72 6E 71 69 6C 20 70 69 61 6E 6F 20 20 41 75 64 6F 31 32 20 54 72 54 65 42 65 20 44 57 59 53 6C 65 20 41 75 64 6F 31 35 20 2D 34 32 20 20 20 20 30 20 20 20 20 20 20 2D 33 34 20 20 20 20 2D 36 34 20 20 20 20 2B 36 33 20 20 20 20 30 20 20 20 20 20 20 30 20 20 20 20 20 20 2B 36 33 20 20 20 F7
12:35:41.731    To iMakie-Extender     SysEx    15 bytes    F0 00 00 66 14 72 02 04 02 02 04 04 04 04 F7
12:35:42.781    To iMakie-P4-Master    SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:42.791    To iMakie-Extender     SysEx    9 bytes     F0 00 00 66 14 0E 00 03 F7  [×9 ch=0..8]
12:35:43.138    To iMakie-Extender     SysEx    20 bytes    F0 00 00 66 14 12 00 56 6F 6C 75 6D 65 6E 20 20 20 20 20 F7
12:35:43.147    To iMakie-Extender     SysEx    16 bytes    F0 00 00 66 14 12 38 2B 30 2C 30 20 64 42 20 F7
12:35:45.383    To iMakie-Extender     SysEx    20 bytes    F0 00 00 66 14 12 00 53 6F 6C 65 72 20 20 4C 6F 70 65 72 F7
12:35:45.395    To iMakie-Extender     SysEx    16 bytes    F0 00 00 66 14 12 38 2D 34 32 20 20 20 20 30 F7
```

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

### 4.10.1 — Tabla exhaustiva de note numbers MCU (2026-06-10 19:09)

Referencia canónica de todos los botones físicos MCU y sus note numbers. Fuente: `doc/MackieControl.md` del repositorio del protocolo.

**Botones de canal (por canal 1–8):**

| Nota | Función |
|------|---------|
| 0–7 | REC/RDY Ch.1–8 |
| 8–15 | SOLO Ch.1–8 |
| 16–23 | MUTE Ch.1–8 |
| 24–31 | SELECT Ch.1–8 |
| 32–39 | V-SELECT (pulsación encoder) Ch.1–8 |

**Assignment (modo V-Pots):**

| Nota | Función |
|------|---------|
| 40 | ASSIGNMENT: TRACK |
| 41 | ASSIGNMENT: SEND |
| 42 | ASSIGNMENT: PAN/SURROUND |
| 43 | ASSIGNMENT: PLUG-IN |
| 44 | ASSIGNMENT: EQ |
| 45 | ASSIGNMENT: INSTRUMENT |

**Fader Banks:**

| Nota | Función |
|------|---------|
| 46 | FADER BANKS: BANK Left |
| 47 | FADER BANKS: BANK Right |
| 48 | FADER BANKS: CHANNEL Left |
| 49 | FADER BANKS: CHANNEL Right |

**Vista y display:**

| Nota | Función |
|------|---------|
| 50 | FLIP |
| 51 | GLOBAL VIEW |
| 52 | NAME/VALUE |
| 53 | SMPTE/BEATS |

**Funciones F1–F8:**

| Nota | Función |
|------|---------|
| 54–61 | F1–F8 |

**Global View (submenú):**

| Nota | Función |
|------|---------|
| 62 | GLOBAL VIEW: MIDI TRACKS |
| 63 | GLOBAL VIEW: INPUTS |
| 64 | GLOBAL VIEW: AUDIO TRACKS |
| 65 | GLOBAL VIEW: AUDIO INSTRUMENT |
| 66 | GLOBAL VIEW: AUX |
| 67 | GLOBAL VIEW: BUSSES |
| 68 | GLOBAL VIEW: OUTPUTS |
| 69 | GLOBAL VIEW: USER |

**Modificadores:**

| Nota | Función |
|------|---------|
| 70 | SHIFT |
| 71 | OPTION |
| 72 | CONTROL |
| 73 | CMD/ALT |

**Automation:**

| Nota | Función |
|------|---------|
| 74 | AUTOMATION: READ/OFF |
| 75 | AUTOMATION: WRITE |
| 76 | AUTOMATION: TRIM |
| 77 | AUTOMATION: TOUCH |
| 78 | AUTOMATION: LATCH |

**Utilities:**

| Nota | Función |
|------|---------|
| 79 | GROUP |
| 80 | UTILITIES: SAVE |
| 81 | UTILITIES: UNDO |
| 82 | UTILITIES: CANCEL |
| 83 | UTILITIES: ENTER |

**Edición:**

| Nota | Función |
|------|---------|
| 84 | MARKER |
| 85 | NUDGE |
| 86 | CYCLE |
| 87 | DROP |
| 88 | REPLACE |
| 89 | CLICK |
| 90 | SOLO (global) |

**Transporte:**

| Nota | Función |
|------|---------|
| 91 | REWIND |
| 92 | FAST FWD |
| 93 | STOP |
| 94 | PLAY |
| 95 | RECORD |

**Cursor y Jog:**

| Nota | Función |
|------|---------|
| 96 | CURSOR UP |
| 97 | CURSOR DOWN |
| 98 | CURSOR LEFT |
| 99 | CURSOR RIGHT |
| 100 | ZOOM |
| 101 | SCRUB |
| 102 | USER SWITCH A |
| 103 | USER SWITCH B |

**Fader Touch (detección táctil — solo salida superficie → DAW):**

| Nota | Función |
|------|---------|
| 104–111 | FADER TOUCH Ch.1–8 |
| 112 | FADER TOUCH MASTER |

**LEDs de estado (solo salida DAW → superficie):**

| Nota | Función |
|------|---------|
| 113 | SMPTE LED |
| 114 | BEATS LED |
| 115 | RUDE SOLO LIGHT |
| 116 | RELAY CLICK |

> **Sobre SHIFT:** el protocolo MCU no define un layer Shift a nivel de nota. La nota 70 (SHIFT) es un Note On/Off normal que Logic recibe y gestiona internamente. El firmware P4 puede gestionar Shift localmente (ver §9) sin enviar nota 70 a Logic, o puede reenviarlo y dejar que Logic maneje el doble función.

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

---

## 9. IMPLEMENTACIÓN P4 — PÁGINAS DE BOTONES (2026-06-10 19:09)

El P4 organiza sus 32 botones físicos en páginas (PG1, PG2). Cada botón envía una nota MCU en modo normal; en modo Shift (botón 26), puede enviar una nota diferente gestionada localmente en firmware.

### 9.1 Enfoque Shift — Local en firmware P4

El Shift **no se envía a Logic** (nota 70 nunca sale al DAW). El firmware P4 intercepta el estado del botón 26 y reinterpreta las notas de los demás botones cuando Shift está activo. Esto permite funciones custom no limitadas por el protocolo MCU estándar.

Botones que **no cambian con Shift:** navegación (BANK/CHAN), modificadores (CTRL/OPT/CMD), ENTER, controles de página (>>PG2, >>VU).

### 9.2 PG1 — Normal y Shift

| Key | Label Normal | Nota Normal | Label Shift | Nota Shift | Comentario |
|-----|-------------|-------------|-------------|------------|------------|
| 0 | TRACK | 0x28 (40) | MIDI TRK | 0x3E (62) | Global View: MIDI Tracks |
| 1 | PAN | 0x2A (42) | INPUTS | 0x3F (63) | Global View: Inputs |
| 2 | EQ | 0x2C (44) | AUDIO TR | 0x40 (64) | Global View: Audio Tracks |
| 3 | SEND | 0x29 (41) | AUD INS | 0x41 (65) | Global View: Audio Instrument |
| 4 | PLUG | 0x2B (43) | AUX | 0x42 (66) | Global View: Aux |
| 5 | INST | 0x2D (45) | BUSSES | 0x43 (67) | Global View: Busses |
| 6 | FLIP | 0x32 (50) | OUTPUTS | 0x44 (68) | Global View: Outputs |
| 7 | GLOB | 0x33 (51) | USER | 0x45 (69) | Global View: User |
| 8 | READ | 0x4A (74) | GROUP | 0x4F (79) | Utilities: Group |
| 9 | WRIT | 0x4B (75) | CANCEL | 0x52 (82) | Utilities: Cancel |
| 10 | TCH | 0x4D (77) | DROP | 0x57 (87) | Drop |
| 11 | LTCH | 0x4E (78) | REPLACE | 0x58 (88) | Replace |
| 12 | TRIM | 0x4C (76) | CLICK | 0x59 (89) | Metrónomo |
| 13 | OFF | 0x4F (79) | CYCLE | 0x56 (86) | Cycle/Loop |
| 14 | SOLO0 | 0x57 (87) | SCRUB | 0x65 (101) | Scrub |
| 15 | SMPT | 0x35 (53) | NAME/VAL | 0x34 (52) | Name/Value toggle |
| 16 | ZOOM | 0x64 (100) | ZOOM | 0x64 (100) | Sin cambio |
| 17 | SCRUB | 0x65 (101) | SCRUB | 0x65 (101) | Sin cambio |
| 18 | NUDGE | 0x55 (85) | NUDGE | 0x55 (85) | Sin cambio |
| 19 | MARK | 0x54 (84) | MARK | 0x54 (84) | Sin cambio |
| 20 | CHAN< | 0x30 (48) | CHAN< | 0x30 (48) | Navegación — sin cambio |
| 21 | CHAN> | 0x31 (49) | CHAN> | 0x31 (49) | Navegación — sin cambio |
| 22 | BANK< | 0x2E (46) | BANK< | 0x2E (46) | Navegación — sin cambio |
| 23 | BANK> | 0x2F (47) | BANK> | 0x2F (47) | Navegación — sin cambio |
| 24 | UNDO | 0x51 (81) | REDO | 0x00 | Sin nota MCU para REDO — acción local o vacío |
| 25 | SAVE | 0x50 (80) | SAVE AS | 0x00 | Sin nota MCU estándar — pendiente decidir |
| 26 | SHIFT | — | — | — | Modificador puro — no envía nota |
| 27 | CTRL | 0x48 (72) | CTRL | 0x48 (72) | Modificador — igual |
| 28 | OPT | 0x47 (71) | OPT | 0x47 (71) | Modificador — igual |
| 29 | CMD | 0x49 (73) | CMD | 0x49 (73) | Modificador — igual |
| 30 | ENTER | 0x53 (83) | ENTER | 0x53 (83) | Igual |
| 31 | >>PG2 | — | — | — | Control de página — no envía nota |

### 9.3 PG2 — Normal y Shift

| Key | Label Normal | Nota Normal | Label Shift | Nota Shift | Comentario |
|-----|-------------|-------------|-------------|------------|------------|
| 0–7 | F1–F8 | 0x36–0x3D | F1–F8 | igual | Sin cambio |
| 8–15 | F9–F16 | 0x3E–0x45 | F9–F16 | igual | Sin cambio |
| 16 | ZOOM | 0x64 | ZOOM | 0x64 | Sin cambio |
| 17 | SCRUB | 0x65 | SCRUB | 0x65 | Sin cambio |
| 18 | NUDGE | 0x55 | NUDGE | 0x55 | Sin cambio |
| 19 | MARK | 0x54 | MARK | 0x54 | Sin cambio |
| 20 | CHAN< | 0x30 | CHAN< | 0x30 | Sin cambio |
| 21 | CHAN> | 0x31 | CHAN> | 0x31 | Sin cambio |
| 22 | BANK< | 0x2E | BANK< | 0x2E | Sin cambio |
| 23 | BANK> | 0x2F | BANK> | 0x2F | Sin cambio |
| 24 | TRIM | 0x4C | GROUP | 0x4F | Reasignado en Shift |
| 25 | SAVE | 0x50 | CANCEL | 0x52 | Reasignado en Shift |
| 26 | SHIFT | — | — | — | Modificador puro |
| 27 | CTRL | 0x48 | CTRL | 0x48 | Igual |
| 28 | OPT | 0x47 | OPT | 0x47 | Igual |
| 29 | CMD | 0x49 | CMD | 0x49 | Igual |
| 30 | ENTER | 0x52 | ENTER | 0x52 | Igual |
| 31 | >>VU | — | — | — | Control de página |

### 9.4 Pendientes de decisión

- **UNDO shift (PG1 key 24):** MCU no tiene nota para REDO. Opciones: `0x00` (sin acción), o reutilizar una F-key libre. Decisión pendiente.
- **SAVE shift (PG1 key 25):** Sin nota MCU estándar para "Save As". Misma decisión.
- **F9–F16 (PG2 keys 8–15):** Las notas 0x3E–0x45 solapan con el submenú Global View. En PG2 se usan como F-keys; en PG1 shift se usan como Global View. El firmware debe distinguir según la página activa.

---

## 10. P4 UIHeader — Barra Superior Persistente (2026-06-12 11:30)

`src/display/UIHeader.cpp` — visible en todas las páginas. Contiene timecode, indicadores de estado y navegación de página.

### 10.1 Layout

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│ [SMPT] [⟳] [S] [♪]       00:00:00:00  (timecode DSEG7)        [Bo][Vu][Fa] [≡] │
├─────────────────────────────────────────────────────────────────────────────────┤
│  vpot[0]   vpot[1]   vpot[2]   vpot[3]   vpot[4]   vpot[5]   vpot[6]   vpot[7] │
└─────────────────────────────────────────────────────────────────────────────────┘
```

- Fila superior (`HEADER_H` px): indicadores + timecode + navegación
- Fila inferior (`ASSIGN_STRIP_H` px): nombres VPot por canal, alineados con columnas

### 10.2 Botones de estado — MIDI bidireccional

Todos usan `header_btn_cb`: `LV_EVENT_PRESSED` → Note On vel 0x7F, `LV_EVENT_RELEASED` → Note On vel 0x00.

| Widget | Nota enviada P4→Logic | Nota recibida Logic→P4 | Variable de estado | Visual activo |
|--------|----------------------|------------------------|-------------------|---------------|
| BEAT/SMPT | 0x35 (53) | 113 = SMPTE / 114 = BEATS | `currentTimecodeMode` | SMPT: fondo relleno; BEAT: solo borde |
| LOOP `⟳` | 0x56 (86) | 0x56 vel>0/0 | `cycleActive` | Fondo naranja/dorado |
| RUDE SOLO `S` | **0x5A (90)** | **0x73 (115)** vel>0/0 | `rudeSoloActive` | Fondo rojo |
| CLICK `♪` | 0x59 (89) | 0x59 vel>0/0 | `g_clickActive` | Fondo verde |

> **Asimetría RUDE SOLO:** Logic envía nota 0x73 para indicar "hay solos activos" (solo indicador LED). P4 envía 0x5A para pedir a Logic que limpie todos los solos. Son notas distintas.

> **Asimetría BEAT/SMPT:** Logic envía 113 ó 114 para informar del modo activo. P4 envía 0x35 para solicitar el cambio. Logic no responde con nota de confirmación — solo actualiza el CC del timecode.

### 10.3 Timecode — display no interactivo

Fuente de datos: CC 64–73 canal 0 ó 15 → `timeCodeChars_clean[10]` (ver §4.9).

**Técnica ghost+real:** dos labels superpuestos al (0,0). Ghost en `COL_HEADER_DIM`, real en `COL_HEADER_BRIGHT`. El ghost siempre muestra `"00:00:00: 00"` dando efecto de dígitos inactivos estilo 7-seg.

**Modo SMPTE** (`currentTimecodeMode == MODE_SMPTE`):
- Label único `s_timecode`, formato `HH:MM:SS:FF` via `formatTimecodeString()`
- Bloques beat ocultos

**Modo BEATS** (`currentTimecodeMode == MODE_BEATS`):
- 4 contenedores independientes `s_beat_cont[0..3]`, cada uno con ghost+real
- Separados por 3 puntos `s_beat_dot[0..2]`
- Anchos: BARS=160px (4 dígitos), BEAT=40px (1), SUBDIV=40px (1), TICKS=120px (3)
- Origen X: `bx[0]=320`, resto calculado dinámicamente con `dot_gap=12`
- Mapeo buffer → bloque: `starts[]={0,6,4,7}`, `counts[]={3,1,1,3}`
- Timecode SMPTE oculto

Throttle de redibujado: máximo 1 frame cada 16 ms (`~60 fps`), controlado por `needsTimecodeRedraw`.

### 10.4 VPot assignment strip

En un MCU físico cada canal tiene un scribble strip LCD de 2 filas. El P4 distribuye esas filas en distintas áreas de la UI:

| Fila MCU real | SysEx 0x12 offsets | Contenido | En P4 |
|---------------|--------------------|-----------|-------|
| Fila superior | 0–55 | Nombre de track | Página VU meters y botones (`trackNames[]`) |
| Fila inferior | 56–111 | Parámetro VPot activo | **Header strip** (`vpotAssignNames[]`) |

El parámetro VPot sube al header porque es contexto global: conviene verlo en cualquier página activa.

8 labels `s_vpot_lbl[0..7]` en la fila inferior del header, alineados con las columnas de canal:

```
posición X = (P4_CH_OFFSET + i) * CH_W
```

- Fuente: SysEx 0x12 offsets 56–111 → `vpotAssignNames[i]` (ver §4.2)
- Texto centrado, font montserrat 14, color `COL_HEADER_BRIGHT`
- `LV_LABEL_LONG_CLIP` — texto recortado si supera `CH_W`
- Actualización: `needsHeaderRedraw = true` en `processControlChange` al recibir CC 48–55

### 10.5 Navegación Bo/Vu/Fa — sin MIDI

Tres botones en la esquina derecha del header (x=812/864/916). Solo navegación local, no envían MIDI.

| Botón | X | Página destino | Activo cuando |
|-------|---|---------------|---------------|
| Bo | 812 | Page 1 (Botones) | `g_currentPage == 1` |
| Vu | 864 | Page 3A (VU meters) | `g_currentPage == 0` |
| Fa | 916 | Page 3B (Faders) | `g_currentPage == 2` |

Estado visual: página activa = `COL_HEADER_BRIGHT`, resto = `COL_HEADER_DIM`. Actualizado en cada `uiHeaderUpdate()` cuando `g_currentPage` cambia.
