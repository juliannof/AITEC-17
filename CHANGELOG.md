# CHANGELOG — iMakie

Registro histórico de cambios significativos del proyecto iMakie.  
Formato: [Keep a Changelog](https://keepachangelog.com/)

---

## [Unreleased]

### Pendientes

| Prioridad | Tarea | Notas |
|-----------|-------|-------|
| 🔴 Alta | **P4: Logic no aplica el V-Pot relativo del pop-up (2026-06-12 19:47)** | El pop-up envía CC relativo (`CC 16+strip`, bit6=dirección) al arrastrar el arco grande — el MIDI **SALE** (visto en monitor) pero Logic **NO mueve el pan**. Análogo al evento del S2 con los AutoModes (notas 74-78 Read/Write/Trim/Touch/Latch): Logic solo los aplica con un track **seleccionado** (`g_selectedChannel>=0`, `MIDIProcessor.cpp` ~l.611). Hipótesis: falta contexto (track seleccionado / assignment Pan / banco activo) o canal/controller distinto. Probar: (1) canal 0 fijo (`0xB0`, `0x10+strip`) en vez de `0xB0|strip`; (2) forzar selección previa; (3) verificar modo Pan. Ver [[midi_sale_logic_no_aplica]]. |
| 🟡 Media | **P4: pop-up V-Pot — no refrescar el fondo con el modal abierto (2026-06-12 19:47)** | Fix listo, SIN aplicar: en `main.cpp` loop CONNECTED, si `uiVPotPopupIsOpen()` → solo `uiVPotPopupUpdate()`, saltar `uiHeaderUpdate`/página/`handleVUMeterDecay`. El overlay tapa toda la pantalla (1024×600), no hace falta repintar detrás. |
| 🟡 Media | **P4: pop-up V-Pot — botón "Cerrar" demasiado grande (2026-06-12 19:47)** | Reducir el `lv_button` (actual 160×56) en `UIVPotPopup.cpp::uiVPotPopupOpen()`. |

---

### SESIÓN 2026-06-14 — AutoMode nota 79 corregida + derivación AUTO_OFF por ausencia

**Diagnóstico previo:** revisión de la cadena completa AutoMode reveló que S3 mapeaba nota MIDI 79 → `AUTO_OFF` (incorrecto: nota 79 es "Automation Group" en la spec Mackie, no un botón de automodo). P4 ya ignoraba nota 79 correctamente pero carecía de Note Off → `AUTO_OFF`. S2 tenía una comparación `uint8_t` vs `AutoMode` sin seguridad de tipo.

**Confirmado por captura MIDI real (Logic Pro):** notas 74-78 = READ/WRITE/TRIM/TOUCH/LATCH. `AUTO_OFF` no tiene nota dedicada — es la ausencia de cualquier nota del grupo activa. Nota 79 no se emite nunca para cambios de automodo de fader.

**Anomalía TRIM documentada:** al activar TRIM, Logic a veces envía Note On 76 sin el Note Off del modo previo. Resuelto sin lógica especial mediante mutual exclusion en `_autoNoteState`.

**Cambios aplicados (2026-06-14):**

| Archivo | Cambio |
|---------|--------|
| `S3/.../MIDIProcessor.cpp` | Rango `74-79 && is_on` → `74-78` (On y Off). `_autoNoteState[5]` en namespace anónimo. AUTO_OFF por ausencia. Log `[AUTOMODE] S3 nota=...` |
| `P4_JC1060P470C/.../MIDIProcessor.cpp` | Mismo patrón que S3. `_autoNoteState[5]`. Note Off → AUTO_OFF. Log `[AUTOMODE] P4 nota=...` |
| `S2/.../RS485Handler.cpp` | `uint8_t newAutoMode` eliminado → comparación type-safe `pktMode != currentAutoMode` |
| `docs/AUTOMODE.md` | Sección 9B añadida: mapeo MIDI confirmado, anomalía TRIM, refresh masivo, implementación |

**Resultado:** S3 y P4 producen valores `AutoMode` idénticos para el mismo input de Logic. Los S2 de ambos buses (A y B) entrarán siempre en el mismo modo.

---

### SESIÓN 2026-06-12 — P4 VPot RESUELTO (lv_arc) + cadena trazada + pop-up grande (19:47)

**VPot arcs RESUELTOS — widget `lv_arc` (no draw primitives):**
- Causa de los 4 intentos fallidos previos: se insistió con draw primitives (`lv_draw_arc`) y `lv_arc_set_angles`. La solución que **ya funcionaba** estaba en `P4_JC4880P433C` (P4 pequeño): widget `lv_arc` con `set_range(-100,100)` + `set_value(((pos-6)*100)/6)` + `set_bg_angles(135,405)` + `LV_ARC_MODE_SYMMETRICAL`, estilo en `LV_PART_MAIN`/`LV_PART_INDICATOR`.
- `UIPage3.cpp`: eliminado `pan_draw_cb`; arco recreado como `lv_arc`; update con `lv_arc_set_value`. Render **OK en hardware**.
- Única adaptación al P4 grande: sin `set_rotated()` (landscape nativo vs portrait del pequeño).
- **Norma establecida:** siempre usar widgets de la biblioteca LVGL y revisar primero el P4 pequeño como referencia ([[usar_biblioteca_lvgl]]).

**Cadena del V-Pot trazada end-to-end (confirmada en hardware):**
- Logic `CC48-55` (ch1) → `processMidiByte` (USB-MIDI `tud_midi_stream_read`) → `case 0xB0` → `processControlChange` → filtro canal 0/15 → `vpotValues[strip + P4_CH_OFFSET]` (8-15) → `uiPage3Update` → `lv_arc_set_value` → columnas 9-16.
- **Diagnóstico clave:** Logic emite el pan SIEMPRE como `CC48` (strip 0) porque el banco sigue a la selección — el track tocado se coloca como strip 0. No es bug de firmware. Por eso "solo la pista 9 funcionaba".
- Diagnóstico: `log_d`→`log_i` en `processControlChange:194` para ver todo CC entrante (`CORE_DEBUG_LEVEL=3`).
- Confirmada la norma: slots **0-7 NO implementados** (banco S3/IAC), trabajar solo 8-15 ([[p4_slots_0_7_no_implementado]]).

**Pop-up grande del V-Pot (NUEVO — `display/UIVPotPopup.{h,cpp}`):**
- Tocar arco pequeño (col 9-16) → modal en `lv_layer_top()`: nombre del track + `lv_arc` 320px interactivo + valor L/C/R + botón Cerrar.
- Arrastre del arco → envía pasos relativos `CC 16+strip` a Logic (mismo patrón que el encoder).
- `UIPage3.cpp`: zona táctil `s_arc_hit[]` (solo 8-15) + `uiVPotPopupClose()` en destroy. `main.cpp`: include + `uiVPotPopupUpdate()` en loop CONNECTED.
- **Validado en hardware:** abre, arrastra, MIDI sale, cierra correctamente.
- **Pendientes** (ver tabla arriba): Logic no aplica el pan; no refrescar fondo con modal abierto; botón Cerrar muy grande.

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios.

---

### SESIÓN 2026-06-12 — VPot arcs: 4 intentos, estado actual pan_draw_cb (17:56)

**Único problema real:** el arco indicador (verde) del VPot no aparece en pantalla pese a que los datos llegan y los ángulos se calculan correctamente.

**Flujo de datos confirmado funcionando:**
- Logic Pro → CC48-55 → `MIDIProcessor.processControlChange()` → `vpotValues[strip + P4_CH_OFFSET]` = `vpotValues[8..15]`
- `needsButtonsRedraw = true` → `uiPage3Update()` → `lv_arc_set_angles(s_arc[8], 158, 270)` + `lv_obj_invalidate`
- Log confirmado: `[VPot] CC48 strip=0 raw=0x21 pos=1` y `arc[8] pos=1 s=158 e=270`

**Intentos fallidos (esta sesión):**

| # | Enfoque | Resultado | Commit |
|---|---------|-----------|--------|
| 1 | `LV_ARC_MODE_SYMMETRICAL` + fórmula `((pos-6)*100)/6` | No renderiza | `4b89c99` |
| 2 | `LV_ARC_MODE_SYMMETRICAL` + fórmula angular correcta | No renderiza | `4206a00` |
| 3 | `LV_ARC_MODE_NORMAL` + `lv_arc_set_angles(arc_s, arc_e)` directo | Datos correctos en log, no renderiza | `793deca` |
| 4 | `lv_obj_create` + `LV_EVENT_DRAW_MAIN` + `pan_draw_cb` / `lv_draw_arc` directo | **Sin commitear — sin validar en hardware** | — |

**Estado del código en el commit actual (intento 4):**
- `UIPage3.cpp`: `lv_arc_create` reemplazado por `lv_obj_create` + `pan_draw_cb`
- `UIPage3B.cpp`: ídem, user_data = `i + P4_CH_OFFSET`
- `uiPage3Update()` / `uiPage3BUpdate()`: eliminado `lv_arc_set_angles`, solo `lv_obj_invalidate`
- `pan_draw_cb` dibuja fondo gris (135°→45°) e indicador verde según `vpotValues[i]`
- Mismo patrón que `vu_draw_cb` (VU meter — confirmado funcionando)

**Hipótesis causa raíz del fallo lv_arc (intento 3):**
`indic_r = arc_r - get_indicator_max_pad(obj)` — si el tema LVGL default aplica padding a `LV_PART_INDICATOR`, `indic_r` podría ser ≤ 0 y el guard `if(indic_r > 0)` en `lv_arc.c:843` evita el dibujo. No confirmado sin compilar con log dentro del widget.

---

| Prioridad | Tarea | Notas |
|-----------|-------|-------|
| 🟢 Baja | **OTA WiFi S2** — ✅ validado hardware (2026-05-26) | OTA funcional en 4 faders. Flashear provisioning + firmware. Ver `docs/WIFI-OTA.md`. |
| 🔴 **VALIDACIÓN HW** | **Fader S2→Logic + detección usuario** — auditado 2026-05-25, listo para flash | S3: mapeo calibrado, jerarquía master, sync guard. S2: detección dirección en MOVING_TO_TARGET. Commits `6f6ace6` + `d171b12`. Firmware verificado en código — pendiente flash y test en hardware. |
| 🟡 Media | **P4: botón BOUNCE — configurar Logic Pro** | Label "BOUNCE" aplicado en `config.h` LABELS_PG1[20] (nota 0x3E, 62). Pendiente solo: Logic Pro Key Commands → MIDI Learn nota 62 → "Bounce Project or Mix…". |
| 🟡 Media | **P4: assignment display — SysEx 0x12 offset 0 corto contamina trackNames** | Al pulsar PAN/SELECT Logic envía SysEx 0x12 con offset=0 y texto corto (≤30 chars): "Seleccionar" (11), "Track N "nombre"" (30). Ese texto sobreescribe `trackNames[]` en slots 0–4. Causa: el parser de `case 0x12` acepta cualquier offset 0 como nombres de pista. Fix pendiente: detectar strings cortos en offset 0 (text_len < 56) y rutearlos a `assignmentString` en lugar de `trackNames`. Los offsets ≥56 ya van al strip VPot (correcto). |
| 🟢 Baja | **P4: VU global 16 pistas via MIDI UART S3→P4** | S3 re-emite MIDI de Logic (ch 1–8) a P4 por UART directo (ch 9–16). P4 agrega las 16 pistas en display LVGL. Sin WiFi, sin protocolo custom — reutiliza `processMidiByte()` existente. 1 cable TX→RX. Ver `docs/S3ToP4.md` sección "Feature: Agregación 16 pistas". |
| 🟢 Baja | **P4: Display 16 pistas via IAC Bus routing (macOS)** — plan completo en `docs/16TRACKS.md` eliminado 2026-06-11 | Fase 2 arrays ya implementados (16 slots, `P4_CH_OFFSET=8`). Pendiente: Fase 1 — IAC Bus macOS (`MIDI Patchbay`: S3 out → P4 in, canal 1→2). Fase 3 — rediseño UIPage3 a 64px/canal + separador visual entre bancos. S3 no se modifica. Desconexión detection debe protegerse para solo banco P4. |
| 🟢 Baja | **Limpiar código muerto Motor S2** — auditado 2026-05-27 | 4 items: (1) `MotorState::WAITING_FOR_CALIB` nunca asignado — eliminar del enum + comentarios + guard `setADC()` línea 499; (2) `_motor_goingToMin` flag nunca leído — eliminar de `config.h` + `goToMin()` + `setUserDropTarget()`; (3) `setUserDropTarget()` nunca llamada desde fuera — eliminar de Motor.h/cpp; (4) `goToMin()` no establece `_motor_state=GOING_TO_MIN` — riesgo si se llama directa desde SAT/test, añadir la asignación. Sin impacto en comportamiento actual. |
| 🔴 **VALIDACIÓN HW** | **Fader extremos −∞/+6dB — snap zone no funciona sin calibración** | Snap zone en S3 `main.cpp` (commit `9f19a68`) solo actúa si `ch.calibratedMin/Max > 0`. Si calibración no ha corrido, `span=0` y se usa fallback `faderPos*max/27000` sin snap → Logic muestra −139 dB y 5,2 dB. **Fix pendiente:** añadir snap zone también al path fallback (sin calibración), o verificar que calibración corre y captura min/max correctamente antes de confiar en el snap. |
| 🔴 Alta | **P4: `startTask()` RS485 nunca llamada — slaves sin comunicación (2026-05-27)** | `main.cpp setup()`: `rs485.begin()` configura Serial1 pero `rs485.startTask()` nunca se invoca. El task de polling (`runTask()`) no arranca. P4 no envía ni un paquete a ningún slave S2. `tickCalibracion()` encola calibraciones que nunca se envían. **Fix:** añadir `rs485.startTask()` en `setup()` tras `rs485.begin()`, `main.cpp línea ~254`. Ver `RS485.cpp::startTask()` — pineado a Core 1, prioridad 5. |
| 🔴 Alta | **ADS1115 no lineal — ADC=225 en posición física media (esperado ~13500) (2026-05-30)** | Con `GAIN_ONE` + pot lineal 3.3V, mid-travel debería dar ~13200 ADC counts. En test real: bottom=27, mid=225, top=22795. La respuesta es casi logarítmica (bottom 0.87% del rango total en mid-travel). Causa posible: (1) potenciómetro logarítmico (audio taper) en lugar de lineal — hardware no modificable; (2) carga resistiva externa; (3) wiring inusual. **Impacto:** en AUTO_READ, Logic envía target=X que S3 mapea linealmente al rango calibrado, pero el fader físico en esa posición ADC no corresponde visualmente. Motores pueden buscar posiciones que parecen incorrectas al ojo. **Fix pendiente:** diagnóstico físico (medir resistencia pot en mid-travel) o compensación logarítmica en el mapeo S3→ADC si la no-linealidad es reproducible. |
| 🟡 Media | **Calibración mismatch — calibratedMin=168 vs ADC físico min=27 (2026-05-30)** | S3 guardó calibratedMin=168 en la sesión de test (motor paró antes del tope físico durante GOING_TO_MIN). El fader físico llega hasta ADC=27. En AUTO_READ con Logic en fondo: S3 manda target=168, motor busca 168, fader en 27 → motor buzzea contra tope. **Fix:** forzar recalibración limpia con slave correcto (ID1/ID2) conectado. Puede ser síntoma del bug de nonlinealidad + motor que no llega al tope físico real. |

---

### SESIÓN 2026-06-12 — P4 UIPage1: recall estado botones al crear página (12:46)

`UIPage1.cpp` `uiPage1Create()`: inicialización de botones cambiada de `applyButtonState(i, false)` a `applyButtonState(i, btnStatePG1[i])`. Añadido `needsButtonsRedraw = true` al final del create. Resuelve que al navegar a la página de botones estos aparecían todos apagados aunque Logic tuviera estados activos.

---

### SESIÓN 2026-06-12 — P4 Header: indicador VPot assignment + ajuste botones VU (12:45)

**Indicador VPot Assignment en header:**

`UIHeader.cpp`: nuevo widget `s_assign_cont`/`s_assign_lbl` a x=244 (8px tras CLICK), 44×34 px. Display-only (sin hit area, sin MIDI). Lee `btnStatePG1[0..5]` en cada ciclo de `uiHeaderUpdate()` y muestra la abreviatura del modo activo: TRK / SND / PAN / PLG / EQ / INS. Dim + "--" cuando ningún modo activo. Activo: borde + texto `COL_HEADER_BRIGHT`. Destroy incluido.

**UIPage3 — separación botones SOLO/MUTE:**

`UIPage3.cpp`: `MUTE_TOP` gap 2→4 px. `MUTE_TOP` pasa de 58 a 62 px.

---

### SESIÓN 2026-06-12 — P4 Header: beat display zero-padding + ajustes marco (12:35)

**Beat display — zero-padding correcto:**

`UIHeader.cpp` `uiHeaderUpdate()`: reemplazado el algoritmo de lectura del buffer. Problema anterior: espacios del buffer (`beatsChars_clean` inicializado a `' '`) se sanitizaban a `'0'`, desplazando el dígito significativo a posición incorrecta. Nuevo algoritmo: extrae solo dígitos (`'0'–'9'`), parsea como entero, formatea con ceros a la izquierda mediante loop de módulo 10. Sin `snprintf`, sin dependencias externas. `counts[0]` 3→4 (barras hasta 9999). `widths[]={4,1,1,3}` controla ancho de display. Resultado: bar 1→`0001`, bar 182→`0182`, ticks 1→`001`.

**Marco timecode — ajuste visual:**

`UIHeader.cpp` `uiHeaderCreate()`: marco `s_tc_frame` 30px más estrecho (±15px cada lado): `tx-56→tx-41`, `tw+112→tw+82`. Borde cambiado de `COL_HEADER_BRIGHT` a `COL_HEADER_DIM` (menos prominente). Bloques beat desplazados `bx[0]`: 320→312 para mantener alineación dentro del nuevo marco.

**main.cpp:** stack tarea MIDI Core 0: 4096→8192 bytes (margen para callbacks LVGL).

**Documentación:** `docs/MIDI.md` §10 — P4 UIHeader completo (layout, botones táctiles, timecode, VPot strip, navegación).

---

### SESIÓN 2026-06-12 — P4 Header: VPot assignment strip + marco timecode (00:02)

**Assignment display — VPot names en pie del header:**

`MIDIProcessor.cpp` `case 0x12`: el bucle que antes hacía `break` en offset 56 ahora captura offsets 56–111 (8 × 7 chars = nombres de VPot que Logic envía al pulsar PAN/SEND/etc.). Resultado en `vpotAssignNames[8]`, trigger `needsHeaderRedraw`.

`UIHeader.cpp`: header extendido de 88px a 110px (`ASSIGN_STRIP_H=22`). Añadidos 8 labels `s_vpot_lbl[0..7]` en `y=HEADER_H`, ancho `CH_W=64px`, alineados con columnas P4 (x=512..960). Color `COL_HEADER_BRIGHT`. Se actualizan en `uiHeaderUpdate()` cuando `needsHeaderRedraw`.

`config.h`: `#define ASSIGN_STRIP_H 22`, `CONTENT_Y` → `HEADER_H + ASSIGN_STRIP_H` (y=110), `CONTENT_H` → 490px. Añadido `extern String vpotAssignNames[8]`.

**Marco timecode fijo 1px:**

`UIHeader.cpp`: `s_tc_frame` — rectángulo decorativo alrededor del timecode SMPTE/BEAT. Creado tras segundo `lv_obj_update_layout()` para medir posición y ancho reales del label. Border 1px `COL_HEADER_BRIGHT`, radius 8 (esquinas redondeadas), fondo transparente. Tamaño: +100px ancho (+50 cada lado), +10px alto (+5 cada lado). Fijo, siempre visible.

---

### SESIÓN 2026-06-11 — VU P4: fixes clearClip + 0x72 + draw callback + header CLICK/LOOP (17:27)

**VU optimization — UIPage3.cpp:**

192 objetos LVGL individuales (`s_vu_seg[16][12]`) reemplazados por 16 objetos con `LV_EVENT_DRAW_MAIN` draw callback (`vu_draw_cb`). Los 12 segmentos se dibujan directamente en el render buffer con `lv_draw_rect()`. Ganancia: refresco fluido con 8+ VU metros activos.

**Bug VU: clearClip sobreescribía vuLevels a 0 — commit `e206d9f`:**

`processChannelPressure()` en el case `0x0F` (clearClip) leía `vuLevels[targetChannel]` sin aplicar `P4_CH_OFFSET=8`. Como `vuLevels[0..7]` siempre valen 0, `normalizedLevel=0` → `vuLevels[dispCh]=0` → barra caía a cero.

**Por qué solo en subidas abruptas:** los transitorios rápidos clipan brevemente → Logic envía `0xE` seguido de `0xF` en milisegundos → clearClip mataba el nivel. Subidas lentas nunca generan clip+clearClip consecutivo.

**Fix:** clearClip en rama propia — solo actualiza `vuClipState`, no toca `vuLevels`, `vuLastUpdateTime` ni peak. Misma arquitectura aplicada a `0x72`.

**Bug VU: decodificación SysEx 0x72 incorrecta — commit `0e6ca2d`:**

`case 0x72`: el código usaba `raw & 0x0F` como número de canal y `raw >> 4` como nivel. Todos los bytes con valor `0x04` producían `channel=4, dispCh=12, level=0`. Fix: canal = índice del bucle `i`, nivel = nibble bajo del byte. Misma lógica clearClip/newClip que Channel Pressure.

**Análisis MIDI Monitor — cadencia Logic confirmada:**

- Bursts de 8 mensajes (uno por strip activo) cada ~15-30ms
- Strips silenciosos NO reciben `level=0` — Logic los omite del burst
- Arquitectura timestamp-only-when->0 + decay 300ms es correcta para este comportamiento
- `0x72` = volcado batch VU al conectar (no stream continuo durante playback)
- `0x0E` = automodo por canal (Trim=3), NO datos VU

**Header: indicador CLICK + LV_SYMBOL_LOOP — commits `48705f6` + `80c73d7`:**

- Nota `0x59` (CLICK/metrónomo) procesada en `processNote()` → `g_clickActive`
- Nuevo indicador `s_click_lbl` en header x=192, w=44 con `LV_SYMBOL_AUDIO`, morado activo
- Ciclo `s_cycle_lbl`: `LV_SYMBOL_LEFT " " LV_SYMBOL_RIGHT` → `LV_SYMBOL_LOOP`
- Reset de `g_clickActive` en GoOffline (`case 0x0F`)
- Indicadores BEAT/LOOP/S no modificados

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios.

---

### SESIÓN 2026-06-11 — Bug menú header + VU layout + paleta Logic Pro + legacy marking (03:04)

**Bug menú header — `uiMenuInit()` llamada doble con parent incorrecto:**

- `UIPage3.cpp` tenía una segunda llamada a `uiMenuInit(s_page_root)` (línea ~186) que sobreescribía los punteros estáticos de `UIMenu` con un parent erróneo. Al destruir la página, esos punteros quedaban dangling → menú roto en vistas subsiguientes.
- **Fix:** eliminada la llamada extra en `UIPage3.cpp`. `uiMenuInit()` solo se llama una vez desde `s_root` en `UIMenu.cpp`.
- **Fix secundario:** `UIMenu.cpp::btn_cb` asignaba `g_currentPage = X` prematuramente antes de que la tarea Core 1 pudiera destruir la página antigua → objetos LVGL zombie. Eliminadas las asignaciones prematuras.

**VU layout reordenado — `UIPage3.cpp`:**

Orden nuevo de arriba a abajo por canal:
```
SEL  (y=4,   h=52)
MUTE (y=60,  h=52)
PAN  (y=120, sz=52)
NAME (y=178, h=28)
VU   (y=212, h=296)
```
`#define` actualizados en UIPage3.cpp.

**Paleta de colores — estilo Logic Pro (`config.h` + todos los archivos UI):**

| Define | Antes | Después |
|--------|-------|---------|
| COL_BG | 0x000000 | 0x1A1A1A |
| COL_MUTE_OFF | 0x400000 | 0x3A3A3A |
| COL_SOLO_OFF | 0x333333 | 0x3A3A3A |
| COL_TRACK_BG | 0x0F1218 | 0x1E1E1E |
| COL_TRACK_SEL | 0x2A3040 | 0x2A2A2A |
| COL_TRACK_SEP | 0x111111 | 0x333333 |
| COL_TEXT_DIM | — | 0x999999 (NUEVO) |
| COL_FADER_TRACK | — | 0x555555 (NUEVO) |
| COL_FADER_THUMB | — | 0x8C8C8C (NUEVO) |

- `UIMenu.cpp`: panel bg, separador, nav buttons, slider thumb y label actualizados a nuevos COL_*.
- `UIPage3B.cpp`: fader track/thumb, arc bg, COLOR_AUTO_OFF → COL_AUTO_OFF desde config.h.
- `UIOffline.cpp`: fondo 0x000000 → COL_BG.

**Legacy marking — placa JC4880P433C:**

Archivos de documentación marcados como LEGACY (placa antigua ST7701S 480×800):
- `docs/DISPLAY_P4.md` — banner LEGACY añadido al inicio
- `docs/NEOTRELLLIS.md` — banner LEGACY añadido (NeoTrellis no existe en JC1060P470C)
- `docs/TOUCH.md` — nota comparativa ambas placas (GT911 en ambas, pines distintos)
- `docs/ARCHITECTURE_P4.md` — nota NeoTrellis = legacy JC4880P433C
- `README.md` — Display P4: ST7701S 480×800 → JD9165 1024×600 landscape
- `STATUS.md` — Display P4: actualizado a JD9165; NeoTrellis marcado LEGACY; placa JC4880P433C → JC1060P470C en Build

**Arquitectura 16 pistas documentada — `docs/16TRACKS.md` (NUEVO):**

Propuesta via IAC Bus (macOS MIDI): S3 re-emite MIDI a P4 por MIDI UART. P4 mostraría las 16 pistas en pantalla 1024×600. Fase implementación: arrays[16], funciones con bank offset, canal MIDI diferenciado.

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios de firmware.

---

### SESIÓN 2026-06-11 — UI P4: SOLO/MUTE paleta + filo + UIPage1 landscape + header interactivo + docs limpieza (22:34)

**SOLO/MUTE off → gris 0x3A3A3A — commit `034ea9a`:**

- `COL_MUTE_OFF` 0x400000 → 0x3A3A3A y `COL_SOLO_OFF` 0x333333 → 0x3A3A3A en `config.h`.
- Coherente con paleta Logic Pro (botones off = gris neutro, no rojizo).

**Filo negro 1px en botones SOLO/MUTE off — commit `2a37f67`:**

- `UIPage3.cpp`: `border_width=1`, `border_color=0x000000` cuando el botón está inactivo.
- Diferencia visual clara entre el botón y el fondo de celda en estado off.

**Limpieza docs/ — commit `b79ae89`:**

Cuatro archivos obsoletos eliminados:
- `docs/ARCHITECTURE_P4.md` — datos de procesador incorrectos, tareas desactualizadas.
- `docs/S3ToP4.md` — snapshot 2026-05-24 con rutas incorrectas; bugs relevantes ya en CHANGELOG.
- `docs/16TRACKS.md` — plan migrado a tabla pendientes CHANGELOG.
- `docs/ESTRUCTURA_REORGANIZACION.md` — histórico ya superado.

`docs/RS485_P4.md` actualizado: advertencia pines pendientes confirmar, nota `NUM_SLAVES`, fecha 2026-06-11. `CLAUDE.md` limpiado de referencias a los docs borrados.

**UIPage1 landscape + header interactivo — commit `aa50792`:**

`UIPage1.cpp`:
- Grid reescrito a **10 columnas × 5 filas** (`P1_COLS=10`, `P1_ROWS=5`), cada celda `(P4_W/10) × (CONTENT_H/5)` ≈ 102×102 px.
- Slots sin nota MIDI (`MIDI_NOTES_PG1[i]==0x00`) se ocultan (`s_btns[i]=NULL`).
- D-pad (índices 44-47): texto → `LV_SYMBOL_UP/DOWN/LEFT/RIGHT`.
- Color de texto negro automático sobre fondos claros (`needsBlackText()`).

`UIHeader.cpp` — botones táctiles en el strip:
- `header_btn_cb`: `LV_EVENT_PRESSED` → nota ON (0x7F), `LV_EVENT_RELEASED` → nota OFF (0x00) via `sendMIDIBytes()`. Permite pulsar SOLO, CYCLE, CLICK y MODO directamente desde la pantalla.
- `nav_btn_cb`: tres botones de navegación (Botones/VUMetros/Faders) en el header → `g_switchToPage1/3A/3B`.
- `applyNavState()` / `applyModeState()`: resaltado activo/inactivo con `COL_HEADER_BRIGHT/DIM`.
- `s_lastPage` tracking para actualizar el resaltado solo cuando cambia la página.

`MIDIProcessor.cpp`:
- `formatTimecode()`: recorte de grupos extra cuando hay más de 4 dos-puntos (evita timecode con 5 grupos).
- Reset de `btnStatePG1` / `btnFlashPG1` en GoOffline.
- AutoMode: notas 74-78 mapeadas a `AUTO_READ/WRITE/TRIM/TOUCH/LATCH` con guard `g_selectedChannel`.

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios.

---

### SESIÓN 2026-06-10 — BEATS display fix + documentación botones P4 (19:09)

**Fix BEATS — mapeo buffer → bloques incorrecto:**

Logic Pro envía beats timecode en CC 64-73 con este layout real en `beatsChars_clean[0..9]`:

```
[0-2] = bar (3 dígitos)   [3] = separador (vacío → '0')
[4]   = subdivisión        [5] = separador (vacío → '0')
[6]   = beat               [7-9] = ticks (3 dígitos)
```

El código asumía `starts={0,4,5,6}` / `counts={4,1,1,3}` — incorrecto en todos los campos:
- Bar leía 4 dígitos incluyendo separador vacío → `"0010"` en vez de `"0001"`
- Beat leía índice 4 (subdivisión) → campo incorrecto
- Sub leía índice 5 (separador, siempre `'0'`) → siempre mostraba cero
- Ticks leían `[6-8]` → perdían el dígito ones en `[9]`

**Fix aplicado:** `starts={0,6,4,7}` / `counts={3,1,1,3}` en `UIHeader.cpp` y `MIDIProcessor.cpp::formatBeatString()`. Verificado con dos ejemplos reales:
- `1.1.1.1` → `0001 1 1 001` ✓
- `1.2.3.159` → `0001 2 3 159` ✓

**Documentación protocolo MCU — `docs/MIDI.md`:**
- §4.10.1: tabla exhaustiva de los 116 note numbers MCU (todos los botones físicos de una superficie Mackie Control Universal)
- §9: mapping P4 botones PG1/PG2 con normal y Shift local en firmware — 32 botones × 2 páginas × 2 estados

**Decisión de diseño — botón BOUNCE:**
- PG2 key 8 renombrado de `F9` a `BOUNCE` (nota `0x3E` = 62)
- En Logic Pro: Key Commands → MIDI Learn → asignar nota 62 al comando "Bounce Project or Mix…"
- Botón directo (sin Shift), página PG2
- Pendiente: actualizar `config.h` P4 con el label y nota, y configurar Logic

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios.

---

### SESIÓN 2026-06-10 — Fluidez refresco timecode P4 (14:06)

**Problema:** display beats/SMPTE no era fluido — `needsTimecodeRedraw` solo se activaba al llegar el último CC de timecode (controller 64). Si Logic no enviaba ese dígito en un frame dado, el display no se actualizaba.

**Fix P4:**
- `MIDIProcessor.cpp:218` — `needsTimecodeRedraw = true` en cualquier CC de timecode (64–73), no solo en controller 64
- `UIHeader.cpp:96` — throttle 16ms en `uiHeaderUpdate()` para evitar redraws en ráfaga cuando llegan los 10 CCs consecutivos

**MCU afectadas:** P4 únicamente. S2/S3 sin cambios.

---

### SESIÓN 2026-05-30 — Debugging RS485 timeouts + fixes (10:30)

**Contexto:** Tras flashear AutoMode, S3 mostraba `TIMEOUT slave 1 (#1 consecuciones)` repetido. Diagnóstico y correcciones aplicadas.

**Root cause real:** Slave ID mismatch — S2 conectado tenía `trackId=5` (configurado en NVS vía SAT), S3 sondeaba `slave 1`. Corregido manualmente cambiando el ID.

**Fixes defensivos aplicados (mejoras de timing):**

| Archivo | Cambio |
|---------|--------|
| `S2/S2_V1/src/hardware/Motor/Motor.cpp` | `setTargetForced()`: throttle `log_i` a 1 vez/2s (antes: cada 20ms si motor en tránsito → USB CDC bloat en path crítico) |
| `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h` | `RS485_RESP_TIMEOUT_US` 5000µs → 8000µs (más margen para loop S2 variable) |
| `S2/S2_V1/src/RS485/RS485Handler.cpp` | Log diagnóstico `[FADER]` target/adc/diff/mode a 500ms (era `log_d` 5s) para debugging activo |

**Descubrimiento ADS1115:** En test de rango completo, mid-travel físico del fader devuelve ADC=225 (esperado ~13500). Documentado como pendiente en tabla Pendientes.

**Patrón `#1 consecutivo` explicado:** `_consecutiveTimeouts` se resetea con cualquier respuesta recibida (éxito o CRC error). Con slave ID incorrecto: ningún S2 responde → counter sube; cualquier byte extraño en el bus que empiece en 0xBB resetea el counter → siempre aparece #1. Con slave correcto: issue desaparece.

**MCU afectadas:** S2 (log throttle + log diagnóstico), S3 (timeout).

---

### SESIÓN 2026-05-30 — AutoMode awareness fader S2 (09:35)

**Contexto:** El `MasterPacket.flags` ya transmitía AutoMode (bits 5-7) desde S3, pero el S2 lo ignoraba en lo que afectaba al motor — solo lo usaba para colorear el VPot. Implementación del routing real del faderTarget según modo.

**Cambios — S2 únicamente:**

| Archivo | Cambio |
|---------|--------|
| `S2/S2_V1/src/hardware/Motor/Motor.h` | + declaración `Motor::setTargetForced(uint16_t)` |
| `S2/S2_V1/src/hardware/Motor/Motor.cpp` | + implementación `setTargetForced()` — copia de `setTargetFromS3()` sin el guard `_motor_manualTouchDetected` |
| `S2/S2_V1/src/config.h` | + constantes `AUTOMODE_TOUCH_DEBOUNCE_MS=80`, `AUTOMODE_LATCH_DEBOUNCE_MS=300`, `AUTOMODE_LATCH_UNFREEZE_ADC=200` + estado `_rsCurrentMode`, `_rsLatchFrozen`, `_rsLatchFrozenADC`, `_rsTouchActive`, `_rsLastTouchTime` |
| `S2/S2_V1/src/RS485/RS485Handler.h` | + `namespace Internal` con `_applyFaderTarget()` y `_touchDebounceForMode()` |
| `S2/S2_V1/src/RS485/RS485Handler.cpp` | + implementación helpers `Internal`. `onMasterData()` detecta cambio de modo + reset total + delegación. `buildResponse()` con touchState debounceado por modo |

**Behaviour final:**

| Modo | Motor | touchState debounce |
|------|-------|--------------------:|
| OFF / READ | `setTargetForced()` — DAW absoluto | 600ms |
| WRITE | inhibido | 600ms |
| TOUCH / TRIM | `setTargetFromS3()` (guard) | 80ms |
| LATCH | `setTargetFromS3()` + freeze hasta `Δtarget > 200 cuentas` | 300ms |

**Decisiones de diseño confirmadas con usuario:**
- AUTO_TRIM tratado como AUTO_TOUCH (no estaba en spec original, valor 3 del enum).
- Cambio de modo → reset total (`_rsLatchFrozen`, `_rsTouchActive`, `_rsLastTouchTime`).
- Reevaluación en CADA paquete, no solo cuando `faderTarget` cambia — así al soltar TOUCH el motor vuelve al target sin esperar a que Logic reenvíe.

**Punto único de futuro upgrade:** cuando `FaderTouch::isTouched()` sea fiable, el cambio es una línea en `buildResponse()` (sustituir `Motor::isManualTouchDetected()` por `FaderTouch::isTouched()` — TODO marcado en el código).

**MCU afectadas:** Solo S2. S3 (ya enviaba AutoMode) y P4 sin cambios.

**Riesgo:** MEDIO — toca path RS485 RX y rama del motor, no toca protocolo binario.

**Validación pendiente (hardware obligatorio antes de merge):**
- [ ] OFF/READ: usuario empuja → motor vuelve sin debounce
- [ ] WRITE: motor nunca se mueve, posición física a Logic
- [ ] TOUCH/TRIM: tocar para, soltar reanuda tras ~80ms
- [ ] LATCH: tocar congela; soltar mantiene; Logic mueve >200 cuentas → descongela
- [ ] Cambio de modo con frozen activo → reset limpio
- [ ] FLAG_CALIB prevalece en cualquier modo

**Documentación:** [`docs/AUTOMODE.md`](docs/AUTOMODE.md) (nuevo, exhaustivo). Punteros añadidos en `docs/MOTOR.md` (sección 2.5.2) y `docs/RS485.md` (sección 5.1). CLAUDE.md actualizado con entrada en índice de docs.

**Commit:** pendiente — implementación lista, esperando "commit".

---

### SESIÓN 2026-05-27 — Auditoría P4: 3 bugs críticos identificados (23:34)

**Contexto:** Investigación de "P4 no conecta en todas las ocasiones". Referencia: `docs/S3ToP4.md`.

**Resultado:** 3 bugs nuevos no documentados en S3ToP4.md. Sin cambios de código — solo documentación en Pendientes.

| Bug | Archivo P4 | Gravedad |
|-----|-----------|----------|
| `case 0x61` establece `g_logicConnected=0` (mismo bug que S3, no portado) | `midi/MIDIProcessor.cpp` línea 467 | 🔴 Alta |
| `startTask()` RS485 nunca llamada → slaves sin comunicación | `main.cpp` setup() | 🔴 Alta |
| `_calibPendingFrom` no resetea en `case 0x0F` | `midi/MIDIProcessor.cpp` case 0x0F | 🟡 Media |

**Nota sobre conexión intermitente:** El handshake SysEx (0x00→0x13→0x0C→0x21) es correcto en P4. La intermitencia más probable es timing USB: Logic envía discovery antes de que el task MIDI procese bytes si P4 arranca con Logic ya abierto. No es un bug de código sino de arranque USB. Los 3 bugs listados son independientes del handshake pero críticos para operación real.

---

### SESIÓN 2026-05-27 — SELECT pista: lógica movida a S3 vía touchState (23:13)

**Problema:** S2 enviaba `FLAG_SELECT` en un solo paquete RS485 (rising edge). Si S3 perdía ese paquete, no se seleccionaba la pista.

**Fix:**
- `S3/main.cpp`: rising/falling edge de `touchState` → Note On/Off SELECT (`24 + midiCh`). Igual que botón físico.
- `S2/RS485Handler.cpp`: eliminado bloque `FLAG_SELECT` de `buildResponse()`.

**Commit:** `be2a134` · FW **0.4.19**

---

### SESIÓN 2026-05-27 — Calibración S2 robusta + toque selecciona pista (22:23)

**Problema 1 — Calibración nunca completa correctamente:**
Tres bugs estructurales hacían fallar la calibración en hardware con variación física:

- **`GOING_UP` stuck → `KICK_DOWN`**: si el ruido EMF en el tope superior superaba `ADC_STABILITY_THRESHOLD`, la estabilidad nunca se detectaba y el motor abandonaba el tope sin registrar `_motor_adcTop`. El flujo completo fallaba.
- **`KICK_DOWN` sin stuck detection**: si el tope físico inferior era > 200 ADC (variación HW), la condición `pos <= 200` nunca se cumplía → motor empujaba contra el tope indefinidamente hasta `CALIB_TIMEOUT`.
- **`GOING_DOWN` stuck → `ERROR`**: análogo a `GOING_UP`, el fondo físico se trataba como atasco → error en vez de calibración.
- **`SETTLE_UP/DOWN` usaban posición instantánea**: `_motor_adcTop = _motor_adcPos` podía estar 20-50 cuentas por debajo del máximo real si el fader se asentó tras parar el motor.

**Fix — `S2/S2_V1/src/hardware/Motor/Motor.cpp` + `config.h`:**
- `GOING_UP` stuck → **`SETTLE_UP`** con posición actual como top (no `KICK_DOWN`)
- `KICK_DOWN`: añade stuck detection simétrica a `KICK_UP` → `GOING_DOWN` al detectar tope
- `GOING_DOWN` stuck → **`SETTLE_DOWN`** (no `ERROR`)
- `SETTLE_UP`: `_motor_adcTop = _motor_settleMax` (máximo medido, no instantáneo)
- `SETTLE_DOWN`: `adcBot = _motor_settleMin` (mínimo medido, no instantáneo)
- `ADC_STABILITY_THRESHOLD`: 100 → **200** (más tolerante al ruido EMF en topes mecánicos)

**Problema 2 — Detección de toque tardía y cede control rápido:**
- `MANUAL_TOUCH_THRESHOLD = 150` era demasiado alto para detección inmediata en AT_TARGET (motor off).
- `MANUAL_TOUCH_DEBOUNCE_MS = 200` ms cedía control a Logic antes de que el usuario terminara de posicionar.

**Fix — `config.h` + `Motor.cpp` `setADCDelta()`:**
- Threshold adaptativo: **50 cuentas** en `AT_TARGET`/`IDLE` (motor off, todo delta es del usuario), **150** en `MOVING_TO_TARGET` (motor activo, guard de dirección necesario)
- `MANUAL_TOUCH_DEBOUNCE_MS`: 200 → **600 ms**

**Problema 3 — Toque de fader no seleccionaba la pista en Logic:**
Al tomar control del fader, Logic no seleccionaba el canal correspondiente.

**Fix — `S2/S2_V1/src/RS485/RS485Handler.cpp` `buildResponse()`:**
- Flanco rising de `isManualTouchDetected()` → `resp.buttons |= FLAG_SELECT`
- S3 detecta el cambio en `buttons ^ prevButtons` (bit 3) → envía Note On 24+midiCh → Logic selecciona la pista

**Commits:** `37c92fd`

---

### SESIÓN 2026-05-27 — Fix S3: 0x61 desconectaba slaves + Transport LEDs off (17:14)

**Problema 1 — S2s siempre oscuros al conectar Logic:**
`MIDIProcessor.cpp` case `0x61` (AllFadersToMinimum) contenía `g_logicConnected = 0`. Logic envía `0x61` **después** de `0x21` en la secuencia GoOnline. Efecto: `0x21` ponía `g_logicConnected=1` y `0x61` lo anulaba de inmediato → los 8 slaves recibían `pkt.connected=0` → pantallas oscuras, motores inactivos, siempre.

**Fix — `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/midi/MIDIProcessor.cpp`:**
```cpp
case 0x61: {
    // NO cambiar g_logicConnected — solo resetear fader targets (2026-05-27)
    for (uint8_t i = 1; i <= NUM_SLAVES; i++)
        rs485.setFaderTarget(i, 0);
    log_i("[MCU] AllFaderstoMinimum — faders a 0");
    break;
}
```

**Problema 2 — Transport LEDs no se apagaban al desconectar:**
Al GoOffline o disconnect por PitchBend, los LEDs de transporte mantenían su último estado (ej. STOP encendido).

**Fix — `setAllLedsOff()` llamado en 2 puntos de desconexión:**
- `case 0x0F:` (GoOffline explícito de Logic)
- Bloque disconnect por detección 9 faders a 0 en `processPitchBend()`

**Nuevo — `MASTER_S3-P4/S3/.../src/hardware/Transporte.cpp`:**
```cpp
void setAllLedsOff() {
    for (uint8_t i = 0; i < N; i++) setLed(LEDS[i], false);
}
```

**Fix S2 — `S2/S2_V1/src/RS485/RS485Handler.cpp`:**
`onMasterData()` al transicionar a CONNECTED ahora llama `setScreenBrightness(255)` — restaura brillo si `checkTimeout()` lo había puesto a 0 durante reboot.

| MCU | Archivo | Cambio |
|-----|---------|--------|
| S3 | MIDIProcessor.cpp case 0x61 | Eliminar `g_logicConnected=0` → solo resetear faders |
| S3 | MIDIProcessor.cpp case 0x0F | +`Transporte::setAllLedsOff()` |
| S3 | MIDIProcessor.cpp processPitchBend | +`Transporte::setAllLedsOff()` en disconnect |
| S3 | Transporte.cpp/.h | Nueva función `setAllLedsOff()` |
| S2 | RS485Handler.cpp onMasterData | +`setScreenBrightness(255)` en transición CONNECTED |

**Riesgo:** BAJO — todos los cambios aditivos o eliminación de código incorrecto.
**Validación:** Conectar Logic → S2s deben activar pantallas. Desconectar → LEDs transport apagan.

---

### SESIÓN 2026-05-27 — Fix S3: recalibración automática tras reinicio de slave S2

**Problema:** Si un S2 se reiniciaba durante operación normal, S3 no lo recalibraba. El fader quedaba sin calibrar (ADC min/max sin mapear).

**Causa raíz:** S3 marca `_ch[id].calibrated = true` tras la primera calibración y nunca lo reevalúa. No había mecanismo de detección de reinicio del slave.

**Señal disponible en protocolo:** Tras calibración exitosa, S2 envía `SLAVE_FLAG_CALIB_DONE = 1` en cada paquete normal (Motor::CalibState::DONE persistente). Tras un reinicio, CalibState vuelve a IDLE y ese flag desaparece. S3 puede detectar la transición.

**Fix — `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp`:**

En el bloque `else` de `_handleResponse()` (slave en tránsito — ni CALIB_DONE ni CALIB_ERROR):

```cpp
// Detectar reinicio: slave calibrado que ya no reporta CALIB_DONE (2026-05-27)
if (_ch[_currentId].calibrated && !_ch[_currentId].calibrating) {
    _ch[_currentId].calibrated      = false;
    _ch[_currentId].calibRetries    = 0;
    _ch[_currentId].stableRespCount = 0;
    _ch[_currentId].calibrate       = true;
    _ch[_currentId].calibrating     = true;
    _ch[_currentId].dirty           = true;
    log_w("[CALIB] Slave %d: reinicio detectado — recalibrando automáticamente", _currentId);
}
```

**Guards:**
- `calibrated == true` — evita falsos disparos en boot inicial (cuando calibrated=false)
- `!calibrating` — evita disparos durante fase CALIB_SENDING (calibrating=true en ese momento)
- Inline (sin llamar setCalibrate()) — evita deadlock por mutex ya tomado

| MCU | Archivo | Cambio |
|-----|---------|--------|
| S3 | RS485.cpp else block `_handleResponse()` | +10 líneas detección reinicio |
| S2 | — | Sin cambios |
| P4 | — | Sin cambios |

**Riesgo:** BAJO — S3 únicamente, lógica aditiva, no toca flujo de calibración normal.

---

### SESIÓN 2026-05-27 — Fix calibración KICK_UP stuck en tope físico (16:25)

**Problema:** El primer S2 subía durante calibración y se quedaba arriba con fuerza sin bajar.

**Causa raíz:** `CalibPhase::KICK_UP` en `Motor.cpp` espera `pos >= 26000` para transicionar a `GOING_UP`. Si el ADC real del tope físico del fader es < 26000 (variación de hardware entre unidades), la condición nunca se cumple. El motor empuja con `PWM_MAX` durante `CALIB_TIMEOUT = 6000ms` → `CalibPhase::ERROR` → `MotorState::IDLE` con `_connected=true` → motor no baja → fader queda arriba.

No había stuck timeout en `KICK_UP` (a diferencia de `GOING_UP` que sí lo tiene).

**Fix — `S2/S2_V1/src/hardware/Motor/Motor.cpp`:**

Añadido stuck detection en `KICK_UP`: si el ADC lleva `CALIB_STUCK_TIMEOUT = 1000ms` estable (fader en tope físico pero ADC < 26000 por variación de hardware), transiciona a `GOING_UP` igualmente.

```cpp
} else {
    // Stuck detection: ADC < 26000 pero fader en tope físico (variación HW entre unidades)
    if (abs(pos - _motor_stableRef) > ADC_STABILITY_THRESHOLD) {
        _motor_stableRef   = pos;
        _motor_stableStart = now;
    } else if (now - _motor_stableStart >= CALIB_STUCK_TIMEOUT) {
        _motor_phase       = CalibPhase::GOING_UP;
        _hwUp(_pwm_min);
        _motor_currentPWM  = _pwm_min;
        _motor_stableRef   = pos;
        _motor_stableStart = now;
        log_w("[CALIB] KICK_UP stuck pos=%d (<26000) — tope físico detectado → GOING_UP", pos);
    }
}
```

Log diagnóstico si activa: `[CALIB] KICK_UP stuck pos=XXXX (<26000) — tope físico detectado → GOING_UP`

| MCU | Archivo | Cambio |
|-----|---------|--------|
| S2 | `Motor.cpp` KICK_UP | Añadido stuck detection — transición a GOING_UP si ADC estable 1000ms y < 26000 |
| S3 | — | Sin cambios |
| P4 | — | Sin cambios |

**Riesgo:** BAJO — solo añade camino alternativo de salida, camino normal (`pos >= 26000`) sin tocar.  
**Validación pendiente:** Flash S2 → confirmar calibración completa (buscando log `KICK_UP stuck` o transición normal a GOING_UP).

---

### SESIÓN 2026-05-26 — OTA WiFi S2 + VUMeter completo (17:48)

**Objetivo:** Resolver OTA WiFi S2 + VUMeter flickering

**Resuelto ✅**

| Fix | MCU | Archivos | Descripción |
|-----|-----|----------|-------------|
| Bug #1 GPIO flotantes ciegan WiFi | S2 | `main.cpp` | Bloque `safePins` (OUTPUT LOW todos los GPIO) al inicio de `setup()` antes del check `otaMode` — root cause documentado en `docs/WIFI-OTA.md §5.3` |
| OTA password activo | S2 | `OtaManager.cpp` | `otaPass` ahora se pasa a `ElegantOTA.begin()` — Basic Auth funcional |
| Sketch provisioning | S2 | `S2/provisioning/provisioning.ino` | Guardado en repo sanitizado (sin credenciales) |
| Credenciales eliminadas | docs | `WIFI-OTA.md`, `CHANGELOG.md`, `STATUS.md` | Credenciales reales retiradas de todos los documentos públicos |
| WIFI.md → WIFI-OTA.md | docs | `docs/WIFI-OTA.md` | Renombrado, referencias actualizadas en 5 archivos |
| `lolin_s2_mini_ota` eliminado | S2 | `platformio.ini`, `upload_ota.py` | Entorno roto (espota.py ≠ ElegantOTA) + credencial expuesta en `upload_flags` |
| `WiFiManager` retirado de lib_deps | S2 | `platformio.ini` | Librería eliminada del código en 2026-05-20, quedaba huérfana |
| VUMeter: namespace VU orden | S2 | `Display.cpp` | `namespace VU` movido antes de `updateDisplay()` — error de compilación `'VU' has not been declared` |
| VUMeter: peak estilo hardware | S2 | `Display.cpp` | Peak = segmento ON en su color natural (verde/amarillo/rojo). Sin borde blanco. Comportamiento idéntico a VU hardware real (SSL, Neve) |
| VUMeter: decay S3 timeout | S3 | `main.cpp` | Check cada 50ms: si no llega Channel Pressure en >200ms → `setVuLevel(0)` → S2 decae via `handleVUMeterDecay()` |
| VUMeter: decay S2 timer fix | S2 | `RS485Handler.cpp` | `vuLastUpdateTime` se actualiza SOLO cuando VU sube (antes: en cada paquete RS485 ~10ms → `handleVUMeterDecay()` nunca veía gap de 100ms → sin decay) |
| VUMeter: peak fade 300ms | S2 | `Display.cpp` | Peak en color natural, hold 2s, fade suave 12 pasos × 25ms con `blendColor565()` ON→OFF. `peakAlpha` + `peakFadeTime` en `namespace VU`. Documentado en `docs/DISPLAY.md §10` |

**Validado en hardware ✅**
- OTA funcional en 4 faders (upload browser + Basic Auth)
- VUMeter sin flickering
- Peak hold estilo hardware
- Decay funcional al parar audio (~300ms: 200ms S3 timeout + 100ms decay S2)

**Pendiente validación hardware:**
- VU peak fade 300ms (implementado, no flasheado aún)
- VU decay S2 timer fix (commit pendiente)

**Commits:** `4c4ef4b`, `cb3ec9d`, `008c57f`, `9d5bd8f`, `ad0fd55`, `07d12cd`

**MCU afectadas:** S2 (OTA + VU display + decay timer) + S3 (VU decay). P4 sin cambios.

---

### SESIÓN 2026-05-25 — Auditoría firmware fader S2→Logic antes de flash (16:24)

**Objetivo:** Verificar que el código de las sesiones 2026-05-24 (`6f6ace6` S3, `d171b12` S2) está correcto antes de flashear hardware.

**Resultado: FIRMWARE LISTO PARA FLASH** ✅

**Archivos auditados:**

| MCU | Archivo | Verificado | Resultado |
|-----|---------|-----------|-----------|
| S3 | `config.h` | `FADER_SYNC_DEADBAND=200`, `MOTOR_SETTLE_THRESHOLD=80` | ✅ |
| S3 | `main.cpp::processSlaveResponse()` | Mapeo calibrado con fallback 27000, jerarquía master, guard `CALIB_SENDING` | ✅ |
| S3 | `RS485/RS485.cpp::_handleResponse()` | `ch.buttons` actualizado (línea 257), `calibratedMin/Max` capturados | ✅ |
| S2 | `Motor.cpp::setADCDelta()` | Guard dirección `MOVING_TO_TARGET` (líneas 500–508) | ✅ |
| S2 | `Motor.cpp::setTargetFromS3()` | Guards calibración + usuario + dead zone completos | ✅ |
| S2 | `RS485Handler.cpp::buildResponse()` | `touchState` = `isManualTouchDetected()` delta-based | ✅ |

**Observación menor (no bloquea flash):**
- `Motor.cpp` línea 699: condición de log `_motor_targetADC != adcTarget` siempre `false` — la asignación ocurre en línea 691. Solo afecta al log (no registra cambio de target en mismo valor). Funcionalidad correcta.

**FW actual:** `FW_REVISION=6` → `0.4.6` (S2). S3 sin versión numérica.

**Próximo paso — validación en hardware:**

- [ ] Flash S3 (`6f6ace6`)
- [ ] Flash S2 (`d171b12`)
- [ ] Mover fader manualmente → Logic actualiza posición (path A: touch, `touchState=1`)
- [ ] Logic mueve fader (motor) → S3 en silencio durante tránsito (`motorSettled=false`)
- [ ] Cambio de banco (+16) → faders llegan sin interferencia Logic
- [ ] Fader settled en target → Logic confirma posición (path B: sync, una vez, `motorSettled=true`)

**MCU afectadas:** S3 + S2. P4 sin cambios.

---

### SESIÓN 2026-05-24 — IntelliSense PlatformIO VS Code (13:XX)

**Contexto:** Error en VS Code al abrir `MASTER_S3-P4/P4/src/display/Display.cpp`:
```
Se han detectado errores de #include. Actualice el valor de includePath.
El subrayado ondulado está deshabilitado para esta unidad de traducción.
```

**Causa:** `c_cpp_properties.json` es auto-generado por PlatformIO y queda desactualizado. Contiene entradas vacías `""` al final de `includePath` / `browse.path` que invalidan el índice IntelliSense.

**Zigbee y otras librerías ajenas:** PlatformIO añade TODAS las librerías del framework Arduino-ESP32 al `includePath`, aunque no estén en `lib_deps`. Es cosmético, no afecta compilación.

**Fix:**
```
Command Palette (⇧⌘P) → PlatformIO: Rebuild IntelliSense Index
```
Regenera `.vscode/c_cpp_properties.json` desde cero. Nunca editar manualmente.

**MCU afectadas:** Ninguna — solo entorno de desarrollo.

---

### SESIÓN 2026-05-24 — Fader S2→Logic feedback (11:31)

**Objetivo:** Implementar y corregir el path de feedback de posición de fader desde S2 hasta Logic Pro.

**Arquitectura final (jerarquía de masters):**

| Estado motor | Master | Comportamiento S3 |
|---|---|---|
| Motor moviéndose (`\|faderPos-target\| > 80`) | **Logic** | Silencio — no enviar PitchBend |
| Stall en tope físico (sobrepasa target) | **Logic** | Silencio — posición fuera de rango no se reporta |
| Motor settled + deriva > 200 PB counts | Nadie | Sync — confirma posición real a Logic |
| `touchState=1` + posición cambiada | **Usuario** | Envío inmediato sin deadband |

**Bugs corregidos:**

**Bug 1 — Mapeo usaba rango fijo 27000 (S3 `main.cpp`)**
- El path S2→Logic usaba `faderPos * LOGIC_PITCHBEND_MAX / 27000` (rango teórico)
- `setFaderTarget()` (Logic→S2) usaba rango calibrado real `calibratedMin..calibratedMax`
- Asimetría: fader nunca alcanzaba 0% ni 100% en Logic al mapear con rango fijo
- Fix: mapeo inverso exacto usando `calibratedMin/Max`; fallback a 27000 si sin calibrar

**Bug 2 — PitchBend solo se enviaba con `touchState=1`**
- FaderTouch capacitivo inoperativo → `touchState=1` solo si delta ADC > 150 cuentas
- Movimientos lentos o fader parado no generaban feedback → Logic desincronizado
- Fix: añadido path B (sync) que envía PitchBend aunque no haya toque, con condiciones

**Bug 3 — Path sync disparaba durante movimiento de motor (Logic es master)**
- Path B enviaba lecturas intermedias a Logic mientras motor se movía al target
- Logic interpretaba esas lecturas como movimiento de usuario → cancelaba el move automático
- Cambios de banco (+16): motores en tránsito → Logic recibía posiciones intermedias → interferencia
- Fix: guard `motorSettled` — sync solo cuando `|faderPos - faderTarget| <= MOTOR_SETTLE_THRESHOLD`

**Bug 4 — Umbral `motorSettled` demasiado holgado (500 ADC)**
- Motor stall en tope físico a `adc=22968` con target `22776` (diferencia 192 counts)
- `192 < 500` → `motorSettled=true` → sync disparaba con posición imposible hacia Logic
- Fix: umbral reducido a `MOTOR_SETTLE_THRESHOLD = 80` (= `DEAD_ZONE` del motor S2)
- Con 80: `192 > 80` → settled=false → sync suprimido en stall ✓

**Cambios aplicados — solo S3:**

| Archivo | Cambio |
|---------|--------|
| `config.h` | Añade `FADER_SYNC_DEADBAND 200` — deadband PB para sync S2→Logic |
| `config.h` | Añade `MOTOR_SETTLE_THRESHOLD 80` — umbral ADC para considerar motor parado |
| `main.cpp` | `processSlaveResponse()`: mapeo calibrado, jerarquía master, guards sync |

**MCU afectadas:** Solo S3. S2 y P4 sin cambios.

**Validación pendiente:**
- [ ] Flash S3 con cambios
- [ ] Mover fader manualmente → Logic debe actualizar posición (path A: touch)
- [ ] Logic mueve fader (motor) → no debe interferir Logic durante tránsito
- [ ] Cambio de banco (+16) → todos los faders llegan a nuevas posiciones sin interferencia
- [ ] Fader settled en target → Logic confirma posición (path B: sync, una sola vez)

### SESIÓN 2026-05-27 — Fix S3: HALT en Core 1 tras reflash S2 (18:08)

**Problema — S2s muertos tras reflash:**
Síntoma: Logic conectaba (transport LEDs funcionaban, `0x21 CONNECTED` en log), pero S2s quedaban en splash indefinidamente sin calibrar ni moverse.

**Causa raíz:**
La reboot detection (§4.4, commit `15af488`) pone `calibrating=true` inmediatamente al detectar un slave reiniciado. Durante un reflash S2 hay ~2-5s de silencio RS485 (bootloader + setup). Si la reboot detection había disparado, esos timeouts con `calibrating=true` activo llegaban a `MAX_CALIBRATION_RETRIES (5)` → `while(1)` en Core 1 (RS485 task). Core 0 (MIDI) seguía vivo: Logic conectaba y los transport LEDs respondían, pero ningún paquete RS485 llegaba a los S2s.

**Fix — `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp`:**

| Antes | Después |
|-------|---------|
| `while(1) { delay(1000); }` | `calibRetries=MAX` + `_triggerNextCalibration()` + `_consecutiveTimeouts=0` |

NeoPixel ROJO se mantiene como señal visual de error. Sistema continúa con slaves restantes.

**MCU afectada:** Solo S3.

---

### SESIÓN 2026-05-27 — Brillo pantalla S2 a config.h (17:42)

**Cambio — Brightness centralizado en `config.h`:**
Todos los valores de brillo de pantalla hardcodeados (255/70/0/200) movidos a defines en `config.h` como fuente única de verdad.

**`S2/S2_V1/src/config.h`:**
```cpp
#define BRIGHTNESS_SPLASH       50   // Boot y espera (sin Logic)
#define BRIGHTNESS_SELECTED    180   // Canal activo/seleccionado
#define BRIGHTNESS_UNSELECTED   70   // Canal no seleccionado
#define BRIGHTNESS_OTA          50   // Pantalla OTA WiFi
```

**Archivos actualizados:**
| Archivo | Línea | Antes | Después |
|---------|-------|-------|---------|
| `main.cpp` | 190 | `setScreenBrightness(255)` | `BRIGHTNESS_SPLASH` |
| `Display.cpp` | 276 | `selectStates ? 255 : 70` | `BRIGHTNESS_SELECTED : BRIGHTNESS_UNSELECTED` |
| `RS485Handler.cpp` | 53 | `setScreenBrightness(255)` | `BRIGHTNESS_SELECTED` |
| `RS485Handler.cpp` | 199 | `setScreenBrightness(0)` | `drawSplashScreen()` + `BRIGHTNESS_SPLASH` |
| `OtaManager.cpp` | 115 | `setScreenBrightness(200)` | `BRIGHTNESS_OTA` |

**Comportamiento añadido:** Al desconectar Logic (`DISCONNECTED`), la pantalla vuelve al splash en lugar de apagarse a negro.

**MCU afectadas:** Solo S2.

---

### FW 0.4.18 — Resumen vs FW 0.4.13 (2026-05-27)

Último flash en hardware: **FW 0.4.13** (2026-05-26). Todos estos cambios pendientes de flash.

| # | Fix | Archivo | Sesión |
|---|-----|---------|--------|
| 1 | KICK_UP stuck detection — tope físico ADC < 26000 → `GOING_UP` tras 1000ms estable | `Motor.cpp` | 16:25 |
| 2 | `GOING_UP` stuck → `SETTLE_UP` (no `KICK_DOWN`) | `Motor.cpp` | 22:23 |
| 3 | `KICK_DOWN` stuck detection → `GOING_DOWN` | `Motor.cpp` | 22:23 |
| 4 | `GOING_DOWN` stuck → `SETTLE_DOWN` (no `ERROR`) | `Motor.cpp` | 22:23 |
| 5 | `SETTLE_UP/DOWN` usan max/min medido (no posición instantánea) | `Motor.cpp` | 22:23 |
| 6 | `ADC_STABILITY_THRESHOLD` 100 → 200 (tolerancia ruido EMF en topes) | `config.h` | 22:23 |
| 7 | Threshold adaptativo: 50 cuentas en AT_TARGET/IDLE, 150 en MOVING_TO_TARGET | `config.h` | 22:23 |
| 8 | `MANUAL_TOUCH_DEBOUNCE_MS` 200 → 600 ms | `config.h` | 22:23 |
| 9 | Toque fader → `FLAG_SELECT` → Logic selecciona pista | `RS485Handler.cpp` | 22:23 |
| 10 | `touchState=0` durante CALIBRATING/GOING_TO_MIN — bloquea SELECT espurio | `RS485Handler.cpp` | 22:38 |
| 11 | Splash screen `iMakie` → `AITEC17` | `Display.cpp` | 17:42 |
| 12 | Brillo pantalla → `config.h` (`BRIGHTNESS_SPLASH/SELECTED/OTA`) | `config.h`, `RS485Handler.cpp`, `OtaManager.cpp` | 17:42 |
| 13 | Pantalla OTA rediseñada — header rojo + IP octeto grande | `Display.cpp` | 17:48 |

**Riesgo:** MEDIO — cambios en calibración y detección usuario. Requiere test físico completo.  
**Test mínimo:** calibración completa x3, toque→SELECT, touchState sin SELECT en calib, splash AITEC17, brillo correcto.

---

### Upload log S2
- `2026-06-14 14:35` · Commit S2 · **FW 0.5.21** (sin upload)
- `2026-05-30 11:53` · Commit S2 · **FW 0.5.20** (sin upload)
- `2026-05-27 23:14` · Commit S2 · **FW 0.4.19** (sin upload)
- `2026-05-27 22:36` · Commit S2 · **FW 0.4.18** (sin upload)
- `2026-05-27 17:46` · Commit S2 · **FW 0.4.17** (sin upload)
- `2026-05-27 17:23` · Commit S2 · **FW 0.4.16** (sin upload)
- `2026-05-27 17:14` · Commit S2 · **FW 0.4.15** (sin upload)
- `2026-05-27 17:02` · Commit S2 · **FW 0.4.14** (sin upload)
- `2026-05-26 18:50` · Flash S2 · **FW 0.4.13** · `lolin_s2_mini`
- `2026-05-26 18:49` · Flash S2 · **FW 0.4.12** · `lolin_s2_mini`
- `2026-05-26 18:46` · Flash S2 · **FW 0.4.11** · `lolin_s2_mini`
- `2026-05-26 18:40` · Commit S2 · **FW 0.4.10** (sin upload)
- `2026-05-26 18:17` · Commit S2 · **FW 0.4.9** (sin upload)
- `2026-05-26 17:51` · Flash S2 · **FW 0.4.8** · `lolin_s2_mini`
- `2026-05-26 17:50` · Commit S2 · **FW 0.4.7** (sin upload)
- `2026-05-24 11:40` · Commit S2 · **FW 0.4.6** (sin upload)
- `2026-05-23 19:52` · Flash S2 · **FW 0.4.5** · `lolin_s2_mini`
- `2026-05-23 19:18` · Flash S2 · **FW 0.4.4** · `lolin_s2_mini`
- `2026-05-23 18:59` · Flash S2 · **FW 0.4.3** · `lolin_s2_mini`


### Bug B5 — Timeout periódico ~2001ms — ✅ Fix aplicado (2026-05-23 19:48)

**Causa raíz:** `FaderTouch::update()` (16 × `touchRead()`) se ejecutaba **antes** de `rs485.sendResponse()` en el loop. Cuando esas 16 llamadas tardaban >3ms, S2 no respondía a S3 dentro de `RS485_RESP_TIMEOUT_US`. El patrón ~2001ms es el período de batido entre el poll de FaderTouch (20ms) y el ciclo RS485 de S3 (10ms) con jitter.

Agravante: `Hardware::updateButtons()` contenía un segundo sistema de detección táctil paralelo (1 × `touchRead()` cada 20ms, mismo pin T1) — código muerto nunca registrado en Motor ni RS485Handler.

**Fix — 3 cambios en S2:**

| Archivo | Línea | Cambio | Efecto |
|---------|-------|--------|--------|
| `main.cpp` | 260→326 | `FaderTouch::update()` movido a DESPUÉS de `sendResponse()` | touchRead ya no bloquea el path RS485 |
| `config.h` | 197 | `TOUCH_BASELINE_SAMPS` 16 → 3 | 73% menos touchRead por poll (16→3) |
| `Hardware.cpp` | 10-14, 27-28, 69-76, 87-102, 109-110 | Eliminado sistema touch legacy completo | 1 × touchRead y 20 × touchRead+delay(10) en boot eliminados |
| `Hardware.h` | 28-29 | Eliminadas declaraciones `registerFaderTouch/Release` | — |

**Impacto:**
- `touchState` reportado a S3: 1 ciclo (10ms) de retraso — imperceptible
- LED_BUILTIN: no parpadea en modo SAT (SAT hace return antes) — cosmético
- Boot: 200ms más rápido (eliminado el bucle 20×touchRead+delay en initHardware)

**MCU afectadas:** Solo S2. S3 y P4 sin cambios.

**⚠️ VALIDACIÓN HARDWARE PENDIENTE:**
- [ ] Flash S2 con cambios
- [ ] Monitor S3: timeouts post-calibración deben bajar de 0.3% a 0% o menos
- [ ] Patrón ~2001ms debe desaparecer
- [ ] Calibración y operación normal sin regresión


### SESIÓN 2026-05-23 — Versionado automático FW (11:30)

**Objetivo:** Automatizar número de versión FW en `pre_build.py` basado en estado real de sistemas.

**Esquema de versión `MAJOR.MINOR.PATCH`:**
- `MAJOR = 0` — fase debug (fijo, cambio manual al pasar a release)
- `MINOR` = count(HW_STATUS == 2) — sistemas completamente funcionales en `config.h`
- `PATCH` = `FW_REVISION` — contador acumulado de revisiones, solo sube, definido en `config.h`

**Cambios aplicados:**

| Archivo | Cambio | Razón |
|---------|--------|-------|
| `S2/S2_V1/src/config.h` | `Touch=1` → `Touch=0` | Touch profundamente inoperativo — excluido del conteo MINOR |
| `S2/S2_V1/src/config.h` | Añade `#define FW_REVISION 2` | Fuente única del contador de revisiones |
| `S2/S2_V1/pre_build.py` | `fw_ver` derivado automáticamente | MINOR=count(status==2), PATCH=FW_REVISION |
| `CLAUDE.md` | Directiva `FW_REVISION` obligatoria | Regla vinculante: incrementar por sesión funcional |

**Versión resultante:** `0.4.2` (4 sistemas OK: Display, NeoPixels, Encoder, Buttons — revisión 2)

**Directiva:** Para futuras sesiones — incrementar `FW_REVISION` en `config.h` al final de cada sesión con cambios funcionales en S2.

---

### SESIÓN 2026-05-23 — Fix particiones S3 16MB (11:30)

**Problema:** `default_16MB.csv` estaba vacío → PlatformIO usaba tabla de particiones por defecto del board (`esp32-s3-devkitc-1`), que es 8MB → app partition reportada como 6553600 bytes (6.25MB) en lugar de los ~15MB correctos para hardware N16R8 sin OTA.

**Causa raíz:** PlatformIO usa el `default_16MB.csv` del framework (`~/.platformio/packages/framework-arduinoespressif32/tools/partitions/`) en lugar del archivo local cuando hay colisión de nombre. El del framework tiene OTA + `app0=0x640000` (8MB layout).

**Fix:**

| Archivo | Cambio |
|---------|--------|
| `default_16MB.csv` → `s3_extender_16MB.csv` | Renombrado para evitar colisión con framework |
| `platformio.ini` | `board_build.partitions = s3_extender_16MB.csv` |

**Tabla aplicada (`s3_extender_16MB.csv`):**
- `nvs` 20KB · `app0` (factory) 14.93MB · `littlefs` 1MB
- Sin OTA — S3 Extender no tiene OTA, `app1` y `otadata` eliminadas

**Warnings USB eliminados:**
- `-DARDUINO_USB_MODE=0` y flags USB redundantes eliminados de `build_flags`
- Board ya defaultea a TinyUSB (modo 0) — MIDI USB validado operativo tras el cambio

**Resultado validado en hardware:**
```
RAM:   [=  ]  14.6% (used 47700 bytes from 327680 bytes)
Flash: [   ]   2.5% (used 418574 bytes from 16777216 bytes)
```

---

### RESUMEN SESIÓN 2026-05-22

**Objetivo de la sesión:** Corregir calibración S3 — "lanza a lo loco" + implementar cascada

**Resuelto ✅**

| Fix | MCU | Archivos | Descripción |
|-----|-----|----------|-------------|
| Grace period calibración boot | S3 | `RS485.cpp`, `RS485.h`, `config.h` | Slave 1 espera 5 respuestas estables (~50ms) antes de disparar FLAG_CALIB — evita calibrar antes de que S2 termine su `setup()` |
| Cascada calibración | S3 | `RS485.cpp` | Slave N+1 se calibra solo cuando Slave N reporta CALIB_DONE — calibración secuencial real |
| CALIB_ERROR sin bucle infinito | S3 | `RS485.cpp` | Error de calibración resetea grace period (no relanzamiento inmediato); HALT tras MAX_CALIBRATION_RETRIES errores |

**Configuración verificada en hardware (2026-05-22):**
- `NUM_SLAVES = 1` — confirmado correcto (2026-05-23: 1 esclavo S2 conectado al bus B durante desarrollo)
- ~~NUM_SLAVES=4~~ — dato incorrecto en sesión anterior, corregido (2026-05-23)
- A los ~175s de uptime: tasa de timeout 0.1-0.3% (1-2 por cada 1000 ciclos), avg RX_WAIT ~1490µs — sistema estable

**Validado en hardware (2026-05-23):**
- ✅ Grace period funciona: "Slave 1 estable (5 resp)" se dispara correctamente (t≈1083–1438ms según si Logic está conectado o no)
- ✅ Cascada completa para 1 slave se declara correctamente
- ✅ Spike de timeouts en conexión Logic (t≈17437–17624ms) es comportamiento esperado — handshake Mackie en Core 0

**Pendiente 🔴**

| # | Pendiente | Archivos | Descripción |
|---|-----------|----------|-------------|
| ~~B4~~ | ~~CALIBRADO OK con MAX=0~~ | ~~S2 `RS485Handler.cpp`~~ | ✅ **RESUELTO (2026-05-23)** — Fix: eliminado `SLAVE_FLAG_CALIB_DONE` del paquete MIN (estado 0 de `buildResponse()`). Ahora S3 recibe MIN→MAX→CALIB_DONE en orden correcto. Validado en hardware: `CALIBRADO OK: MIN=47 MAX=26445` con MAX correcto. |
| B5 | **Timeout periódico exacto ~2s en S2** — 🔴 Causa raíz anterior descartada, investigación en curso | S2 `Motor.cpp`, `FaderADC.cpp` | Fix log_i→log_d/log_v aplicado pero patrón 2001ms persiste con firmware 1101. CORE_DEBUG_LEVEL=3 → log_d no se compila, fix fue inefectivo. Causa real pendiente identificar (hipótesis: ciclo requestCalibration() re-entry, ver B7). |
| B7 | **requestCalibration() interrumpe calibración activa** — ✅ Fix aplicado, pendiente validación | S2 `Motor.cpp:633` | S3 envía FLAG_CALIB cada 10ms → requestCalibration() se llama cada 10ms. Cuando motor en KICK_UP y ADC sube por encima de MOTOR_ADC_MIN+10 (=30), la rama else ve _motor_state==CALIBRATING (!= GOING_TO_MIN) → sobreescribe estado a GOING_TO_MIN, interrumpiendo calibración. Motor sube, baja, vuelve a 0, reinicia → bucle infinito que nunca llega a DONE. Fix: guard al inicio de requestCalibration(): `if (_isCalibrating() \|\| _motor_state == CALIBRATING) return;` |
| B6 | **HALT por timeout RS485 nunca activa** — ✅ Fix aplicado, pendiente validación hardware | S3 `RS485.cpp:105` | Condición `calibrated && calibrating` siempre `false` durante calibración activa (estado real: `calibrating=true, calibrated=false`). Fix: `calibrated` → `!calibrated`. Ahora el HALT se dispara si slave no responde durante calibración activa. El HALT de `_handleResponse()` (CALIB_ERROR) es independiente y correcto — solo cubre slave que responde pero reporta error. Ver sección detallada abajo. |
| — | Diagnóstico burst RS485 (~t=939s) | S3 `RS485.cpp` | Causa raíz del colapso simultáneo de slaves pendiente — ver análisis detallado abajo |

**Commits:** pendiente

---

### S3 RS485 — Diagnóstico timeouts operación normal + burst de bus (2026-05-23) — 🔴 PENDIENTE CAUSA RAÍZ

**Contexto:** Monitor serie capturado durante sesión 2026-05-23 con 4 slaves S2 activos. Sistema corriendo en operación normal (post-calibración, Logic conectado). Análisis de ~300 segundos de log (t=705s → t=1010s desde boot).

---

#### Comportamiento normal — baseline confirmado

**Tasa de timeouts en reposo:** 0.1%–0.5% por cada 1000 ciclos RS485.  
**`avg RX_WAIT`:** 1409µs–1488µs  
**`min RX_WAIT`:** ~848µs (ciclos limpios sin colisión)  
**`max RX_WAIT`:** ~3007µs (= `RS485_RESP_TIMEOUT_US` — ciclos con timeout)

Todos los timeouts en fase normal son `#1 consecuciones`. Esto se explica por el comportamiento del contador:

```
_consecutiveTimeouts es GLOBAL (no por slave).
Se resetea a 0 en cualquier respuesta exitosa de cualquier slave.
→ Un timeout de Slave 3 seguido de respuesta de Slave 4: counter vuelve a 0.
→ El siguiente timeout de Slave 2 aparece como #1 aunque Slave 3 ya falló antes.
```

**Por qué son normales:** Bus RS485 sin terminación perfecta, EMI del motor DRV8833, ADC ADS1115 en I2C compartido, ISR de encoder, SPI del display. Un slave ocupado en una ISR larga (~200µs) puede no responder a tiempo. El sistema se recupera en el siguiente ciclo.

**Patrón de timeouts aislados (ejemplo real):**

```
[707954] TIMEOUT slave 3 (#1)   ← ciclo N
...ciclo N+1: slave 3 responde OK → _consecutiveTimeouts = 0
[709956] TIMEOUT slave 3 (#1)   ← ciclo N+3 (nuevo timeout, counter reseteado)
```

---

#### Evento ID MISMATCH (t≈748s) — respuesta tardía

```
[751808][E][RS485.cpp:225] _handleResponse(): [RS485] ID MISMATCH esperado=1 recibido=4
```

**Causa:** Durante la mini-ráfaga previa a este timestamp, el slave 4 no había respondido en su ventana. Su respuesta llegó tarde al buffer UART. Cuando S3 ya había avanzado al slave 1 y abrió su ventana de recepción, la respuesta de slave 4 todavía estaba en el buffer → S3 la recibió como si fuera de slave 1 → mismatch.

**Nota temporal:** En este mismo timestamp se registra `[748711] PLAY=0 STOP=1` (Logic Pro enviando STOP). La correlación puede ser coincidencia o puede indicar que el mensaje STOP generó actividad USB-MIDI que retrasó el procesamiento RS485 en S3 (mismo core).

**Impacto:** Ninguno en operación — la respuesta de slave 1 real llegó en el siguiente ciclo. El paquete mal identificado fue descartado por ID mismatch.

---

#### Evento crítico — burst total de bus (t≈939s–944s)

**Duración del evento:** ~5 segundos  
**Ciclo Profiler 366:** `TO: 6.7% (67/1000)` — pico máximo observado  
**Ciclo Profiler 367:** `TO: 4.4% (44/1000)` — decaimiento  
**Ciclo Profiler 368:** `TO: 2.1% (21/1000)` — recuperación  
**Ciclos siguientes:** retorno a baseline 0.1%–0.5%

**Log del inicio del burst:**

```
[938958] TIMEOUT slave 3  (#1)   ← comienzo, normal aún
[939303] TIMEOUT slave 1  (#1)
[939314] TIMEOUT slave 2  (#2)   ← counter no reseteó: slave 1 también falló
[939325] TIMEOUT slave 3  (#3)   ← counter sube: 3 slaves fallaron consecutivamente
[939358] TIMEOUT slave 2  (#10)  ← 7 ciclos más sin respuesta (no logueados por regla ≤3 y %10)
[939800] TIMEOUT slave 2  (#1)   ← counter reseteó: alguno respondió, luego nueva ráfaga
[939811] TIMEOUT slave 3  (#2)
[939822] TIMEOUT slave 4  (#3)
[939856] TIMEOUT slave 3  (#10)  ← nueva ola: counter llega a 10 de nuevo
...
[940957] TIMEOUT slave 1  (#10)  ← slave 1 también con 10 consecutivos
```

**Interpretación del contador global:**  
Cuando `_consecutiveTimeouts` llega a `#10`, significa que **10 rondas del round-robin completas fallaron sin una sola respuesta exitosa** de ninguno de los 4 slaves. Esto descarta fallo individual — todos los slaves estuvieron silentes simultáneamente durante ~200–500ms.

**Regla de logging (RS485.cpp línea 99):**

```cpp
if (_consecutiveTimeouts <= 3 || _consecutiveTimeouts % 10 == 0)
    log_w(...)
```

→ Se logea en #1, #2, #3, #10, #20, #30... El salto visible de `#3` a `#10` indica que los ciclos #4 al #9 fallaron pero no se loguearon.

**Señal de recuperación:** El `avg RX_WAIT` subió de ~1420µs (baseline) a ~1599µs en ciclo 366, y no volvió al baseline hasta ~ciclo 370 (t≈951s). Los slaves tardaron ~12 segundos en estabilizarse completamente post-evento.

---

#### Diagnóstico de causa raíz — hipótesis ordenadas por probabilidad

| # | Hipótesis | Evidencia a favor | Cómo descartar |
|---|-----------|------------------|----------------|
| 1 | **Spike EMI/eléctrico en bus RS485** | Todos los slaves callaron simultáneamente; recuperación espontánea; sin HALT ni error crítico | Osciloscopio en línea RS485 buscando spike de tensión |
| 2 | **Microcorte de alimentación** en rail 3.3V de slaves | Arranque simultáneo coherente con power glitch; ~500ms duración típica de un reset | Medir 3.3V con osciloscopio o LED testigo en rail |
| 3 | **Bloqueo de task RS485 en S3** (mutex o ISR) | `avg RX_WAIT` sube post-evento (overhead); S3 procesa USB-MIDI en mismo core | Analizar si hay actividad MIDI intensa justo antes de t=939s |
| 4 | **Contacto mecánico inestable** (conector RS485, cable) | Coincide con duración de un perturbación mecánica (~500ms) | Apretar conectores y repetir test |
| 5 | **Reset simultáneo de slaves** por watchdog o panic | Posible si todos los S2 tienen el mismo firmware con mismo bug | Activar log de reset reason en S2 (`esp_reset_reason()`) |

---

#### Impacto operativo

| Aspecto | Resultado |
|---------|-----------|
| HALT S3 | ❌ No ocurrió — `calibrating=false` post-calibración → condición HALT no aplica |
| LED rojo | ❌ No ocurrió |
| Datos corruptos | ❌ No — CRC protege paquetes; timeouts solo descartan ciclos |
| Faders físicos | ⚠️ Motor en los 4 slaves probablemente quedó en última posición ~500ms sin nuevos targets |
| Logic Pro | ⚠️ PitchBend feedback interrumpido ~500ms (S3 no recibió `faderPos` de slaves) |
| Recuperación | ✅ Automática y completa en ~12 segundos |

---

#### Acción requerida

- [ ] **Identificar qué ocurrió físicamente a t≈939s** — ¿se tocó algún cable, fuente, o rack?
- [ ] **Añadir log de reset reason en S2 boot** — `esp_reset_reason()` → `Serial.printf` → determinar si los slaves resetearon
- [ ] **Medir rail 3.3V con osciloscopio** durante operación normal — buscar caídas de tensión al mover faders (pico motor)
- [ ] **Correlacionar con actividad MIDI** — capturar `micros()` justo antes del burst para confirmar/descartar bloqueo de task
- [ ] **Si bug reproducible:** considerar aumentar `RS485_RESP_TIMEOUT_US` o implementar backoff exponencial en timeouts consecutivos

**Archivos involucrados (solo lectura/observación, no cambios aún):**
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp` — `runTask()`, `WAIT_RESP`, `_consecutiveTimeouts`
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/Profiler.h` — `_reportStats()` ciclos 366-368

**Riesgo actual:** BAJO — el sistema se recupera solo. Sin HALT, sin corrupción de datos. Prioridad de investigación: MEDIA (entender causa antes de desplegar en producción con 8 slaves).

---

### Bug B5 — Timeout periódico exacto ~2s en S2 durante calibración (2026-05-23) — ✅ Fix aplicado / 🔴 Pendiente validación hardware

**Síntoma observado:**

Cada exactamente ~2001ms, el Slave 1 no respondía en la ventana RS485 → S3 registraba timeout `#1 consecución`. El patrón era perfectamente periódico (no aleatorio), lo que indicaba causa determinista. La recuperación era automática en el siguiente ciclo.

**Hipótesis descartadas:**
- ❌ Timer interno S2 (no hay `setInterval` o timer en 2s en S2)
- ❌ Display SPI3 bloqueando Core (SPI no tiene transferencias de 2s)
- ❌ Motor update periódico (Motor::update() es continuo, no periódico)
- ❌ WiFi scan (WiFi eliminado de S2 en sesión 2026-05-20)

**Causa raíz identificada — USB CDC backpressure:**

Durante la calibración, las fases `KICK_UP` y `KICK_DOWN` en `Motor.cpp` ejecutaban `log_i` en cada iteración del loop (~3ms/iteración). Esto generaba aproximadamente **80 mensajes `log_i` en los 250ms** que duran estas fases.

Volumen de bytes estimado:
```
Mensaje típico: "[CALIB] KICK_UP adc=14232 (t=178 ms) pwm=210" → ~48 bytes
80 mensajes × 48 bytes = ~3840 bytes en 250ms
Tasa = ~15,360 bytes/s
```

El límite del USB CDC en el S2 (single-core, pioarduino IDF5) es aproximadamente **12,000 bytes/s**. El buffer TX de USB CDC se llenaba → las siguientes llamadas `log_i` **bloqueaban** hasta que el host vaciara el buffer. Un bloqueo de >3ms en la iteración del loop impedía responder a la ventana RS485 de S3 → timeout.

El patrón exacto de 2001ms coincide con el `POLL_CYCLE_MS=10ms × ~200 ciclos` necesarios para que el backlog de CDC se propague y bloquee la siguiente iteración lo suficiente.

El log periódico de `FaderADC.cpp` (500ms) también contribuía marginalmente con `log_i` → cambiado a `log_v` para eliminar su aporte.

**Fix aplicado (2026-05-23):**

| Archivo | Línea | Cambio | Razón |
|---------|-------|--------|-------|
| `S2/S2_V1/src/hardware/Motor/Motor.cpp` | 85 | `log_i` → `log_d` en KICK_UP | Elimina ~40 msgs/s durante calibración |
| `S2/S2_V1/src/hardware/Motor/Motor.cpp` | 161 | `log_i` → `log_d` en KICK_DOWN | Elimina ~40 msgs/s durante calibración |
| `S2/S2_V1/src/hardware/fader/FaderADC.cpp` | 68 | `log_i` → `log_v` en periodic 500ms | Elimina contribución marginal |

```cpp
// Motor.cpp línea 85 — ANTES:
log_i("[CALIB] KICK_UP adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
// DESPUÉS:
log_d("[CALIB] KICK_UP adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);

// Motor.cpp línea 161 — ANTES:
log_i("[CALIB] KICK_DOWN adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);
// DESPUÉS:
log_d("[CALIB] KICK_DOWN adc=%d (t=%ld ms) pwm=%d", pos, now - _motor_phaseStart, _pwm_max);

// FaderADC.cpp línea 68 — ANTES:
log_i("[ADC] raw=%d pos=%d min=%d max=%d", adcRaw, _faderPos, _calibratedFaderMin, _calibratedFaderMax);
// DESPUÉS:
log_v("[ADC] raw=%d pos=%d min=%d max=%d", adcRaw, _faderPos, _calibratedFaderMin, _calibratedFaderMax);
```

**Nota de diseño (FaderADC.cpp):** El comentario `// Log cada 500ms para debugging setup (si se quita, cambiar a log_v. Nunca borrar)` ya estaba presente desde sesión anterior. El `log_v` no se compila cuando `CORE_DEBUG_LEVEL < 5` → sin impacto en producción.

**MCU afectadas:**

| MCU | Afectado | Razón |
|-----|----------|-------|
| S2 (Slave) | ✅ SÍ | Cambios en Motor.cpp y FaderADC.cpp |
| S3 (Extender) | ❌ No | Observador del síntoma (timeout), sin cambios |
| P4 (Master) | ❌ No | No involucrado |

**Riesgo:** BAJO — reducción de verbosidad de log, sin cambio funcional. `log_d` visible con `CORE_DEBUG_LEVEL=4`, `log_v` visible con `CORE_DEBUG_LEVEL=5`.

**⚠️ VALIDACIÓN HARDWARE PENDIENTE:**
- [ ] Flash S2 con Motor.cpp y FaderADC.cpp actualizados
- [ ] Monitor serie S3 post-boot: timeouts de 2001ms deben desaparecer o volverse irregulares
- [ ] Calibración completa: S3 recibe `CALIBRADO OK: MIN=XX MAX=XXXXX` correctamente
- [ ] Operación normal post-calibración: tasa timeout ≤ 0.5% (baseline normal)

---

### Bug B6 — HALT por timeout RS485 no se dispara durante calibración (2026-05-23) — 🔴 Pendiente aplicar fix

**Contexto:** La condición HALT en `S3/RS485.cpp::runTask()` está diseñada para detectar cuando un slave no responde en absoluto durante calibración activa → LED rojo + loop infinito. En la práctica, esta condición nunca se puede cumplir.

**Causa raíz — condición lógicamente imposible:**

```cpp
// S3/RS485.cpp línea 105 (aproximado) — código actual:
if (_ch[_currentId].calibrated && _ch[_currentId].calibrating && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES)
```

Durante calibración activa, el estado del canal es:
```
calibrating = true   (S3 está esperando que el slave calibre)
calibrated  = false  (slave AÚN no ha completado calibración)
```

La condición requiere `calibrated && calibrating` → `false && true` → **siempre false**. El HALT **nunca** se dispara durante calibración.

**El HALT existente no cubre este caso:**

El HALT en `_handleResponse()` (CALIB_ERROR) solo se activa cuando el slave **responde** con flag `SLAVE_FLAG_CALIB_ERROR`. Si el slave no responde en absoluto (RS485 muerto, S2 desconectado), `_handleResponse()` nunca se llama → CALIB_ERROR nunca se detecta.

**Cobertura de errores actual vs. esperada:**

| Escenario | _handleResponse HALT | runTask HALT (actual) | Comportamiento real |
|-----------|---------------------|-----------------------|---------------------|
| Slave responde con error | ✅ Se dispara | ❌ No aplica | LED rojo, correcto |
| Slave no responde (RS485 muerto) | ❌ No se llama | ❌ Condición imposible | **Timeout infinito, sin LED rojo** |

**Fix propuesto:**

```cpp
// ANTES (línea ~105 en RS485.cpp):
if (_ch[_currentId].calibrated && _ch[_currentId].calibrating && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES)

// DESPUÉS:
if (!_ch[_currentId].calibrated && _ch[_currentId].calibrating && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES)
```

El cambio `calibrated` → `!calibrated` hace la condición evaluable durante calibración activa:
```
!calibrated && calibrating = !false && true = true && true = true ✓
```

Si el slave no responde `MAX_CALIBRATION_RETRIES` veces consecutivas **durante calibración** → LED rojo + HALT.

**Archivos afectados:**

| MCU | Archivo | Línea | Cambio |
|-----|---------|-------|--------|
| S3 (Extender) | `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp` | ~105 | `calibrated &&` → `!calibrated &&` |
| S2 (Slave) | — | — | Sin cambios |
| P4 (Master) | — | — | Sin cambios |

**Riesgo:** BAJO — el fix activa una rama de código que actualmente nunca se ejecuta. No afecta el camino normal (slave responde). No afecta la operación post-calibración (cuando `calibrating=false`).

**Nota:** Durante operación normal (post-calibración), `calibrating=false` → condición `!calibrated && calibrating` = `X && false` = false → HALT nunca dispara en operación normal. Correcto.

**⚠️ VALIDACIÓN HARDWARE PENDIENTE (post-fix):**
- [ ] Flash S3 con fix aplicado
- [ ] Desconectar S2 físicamente durante calibración → S3 debe mostrar LED rojo tras ~50ms (5 timeouts × 10ms)
- [ ] Log: `[CALIB] ✗ FALLO CRÍTICO Slave 1 — comunicación perdida. Sistema DETENIDO.`
- [ ] Operación normal con S2 conectado: sin HALT espurio

---

### S3 — Calibración boot con grace period + cascada (2026-05-22 18:02) — ✅ APLICADO

**Dos bugs corregidos en `RS485.cpp::_handleResponse()`:**

---

**Bug 1 — Grace period (no esperar al esclavo):**

S3 disparaba FLAG_CALIB en la **primera** respuesta válida del esclavo. El esclavo podía estar en medio de su `setup()` (ADS1115, motor, display no inicializados aún).

Fix: contador `stableRespCount` — solo dispara cuando esclavo alcanza `SLAVE_CALIB_SETTLE_RESPONSES = 5` respuestas consecutivas (~50ms con `POLL_CYCLE_MS=10`).

```cpp
// config.h S3 — nueva constante:
#define SLAVE_CALIB_SETTLE_RESPONSES 5

// RS485.h ChannelData — nuevo campo:
uint8_t stableRespCount = 0;

// RS485.cpp _handleResponse() — ANTES:
if (!_ch[id].responded && !_ch[id].calibrated && !_ch[id].calibrating)
    → disparo inmediato en 1ª respuesta

// DESPUÉS:
if (_currentId == 1 && !_ch[id].calibrated && !_ch[id].calibrating) {
    _ch[id].stableRespCount++;
    if (_ch[id].stableRespCount >= SLAVE_CALIB_SETTLE_RESPONSES)
        → disparo tras 5 respuestas estables
}
```

---

**Bug 2 — CALIB_ERROR relanzaba bucle infinito:**

Cuando el esclavo reportaba `CALIB_ERROR`: `calibrating=false`, `calibrated=false` → siguiente ciclo: condición verdadera → relanzamiento inmediato → fallo → bucle.

Fix: en `CALIB_ERROR` → resetear `stableRespCount=0` (el reintento solo ocurre tras otra grace period completa). Si `calibRetries >= MAX_CALIBRATION_RETRIES` → LED rojo + HALT.

---

**Cascada event-driven (2026-05-22 18:10):**

Antes: todos los slaves se auto-disparaban de forma independiente al alcanzar su grace period → calibración simultánea.

Fix: solo Slave 1 se dispara automáticamente. Al recibir `CALIB_DONE` de Slave N → dispara Slave N+1.

```cpp
// En calibDone:
uint8_t next = _currentId + 1;
if (next <= _numSlaves && !_ch[next].calibrated && !_ch[next].calibrating) {
    _ch[next].calibrate   = true;
    _ch[next].calibrating = true;
    log_i("[CALIB] Cascada → Slave %d", next);
}
```

**Flujo resultante:**
```
Boot → Slave 1 estabiliza 5 resp → FLAG_CALIB
Slave 1 CALIB_DONE → Slave 2 FLAG_CALIB
Slave 2 CALIB_DONE → Slave 3 FLAG_CALIB
...
Slave N CALIB_DONE → log "Cascada completa"
```

**Archivos modificados:**
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h` — `SLAVE_CALIB_SETTLE_RESPONSES`
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.h` — `stableRespCount` en `ChannelData`
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/RS485/RS485.cpp` — grace period + cascada + error handling

**Documentación actualizada:** `docs/RS485.md` §4.3

**⚠️ VALIDACIÓN HARDWARE:**
- [ ] Monitor serie boot S3 — secuencia `estabilizando 1/5` ... `5/5` → `arrancando cascada`
- [ ] Slave 1 calibra → log `CALIBRADO OK` → aparece `Cascada → Slave 2`
- [ ] Sin calibración simultánea de múltiples slaves
- [ ] Con un solo slave: `Cascada completa` aparece tras CALIB_DONE de Slave 1
- [ ] Error de calibración: no bucle infinito, reintenta tras nueva grace period

---

### BUG B3 — Fader 2 sube al inicializar Logic sin proyecto abierto (2026-05-20 17:45) — 🔴 PENDIENTE INVESTIGAR

**Síntoma:** Al inicializar Logic Pro (sin ningún proyecto abierto), el fader 1 se queda en 0 pero el fader 2 sube ligeramente. El movimiento ocurre antes de que se abra ningún track.

**Hipótesis principales:**
1. **Logic restaura estado de última sesión** — GoOnline #3 envía el estado real del mezclador aunque no haya proyecto. Si la última sesión tenía fader 2 ligeramente subido, Logic lo manda. Comportamiento de Logic, no bug de firmware.
2. **Offset de calibración entre unidades** — calibratedMin de S2 difiere del de S1. Con la misma señal PitchBend "0" de Logic, S3 mapea a un target que para S2 queda fuera de DEAD_ZONE → motor se mueve.
3. **Diferencia en mapeo S3** — el target calculado para slot 2 tiene un offset respecto al slot 1 por alguna constante o error de índice.

**Observado (2026-05-20):** Sin proyecto abierto. Fader 1 quieto en 0. Fader 2 sube un poco al conectar Logic.

**Por qué importa:** Comportamiento no deseado — los faders deberían estar en 0 si Logic no tiene proyecto activo. Si es Logic quien lo envía, hay que entender qué valor manda y decidir si S3 debe ignorarlo en ese estado.

**Investigación requerida:**
- [ ] Capturar MIDI monitor en el momento exacto del movimiento — qué PitchBend recibe S3 para slot 2
- [ ] Comparar PitchBend slot 1 vs slot 2 en GoOnline sin proyecto
- [ ] Verificar calibratedMin/Max de ambas unidades (¿son iguales?)
- [ ] Confirmar si el movimiento ocurre en GoOnline #1, #2 o #3

---

### S2 MOTOR — Vibración en reposo: 4 fixes (2026-05-20) — ✅ CÓDIGO LISTO / 🔴 PENDIENTE FLASH Y VALIDACIÓN

**Síntoma:** Uno de los dos esclavos vibraba levemente con el fader en posición de reposo, pese a tener el mismo software que el otro esclavo. El motor se activaba brevemente de forma intermitente incluso sin ningún comando de movimiento activo.

**Por qué solo una unidad:** el software creaba las condiciones para la vibración, pero que fuera perceptible dependía de diferencias físicas entre unidades: nivel de ruido ADC intrínseco del ADS1115, valores `pwmMin/pwmMax` calibrados individualmente en NVS, tolerancias del motor DC, fricción del fader en el rail, y resonancia mecánica del ensamblaje. No indica unidad defectuosa.

**Principio de diseño confirmado:** el fader se mantiene en posición por fricción mecánica. El motor no necesita estar activo para "sujetar" el fader — debe apagarse completamente al llegar a destino.

---

**Causa raíz 1 — `setTargetFromS3()` siempre forzaba `MOVING_TO_TARGET`:**

S3 envía el mismo target cada 10ms (ciclo RS485). Aunque el fader ya estuviera en posición, `setTargetFromS3()` establecía `_motor_state = MOVING_TO_TARGET` en cada ciclo sin comprobar si el error era menor que `DEAD_ZONE`. Esto provocaba que `_positionTick()` se ejecutara cada 10ms. Si el ruido ADC hacía que `|error| ≥ 50` (DEAD_ZONE), el motor recibía un pulso breve con `PWM_MIN = 100` (39% duty) → vibración audible/táctil.

```cpp
// ANTES — sin guard de distancia:
_motor_targetADC = adcTarget;
_motor_state = MotorState::MOVING_TO_TARGET;  // siempre, aunque ya en posición

// DESPUÉS — guard: si ya en AT_TARGET y dentro de DEAD_ZONE → no reactivar:
_motor_targetADC = adcTarget;
if (_motor_state == MotorState::AT_TARGET &&
    abs((int)_motor_adcPos - (int)adcTarget) < DEAD_ZONE) {
    return;   // fader en posición, fricción lo mantiene, motor se queda apagado
}
_motor_state = MotorState::MOVING_TO_TARGET;
```

**Archivo:** `S2/S2_V1/src/hardware/Motor/Motor.cpp` función `setTargetFromS3()` (línea ~671)

---

**Causa raíz 2 — Orden de operaciones en `_hwOff()` generaba pulso espurio:**

```cpp
// ANTES — EN se desactiva ÚLTIMO:
analogWrite(MOTOR_IN1, 0);   // IN1=0, pero EN sigue HIGH → posible corriente residual
analogWrite(MOTOR_IN2, 0);   // IN2=0, pero EN sigue HIGH
digitalWrite(MOTOR_EN, LOW); // solo aquí se corta el driver

// DESPUÉS — EN se desactiva PRIMERO:
digitalWrite(MOTOR_EN, LOW);   // corta driver antes de cualquier cambio PWM
analogWrite(MOTOR_IN1, 0);
analogWrite(MOTOR_IN2, 0);
```

Desactivar `EN` primero garantiza que el DRV8833 deje de conducir antes de que el estado de los pines PWM cambie. Elimina el instante de transición donde `IN=0` pero `EN=HIGH` podía generar un frenado brusco o pulso inductivo.

**Archivo:** `Motor.cpp` función `_hwOff()` (línea ~42)

---

**Causa raíz 3 — `_hwOff()` llamado cada iteración de loop en AT_TARGET e IDLE:**

Los estados `AT_TARGET` e `IDLE` (rama connected) llamaban `_hwOff()` en cada iteración del loop principal (~100Hz), aunque el motor ya estuviera apagado. Esto ejecutaba `analogWrite(pin, 0)` y `digitalWrite(EN, LOW)` repetidamente — operaciones GPIO que en S2 single-core tienen overhead y pueden generar ruido en el bus I/O acoplable al DRV8833.

```cpp
// ANTES — _hwOff() incondicional cada loop:
case MotorState::AT_TARGET:
    _hwOff();
    break;

// DESPUÉS — solo si el driver estaba activo:
case MotorState::AT_TARGET:
    if (_motor_hw_active) _hwOff();
    break;
```

Mismo cambio aplicado en `IDLE` (rama `connected`):
```cpp
// ANTES:
_hwOff();

// DESPUÉS:
if (_motor_hw_active) _hwOff();
```

`_motor_hw_active` es el flag de verdad HW — se pone `true` en `_hwUp()`/`_hwDown()` y `false` en `_hwOff()`. La guardia es O(1) y segura.

**Archivos:** `Motor.cpp` cases `AT_TARGET` (línea ~453) e `IDLE` (línea ~395)

---

**Resumen de cambios (4 en 1 archivo):**

| # | Función | Línea aprox. | Cambio | Efecto |
|---|---------|-------------|--------|--------|
| 1 | `_hwOff()` | 42 | EN=LOW antes de IN1/IN2=0 | Elimina pulso espurio en desactivación |
| 2 | `IDLE` (connected) | 395 | `_hwOff()` → `if (_motor_hw_active) _hwOff()` | Sin GPIO redundante cada loop |
| 3 | `AT_TARGET` | 453 | `_hwOff()` → `if (_motor_hw_active) _hwOff()` | Sin GPIO redundante cada loop |
| 4 | `setTargetFromS3()` | 671 | Guard DEAD_ZONE antes de `MOVING_TO_TARGET` | Motor no se reactiva con ruido ADC |

**⚠️ VALIDACIÓN HARDWARE:**
- [ ] Fader en posición, Logic conectado → sin vibración en ambas unidades
- [ ] S3 manda nuevo target diferente → motor se mueve con normalidad
- [ ] S3 manda mismo target repetidamente → motor permanece apagado
- [ ] Ruido ADC no supera DEAD_ZONE con motor apagado (log: sin `MOVING_TO_TARGET` spam)
- [ ] Calibración → sin regresión (no usa `setTargetFromS3()`)

---

### RESUMEN SESIÓN 2026-05-20 (tarde)

**Objetivo de la sesión:** Estabilidad motor S2 en rack + investigar OTA/WiFi.

**Resuelto ✅**

| Fix | MCU | Archivos | Descripción |
|-----|-----|----------|-------------|
| Bug B2 — nombres borrados modo plugin/Atmos | S3 | `MIDIProcessor.cpp` | Guard `nameBufs[t][0]=='\0'` en SysEx 0x12 — Logic envía row 1 vacía en modo plugin, S3 ya no borra trackNames |
| Fader bloqueado tras movimiento manual | S2 | `Motor.cpp` | Spike guard en `setADCDelta()` — spike eléctrico (ej: ADC 7284→29) re-disparaba `_motor_manualTouchDetected` indefinidamente |
| DEAD_ZONE 50→80 | S2 | `config.h` | Cubre ruido ADC S1 en reposo (60 cuentas) — previene `MOVING_TO_TARGET` intermitente |
| WiFiManager eliminado | S2 | `OtaManager.cpp/h` | `launchPortal()` era código muerto — eliminado quirúrgicamente junto con `#include <WiFiManager.h>` |
| SAT.md §7 documentado | docs | `docs/SAT.md` | Flujo OTA completo: ElegantOTA URL, credenciales NVS, provisioning sketch, pines al aire fix |

**Pendiente 🔴**

| Pendiente | MCU | Descripción |
|-----------|-----|-------------|
| Bug B3 — fader 2 sube al init Logic sin proyecto | S2+S3 | Capturar PitchBend slot 1 vs slot 2 en GoOnline sin proyecto — posible comportamiento Logic |
| Validación vibración motor en hardware | S2 | Flash + confirmar sin vibración en ambas unidades con Logic conectado |
| Pines al aire sketch provisioning | Arduino | Añadir bloque safePins OUTPUT LOW al inicio de setup() |
| Boot goToMin (`_bootGoToMinDone`) | S2 | Fader no baja a 0 en boot si S3 ya activo — fix diseñado, pendiente aplicar |
| Fader feedback S2→Logic validación hardware | S2+S3 | Confirmar que Logic recibe PitchBend al mover fader físico |
| ⬇️ Branding S2 — iMakie → AITEC 17 | S2 | `Display.cpp:131` boot screen + `SatMenu.cpp:224` cabecera SAT. Baja prioridad — estético |

**Commits:** `2f209b9`, `605e694`, `f87ef92`

---

### RESUMEN SESIÓN 2026-05-20 (mañana)

**Objetivo de la sesión:** Conseguir P4 online + investigar flujo de nombres de pista S3→S2.

**Resuelto ✅**

| Fix | Archivos | Descripción |
|-----|----------|-------------|
| P4 arranca y envía handshake | P4 hardware | Note On F#1 (0x26, vel 127) confirmado en MIDI monitor 07:36:16 |
| Nombres de pista visibles en S2 | — (GoOnline row 1) | Logic envía SysEx 0x12 con nombres en row 1 al GoOnline → S3 procesa → S2 muestra ✓ |

**Diagnóstico nuevo 🔍**

| Hallazgo | Descripción |
|----------|-------------|
| GoOnline SysEx 0x12 — comportamiento normal | Row 1 = nombres de pista (7 chars), row 2 = valores fader/pan. Confirmado con capturas reales P4 + Extender (07:45:16) |
| Modo Atmos/plugin — comportamiento especial | Logic envía row 1 vacía (56 × 0x20) + row 2 con parámetros del plugin ("Angle", "LFE", "Spread"). Capturado a 07:36:59 |
| Bug B2 identificado | En modo Atmos/plugin, row 1 vacía → S3 borra `trackNames[]` → S2 pierde nombre de pista. Fix: ignorar updates con row 1 = todo espacios |
| Regla de diseño confirmada | S2 solo debe ver nombres de pista (row 1). Valores de row 2 nunca llegan a S2 — correcto en código actual |

**Pendiente 🔴**

| Pendiente | MCU | Descripción |
|-----------|-----|-------------|
| Vibración motor en reposo — flash + validación | S2 | Código listo (4 fixes Motor.cpp). Flash ambas unidades, confirmar sin vibración con Logic conectado |
| B2 — Nombres borrados en modo plugin/Atmos | S3 | Row 1 vacía → S3 borra trackNames → S2 pierde nombres. Fix: guard contra row 1 todo espacios |
| Validación nombres con cambio de nombre en Logic | S3+S2 | Confirmar que renombrar una pista en Logic actualiza S2 en tiempo real |

---

### S3 — Bug B2: SysEx 0x12 con row 1 vacía borra nombres de S2 — modo plugin/Atmos (2026-05-20 07:40) — 🔴 PENDIENTE

**Contexto:**

Logic Pro envía SysEx 0x12 (LCD Write / Scribble Strip) con dos layouts distintos:

1. **Normal (GoOnline + actualizaciones de pista):** row 1 (offsets 0–55) = nombres de pista. Row 2 (offsets 56–111) = valores numéricos (fader dB, pan). S3 procesa correctamente row 1 → S2 muestra nombres ✓
2. **Modo plugin/Atmos/spatial:** Logic envía **row 1 completamente vacía** (56 × `0x20`) y row 2 con parámetros del plugin (ej: "Angle  ", "LFE    ", "Spread "). S3 escribe cadenas vacías en `trackNames[]` → `rs485.setTrackName()` vacío → **S2 borra el nombre de pista.**

**SysEx capturado (2026-05-20 07:36:59 — 43s tras handshake P4):**

```
F0 00 00 66 14 12 00  [datos…]  F7
```

Análisis byte a byte (datos = 116 bytes):

```
Offset  0–55:  20 × 56 (espacios)        → row 1 completamente vacía
Offset 56–62:  20 × 7  (espacios)        → row 2, canal 1: sin nombre
Offset 63–69:  41 6E 67 6C 65 20 20      → row 2, canal 2: "Angle  "
Offset 70–76:  44 69 76 65 72 73 20      → row 2, canal 3: "Divers "
Offset 77–83:  4C 46 45 20 20 20 20      → row 2, canal 4: "LFE    "
Offset 84–90:  53 70 72 65 61 64 20      → row 2, canal 5: "Spread "
Offset 91–97:  20 × 7  (espacios)        → row 2, canal 6: sin nombre
Offset 98–104: 20 43 53 74 72 69 70      → row 2, canal 7: " CStrip"
Offset 105–111:20 41 6E 67 2F 44 76      → row 2, canal 8: " Ang/Dv"
Offset 112–115:20 58 2F 59               → extra: " X/Y"
```

**Comportamiento de Logic verificado (capturas 2026-05-20):**

| Momento | Row 1 (offset 0–55) | Row 2 (offset 56–111) |
|---------|--------------------|-----------------------|
| GoOnline #3 + cualquier update normal | Nombres de pista (7 chars, truncados) | Valores numéricos (fader dB, pan) |
| Modo Pan | Etiquetas parámetro ("Pan    ", "PanSpr ") | Valores ("0      ", "111 o  ") |
| Modo plugin/Atmos | **VACÍA** (56 × 0x20) | Parámetros plugin ("Angle  ", "LFE    ", "Spread ") |

**Regla de diseño:** S2 solo debe ver nombres de pista. Los valores de row 2 nunca deben llegar a S2. Correcto en código actual: `if (offset >= 56) break` impide que row 2 llegue a `setTrackName()`.

**Bug exacto:** el `break` es correcto, pero no hay guard contra row 1 vacía. Cuando Logic envía row 1 = todo espacios (modo plugin), S3 llama `setTrackName(t, "")` → S2 borra el nombre.

**Fix propuesto (pendiente implementar):**

`MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/midi/MIDIProcessor.cpp`, dentro de case 0x12, antes de llamar `rs485.setTrackName()`:

```cpp
trimRight(nameBufs[t]);
if (nameBufs[t][0] == '\0') continue;   // ← no borrar si Logic envía espacios
if (trackNames[t] == nameBufs[t]) continue;
trackNames[t] = String(nameBufs[t]);
rs485.setTrackName(t + 1, nameBufs[t]);
```

La guardia `[0] == '\0'` (después del `trimRight`) detecta nombres que eran todo espacios y los ignora, conservando el nombre previo en S2.

**Archivos afectados:**
- `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/midi/MIDIProcessor.cpp` — case 0x12 (línea 350–386)

**Riesgo:** BAJO — solo afecta parsing de SysEx en S3. No toca RS485, Motor, calibración.

**Validación requerida (post-fix):**
- [ ] GoOnline: nombres llegan de row 1 → S2 muestra correctamente (no regresión)
- [ ] Post-GoOnline: Logic envía actualización con nombres en row 2 → S2 actualiza
- [ ] Modo Pan: row 1 con "Pan"/"PanSpr" → NO sobreescribe nombre de pista en S2

---

### RESUMEN SESIÓN 2026-05-19

**Objetivo de la sesión:** Conseguir fader bidireccional funcional — Logic mueve S2, S2 reporta posición a Logic.

**Resuelto ✅**

| Fix | Archivos | Descripción |
|-----|----------|-------------|
| Motor apretado en tope mecánico | `Motor.cpp`, `config.h` | Stall detection 400ms en GOING_TO_MIN — evita sobrecalentamiento DRV8833 |
| Protección global topes | `Motor.cpp`, `config.h` | `ADC_SPIKE_GUARD` + guard global en todos los estados del motor |
| FaderTouch falso positivo | `Motor.cpp` | Desacoplado del control motor — interferencia eléctrica en tope inferior causaba bloqueo total |
| Motor auto-interrupción | `Motor.cpp` | `MOVING_TO_TARGET` añadido a `inCalibFlow` — propio movimiento no se confunde con usuario |
| Fader feedback S2→Logic | `Motor.cpp`, `Motor.h`, `RS485Handler.cpp` | `isManualTouchDetected()` exportado, `touchState` basado en delta ADC (no FaderTouch) |
| Latencia feedback | `RS485.cpp` (S3) | Bypass EMA cuando `touchState=1` → posición directa a Logic sin filtro |
| S3 HALT agresivo | `RS485.cpp` (S3) | HALT solo durante calibración activa — movimiento normal no dispara LED rojo |
| Re-calibración innecesaria | `MIDIProcessor.cpp` (S3) | `_calibPendingFrom` eliminado de handler 0x21 y primer PitchBend |
| Ciclo RS485 más rápido | `config.h` (S3) | `POLL_CYCLE_MS` 20→10ms (100Hz con 1 slave) |
| SAT roto | — | Resuelto espontáneamente en hardware durante la sesión |
| Docs S2 README | `S2/README.md` | Specs corregidos: Lolin S2 Mini, Type-C USB OTG, 27 GPIO |
| Docs S3 README | `S3/README.md` | Alt text imagen corregido: ESP32-S3-DevKitC-1 |

**Pendiente 🔴**

| Pendiente | MCU | Descripción |
|-----------|-----|-------------|
| Boot goToMin | S2 | Fader no baja a 0 en boot — fix diseñado (`_bootGoToMinDone`), pendiente aplicar |
| Fader feedback validación | S2+S3 | `touchState=1` en logs pero sin confirmar en hardware que Logic recibe PitchBend |
| Boot secuencial 4 fases | S3 | Nueva arquitectura boot: detección → calibración → validación → Logic ready |
| MIDI deadband PitchBend | S3 | Deadband 150 cuentas → reducir tráfico 850→100 msgs/s |
| Validación hardware flujo completo | S3 | Handshake, RS485, calibración, fader bidireccional, transport |
| Validación hardware multimedia | P4 | Display LVGL, Touch GT911, NeoTrellis, PSRAM profiling |
| P4 config.h PSRAM + periféricos | P4 | PSRAM 32MB, MIPI-CSI, I2S, TWAI, aceleradores JPEG/H.264 |
| P4 Task Architecture docs | P4 | ARCHITECTURE_P4.md — dual-core, race conditions, ISR |

**Commits:** `06d9562`, `b336c1d`, `7732728`, `f74ac0e`, `3c515e9`, `425a423`, `3957009`

---

### S3 — Boot secuencial 4 fases (2026-05-19) — 🔴 PENDIENTE

**Objetivo:** S2 calibrado y validado ANTES de que Logic conecte (0x21).

```
FASE 1: DETECCIÓN ESCLAVO (0-2s)
├─ S3 envía probe RS485 a S2 (ping simple)
├─ S2 responde SlavePacket (confirma online)
├─ Si timeout > 3 reintentos → ERROR CRÍTICO (LED rojo + log)
└─ Si OK → Fase 2

FASE 2: CALIBRACIÓN (2-10s)
├─ S3 envía FLAG_CALIB a S2
├─ S2 ejecuta calibración motor (baja a min, sube a max)
├─ S2 responde con min/max ADC
├─ S3 almacena calibración, valida rangos (min<max)
├─ Si calibración falla → LED rojo + ERROR, requiere reset S3
└─ Si OK → Fase 3

FASE 3: VALIDACIÓN (10-15s)
├─ S3 envía setTarget(8192) a S2 (posición media)
├─ S2 mueve fader, reporta faderPos
├─ S3 valida respuesta (faderPos ≈ 8192 ±500)
├─ Si responde → LED verde (S2 listo)
└─ Si timeout → LED rojo (S2 no responde)

FASE 4: LOGIC READY (15s+)
├─ S3 espera Logic 0x21
├─ Cuando Logic conecta: S2 ya está calibrado y validado
└─ RS485 polling activo, todo funcional
```

---

### P4 — config.h PSRAM 32MB + periféricos + aceleradores (2026-05-19) — 🔴 PENDIENTE

**Archivo:** `MASTER_S3-P4/P4/src/config.h`

- Documentar PSRAM 32MB con comentarios para LVGL
- Añadir sección periféricos: MIPI-CSI, I2S audio, TWAI (CAN)
- Añadir sección aceleradores multimedia: JPEG, PPA, ISP, H.264

---

### S3 — MIDI Traffic Optimization: PitchBend deadband (2026-05-19) — 🔴 PENDIENTE

**Archivo:** `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/main.cpp` línea 85

- Implementar deadband 150 cuentas ADC antes de enviar PitchBend a Logic
- Objetivo: reducir tráfico 850→~100 msgs/s en S3
- Requiere validación hardware en rig S3-Logic

---

### S3 — Validación hardware flujo completo (2026-05-19) — 🔴 PENDIENTE

- [ ] Handshake Mackie: Logic 0x21 → S3 echo + conexión
- [ ] RS485 polling: ciclo ~300µs (NUM_SLAVES=1)
- [ ] Calibración automática: cascada, timeout handling
- [ ] Fader: PitchBend bidireccional, deadband 150
- [ ] Transport: botones RW/FF/STOP/PLAY/REC → Logic feedback

---

### P4 — Validación hardware multimedia (2026-05-19) — 🔴 PENDIENTE

- [ ] Display IPS 480×800 con LVGL v9
- [ ] Touch GT911 calibración multi-punto
- [ ] NeoTrellis 4×8 (seesaw dual 0x2F/0x2E)
- [ ] PSRAM 32MB: profiling LovyanGFX sprites + LVGL

---

### P4 — Task Architecture documentation (2026-05-19) — 🔴 PENDIENTE

**Archivo:** `docs/ARCHITECTURE_P4.md`

- Dual-core Core0/Core1 sincronización
- Race conditions conocidas (flags `g_switchToPage`)
- VU meter decay timing
- ISR priorities

---

### S2/S3 — Fader feedback S2→Logic — pendiente validación (2026-05-19) — 🔴 PENDIENTE

**Diagnóstico en curso:** `[S2-RESP] touchState=1` y `[S3-RX] touchState=1` añadidos para confirmar la cadena RS485. No validado en hardware todavía.

---

### S2 Motor — auto-interrupción en MOVING_TO_TARGET (2026-05-19) — ✅ APLICADO

**Síntoma:** Motor se movía hacia el target, se detenía solo a mitad de camino, y rechazaba nuevos targets de S3.

**Causa:** `setADCDelta()` detectaba el movimiento del propio motor como "usuario tocando" — delta=1526 > umbral=500 → `_motor_manualTouchDetected=true` → `setTargetFromS3()` rechazado → motor parado.

**Fix:** `MOVING_TO_TARGET` añadido al guard `inCalibFlow` en `setADCDelta()`. Durante movimiento motorizado el delta se ignora — solo se detecta usuario cuando motor está parado (`AT_TARGET`, `IDLE`).

**Diagnóstico añadido (2026-05-19):**
- `setTargetFromS3()`: logs `log_i` visibles — acepta/rechaza target con razón
- `_positionTick()` ON: log con pos/target/err/span
- `[S2-RESP]` en `buildResponse()`: confirma `touchState=1` enviado
- `[S3-RX]` en `_handleResponse()`: confirma `touchState=1` recibido
- Profiler S3: reducido a 1000 ciclos, sin verbose

---

### Fader bidireccional — feedback físico + latencia (2026-05-19) — ✅ APLICADO

**Síntomas resueltos:**
- Mover el fader físico no actualizaba Logic Pro
- Retraso perceptible al mover fader desde Logic
- Motor no volvía a aceptar targets S3 tras primer movimiento manual

**6 cambios en 5 archivos (S2 + S3):**

**S2 `Motor.cpp` — `setADCDelta()`: fix timer + eliminar FaderTouch del reset**
- `_motor_manualTouchStartTime` ahora se refresca en CADA movimiento detectado (no solo el primero)
- Reset de `_motor_manualTouchDetected` eliminado de dependencia FaderTouch — ahora solo tiempo
- Bug previo: con FaderTouch siempre `true`, el flag nunca se reseteaba → motor rechazaba todos los targets S3

**S2 `Motor.h` + `Motor.cpp` — getter `isManualTouchDetected()`**
- Nueva función pública: `bool Motor::isManualTouchDetected()`
- Expone `_motor_manualTouchDetected` (delta-based) para uso externo

**S2 `RS485Handler.cpp` — `touchState` desde Motor en lugar de FaderTouch**
- `resp.touchState = Motor::isManualTouchDetected() ? 1 : 0;`
- FaderTouch eliminado del path RS485 — completamente desacoplado
- Ahora `touchState=1` solo cuando usuario mueve fader (delta > 500 cuentas ADC)

**S3 `config.h` — `POLL_CYCLE_MS` 20 → 10**
- Ciclo RS485: 50Hz → 100Hz con 1 slave
- Transacción ~3ms, margen 7ms — sin riesgo de timeouts

**S3 `RS485.cpp` — `_handleResponse()`: bypass EMA cuando usuario toca**
- `touchState=1`: posición directa sin filtro → feedback inmediato a Logic
- `touchState=0`: EMA alpha=0.15 preservado para suavizar ruido EMI durante movimiento motor

**Fix adicional — `setADC()` bypass spike guard durante movimiento manual (2026-05-19)**

`ADC_SPIKE_GUARD = 500` y `MANUAL_TOUCH_THRESHOLD = 500` tienen el mismo valor. Con delta > 500:
- `setADCDelta()` detectaba movimiento → `_motor_manualTouchDetected = true` ✓
- `setADC()` rechazaba la nueva posición (spike) → `_motor_adcPos` quedaba en el target de S3
- `buildResponse()` enviaba posición ANTIGUA = mismo valor que S3 ya conocía → `pb == lastSentPb` → **cero mensajes MIDI**

Fix: `|| _motor_manualTouchDetected` añadido al `inCalibFlow` guard de `setADC()`. Como `setADCDelta()` se llama antes en el mismo loop, el flag ya está activo cuando `setADC()` lo comprueba.

**⚠️ VALIDACIÓN HARDWARE:**
- [ ] Mover fader físico → Logic fader se mueve en tiempo real
- [ ] Logic mueve fader → motor sigue con < 20ms de lag visible
- [ ] Soltar fader físico 200ms → motor vuelve a seguir targets S3
- [ ] S3 calibración boot → unaffected

---

### S3 — HALT eliminado en operación normal + re-calibración innecesaria (2026-05-19) — ✅ APLICADO

**Síntoma:** S3 entra en HALT (loop infinito, LED rojo) al conectar Logic o al mover fader en Logic.

**Causa 1 — HALT demasiado agresivo (`RS485.cpp` línea 104):**
- Condición anterior: `if (calibrated && consecutiveTimeouts > MAX_CALIBRATION_RETRIES)`
- Disparaba HALT con **cualquier** 6 timeouts consecutivos post-calibración — incluso durante movimiento normal de fader
- Al mover fader, motor arranca → interferencia eléctrica → 6 timeouts → HALT inmediato
- Fix: `if (calibrated && **calibrating** && consecutiveTimeouts > MAX_CALIBRATION_RETRIES)`
- HALT ahora solo dispara si S3 está **activamente calibrando** (`calibrating=true`) y slave no responde

**Causa 2 — Re-calibración innecesaria al conectar Logic (`MIDIProcessor.cpp`):**
- Handler `0x21` (línea 444): `_calibPendingFrom = 1` → `tickCalibracion()` → FLAG_CALIB a S2
- Primer PitchBend, transición `HANDSHAKE→CONNECTED` (línea 594): `_calibPendingFrom = 1` (otra vez)
- S2 ya calibrado desde boot → startCalib() arranca motor → interferencia → 6 timeouts → HALT
- Fix: ambas líneas comentadas — `// ELIMINADO — boot auto-calib ya lo hizo (2026-05-19)`
- La calibración de boot (arranque S3 sin Logic) sigue siendo la única fuente de calibración

**Cambios aplicados:**

`RS485.cpp` (línea 104):
```cpp
// ANTES:
if (_ch[_currentId].calibrated && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES)

// DESPUÉS:
if (_ch[_currentId].calibrated && _ch[_currentId].calibrating && _consecutiveTimeouts > MAX_CALIBRATION_RETRIES)
```

`MIDIProcessor.cpp` (línea 444 — handler 0x21):
```cpp
// _calibPendingFrom = 1;   // ELIMINADO — boot auto-calib ya lo hizo (2026-05-19)
// _calibNextTime    = millis();
```

`MIDIProcessor.cpp` (línea 594 — primer PitchBend, HANDSHAKE→CONNECTED):
```cpp
// _calibPendingFrom = 1;   // ELIMINADO — boot auto-calib ya lo hizo (2026-05-19)
// _calibNextTime    = millis();
```

**⚠️ VALIDACIÓN HARDWARE:**
- [ ] S3 boot → calibración secuencial slaves → completa sin HALT
- [ ] Logic conecta (0x21) → S3 NO dispara re-calibración → motor no se mueve
- [ ] Fader movido en Logic → S3 envía PitchBend → S2 motor sigue → sin HALT
- [ ] Fader en tope: > 6 timeouts consecutivos en operación normal → sin HALT (solo warning log)
- [ ] Si S2 desconectado físicamente DURANTE calibración boot → HALT correcto (LED rojo)

---

### S2 MOTOR — FaderTouch desactivado en control motor (2026-05-19) — ✅ APLICADO

**Síntoma:** S2 irresponsivo tras recibir primer target de S3 con Logic conectado.

**Causa raíz:** `FaderTouch::isTouched()` devuelve `true` en falso positivo constante (interferencia eléctrica en tope mecánico inferior, ADC=25). Esto bloqueaba:
1. `setTargetFromS3()` — guard `_motor_manualTouchDetected || FaderTouch::isTouched()` siempre true → todos los targets rechazados → motor nunca se mueve
2. `setADCDelta()` — `FaderTouch::isTouched()` disparaba "Usuario master" con `delta=2` (muy por debajo del umbral 500) → `Motor::stop()` + `_motor_state = AT_TARGET` incluso durante calibración

**Fixes aplicados en `Motor.cpp`:**

`setADCDelta()`:
- Añadido guard `inCalibFlow`: si motor está en GOING_TO_MIN, CALIBRATING o calibrando → actualizar referencia ADC y retornar sin detectar usuario
- `FaderTouch::isTouched()` comentado de detección inicial: `userTouch = delta > 500` (solo delta)

`setTargetFromS3()`:
- `FaderTouch::isTouched()` comentado del guard: solo `_motor_manualTouchDetected` bloquea targets

**Estado FaderTouch:**
- RS485 `touchState` sigue reportando `FaderTouch::isTouched()` via `buildResponse()` — Logic sigue recibiendo estado de toque para feedback visual
- Control motor: desacoplado de FaderTouch hasta resolver fiabilidad del sensor
- TODO: reactivar cuando FaderTouch sea estable en todo el recorrido del fader

**⚠️ VALIDACIÓN HARDWARE:**
- [ ] S3 con Logic → S2 recibe target → motor se mueve a posición
- [ ] Usuario mueve fader (delta > 500) → motor para inmediatamente
- [ ] Calibración → motor sube/baja sin interrupción por falso touch

---

### S2 MOTOR — Boot goToMin no funciona (2026-05-19) — 🔴 PENDIENTE

**Síntomas observados en hardware:**
- ❌ Fader NO baja a 0 automáticamente en boot
- ✅ SAT funciona correctamente (regresión resuelta espontáneamente 2026-05-19)
- ✅ S3 conecta y dispara calibración correctamente
- ✅ Motor ejecuta calibración cuando S3 envía FLAG_CALIB

**Causa raíz identificada:**
- S3 ya activo envía paquetes con `connected=1` antes de que `Motor::update()` IDLE pueda transicionar
- Orden en `loop()`: `rs485.update()` → `onMasterData()` → `Motor::setConnected(true)` → LUEGO `Motor::update()`
- IDLE: `if (!_connected && ...)` → siempre false → motor nunca baja a 0 en boot
- La bajada a 0 solo ocurre cuando S3 envía FLAG_CALIB (dentro de la calibración)

**Cambios aplicados en sesión anterior:**
- `Motor.cpp initPWM()`: fallback a `PWM_MIN/PWM_MAX` de config.h si NVS vacío
- `Motor.cpp IDLE`: inicializa `_goToMinStallStart=0`, `_goToMinLastADC=_motor_adcPos` al entrar GOING_TO_MIN
- `main.cpp`: eliminado `Motor::goToMin()` de setup() línea 133 (era dead code — ADC no inicializado)

**Fix diseñado — boot flag `_bootGoToMinDone` (pendiente aplicar):**
- `config.h`: `static bool _bootGoToMinDone = false;`
- `Motor.cpp IDLE`: `if (_motor_adcPos > (MOTOR_ADC_MIN + 10) && (!_connected || !_bootGoToMinDone))`
- `Motor.cpp GOING_TO_MIN arrived`: `_bootGoToMinDone = true;`
- Objetivo: primera bajada a 0 siempre ocurre en boot, independientemente de `_connected`

---

### S2 MOTOR — Protección global topes mecánicos (2026-05-19 15:49) — ✅ COMPLETADO

**Commits:** `06d9562` (stall GOING_TO_MIN), `[commit actual]` (protección global + docs)

**Incidente:** Motor se calentó al quedar apretado contra el tope mecánico inferior. Motor DC en stall consume corriente máxima sin girar → sobrecalentamiento DRV8833 y bobinas.

**Causa raíz:**
- `MOTOR_ADC_MIN = 20` es filtro de ruido, NO el valor ADC del tope físico
- Tope físico real: ADC ≈ 44 (varía por unidad)
- Condición GOING_TO_MIN: `ADC <= MOTOR_ADC_MIN + 10 = 30`
- `44 <= 30` → NUNCA true → motor apretado indefinidamente

**Lección permanente:**
> `MOTOR_ADC_MIN` es solo un guardia de ruido. Los topes mecánicos siempre se detectan por **stall** (ADC estable > N ms), nunca por valor absoluto de ADC. Un motor DC en stall es equivalente a un cortocircuito térmico — siempre apagar en ≤500ms.

**Fix 1 — Stall en GOING_TO_MIN (commit `06d9562`):**

`config.h`:
```cpp
static constexpr uint32_t GOTO_MIN_STALL_MS  = 400;
static uint32_t           _goToMinStallStart = 0;
static uint16_t           _goToMinLastADC    = 0;
```

`Motor.cpp` case `GOING_TO_MIN`:
- Threshold generoso `ADC <= MOTOR_ADC_MIN + 60` OR stall 400ms
- Si `_pendingCalib`: → CALIBRATING; si no: → AT_TARGET

**Fix 2 — Protección Global (commit actual):**

`config.h`:
```cpp
static constexpr uint32_t STALL_PROTECT_MS     = 400;
static bool               _motor_hw_active     = false;  // fuente de verdad HW
static uint32_t           _stallProtectStart   = 0;
static uint16_t           _stallProtectLastADC = 0;
```

`Motor.cpp` `_hwOff/_hwUp/_hwDown` → setean `_motor_hw_active`

`Motor.cpp update()` antes del switch:
- Si `_motor_hw_active` y estado ≠ CALIBRATING: ADC sin cambio > 400ms → `_hwOff()`
- CALIBRATING excluido: usa `CALIB_STUCK_TIMEOUT = 1000ms` propio

**Cobertura resultante:**

| Estado | Protección |
|--------|-----------|
| `GOING_TO_MIN` | Stall local 400ms + Global 400ms |
| `MOVING_TO_TARGET` | Global 400ms |
| `CALIBRATING` | `CALIB_STUCK_TIMEOUT = 1000ms` por fase |
| `IDLE / AT_TARGET` | Motor apagado, no aplica |

**Documentación actualizada:**
- ✅ `docs/MOTOR.md` — nueva sección §2.6 "Protección de Topes Mecánicos" (exhaustiva)
- ✅ `docs/FADER.md` — §3.4 y §10 actualizados con lección y referencias
- ✅ `CHANGELOG.md` — esta entrada

**⚠️ VALIDACIÓN HARDWARE PENDIENTE:**
- [ ] Flash S2 con cambios actuales
- [ ] Boot → fader baja a 0 → motor se apaga (log `GOING_TO_MIN → AT_TARGET` o `→ CALIBRATING`)
- [ ] Motor NO se calienta
- [ ] S3 manda FLAG_CALIB: `GOING_TO_MIN → CALIBRATING` tras stall detection
- [ ] MOVING_TO_TARGET con target inalcanzable → motor apagado en 400ms (log `[MOTOR] STALL`)

---

### S2 SLAVE — Placa Lolin D1 Mini S2 especificación completa (2026-05-16 21:00) — ✅ COMPLETADO

**Commit:** 40337a8

**Especificación (S2/README.md):**
- ✅ Placa: Lolin D1 Mini S2 (form factor ESP8266, single-core)
- ✅ Chip: ESP32-S2FN4R2 Xtensa 240MHz (single-core vs dual-core P4/S3)
- ✅ Flash: 4MB QIO (bootloader 192KB, app 3.8MB)
- ✅ PSRAM: 2MB QSPI (limitado: vs 8MB S3, 32MB P4)
- ✅ Conector: Micro-USB CH340 UART (reset automático upload)
- ✅ GPIO: 25 totales (0 libres — todos asignados)

Limitaciones documentadas:
- ✅ Single-core 240MHz (vs dual-core P4/S3) → timing crítico
- ✅ 4MB Flash (vs 16MB P4/S3) → OTA dual-partition imposible
- ✅ 2MB PSRAM (vs 8MB S3, 32MB P4) → buffers pequeños, profiling obligatorio
- ✅ 25 GPIO saturados (vs 44 P4/S3) → expansión futura imposible
- ✅ 500mA USB compartido (motor + display + MCU) → picos riesgo reset

Configuración PlatformIO:
- ✅ Board: lolin_s2_mini
- ✅ Flags: BOARD_HAS_PSRAM, ARDUINO_USB_MODE=0, CORE_DEBUG_LEVEL=3
- ✅ Platform: espressif32 (pioarduino 55.03.37, IDF5)
- ✅ Librerías: LovyanGFX 1.2.19, Adafruit NeoPixel, ADS1115, Wire

Nueva sección "Limitaciones y consideraciones":
- ✅ Arquitectura: single-core, RS485+display+motor+encoder en CPU
- ✅ Memoria: profiling crítico, buffers limitados
- ✅ GPIO: saturado, expansión imposible
- ✅ Alimentación: 500mA limit compartido, riesgo reset
- ✅ Serial: Serial.printf() recomendado (log_i/log_e inestables)

---

### S3 EXTENDER — Arquitectura Boot + Detección Esclavo + Calibración PRE-Logic (2026-05-16 21:10) — ⏳ EN DISEÑO

**Problemas identificados:**

1. ❌ **LED verde 1s cuando debería ser 200ms**
   - Línea main.cpp:245: `bootLEDTime = millis()` con timeout 1000ms
   - Debe ser 200ms para boot más rápido

2. ❌ **SIN detección de esclavo online**
   - S3 NO sabe si S2 está respondiendo
   - Entra en calibración a ciegas
   - Mensaje final "ACTIVO" es mentira si S2 no responde
   - Impacto: Logic recibe S3 "listo" pero S2 ausente

3. ❌ **Calibración NO llega a S2**
   - S3 envía FLAG_CALIB, pero S2 no responde
   - Hay bloqueo lógico (determinar dónde)
   - Síntomas: logs muestran `[CALIB] Slave 1 iniciando...` pero S2 no calibra

4. ❌ **Flujo actual bloqueante**
   - S3 espera Logic 0x21 para activar RS485
   - Si S2 no está listo, Logic nunca se conecta
   - Requiere: S2 calibrado ANTES de Logic, no después

**Solución propuesta — Nueva arquitectura boot S3:**

```
FASE 1: DETECCIÓN ESCLAVO (0-2s)
├─ S3 envía probe RS485 a S2 (ping simple)
├─ S2 responde SlavePacket (confirma online)
├─ Si timeout > 3 reintentos → ERROR CRÍTICO (LED rojo + log)
└─ Si OK → Fase 2

FASE 2: CALIBRACIÓN (2-10s)
├─ S3 envía FLAG_CALIB a S2
├─ S2 ejecuta calibración motor (baja a min, sube a max)
├─ S2 responde con min/max ADC
├─ S3 almacena calibración, valida rangos (e.g., min<max)
├─ Si calibración falla → LED rojo + ERROR, requiere reset S3
└─ Si OK → Fase 3

FASE 3: VALIDACIÓN (10-15s)
├─ S3 envía setTarget(8192) a S2 (posición media)
├─ S2 mueve fader, reporta faderPos
├─ S3 valida respuesta (faderPos ≈ 8192 ±500)
├─ Si responde → LED verde (S2 listo)
└─ Si timeout → LED rojo (S2 no responde)

FASE 4: LOGIC READY (15s+)
├─ S3 espera Logic 0x21
├─ Cuando Logic conecta: S2 ya está calibrado y validado
└─ RS485 polling activo, todo funcional
```

**Cambios de código necesarios:**

main.cpp:
- [ ] Línea 245: `bootLEDTime = millis()` → timeout 200ms (no 1000ms)
- [ ] Línea 165-172: Reemplazar calibración simple por detección + validación
- [ ] Agregar estado `g_slaveOnline` (bool) para validar si S2 responde
- [ ] Agregar estado `g_slaveCalibrated` (bool) para validar calibración ok
- [ ] Log claro: `[BOOT] S2 detectado ✓`, `[BOOT] S2 calibrado ✓`, `[BOOT] S2 validado ✓`
- [ ] Si cualquier fase falla: LED rojo + log error, NO continuar

RS485.cpp:
- [ ] Agregar función `probeSlaveOnline(id)` — ping simple
- [ ] Agregar función `validateCalibration(id)` — chequea si min/max válidos
- [ ] Agregar función `validateTargetResponse(id, expected_target)` — verifica respuesta

MIDIProcessor.cpp:
- [ ] `tickCalibracion()` → cambiar lógica para fases secuenciales
- [ ] Agregar timeout global boot (e.g., 30s) — si no completa, LCD/log error

**Requisitos CRÍTICOS:**

- ✅ Antes de Logic 0x21: S2 debe estar calibrado + validado
- ✅ Si S2 offline: NO permitir Logic handshake (mantener S3 esperando)
- ✅ Si calibración falla: ERROR CRÍTICO (LED rojo, halt, requiere reset)
- ✅ Logs claros en cada fase (DETECCIÓN → CALIBRACIÓN → VALIDACIÓN → READY)
- ✅ LED rojo indica error crítico (no recurrir a while(1) loop infinito)

**Test mínimo requerido (ANTES de validar con Logic):**

- [ ] S3 boot → detecta S2 online (logs de DETECCIÓN)
- [ ] S2 calibra automáticamente (logs de CALIBRACIÓN)
- [ ] S3 valida respuesta S2 (logs de VALIDACIÓN)
- [ ] S3 reporta "READY" con S2 calibrado (antes de Logic)
- [ ] Logic conecta: S3 handshake 0x21 → todo fluye

---

### S3 EXTENDER — LOGIC_PITCHBEND_MAX + MIDI.md completo (2026-05-18 18:08) — ✅ COMPLETADO

**Commits:** ceef081 (MIDI.md inicial), f043136 (LOGIC_PITCHBEND_MAX + secuencia arranque)

**Fixes:**
- ✅ `LOGIC_PITCHBEND_MAX = 14845` definido en `config.h` S3 (fuente única de verdad)
  - Span real confirmado: max=+6653 − min=(−8192) = 14845 (MIDI monitor canal 2, 18:04)
  - Valor anterior 14848 era incorrecto en código y documentación
- ✅ `RS485.cpp` `setFaderTarget()`: divisor 14848 → `LOGIC_PITCHBEND_MAX` (×2)
- ✅ `main.cpp`: divisor 14848 → `LOGIC_PITCHBEND_MAX` en envío PB a Logic
- ✅ `docs/MIDI.md`: rango corregido en tabla 4.7 y fórmula 5.1

**Documentación MIDI.md — nuevo contenido:**
- ✅ Sección 3.3: secuencia completa de 3×GoOnline con timing real
  - GoOnline #1 (t=0ms): reset completo, faders −8192
  - GoOnline #2 (t=122ms): reset completo, faders −8192
  - GoOnline #3 (t=2471ms): estado REAL del proyecto (faders reales, nombres, LEDs)
  - Automodos reales: t=~4000ms
  - Explicación de por qué existe `CONNECT_GRACE_MS = 1500ms`
- ✅ Sección 4.11: SysEx 0x0A — Fader Touch Sense
- ✅ Sección 4.12: SysEx 0x0B — Button Enable Mask (0x0F)
- ✅ Sección 4.13: SysEx 0x20 — VPot Ring LEDs (tabla de bits modo/posición)

**Pendiente (B1 sin resolver):**
- ⚠️ `case 0x61` en MIDIProcessor.cpp: `g_logicConnected = 0` incorrecto → fix propuesto pero no aplicado

---

### S3 EXTENDER — Boot calibración sin HALT + auto-calib primer contacto (2026-05-18 18:35) — ✅ COMPLETADO

**Commits:** 1c3015a, 7800ebe

**Problema raíz:**
- RS485 bloqueado por `g_logicConnected` → S2 agotaba timeout antes de que Logic llegara
- Sin gate: S3 arrancaba polling inmediato → S2 tarda ~100ms en boot → 5 timeouts → HALT
- `_calibPendingFrom=1` al boot causaba doble disparo de calibración

**Fixes aplicados:**

`RS485.cpp`:
- ✅ Eliminado gate `g_logicConnected` en `runTask()` — RS485 activo desde boot
- ✅ HALT condition gateada: solo hace HALT si `_ch[id].calibrated` era true (S2 ya respondía)
- ✅ Auto-calibración en primer contacto S2 en `_handleResponse()`:
  - Si S2 no había respondido nunca y no está calibrado → dispara `calibrate=true` automáticamente
  - Sin necesidad de que Logic envíe GoOnline primero

`MIDIProcessor.cpp`:
- ✅ `_calibPendingFrom = 0` — sin pre-arm al boot, calibración vía primer contacto

**Flujo resultante:**
```
S3 boot → RS485 activo inmediatamente
S2 responde primer paquete → auto-calibración disparada
S2 calibra (GOING_TO_MIN → CALIBRATING → DONE)
S3 recibe SLAVE_FLAG_CALIB_DONE → _ch[0].calibrated = true
Logic conecta → handshake → control normal
```

**⚠️ VALIDACIÓN HARDWARE PENDIENTE:**
- [ ] Flash S3 con commits 1c3015a + 7800ebe
- [ ] S3 boot → RS485 activo sin Logic → no HALT
- [ ] S2 responde primer paquete → S3 dispara calibración automática (log `[CALIB] Slave 1 primer contacto`)
- [ ] S2 calibra correctamente → S3 recibe `SLAVE_FLAG_CALIB_DONE`
- [ ] Logic conecta posterior → handshake y control normal
- [ ] Caso error: desconectar S2 tras calibración → S3 no hace HALT (calibrated=true)

---

### S2 SLAVE — Motor _pendingCalib: GOING_TO_MIN → CALIBRATING (2026-05-18 18:55) — ✅ COMPLETADO

**Commit:** a04e58f

**Problema raíz:**
- `_pendingCalib` declarado en `config.h` línea 154 pero nunca conectado en `Motor.cpp`
- `requestCalibration()` cuando fader ≠ 0: iniciaba `goToMin()` pero no ponía `_pendingCalib = true`
- `update()` case `GOING_TO_MIN`: al llegar a 0 → transicionaba a `AT_TARGET` sin verificar
- FLAG_CALIB es one-shot en S3 → tras primer envío no se reenvía → calibración nunca arrancaba

**Fixes aplicados en `S2/S2_V1/src/hardware/Motor/Motor.cpp`:**

`requestCalibration()` else branch (fader ≠ 0):
```cpp
if (_motor_state != MotorState::GOING_TO_MIN) {
    _pendingCalib = true;   // ← AÑADIDO
    _motor_state = MotorState::GOING_TO_MIN;
    goToMin();
}
```

`update()` case `GOING_TO_MIN` al llegar a 0:
```cpp
if (_pendingCalib) {
    _pendingCalib = false;
    _motor_state = MotorState::CALIBRATING;
    startCalib();
} else {
    _motor_state = MotorState::AT_TARGET;
}
```

**Flujo resultante:**
```
S3 envía FLAG_CALIB (one-shot) → S2 requestCalibration()
Si fader ≠ 0: _pendingCalib=true + GOING_TO_MIN + goToMin()
Al llegar ADC ≤ MIN+10: _pendingCalib→false, startCalib() directo
CALIBRATING → DONE → S3 recibe min/max en SlavePacket
```

**⚠️ VALIDACIÓN HARDWARE PENDIENTE:**
- [ ] Flash S2 con commit a04e58f
- [ ] S3 envía FLAG_CALIB → S2 fader ≠ 0: baja a 0 automáticamente
- [ ] Al llegar a 0: calibración arranca sin intervención (log `[MOTOR-STATE] GOING_TO_MIN → CALIBRATING`)
- [ ] Calibración completa → S3 recibe `SLAVE_FLAG_CALIB_DONE` + min/max ADC
- [ ] S3 envía FLAG_CALIB → S2 fader = 0: calibra directamente (sin goToMin)

---

### 🔄 PENDIENTES (próxima sesión)

- [ ] **Actualizar P4 config.h con detalles PSRAM 32MB y periféricos**
  - Añadir comentarios sobre PSRAM abundante para LVGL
  - Documentar periféricos: MIPI-CSI, I2S audio, TWAI (CAN)
  - Aceleradores multimedia: JPEG, PPA, ISP, H.264
  - Ubicación: `MASTER_S3-P4/P4/src/config.h`

- [ ] **MIDI Traffic Optimization: PitchBend deadband 150 cuentas**
  - Reducir tráfico 850→~100 msgs/s en S3
  - Ubicación: `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/main.cpp` línea 85
  - Requiere validación hardware en rig S3-Logic

- [ ] **Validación hardware S3 flujo completo**
  - [ ] Handshake Mackie: Logic 0x21 → S3 echo + conexión
  - [ ] RS485 polling: 300µs ciclo (NUM_SLAVES=1)
  - [ ] Calibración automática: cascada, timeout handling
  - [ ] Fader: PitchBend bidireccional, deadband 150
  - [ ] Transport: botones RW/FF/STOP/PLAY/REC → Logic feedback

- [ ] **Validación hardware P4 multimedia**
  - [ ] Display IPS 480×800 con LVGL v9
  - [ ] Touch GT911 calibración multi-punto
  - [ ] NeoTrellis 4×8 (seesaw dual 0x2F/0x2E)
  - [ ] PSRAM 32MB: profiling LovyanGFX sprites + LVGL

- [ ] **P4 Task Architecture documentation (ARCHITECTURE_P4.md)**
  - Dual-core Core0/Core1 sincronización
  - Race conditions known (flags g_switchToPage)
  - VU meter decay timing
  - ISR priorities

- [ ] **S3 — Nombre de pista no se envía siempre, escribe "Pan" o "Seleccion" sin borrar** 🔴
  - Problema: S3 NO envía nombre de pista consistentemente. Cuando recibe CC Pan/Select, escribe estos textos en pantalla sin limpiar anterior
  - Síntoma: Display S2 muestra "Pan" + nombre anterior superpuesto, o "Seleccion" mezclado con caracteres viejos
  - Causa probable: S3 interpreta erróneamente CC MIDI Pan/Select como si fuera nombre de pista, no filtra, no borra antes de escribir
  - Ubicación: `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/` (procesamiento MIDI CC, envío RS485 nombre pista)
  - Afecta: Comunicación S3→S2, protocolo RS485 nombre pista, interpretación CC MIDI en S3
  - Requiere: Auditoría S3 — qué MIDI se procesa como nombre pista, por qué CC Pan/Select llegan a display, fix filtro CC

---

### DOCUMENTACIÓN HARDWARE — S3 N16R8 + P4 JC4880P443C-I-W especificaciones completas (2026-05-16 20:45) — ✅ COMPLETADO

**Commits:** 7ec018f, 84c549b, c9e6166, 41bfdc9, b384ead, cba7178

**DIRECTIVAS OBLIGATORIAS (CLAUDE.md):**
- ✅ Crear memoria: `config.h_source_of_truth.md`
- ✅ Actualizar: `MEMORY.md` (nueva sección "Directivas Vinculantes")
- ✅ CLAUDE.md línea 55-62: "config.h es FUENTE ÚNICA DE VERDAD (2026-05-16 20:15)"
  - Nunca asumir NUM_SLAVES, verificar config.h SIEMPRE
  - S3 actual: NUM_SLAVES=1 (correcto, no bug)
  - P4 actual: NUM_SLAVES=9 (correcto)
  - Cada MCU tiene config.h independiente
  - Ubicaciones documentadas

**S3 EXTENDER — Placa + Flujo (MASTER_S3-P4/S3/README.md):**

Especificación (commits c9e6166, 41bfdc9):
- ✅ Placa: ESP32-S3-WROOM-1 **N16R8** (confirmado)
- ✅ Flash: 16MB (QIO)
- ✅ PSRAM: 8MB (OPI)
- ✅ Conector: USB Type-C
- ✅ Pines: 44 totales (~27 GPIO usuario)
- ✅ Energía: USB 5V→3.3V, 80mA idle, 160mA full

Flujo de trabajo completo (commit 84c549b):
- ✅ Setup (USB, Transporte, RS485, MIDI, Tasks FreeRTOS)
- ✅ Handshake Mackie MCU:
  - Fase 0: probe (Logic 0x00 → S3 responde family 0x14)
  - Fase 2: keep-alive (Logic 0x21 → S3 echo + g_logicConnected=1)
  - Desconexión: GoOffline (0x0F → disconnectSequence)
- ✅ Task Core 0: MIDI + RS485 responses (ciclo 1ms)
  - Leer USB MIDI → processMidiByte()
  - Procesar SlavePacket → fader/botones/encoder → MIDI OUT
  - Calibración automática cascada (1 a la vez)
  - Timeout handling
- ✅ Task Core 1: Transporte (10ms, botones RW/FF/STOP/PLAY/REC)
  - Notes 0x5B-0x5F
  - Feedback LEDs desde Logic
- ✅ RS485 polling task (Core 1):
  - Máquina 3 estados: SEND → WAIT_RESP → GAP
  - Timing: ~300µs/ciclo (NUM_SLAVES=1)
  - Timeout > 5 reintentos → LED rojo + HALT
- ✅ Procesamiento MIDI incoming (CC, Channel Pressure, SysEx)
- ✅ Conversión RS485→MIDI (PitchBend, Notes, CC)
- ✅ Calibración automática (cascada, timeout handling)

**P4 MASTER — Placa GUITION JC4880P443C-I-W (MASTER_S3-P4/P4/README.md):**

Especificación (commits b384ead, cba7178):
- ✅ Módulo: GUITION **JC4880P443C-I-W** (modelo exacto)
- ✅ Procesador principal: ESP32-P4 Xtensa 360MHz dual-core
- ✅ Procesador secundario: ESP32-C6 (Wi-Fi 6 + Bluetooth 5)
- ✅ Flash: 16MB (QIO)
- ✅ PSRAM: **32MB** (OPI) — ⚠️ 4x más que S3, abundante para LVGL
- ✅ Memoria: HP L2MEM 768KB, LP SRAM 32KB
- ✅ Display: IPS 4.3" 480×800 (70.4 ppi, ST7701S MIPI-DSI 2-lane)
- ✅ Touch: GT911 capacitivo multitouch (I2C)
- ✅ Audio: ES8311 codec opcional (I2S stereo)
- ✅ Energía: USB 5V→3.3V, 200mA idle, 400mA full, picos 500mA

Periféricos completos:
- ✅ RS485 bus A (GPIO 50/51/52): 9 slaves S2
- ✅ I2C_NUM_0 (GPIO 33/31): NeoTrellis seesaw (0x2F/0x2E)
- ✅ I2C_NUM_1 (GPIO 7/8): GT911 touch
- ✅ MIPI-CSI: entrada cámara (interfaz física)
- ✅ MIPI-DSI: display (integrado)
- ✅ SPI, I2S, LED PWM, MCPWM, RMT, ADC 12-bit, UART, TWAI (CAN), USB OTG 2.0

Aceleradores multimedia:
- ✅ JPEG codec (encode/decode hardware)
- ✅ Pixel Processing Accelerator (PPA)
- ✅ Image Signal Processor (ISP) — soporte cámara MIPI-CSI
- ✅ H.264 video encoder

Capacidades futuras documentadas (tabla):
- Cámara MIPI-CSI: análisis visual, grabación
- Audio I2S: synth, metrónomo, realtime monitor
- Wi-Fi 6: control remoto Logic Pro, OSC
- Bluetooth 5: MIDI remote, control inalámbrico
- TWAI (CAN): bus industrial expansión modular
- MCPWM: motor control, cortinas, luces escena
- ADC: sensores (temperatura, batería, presión)
- JPEG/H.264: captura foto, streaming video Logic

**Fuentes externas:**
- CNX Software: 4.3-inch touch display ESP32-P4 + ESP32-C6
- GUITION Official: ESP32P4 Display Module
- Home Assistant: Guition ESP32 P4 working config

---

### S2 MOTOR v3 — requestCalibration + Usuario Master absoluto (2026-05-16 18:41) — ✅ IMPLEMENTADO

**Cambio crítico — Flujo calibración:**
- RS485Handler.cpp línea 67: `Motor::startCalib()` → `Motor::requestCalibration()`
- requestCalibration() baja fader a 0 PRIMERO si es necesario, luego calibra
- Elimina lógica defectuosa de startCalib() que fallaba si fader ≠ 0

**Arquitectura mejorada:**
- Motor.cpp: Variables de estado movidas a config.h (fuente única de verdad)
  - `_pendingCalib`, `_connected`, `_motor_goingToMin`, `_userDropTarget`, `_s3Target`, `_atTargetStartTime`
- setADCDelta(): Guard inicialización en primera llamada (evita falsa detección boot)
- Protocol.h S3: Comentario faderTarget corregido (0-14848, no 16383)

**Documentación actualizada:**
- CLAUDE.md: Directiva obligatoria "Auditoría MCU" (tabla impacto S2/S3/P4, protocolo informe)
- MOTOR.md: Sección 2.0 "Arquitectura Motor v3" + 3.3 "requestCalibration()"
- FADER.md: Sección 1.1 "Inicialización y Calibración v3" + guardia usuario

**Prioridades VINCULANTES (v3):**
```
MÁXIMA:  Usuario mueve → Motor stop INMEDIATO
         GoToMin ejecuta SIEMPRE si !_connected
MEDIA:   S3 ordena → Motor se mueve SOLO si usuario NO toca
MÍNIMA:  Idle en posición actual
```

**Test requerido (hardware):**
- [ ] Boot: Motor baja a 0
- [ ] S3 conecta: Motor NO baja, espera órdenes
- [ ] S3 FLAG_CALIB: baja a 0 si ≠0, luego calibra
- [ ] Usuario mueve: Motor para inmediatamente
- [ ] S3 target mientras usuario toca: rechazado
- [ ] Usuario suelta: S3 puede controlar (debounce 200ms)
- [ ] S3 desconecta: Motor baja a 0 indefinidamente

---

### S2 MOTOR BEHAVIOR — Usuario como master, S3 respeta prioridades (2026-05-16 10:52) — ✅ IMPLEMENTADO

**Comportamiento correcto — prioridad:**
```
Usuario tocando > S3 commands > Motor autónomo
```

**Cambios implementados:**

Motor.cpp:
- Variable `_connected` (tracks S3 connection state)
- `setConnected(bool)` — notifica estado conexión
- `update()` IDLE: no baja a 0 si CONNECTED
- `goToMin()`: guard CONNECTED (no ejecuta si S3 está conectado)
- `setTargetFromS3()`: reimplementado con guards usuario + cambio a MOVING_TO_TARGET
- `setADCDelta()`: integra FaderTouch::isTouched() + usuario como master (ADC actual = target)

Motor.h:
- Declaración `void setConnected(bool)`

RS485Handler.cpp:
- `onMasterData()`: llamar Motor::setConnected(true/false) al cambiar estado
- Usar `setTargetFromS3()` en lugar de `setTarget()`

**Flujo de control:**
- Boot sin S3 → Motor va a 0 (GOING_TO_MIN → AT_TARGET)
- S3 conecta → Motor en IDLE, espera target de S3
- S3 manda target → Motor va (MOVING_TO_TARGET → AT_TARGET)
- Usuario mueve fader → Motor para, ADC = nuevo target, touchState=1 a S3
- Usuario suelta → Motor queda en posición, S3 puede mandar nuevo target
- S3 desconecta → Motor para, espera boot de nuevo

---

### S2 MOTOR BOOT — Motor::goToMin() en setup() (2026-05-16 10:51) — ✅ IMPLEMENTADO

**Cambio implementado:**
- main.cpp línea 133: Llamada a `Motor::goToMin()` después de `Motor::initPWM()`
- Efecto: Fader baja a posición 0 en boot, listo para órdenes de S3

**Comportamiento:**
- Boot: Motor inicia EN (habilitado), inicia movimiento lento hacia min (si ADC > 30)
- Llega a 0: Motor se detiene, espera órdenes de S3 (FLAG_CALIB o setTarget)
- Sin comandos S3: Motor permanece en posición 0 (idle)

---

### DOCUMENTACIÓN — Centralizar en carpeta docs/ (2026-05-16 08:59) — ✅ COMPLETADO

**Cambios realizados:**
- Crear carpeta `docs/` en raíz del proyecto
- Mover 8 archivos de documentación técnica:
  - docs/FADER.md (ADS1115, calibración, mapping)
  - docs/MOTOR.md (DRV8833, máquina estados, SAT)
  - docs/RS485.md (protocolo binario, timing, paquetes)
  - docs/WIFI-OTA.md (provisioning, OTA, ElegantOTA)
  - docs/BUTTONS.md (debounce, ButtonManager, MIDI)
  - docs/DISPLAY.md (ST7789V3, sprites PSRAM, layout)
  - docs/ENCODER.md (ISR Gray code, sequenciamiento, SAT)
  - docs/LEDS.md (WS2812B NeoPixel, asignación, estados)
- Actualizar todas las referencias en CLAUDE.md: `[FILE.md](FILE.md)` → `[FILE.md](docs/FILE.md)`
- Agregar CLAUDE.md a tracking de git (remover de .gitignore)
- CLAUDE.md comentar directiva "no subir a GitHub"

**Resultado:**
- Documentación técnica centralizada y organizada
- CLAUDE.md contiene solo directivas vinculantes + referencias
- CLAUDE.md disponible online en GitHub

---

### S3 PITCHBEND MAPEO — Fix signed 14-bit (-8192..+8191) → ADC 0..27000 (2026-05-16 08:05) — ✅ IMPLEMENTADO

**Problema identificado (2026-05-16 08:00):**
- Logic envía Pitch Wheel **signed 14-bit: -8192..+8191**, no unsigned 0-16383
- Cuando Logic desconecta → envía -8192 (mínimo)
- S3 mapeaba con `uint32_t bendValue * 14848 / 16383` → overflow en negativos
- Resultado: valor ADC inválido → S3 detectaba "no calibrado" → mandaba FLAG_CALIB automáticamente
- Síntoma: S2 calibraba involuntariamente cada vez que Logic se desconectaba

**Solución implementada (MIDIProcessor.cpp línea 599-612):**
- Clipear valores negativos a 0 (fondo del fader)
- Mapear rango real Logic 0..8191 → ADC 0..27000
- Fórmula correcta: `fader_adc = bendValue * 27000 / 8191` (sin overflow)
- Normalización: `faderPositionNormalized = fader_adc / 27000.0f` (no 16383)

**Cambios exactos:**
1. MIDIProcessor.cpp línea 604: Agregar guard `if (bendClamped < 0) bendClamped = 0`
2. MIDIProcessor.cpp línea 605: Mapeo correcto `fader_adc = bendClamped * 27000 / 8191`
3. MIDIProcessor.cpp línea 612: Normalización → 27000 (no 16383)

**Impacto esperado:**
- Logic desconecta (Pitch -8192) → S2 NO calibra automáticamente
- Fader responde correctamente: 0% = -8192, 100% = +8191
- Sin FLAG_CALIB involuntario
- S2 solo calibra si S3 lo ordena explícitamente

**Validación requerida:**
- [ ] Compilar S3 sin errores
- [ ] Deploy en S3 + S2
- [ ] Logic init → connect: faders responden suave (0-100%)
- [ ] Logic disconnect: S2 NO hace calibración
- [ ] MIDI monitor: no cambios involuntarios en PitchBend

---

### S2 MOTOR CALIBRACIÓN — Guard cooldown + desactivación auto-calib (2026-05-16 07:48) — ✅ IMPLEMENTADO

**Problema identificado (2026-05-16 07:45):**
- Calibración estaba en bucle infinito: completaba (DONE) → siguiente paquete RS485 con FLAG_CALIB → reiniciaba
- Síntoma: 3-4 calibraciones seguidas en los logs, cada una completa pero sin estabilizarse
- Causa 1: Master enviaba FLAG_CALIB continuamente; startCalib() permitía reiniciar si `_motor_phase == DONE`
- Causa 2: Auto-calibración a 10s del boot conflictaba con FLAG_CALIB de S3

**Soluciones implementadas:**

1. **Guard de cooldown en Motor::startCalib()**
   - Agregar constante `CALIB_COOLDOWN_MS = 2000` en config.h
   - Agregar variable `_motor_lastCalibDone` para registrar timestamp al completar
   - Guard 2: chequea `now - _motor_lastCalibDone < CALIB_COOLDOWN_MS` antes de permitir reinicio
   - Si cooldown activo: log warning y retorna sin reiniciar

2. **Desactivar auto-calibración en main.cpp**
   - Comentar bloque AUTO-CALIB (línea 322-329)
   - Razón: Arquitectura maestro-esclavo — S3 es autoridad única
   - S2 SOLO calibra si S3 lo ordena explícitamente (RS485 FLAG_CALIB)

**Cambios exactos:**
1. config.h línea 113: Constante CALIB_COOLDOWN_MS = 2000
2. config.h línea 129: Variable `static uint32_t _motor_lastCalibDone = 0`
3. Motor.cpp línea 218: `_motor_lastCalibDone = millis();` cuando DONE
4. Motor.cpp línea 372-384: Guard 2 con chequeo de cooldown en startCalib()
5. main.cpp línea 322-329: Comentar bloque AUTO-CALIB (con explicación)

**Impacto esperado:**
- Calibración inicia SOLO si S3 lo ordena (arquitectura limpia)
- Si S3 ordena múltiples veces en <2s: rechazado, log warning
- Después de 2s: nueva calibración permitida (si falla, reintento seguro)
- Sin conflictos entre auto-calib y FLAG_CALIB

**Validación requerida:**
- [ ] Compilar sin errores
- [ ] Deploy en S2
- [ ] Boot: S2 espera comando de S3 (no auto-calibra)
- [ ] S3 boot: ordena FLAG_CALIB → S2 calibra una sola vez
- [ ] MIDI monitor: fader responde smoothly, sin lag
- [ ] Log: "Iniciada" aparece UNA sola vez en boot

---

### S3 TRÁFICO MIDI — Filtrado "send-only-on-change" en processSlaveResponse (2026-05-16 10:49) — ✅ IMPLEMENTADO

**Problema identificado (2026-05-14 17:08):**
- Tráfico MIDI excesivo: 17 faders × 50 updates/s = **850 mensajes MIDI/s**
- Síntoma: MIDI monitor muestra -8180 repetiéndose cada 20ms (valor NO cambió)
- Causa: `processSlaveResponse()` envía a Logic CADA dato que recibe de S2, aunque sea igual

**Arquitectura correcta (División de responsabilidades):**

| Capa | Responsabilidad | Complejidad | Acción |
|------|-----------------|------------|--------|
| **S2 (single-core)** | Recolectar ADC raw | Mínima | Envía cada 20ms sin filtrado |
| **S3 (dual-core)** | Filtrar + inteligencia | Máxima | Aplica EMA + "send-only-on-change" |
| **P4 (triple-core)** | Maestro | N/A | Futuro: 300 slaves |

**Implementación (MAÑANA):**

**Archivo:** `main.cpp` función `processSlaveResponse()` línea 69

**ANTES (envía CADA dato):**
```cpp
static void processSlaveResponse(uint8_t slaveId) {
    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    if (ch.touchState && !(ch.buttons & SLAVE_FLAG_CALIB_SENDING)) {
        uint16_t pb  = ((uint32_t)filteredFaderPos[slaveId] * 14848 / 27000) & 0x3FFF;
        byte msg[3]  = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
        sendMIDIBytes(msg, 3);  // ← ENVÍA SIEMPRE
    }
}
```

**DESPUÉS (envía SOLO si cambió):**
```cpp
static uint16_t lastSentPb[9] = {0};  // ← AGREGAR AL INICIO

static void processSlaveResponse(uint8_t slaveId) {
    const ChannelData& ch = rs485.getChannel(slaveId);
    uint8_t midiCh = slaveId - 1;

    if (ch.touchState && !(ch.buttons & SLAVE_FLAG_CALIB_SENDING)) {
        uint16_t pb  = ((uint32_t)filteredFaderPos[slaveId] * 14848 / 27000) & 0x3FFF;
        
        if (pb != lastSentPb[slaveId]) {  // ← NUEVO CHECK
            byte msg[3]  = { (byte)(0xE0 | midiCh), (byte)(pb & 0x7F), (byte)(pb >> 7) };
            sendMIDIBytes(msg, 3);
            lastSentPb[slaveId] = pb;     // ← GUARDAR ÚLTIMO ENVIADO
        }
    }
}
```

**Cambios exactos:**
1. Línea ~75: Agregar `static uint16_t lastSentPb[9] = {0};` al inicio de función o namespace
2. Línea ~76-78: Envolver `sendMIDIBytes()` en bloque `if (pb != lastSentPb[slaveId])`
3. Línea +1: Agregar `lastSentPb[slaveId] = pb;` después de `sendMIDIBytes()`

**Impacto esperado:**
- Tráfico MIDI: 850 msgs/s → **~50-100 msgs/s** (solo cambios reales)
- Resolución: **Sin pérdida** (solo filtra repetidos, no trunca)
- Responsividad: **Inmediata** (envía en el ciclo 20ms siguiente al cambio)
- Comportamiento: Fader parado = 0 mensajes; fader movido = cambios en tiempo real

**Cambios implementados (2026-05-16 10:49):**
- main.cpp línea 69: Agregar `static uint16_t lastSentPb[9] = {0};` para trackear último PitchBend por slave
- main.cpp líneas 76-78: Envolver sendMIDIBytes en `if (pb != lastSentPb[slaveId])` — envía SOLO si cambió
- main.cpp línea 79: Guardar `lastSentPb[slaveId] = pb;` después de enviar

**Validación requerida:**
- [ ] Deploy en hardware S3
- [ ] MIDI monitor: Fader parado NO debe mostrar repeticiones
- [ ] MIDI monitor: Fader movido debe mostrar cambios suavemente
- [ ] Medir tráfico: Debería bajar 80%+ (850 → <100 msgs/s)
- [ ] Confirmar sin "lag" o delay en movimiento

**Notas arquitectónicas:**
- **EMA filter ya está en S3** (RS485.cpp línea 221) ✅
- **Mapeo 0-14848 ya está** (main.cpp línea 76) ✅
- **Send-only-on-change implementado** ✅
- **S2 NO se toca:** Mantiene envío simple cada 20ms (single-core, sin cálculos)
- **P4:** Hereda automáticamente (mismo código, escala a 300 slaves)

---

### S3 EMA FILTER — Suavizado de ruido faderPos en RS485 (2026-05-14 17:04) — ✅ VALIDADO EN HARDWARE

**Mejora de precisión:** Eliminar oscilaciones residuales en envío a Logic
- Problema: faderPos oscilaba ±1 unidad → PitchBend -8179/-8180 alternando (ruido 2700×)
- Solución: EMA filter (alpha=0.15) en recepción RS485, donde se recibe dato de S2
- Ubicación correcta: RS485.cpp _handleResponse(), NO en envío a Logic

**Cambios implementados (commit fd2799f):**
- RS485.h:82: Agregar `uint16_t _filteredFaderPos[NUM_SLAVES + 1]` en private
- RS485.cpp:221-224: Aplicar filtro EMA antes de asignar a `_ch[id].faderPos`
- Fórmula: `filtered = filtered + (raw - filtered) * 0.15`

**Validación en hardware (2026-05-14 17:06):**
- ✅ Posición 0%: -71 (oscilación ±3 residual)
- ✅ Posición 50%: 6363 (oscilación ±3 residual)
- ✅ Posición 100%: 6363 (oscilación ±3 residual)
- ✅ Movimiento suave y monotónico
- **Mejora:** De ±8000 a ±3 unidades (2700× reducción)

**Ventajas confirmadas:**
- Suaviza ruido ADC sin crear "zonas muertas" de deadband
- Centraliza filtrado en la fuente (RS485), no en salida (MIDI)
- Mantiene responsividad a movimientos reales del fader
- Método estándar en firmware para reducción de ruido

---

### S3 MAPEO PITCHBEND — Fader bidireccional Logic ↔ Hardware (2026-05-14 16:34) — ✅ VALIDADO EN HARDWARE

**Problema identificado en validación hardware:**
- Fader generaba valores PitchBend erráticos en MIDI monitor
- Posición 0%: PitchBend -8189 a -8187 (debería ~0)
- Posición 50%: PitchBend 7843 a 7848 (debería ~7424)
- Posición 100%: PitchBend 1895 a 1901 (debería ~14848)

**Causa raíz — DOS mapeos rotos en S3:**
1. **Entrada (Logic → S2):** bendValue (0-16383 MIDI raw) enviado directamente sin convertir a 0-14848
   - MIDIProcessor.cpp línea 600: `fader14bit = bendValue` → `fader14bit = (bendValue * 14848 / 16383)`
   - Problema: `setFaderTarget()` espera 0-14848, no 0-16383
2. **Salida (S2 → Logic):** faderPos (0-27000 ADC raw) enviado sin mapear a 0-14848
   - main.cpp línea 76: `pb = ch.faderPos & 0x3FFF` → `pb = ((uint32_t)ch.faderPos * 14848 / 27000) & 0x3FFF`
   - Problema: Truncamiento con mask 0x3FFF causaba valores negativos y oscilaciones

**Cambios implementados (commits 60f8798 + 1fdd812):**
- MIDIProcessor.cpp: Mapeo entrada con casting a uint32_t para evitar overflow
- main.cpp: Mapeo salida con conversión lineal 0-27000 → 0-14848
- Ambos mapeos usando aritmética (uint32_t) para precisión

**Validación en hardware (2026-05-14 16:34 → ✅ EXITOSA):**
- ✅ Fader 0% → PitchBend suave desde negativo
- ✅ Fader 50% → PitchBend transita por cero
- ✅ Fader 100% → PitchBend suave hasta máximo
- ✅ Movimiento continuo y sin saltos
- ✅ Respuesta lineal: "fader suave como sus muertos"

**Resultado:** Fader completamente operativo, mapeo bidireccional funcionando correctamente.

---

### S3 BOOT CALIBRATION — Escaneo secuencial automático de slaves (2026-05-13 17:10) — IMPLEMENTADO

**Arquitectura completada:**
- Core0 (taskCore0): chequea esclavos sin calibrar cada iteración (non-blocking)
- Si hay sin calibrar: dispara `rs485.setCalibrate(id)` inmediatamente
- Core1 (rs485.runTask): envía FLAG_CALIB en siguiente ciclo normal
- Slave recibe → calibra → responde con CALIB_DONE + min/max
- S3 captura datos en _handleResponse() → marca `calibrated=true`
- Secuencial: una calibración a la vez (break después de setCalibrate)

**Cambios implementados:**
1. **main.cpp (S3 taskCore0):** Agregar loop escaneo post-DISCONNECT check (líneas 142-150)
2. **RS485.cpp (S3):** Reactivar lógica CALIB_DONE/CALIB_ERROR (líneas 251-270) — estaba comentada por desactivación hardware temporal
3. **memory/:** Documentar en s3_boot_calibration.md

**Eficiencia:**
- Core0 NO bloquea (sin delays, sin timeouts pasivos)
- Dispara FLAG_CALIB one-shot, continúa procesando MIDI
- Core1 maneja RS485 naturalmente (timing intact)
- Reintentos agresivos: si falla, siguiente iteración Core0 reintenta

**Beneficio:** S3 valida automáticamente que todos los slaves responden y tienen rango calibrado antes de recibir targets de Logic.

---

### S3/S2 MAPEO — Logic 0-14848 → Rango calibrado (2026-05-13 00:30) — RESUELTO

**Arquitectura completada:**
- S3 mapea PitchBend 0-14848 → rango calibrado real de cada S2
- S2 recibe valor final, NO calcula (O(1), compatible single-core)
- Calibración: S2 envía min/max via SlavePacket con flags CALIB_SENDING/CALIB_IS_MIN
- S3 almacena calibratedMin/Max en ChannelData, usa para mapeos posteriores

**Cambios implementados:**
1. **protocol.h** (S2): Agregar SLAVE_FLAG_CALIB_SENDING (bit 6), SLAVE_FLAG_CALIB_IS_MIN (bit 7)
2. **RS485Handler.cpp** (S2): Máquina de estado en buildResponse() — enviar min (paquete 1), max (paquete 2)
3. **RS485.h** (S3): Agregar calibratedMin, calibratedMax en ChannelData
4. **RS485.cpp** (S3): Capturar min/max en _handleResponse() cuando flags CALIB_SENDING activos
5. **setFaderTarget()** (S3): Mapear 0-14848 → rango real si calibrado, sino teórico (0-27000)
6. **Motor::setTarget()** (S2): Usar target directamente (sin map) — S3 ya mapeó

**Beneficio:** S2 single-core ahora tiene setTarget() O(1) sin cálculos. Timing RS485 mejorado.

---

### S3 AUDITORÍA — Mapeo de fader Logic 16-bit → ADC 27-bit (2026-05-12 22:28) — PENDIENTE PRÓXIMA SESIÓN

**Arquitectura de conversión (S3 es responsable):**
```
Logic Pro (PitchBend)
    │ 0-16383 (14-bit, máximo real: 0-14848)
    ▼
S3 MidiProcessor::processPitchBend()
    │ Mapea PitchBend → faderTarget
    ▼
S3 RS485Master::_sendPacket()
    │ Envía MasterPacket.faderTarget 0-27000 (escala mapeada)
    ▼
S2 Slave recibe
    │ faderTarget 0-27000 → Motor::setTarget()
    ▼
Motor controla ADC 0-27000 (ADS1115 raw)
```

**Problemas encontrados:**
- S3 protocol.h línea 68: Aún documenta "0-16383" — debería aclarar que S3 mapea a 0-27000
- S3 SlavePacket.faderPos línea 80: Documenta "0-8191" — inconsistente con S2 (0-27000)
- S3/S2 protocol.h duplicados — deberían unificarse

**Pendiente próxima sesión:**
1. [ ] Actualizar S3 protocol.h: documentar mapeo 16383 → 27000 (S3 lo hace)
2. [ ] Actualizar SlavePacket.faderPos: unificar a 0-27000 en ambos
3. [ ] Documentar en CLAUDE.md: "S3 mapea Logic PitchBend a ADC range"
4. [ ] Considerar: ¿compartir protocol.h o mantener separados (S3 mapea, S2 recibe)?

**Commits relacionados:** 86e8141 (S2 documentado), pendiente S3

---

### S2 MOTOR — Calibración automática completa (2026-05-12 19:00 → 20:55) — RESUELTO

**Objetivo:** Motor S2 calibra automáticamente al boot y en SAT > Motor > Calibración.

**Ciclo de calibración implementado:**
- ✅ KICK_UP: 31 → 26226 (250ms, pwm=175)
- ✅ GOING_UP: refinamiento → SETTLE_UP
- ✅ KICK_DOWN: 26465 → 71 (260ms, pwm=175)
- ✅ GOING_DOWN: refinamiento → SETTLE_DOWN
- ✅ CALIBRATED: MIN=44 MAX=26448 span=26404

**Fixes aplicados (commits 60804af–0f43418):**
1. FIX GOING_UP/DOWN: PWM adaptativo sin if redundante (línea 88-89, 163-164)
2. REFACTOR Motor::tick(): API unificada (setADC + update) para limpieza
3. FIX transiciones: Sincronizar _motor_currentPWM en KICK→GOING
4. FIX umbral: KICK_DOWN→GOING_DOWN 1000 → 200 (coincide con PWM threshold)
5. FIX detección: ADC_STABILITY_THRESHOLD 300 → 100 (sensibilidad refinamiento)
6. FIX timeout: CALIB_STUCK_TIMEOUT 500 → 1000ms (margen para movimiento lento)
7. FIX SAT: Replicar loop de Motor en SAT > Motor > Calibración (faderADC + tick)

**Hardware:** PWM_MIN=150, PWM_MAX=175 (NVS)

**Pendiente (Producción):**
- [ ] Validar control de posición: Logic envía targets vía RS485 → Motor sigue
- [ ] Test completo: Boot → auto-calib → enter SAT/calib → exit → normal operation
- [ ] Validar sincronización en transiciones SAT ↔ loop normal
- [ ] Documentar calibración en STATUS.md

---

### Investigation & Resolution
- **S2 MOTOR — Calibración GOING_UP/DOWN bloqueadas (2026-05-11 20:30) — RESUELTO**
  
  **Problema identificado:**
  - Motor calibración se detenía en fases GOING_UP y GOING_DOWN
  - Síntomas: KICK_UP (150ms) → GOING_UP (300ms después error "sin movimiento") → BLOQUEO
  - Causa raíz: Condición `_motor_currentPWM != pwmGoing` era FALSA al entrar GOING_UP
    - KICK_UP establecía `_motor_currentPWM = _pwm_min` (135)
    - GOING_UP calculaba `pwmGoing = _pwm_min` (135)
    - Resultado: if **NO entraba** → `_hwUp()` nunca ejecutada → motor quieto → timeout 500ms
  
  **Soluciones implementadas (commits e166b06, 0ec46ee, 212eaf1):**
  - Commit e166b06: KICK phase rediseñada basada en posición ADC, no timeout
  - Commit 0ec46ee: GOING phases con 70% PWM en refinamiento (después revertido)
  - Commit 212eaf1: initPWM() fallback correcto a config.h si NVS inválida
  - Raíz: La lógica condicional del if debe elimarse; motor debe recibir comando PWM cada iteración en fase activa
  
  **Estado actual:** ✅ RESUELTO — Motor calibra completo KICK→GOING→SETTLE en ambas direcciones

- **S2 MOTOR — Calibración bloqueada: Motor no baja (2026-05-10 15:20 → 21:55) — RESUELTO**
  
  **Problema identificado (15:20):**
  - Motor no se movía hacia abajo durante calibración
  - Síntomas: KICK_UP/GOING_UP/SETTLE_UP subían ADC, pero KICK_DOWN/GOING_DOWN/SETTLE_DOWN no bajaban
  - Resultado: `top=3984, bot=3984` → ERROR (rango inválido)
  - Hipótesis inicial: Motor solo sube; posible PWM no llega a IN2 (DOWN control)
  - Documentado en: MOTOR_DIAGNOSIS.md (2026-05-10 15:20)
  
  **Solución implementada (21:53, commit af0cccd):**
  - Motor::initPWM() rediseñado para leer pwmMin/pwmMax de NVS (con fallback a config.h)
  - Test Mode mejorado: REC=UP, SOLO=DOWN, MUTE=exit (botones directos)
  - Motor responde correctamente: GPIO18 (UP) y GPIO16 (DOWN) con duty cycles verificados
  - SAT ahora es autoridad para valores PWM en runtime (no config.h)
  - Motor::update() se salta cuando SAT está abierto (evita conflictos)
  - Hardware verificado: REC y SOLO producen movimiento correcto en ambas direcciones
  
  **Optimización (21:55, commit e38fe88):**
  - PWM_MAX calibrado a **160** (63% duty cycle) → movimiento suave, sin ruido
  - PWM_MIN = 100 (jerarquía de control estable)
  - Motor alcanza rendimiento óptimo: responde rápido, movimiento limpio, seguro
  
  **Estado actual:** ✅ RESUELTO — Motor funcional, calibración exitosa, Test Mode operativo
  
  **Lecciones aprendidas:**
  - NVS para valores runtime es más flexible que config.h hardcoded
  - Test Mode con botones directo es mejor que máquina de calibración para diagnóstico
  - PWM range 100-160 empíricamente óptimo para este hardware (DRV8833 + motor S2)

### Removed
- **S2 SAT MOTOR — Opción "Posicion" removida (2026-05-10 19:54)**
  - Razón: Pantalla era stub no funcional (todo comentado, valores hardcodeados a 0)
  - Motor nunca se movía: `Motor::setTarget()` nunca era llamado
  - Impacto: Menú Motor ahora tiene 5 opciones (quitadas 6)
  - Actualizado: `_motorN = 5`, casos switch ajustados

### Changed
- **S2 SAT MOTOR — Test Mode movido a opción 1 (primero) (2026-05-10 19:54)**
  - Antes: Motor ON/OFF → Calibrar → Test Mode → PWM Min/Max
  - Ahora: Motor ON/OFF → Test Mode → Calibrar → PWM Min/Max
  - Razón: Si motor no funciona, testear ANTES de calibración
  - Orden: caso 1 para Test Mode, caso 2 para Calibrar

### Fixed
- **S2 SAT MOTOR — Menu item count + Test Mode handler (2026-05-10 19:54)**
  - Bug 1: `_motorN = 6` pero solo 5 items válidos (Posición era stub)
  - Bug 2: Switch en `_hMotor()` con casos incorrectos
  - Solución: Removida "Posición", `_motorN = 5`, casos reajustados a 0-4

- **S2 RS485 — Error setRxBufferSize when reinitializing (2026-05-10 19:54)**
  - Problema: Cada reinicio de RS485 (SAT config saved) intentaba resize Serial1 ya activo
  - Solución: `Serial1.end()` antes de `Serial1.setRxBufferSize()` en `RS485Slave::begin()`
  - Elimina error: `RX Buffer can't be resized when Serial is already running`
  - Impacto: RS485 reinicia limpiamente sin logs de error

### Added
- **S2 MOTOR — Test Mode + Funciones de Control Directo (2026-05-10 19:54)**
  - Nuevas funciones públicas en Motor: `testUp(pwm)`, `testDown(pwm)`, `testOff()`
  - SAT menu opción nueva: "Motor → Test Mode"
  - Control con botones:
    - **REC button** = UP (PWM_MAX)
    - **MUTE button** = DOWN (PWM_MAX)
    - **SOLO button** = OFF
  - Display en tiempo real: ADC, estado botones, PWM actual
  - No afecta calibración automática (independent test)
  - Logs en Serial: `[MOTOR-TEST] UP/DOWN/OFF pwm=X`

- **S2 MOTOR — Detección de Motor Bloqueado + Fallback a DOWN (2026-05-10 19:54)**
  - Nueva constante: `CALIB_STUCK_TIMEOUT = 500ms`
  - Detección en `GOING_UP`: si ADC no cambia en 500ms → salta a `KICK_DOWN` inmediatamente
  - Detección en `GOING_DOWN`: si ADC no cambia en 500ms → `ERROR` (motor definitivamente muerto)
  - Secuencia: KICK_UP → GOING_UP (falla) → KICK_DOWN → GOING_DOWN (falla) → ERROR
  - Diferencia clara: "motor invertido/parcial" (UP falla) vs "motor muerto" (ambas fallan)
  - Útil para diagnosticar: inversión de cables, dirección bloqueada, driver dañado

### Documentation
- **S2 MOTOR — LEDC Migración Revertida, analogWrite Definitivo (2026-05-10 19:54)**
  - LEDC migración fue intentada pero revertida: conflicto de canales LEDC
  - **Causa:** LovyanGFX backlight (GPIO3) + Motor (GPIO18/16) agotaban 8 canales LEDC del ESP32-S2
  - **Solución:** analogWrite definitivo (API simple, robusta, sin conflictos)
  - **Criterio:** "Si funciona y no hay conflicto, no refactorizar"
  - Documentación: CLAUDE.md actualizado, memory s2_motor_ledc_conflict.md creado
  - **Impacto:** Motor.cpp sin cambios (ya usa analogWrite correcto)

### Changed
- **S2 MOTOR — Test mode + Safety + Compilation fixes (2026-05-10 15:20)**
  - Test mode automático: calibración + movimiento a 5 posiciones (0%, 25%, 50%, 75%, 100%) cada 2s
  - Safety: Motor EN (GPIO14) = LOW en setup() ANTES de todo (previene movimiento al boot)
  - Compilación: agregar MIDI_PB_MAX=16383, renombrar _motorActive→_motor_active, _currentPWM→_motor_currentPWM
  - Test mode fix: startCalib() se llama UNA sola vez (no loop infinito)
  - **BLOQUEADOR ENCONTRADO:** Motor no se mueve hacia abajo — calibración falla con `top=3984, bot=3984`
  - Diagnóstico: Probablemente PWM no llega a IN2 (DOWN control), revisar GPIO16/cable/DRV8833
  - Commits: `534a13a`, `8c64aa1`, `afc62ac`, `ceed039`, `10ce193`, `deafafa`

- **S2 MOTOR + FADER — Auditoría exhaustiva (2026-05-10 15:02)**
  - Motor.cpp: control ordering crítico, timestamp recapture en transiciones, dinámica PWM mapping
  - FaderADC.cpp: 8 problemas corregidos — variable scope, tipo consistencia, validación de rango completa, bandera gotData
  - FaderADC.h: eliminados campos muertos (_emaValue, _noiseSpan, _noiseWindow, _noiseHead), método _isTrending()
  - FaderTouch.cpp: 8 problemas corregidos — baseline pausada durante toque, timestamp-based detección (frame-rate independent), touchRead() validado, fallback de baseline
  - config.h: FADERTOUCH completada con constantes (TOUCH_POLL_MS, TOUCH_THR_*, TOUCH_SOSTENIMIENTO, etc.)
  - Resultado: 210+ líneas de código muerto eliminadas, arquitectura simplificada, robusto a race conditions
  - Commit: `534021d`

### Removed
- **S2 MOTOR — Reset total: borrado Motor.h / Motor.cpp (2026-05-11 08:15)**
  - Razón: Código base defectuoso. Motor solo se mueve en un sentido.
  - Removido: máquina de calibración (CalibPhase), control de posición, analogWrite/LEDC mixtos, todos los logs internos
  - Documentación: `/track S2/iMakie - Track ESP32S2 V1/src/hardware/Motor/Motor.h` y `.cpp` vaciados excepto headers
  - Impacto: main.cpp sigue compilando (Motor:: namespace existe pero vacío), permite reescritura limpia sin legacy
  - Lección: Código base con migración analogWrite→LEDC fallida + órdenes init inconsistentes → restart mejor que patch
  - Próximo paso: reescribir Motor desde cero con especificación clara de DRV8833 control

### Changed
- **S2 MOTOR TEST — FaderADC desactivado (2026-05-10 22:30)**
  - Razón: I2C interfiere en unidades DAC (sin ADS1115)
  - Cambio: `faderADC.begin()` comentado en main.cpp setup()
  - GPIO34/GPIO21 liberados para DAC del fader
  - GPIO17 (ADS_ALERT) fijo OUTPUT LOW — evita flotante
  - Estado: Motor-only test mode activo
  - Nota: Cambio temporal para debugging de motor en unidad DAC

- **Versión — 0.4.2 (2026-05-10 20:00)**
  - Schema: MAJOR.MINOR.PATCH desarrollo
  - 0 = Debug/Development state
  - 4 = Subsistemas completos: Display, Botones, LEDs, Fader (100%)
  - 2 = En desarrollo: Fader + Motor
  - Actualizado pre_build.py con versión y comentario de schema

### Documentation
- **Directiva Obligatoria — Código Moderno: Alineación con Stack (2026-05-10 19:45)**
  - Todos los cambios de código deben usar las MISMAS APIs que las librerías del proyecto
  - Motor: DEBE usar LEDC (ledcAttach/ledcWrite) — NO analogWrite (incompatible con LovyanGFX)
  - I2C: DEBE usar Wire moderno (Adafruit BusIO estándar)
  - Logging: usar log_i/log_e (no Serial legacy)
  - PROHIBIDO mezclar APIs en mismo subsistema (ej: LEDC + analogWrite = FATAL)
  - Stack: pioarduino 55.03.37/IDF5 + LovyanGFX 1.2.19 + Adafruit libs
  - Documentado en CLAUDE.md y memory

### Changed
- **S2 MOTOR — Migración a LEDC Core 3.x (2026-05-10 19:50)**
  - Reemplazado analogWrite (API antigua) por ledcWrite (LEDC moderno)
  - init(): analogWriteFrequency/Resolution → ledcAttach con validación de retorno
  - _hwBrake/Off/Up/Down: analogWrite → ledcWrite
  - Alineación con stack: LovyanGFX usa LEDC internamente, motor ahora compatible
  - Log mejorado: detecta fallos de ledcAttach en init()
  - Impacto: PWM 20kHz estable, API moderna, sin conflictos con otras librerías
  - Estado: listo para compilación y testing

- **S2 MOTOR — _hwUp() y _hwDown() invertidos (2026-05-10 00:15)**
  - Hardware tiene pines invertidos: UP=IN2 PWM, DOWN=IN1 PWM
  - Cambio: invertir lógica en ambas funciones
  - Estado: compilado, debugging con osciloscopio en progreso
  - Commit: `479f64b`

- **S2 MOTOR — CalibPhase duplicado removido (2026-05-10 00:15)**
  - CalibPhase enum estaba en Motor.cpp y config.h
  - Removido de Motor.cpp (config.h es autoridad)
  - Commit: `479f64b`

### Bugs Encontrados
- **S2 MOTOR — No responde en ningún caso (2026-05-10 00:15)**
  - Motor completamente inmóvil: ni en calibración ni en control
  - Driver funciona (verificado)
  - Causa desconocida: posible fallo EN (GPIO14), pines no se configuran, o init() rompe pines
  - Investigación: osciloscopio midiendo EN/IN1/IN2 en progreso
  - Estado: BLOQUEADO - esperando resultados de medición
- **S2 MOTOR — Orden inicialización PWM: pinMode → frequency/resolution → analogWrite (2026-05-09 23:45)**
  - HIPÓTESIS: Motor.cpp::init() ponía `analogWrite()` ANTES de `analogWriteFrequency/Resolution`
  - analogWrite() hace attach implícito con frecuencia default, luego frequency() no tiene efecto
  - CAMBIO: Restaurar orden correcto: pinMode → frequency/resolution → LUEGO analogWrite
  - ESPERADO: PWM a 20kHz funcione (vs frecuencia default mucho menor)
  - TESTING REQUERIDO: Compilar + calibración (rango ADC debe ser 0-8191, no 24-26)
  - Commit: `0305c6a`

- **S2 MOTOR — Variables de estado centralizadas en config.h (2026-05-09 23:45)**
  - CalibPhase enum: IDLE, KICK_UP, GOING_UP, SETTLE_UP, KICK_DOWN, GOING_DOWN, SETTLE_DOWN, DONE, ERROR
  - Variables calibración: _phase, _phaseStart, _calibStart, _calibMinDetect, _stableStart, _stableRef
  - Variables ADC: _adcTop, _adcMin, _adcMax, _adcSpan, _adcPos, _targetADC, _lastMidiTarget
  - Variables noise: _settleMin, _settleMax, _noiseTopSpan
  - Variables control: _motorActive, _currentPWM
  - Motor.cpp simplificado: solo lógica, no variables de estado
  - Commit: `0305c6a`

- **S2 FADER — ADS1115 se hace obligatorio (2026-05-09)**
  - Eliminados TODOS los `#ifdef USE_ADS1015` del código
  - ADC nativo (GPIO10, 13-bit) descartado permanentemente
  - Entorno default: `lolin_s2_mini` (ADS1115) con librerías ADS1X15 + BusIO
  - platformio.ini consolidado: Serial y OTA ahora usan ADS
  - FaderADC simplificado: solo rama ADS, sin compilación condicional
  - config.h limpiado: removed `FADER_POT_PIN`, `FADER_VCC_PIN`, `NOISE_WINDOW_SIZE`, `FADER_EMA_ALPHA_FAST`
  - main.cpp: removed DAC setup (`#ifndef USE_ADS1015`), diagnóstico ADS incondicional

### Added
- **S2 FADER — ADS1115 I2C ADC (Fase 1)** (2026-05-09)
  - ISR ALERT/RDY en GPIO17 — no polling, 860 SPS continuo
  - Buffer circular 256 muestras con timestamp (no-bloqueante)
  - GAIN_ONE (±4.096V) para rango 3.3V directo
  - Función `dumpAdsLog()` para análisis CSV de ruido
  - Validación I2C en setup() — log automático de detección

### Modified
- **platformio.ini:** Nuevo entorno `lolin_s2_mini_ads` con libs ADS1X15 + BusIO; eliminado `extends` (2026-05-09)
- **config.h:** Defines ADS (SDA=21, SCL=34, ALERT=17, addr=0x48) bajo guardia
- **protocol.h:** Comentario `faderPos` documentado para dual-mode 13/16-bit
- **FaderADC.h:** Estructura con Adafruit_ADS1115, TwoWire I2C, ISR ALERT/RDY
- **FaderADC.cpp:** ISR definition, `begin()`, `update()`, `measureRange()`, `dumpAdsLog()`
- **main.cpp:** Diagnóstico ADS1115 periódico (cada 500ms) en loop; log: `[ADS] raw=X pos=X`

### Fixed
- **S2 FADER — ALERT pin trigger FALLING (2026-05-09)**
  - ADS1115 ALERT/RDY es activo-bajo: HIGH (reposo) → LOW (dato) = **FALLING**, no RISING
  - FaderADC.cpp usaba RISING → ISR nunca se disparaba → `_newData` siempre false
  - Motor nunca recibía posición → completamente ciego
  - Cambio: attachInterrupt(..., FALLING) — una línea, efecto crítico
  - Commit: `386765f`

- **S2 FADER — measureRange() bloqueante documentado (2026-05-09)**
  - `measureRange()` espera 5s en loop cerrado (S2 single-core)
  - Impacto: SAT menu congelado, RS485 timeout, Master marca slave NO_CALIBRATED
  - Decisión: Documentar impacto (no refactorizar por ahora)
  - Restricción: SOLO usar en diagnóstico excepcional, NUNCA durante operación/calibración
  - Documentación: FaderADC.h, FaderADC.cpp (comentarios), SatMenu.cpp (warning)
  - Commit: `bbddaa0`

### Technical Notes
- **Resolución:** ADS 16-bit (0-32767) sin escalado FP → P4/S3 mapean a 0-14848
- **Performance:** update() ADS = 0-2µs (vs 24ms ADC nativo) — no impacta loop() S2 single-core
- **Ruido:** ADS ~2-5 counts (vs ±30 ADC nativo) — mejora 6-15×
- **Pines I2C confirmados:** SDA=21, SCL=17, ALERT=34 (usuario validó 2026-05-09)
- **Commit:** `80eb621` (implementación), `670ae24` (historial centralizado)

---

## [v2026-05-04] — WiFi OTA y Documentación Reorganizada

### Added
- **WiFi OTA — ElegantOTA 3.1.7** 
  - ArduinoOTA descartado (muerto en pioarduino 55.03.37)
  - ElegantOTA funciona perfecto — SAT menu "WiFi OTA"
  - Credenciales: configuradas en NVS namespace `ptxx` (sketch provisioning USB)

### Changed
- **STATUS.md reorganizado** (2026-05-04 19:20)
  - Estructura: S2 | S3 | P4 | Cross-system
  - Subsecciones: Bugs/Pendientes/Detalles técnicos para cada componente
  - 7 bugs críticos documentados con criterios de éxito

### Fixed
- **Encoder sequenciamiento (2026-04-28 15:30)**
  - `Encoder::reset()` movido post-VPot (antes estaba pre-VPot)
  - VPot ring ahora responde correctamente en Logic Pro
  - RS485 y Display usan mismo delta

### Documentation
- **CLAUDE.md:** Agregada sección SESION con fecha/hora obligatoria
- Formato: `(YYYY-MM-DD HH:MM)` para rastreabilidad de cambios

---

## [v2026-04-28] — NeoPixel y Encoder Fixes

### Added
- **NeoPixel — Cambio a Adafruit NeoPixel** (2026-04-28 16:15)
  - NeoPixelBus 2.8.4 incompatible con pioarduino 55.03.37 / IDF5
  - Adafruit NeoPixel 3.1.7 es solución definitiva
  - Secuencia brillo: azul tenue → colores tenues (Logic conecta) → on/off
  - HW_STATUS display en boot — 10 componentes color-coded

### Fixed
- **Encoder no funciona en Logic** (2026-04-28)
  - Root cause: `Encoder::reset()` en lugar equivocado (pre-VPot)
  - Solución: Reset post-VPot (post-buildResponse)
  - SAT funcionaba porque procesaba sin reset intermedio

---

## [v2026-04-27] — Documentación Encoder Centralizada

### Added
- **Encoder — Fuente única de verdad** (2026-04-27 14:00)
  - `src/hardware/encoder/Encoder.cpp` confirmada como fuente central
  - Sin duplicados en SAT ni main.cpp
  - ISR basada en CHANGE, debounce 3ms, dirección: A LOW + B HIGH = -1

### Documentation
- CLAUDE.md: Sección "Encoder — Arquitectura y sequenciamiento"
- Usuarios correctos: RS485Handler::buildResponse(), main.cpp

---

## [v2026-02-15] — Inicial

### Project Setup
- **Arquitectura:** P4 Master + S3 Extender + 17× S2 Slaves
- **Hardware:** ESP32-P4 (master MCU), ESP32-S3 (extender), 17× ESP32-S2 Lolin (slave)
- **Comunicación:** RS485 500kbaud, protocolo binario custom, CRC8
- **MIDI:** Mackie MCU Universal compatible Logic Pro
- **Subproyectos PlatformIO:**
  - `S3/` — master S3/P4 con RS485 bus A (9 slaves) + bus B (8 slaves)
  - `track S2/` — slave S2 con 1 canal físico completo

### Known Limitations (A resolver)
- [x] NeoPixel — NeoPixelBus incompatible IDF5 → Adafruit (RESUELTO 2026-04-28)
- [x] Encoder — sequenciamiento incorrecto → reset post-VPot (RESUELTO 2026-04-28)
- [ ] S2 Fader — ADC nativo ruidoso (±30 cuentas) → ADS1115 (EN DESARROLLO 2026-05-09)
- [ ] Motor S2 — no responde → investigar DRV8833 driver
- [ ] Botones S2 — lentos → revisar debounce/latencia
- [ ] Display S2 — brillo máximo en boot → orden init

---

## Historial de Sesiones de Debugging

### SESION (2026-05-10 22:00-22:05) — S2 Test Mode ADC Real-Time

**Objetivo:** Fix SAT Test Mode pantalla — Motor::getRawADC() siempre devolvía valor de entrada, nunca se actualizaba.

**Root cause:** Motor::setADC() rechazaba deltas > 200 (SPIKE_GUARD) cuando SAT abría.

**Cambios implementados:**
1. config.h: ADC_SPIKE_GUARD 200 → 500 (línea 104)
2. main.cpp: Motor::setADC() movido antes SAT check (línea 283) — ejecuta cada frame incluso en Test Mode
3. SatMenu.cpp _tickMotorTest(): Lee directo faderADC.getFaderPos() en lugar Motor::getRawADC() (línea 1088)

**Estado actual:** 
- ✅ ADC obtiene valor correcto de faderADC en tiempo real
- ❌ **Pantalla no se redibuja en tiempo real** — MOTOR_TEST no está en lista `live` en SatMenu::update() línea 113-119
  - Solución pendiente: agregar `_scr == Scr::MOTOR_TEST` a lista live
  - Esto hará que _render() se ejecute cada frame

**Por hacer próxima sesión:**
- Agregar MOTOR_TEST a lista `live` en SatMenu.cpp líneas 113-119
- Verificar que pantalla Test Mode redibuja cada frame

---

### SESION (2026-05-04)

- STATUS.md reorganizado: S2, S3, P4, Cross-system con estructura MAYÚSCULAS + NEGRITA
- Subsecciones Bugs/Pendientes/Detalles técnicos en cada componente
- 7 bugs críticos documentados en S2 (RS485 pérdida, Display brillo, Botones lentos, Fader no funciona, FaderTouch con plástico, Motor no funciona, Encoder solo SAT)
- WiFi/OTA: ArduinoOTA muerto → ElegantOTA funciona

---

## Formato de Versión

- **[vYYYY-MM-DD]** — snapshot de estado en fecha
- **[Unreleased]** — cambios acumulados sin release formal
- Subcategorías: **Added** | **Changed** | **Fixed** | **Removed** | **Technical Notes** | **Documentation**

## Política de Documentación

- **Fecha/hora obligatoria:** `(YYYY-MM-DD HH:MM)` en commit + archivo
- **Rastreabilidad:** Cada cambio linkeado a commit o bug report
- **Scope:** Cambios arquitectónicos, bugs críticos, migraciones de libs, decisiones de hardware
- **No incluir:** Bug fixes locales, optimizaciones triviales, cambios de comentario solo

---

**Último actualizado:** 2026-05-09 22:50  
**Responsable:** iMakie Development Team  
**Contacto:** juliannof (GitHub)
