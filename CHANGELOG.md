# CHANGELOG — iMakie

Registro histórico de cambios significativos del proyecto iMakie.  
Formato: [Keep a Changelog](https://keepachangelog.com/)

---

## [Unreleased]

### Pendientes

| Prioridad | Tarea | Notas |
|-----------|-------|-------|
| 🟡 Media | **S2: `POSITION_FINE_PWM_K=0.5` implementado, pendiente afinar en banco (2026-08-13 15:10)** | Suelo de PWM del frenado fino ya separado del `pwm_min` de arranque (ver sesión 2026-08-13 15:10 abajo). Validación parcial: calibración OK, faders llegan "más o menos a su sitio" — falta iterar `K` en banco hasta que el frenado se sienta fino sin perder fuerza cerca de los extremos. |
| 🟡 Media | **S2: jitter anti-cascada en `setTargetFromS3`/`setTargetForced` implementado, sin validar en banco (2026-08-13 15:10)** | `MOVING_JITTER_MAX_MS=150` + helper `_readyToMove()` aplicado (mismo patrón que `GOTOMIN_JITTER_MAX_MS`). Pendiente cargar un proyecto real y confirmar que baja/desaparece el `[RS485] ID MISMATCH` (era aleatorio entre slaves, no un slave fijo). Ver sesión 2026-08-13 15:10 abajo, punto 10. |
| 🔴 Alta | **P4: pinout RS485 bus A localizado (A=GPIO26, B=GPIO27) pero SIN TESTEAR, config.h sin actualizar (2026-07-30)** | El usuario conectó el rig completo de 9×S2 al bus A del P4 y localizó el pinout real: A=GPIO26, B=GPIO27 (confirmado que no coincide con `LCD_RST_PIN`, coincidencia numérica era del conector, no del GPIO real). Transceiver **auto-direccional** — sin pin DE/RE, a diferencia del bus B (S3). Pendiente: (1) testear un ciclo TX/RX real con un S2 antes de dar el pinout por bueno; (2) portar a `P4/config.h` (sigue con `TX=52/RX=51/EN=50` heredados de la placa antigua); (3) adaptar `P4/RS485.cpp` para no usar `RS485_ENABLE_PIN` (4 puntos: `begin()`, `_sendPacket()` ×2) — bus B en S3 no se toca, sigue necesitando su enable. No subir `NUM_SLAVES` (actualmente `0`) ni activar `rs485.startTask()` hasta cerrar esto. Detalle: `docs/RS485_P4.md` §1.2. |
| 🟡 Media | **Fader S2: convergencia a target lejano validada parcialmente (V1/V2), V3-V8 pendientes (2026-07-22)** | Arquitectura movida a "S2 dueño de calibración" (ver sesión 2026-07-22 abajo) resolvió los STALL de la sesión 2026-07-20 desde otro ángulo, más 3 bugs nuevos encontrados y corregidos en banco: `requestCalibration()` perdía `_pendingCalib`, asimetría de PWM en `GOING_DOWN` (fondo real no alcanzado), y bucle STALL en `_positionTick()` con target lejano (nueva constante `POSITION_CRUISE_ERR`). V1/V2 (boot, autocalibración, aplicar último PB) confirmados en banco. Pendiente: V3-V8 (READ con push manual, WRITE, LATCH, desconexión, 10× recalibración). |
| 🔴 Alta | **S2: fader sube a máximo y no se detiene tras calibración correcta, con Logic abierto — DIAGNÓSTICO SIN CONFIRMAR (2026-07-24 20:16)** | Reportado por el usuario. Hipótesis principal (análisis estático, ver sesión 2026-07-24 abajo): bucle infinito empuje→STALL→cooldown(2s)→reintento en modo DAW-absoluto (OFF/READ) cuando el target de Logic está en/cerca del máximo y el margen de calibración (`marginTop=20`, `Motor.cpp`) resulta insuficiente frente a la fricción real cerca del tope físico — `setTargetForced()` no tiene límite de reintentos (a diferencia de `requestCalibration()`). Pendiente confirmar con log serie (`[MOTOR] STALL...` / `cooldown STALL activo` repitiéndose) y el AutoMode activo en el momento del fallo. Hallazgo secundario (bug real, probablemente inerte): `RS485Handler.cpp:186,378` llaman `Motor::setTarget(Motor::getRawADC())` pasando ADC crudo a una función que espera PitchBend 0-16383 — corrompe `_motor_lastMidiTarget`, consumido solo en el `map()` sin clamp de `Motor.cpp:274-275`. Probablemente la misma familia de fallo que la fila siguiente (motor empujando de forma sostenida, quemando motores) — revisar juntas. |
| 🔴 **CRÍTICA** | **S2: IMPOSIBLE que el motor empuje el fader hacia arriba de forma sostenida por encima del valor de calibración durante más de 1 segundo — SIN IMPLEMENTAR, quema motores en banco (2026-08-02 17:05)** | Requisito explícito del usuario, en banco con hardware real: hay que garantizar por firmware que, en **cualquier circunstancia excepto `KICK_UP`/`GOING_UP` de la calibración de arranque** (`CalibPhase`), el motor S2 NO pueda mantener `_hwUp()` activo de forma continua más de 1000ms — especialmente relevante cuando la posición ya está en/cerca de `_calibratedFaderMax`. El guard `STALL_PROTECT_MS` (400ms, `Motor.cpp:439-461`) **no cubre este caso**: solo corta si el ADC no cambia en absoluto; con jitter eléctrico real (ruido en el ADC, ver sesión 2026-08-02 17:05 abajo) el timer se resetea indefinidamente aunque el fader no progrese hacia el target, dejando el motor empujando sin límite de tiempo total. Implementar: nuevo guard de tiempo continuo en dirección "up" (independiente de si hay movimiento ADC o no), variables/constantes nuevas en `S2/config.h` (fuente única de verdad), lógica en `Motor::update()` junto al bloque STALL_PROTECT existente. Validar en banco: empujar/soltar el fader repetidamente con Logic en modo READ y confirmar que el motor se corta al segundo exacto y no antes en movimientos largos legítimos. Contexto completo de la investigación de esta sesión (handshake MCU en bucle, degradación del bus RS485, touch fantasma en slaves 1/2/7): ver sesión 2026-08-02 17:05 abajo. |
| 🔴 Alta | **P4: Logic no aplica el V-Pot relativo del pop-up (2026-06-12 19:47)** | El pop-up envía CC relativo (`CC 16+strip`, bit6=dirección) al arrastrar el arco grande — el MIDI **SALE** (visto en monitor) pero Logic **NO mueve el pan**. Análogo al evento del S2 con los AutoModes (notas 74-78 Read/Write/Trim/Touch/Latch): Logic solo los aplica con un track **seleccionado** (`g_selectedChannel>=0`, `MIDIProcessor.cpp` ~l.611). Hipótesis: falta contexto (track seleccionado / assignment Pan / banco activo) o canal/controller distinto. Probar: (1) canal 0 fijo (`0xB0`, `0x10+strip`) en vez de `0xB0|strip`; (2) forzar selección previa; (3) verificar modo Pan. Ver [[midi_sale_logic_no_aplica]]. |
| 🟡 Media | **P4: pop-up V-Pot — no refrescar el fondo con el modal abierto (2026-06-12 19:47)** | Fix listo, SIN aplicar: en `main.cpp` loop CONNECTED, si `uiVPotPopupIsOpen()` → solo `uiVPotPopupUpdate()`, saltar `uiHeaderUpdate`/página/`handleVUMeterDecay`. El overlay tapa toda la pantalla (1024×600), no hace falta repintar detrás. |
| 🟡 Media | **P4: pop-up V-Pot — botón "Cerrar" demasiado grande (2026-06-12 19:47)** | Reducir el `lv_button` (actual 160×56) en `UIVPotPopup.cpp::uiVPotPopupOpen()`. |
| 🟡 Media | **P4: portar fixes de calibración de S3 cuando RS485 bus A esté activo (2026-06-14)** | Dos fixes aplicados en S3 deben replicarse en P4 cuando implemente calibración de sus 9 slaves (bus A): (1) `_handleResponse()`: rechazar `CALIB_DONE` con `MIN=0 MAX=0` e incrementar `calibRetries` — igual que `RS485.cpp:311` de S3. (2) Si P4 usa `buildResponse()` propio en S2 por bus A: verificar que `pendingNewCalib` guard está presente. Commits de referencia: `fd50ed0` (S2 fix) + `432e2c5` (S3 fix). |
| 🔴 Alta | **ExPressif: JV-2080 no cambia de modo Patch↔Performance pese a SysEx correcto (2026-07-02 18:33)** | El toggle (ahora tab "SON" independiente, ver sesión 2026-07-04) manda `sendSoundMode()` (SysEx DT1 "Sound Mode", `MIDIOut.cpp`), pero **el JV-2080 físico no cambia de modo**. Contenido del SysEx verificado byte a byte contra fuente externa (coincide con `F0 41 10 6A 12 00 00 00 00 <00/01> <00/7F> F7`, ver nota metodológica abajo — pendiente re-verificar contra `JV-2080_OM.pdf` p.187-188 directamente, no solo por comentario previo). Device ID (`JV2080_DEVICE_ID=0x10`) probado, no era la causa. Bank Select (CC0/CC32) + Program Change SÍ funcionan, solo el SysEx de Sound Mode falla. Hipótesis pendientes: (1) checksum/dirección real de "Sound Mode" puede no coincidir con lo documentado si la fuente previa era incorrecta — releer el manual oficial línea por línea; (2) framing USB-MIDI del SysEx (`sendSysEx12()`, 4 paquetes de 3 bytes) sin confirmar con MIDI Monitor en el tramo final hacia el JV-2080; (3) ruta física P4→JV-2080 puede estar filtrando SysEx aunque deje pasar CC/PC — sin confirmar. **Nota metodológica:** una verificación usó WebSearch/Sweetwater en vez del manual oficial — el usuario marcó esa fuente como no válida para este proyecto (ver `feedback_fuente_verificacion_tecnica`). Próximo paso: MIDI Monitor + contrastar contra el PDF oficial. |
| 🟡 Media | **ExPressif: TG55/D110/WAVE sin descriptor de sonidos en UIBank (2026-07-04)** | Tras la integración multi-synth, `kSynthDesc[]` (`UIBank.cpp`) solo tiene datos reales para JV2080 y TRITON — los otros 3 sintetizadores del ciclo (`ExSynth`) quedan con descriptor vacío (Sonidos/Performances muestran grid vacío sin crashear, comportamiento defensivo intencional, no error). Pendiente si se necesitan: tabla de bancos + nombres + SysEx de modo para cada uno, siguiendo el patrón `TritonPatches.h/.cpp`. |
| 🟢 Baja | **ExPressif: Triton no fuerza canal MIDI por modo Program/Combination (2026-07-04)** | A diferencia del JV-2080 (Patch=12/Performance=1, `MIDI_CH_PATCH/PERFORM`), el Triton usa `chProg=chCombi=0` en su `SynthSoundDesc` ("no forzar", usa `g_midiChannel` tal cual) — decisión explícita del usuario ("esto no es relevante ahora"), no un olvido. Si más adelante hace falta separar tracks de Logic para Program/Combination del Triton, añadir constantes `TRITON_CH_PROG/COMBI` en `config.h` y pasarlas en `kSynthDesc[]`. |
| 🟢 Baja | **ExPressif: persistir página+pestaña activa de UIBank (2026-07-01, mecanismo cambiado 2026-07-04)** | Sigue sin implementar. Con el rediseño de paginación (grid 2×4, NeoTrellis-only, sin `lv_tileview`) el mecanismo sería: guardar `g_bankTab` + página activa de cada tab en NVS, y en `ensure_tab_built()` saltar a esa página en vez de forzar página 0. |
| 🟡 Media | **ExPressif: `UIKaosEdit.cpp` — geometría sin terminar de validar en hardware (2026-07-14)** | Pantalla nueva (editor de memoria Kaos). Ya corregidos en vivo: caja/tamaño del canal, y un error de ejes que apilaba canal+Guardar+Cancelar en el mismo borde físico (ver sesión 2026-07-14). Sigue pendiente confirmar en pantalla real las listas EJE X/EJE Y (posición, si el nº de filas cabe sin solapar con el título/canal). |
| 🟡 Media | **ExPressif: NeoTrellis sigue activo con `UIKaosEdit` abierto (2026-07-14)** | A diferencia de `UIBank` (que suspende HOLD/PANIC/preset/synth en el NeoTrellis mientras está abierto, `g_bankOpen`), el editor de memoria Kaos no tiene un gate equivalente — con el editor táctil abierto, las teclas físicas (preset, synth, brillo, panic) se siguen procesando por debajo y pueden cambiar `g_currentSynth`/preset activo mientras se edita otro. No corrompe datos (Guardar usa el synth/slot capturados al abrir), pero es confuso. Pendiente: mismo patrón que `g_bankOpen` para el editor, o aceptar el comportamiento si no molesta en uso real. |
| 🟡 Media | **ExPressif: TRITON Grupo B (efectos vía D-mod) y JV-2080 LFO Depth sin implementar (2026-07-14)** | Bloqueados en `aitec_kaos_brief_2026-07-13.md` §3: falta decidir si el `:Src` del D-mod se preconfigura una vez en el patch (`needs_route=false`) o Code lo reenvía por SysEx cada vez que se selecciona la memoria (`needs_route=true`, con el payload exacto). No se ha tocado el modelo NVS (`KaosSlot`) para soportar `needs_route`/`route_payload` todavía — el catálogo actual (`KaosParams.cpp`) solo cubre memorias sin bloqueo. |
| 🟡 Media | **S2: decisión arquitectónica pendiente — heurístico de touch (delta ADC) vs. recuperar sensor capacitivo (2026-08-13 15:10)** | Discusión sin resolver esta sesión. El principio "usuario es master" es correcto, pero la detección se infiere solo de deltas de ADC (sin sensor de touch fiable — `FaderTouch` capacitivo desactivado desde 2026-06-14 por no ser fiable en todo el recorrido). Ese heurístico ha sido la causa raíz de facto de la mayoría de bugs de las últimas 5 sesiones (touchState storms, faders huérfanos, ahora también correlacionado con el `ID MISMATCH` de hoy). Disyuntiva: seguir parcheando el heurístico (retornos decrecientes) vs. invertir en que el capacitivo vuelva a ser fiable (elimina la clase de bug en la raíz). Sin decisión tomada — pendiente de que el usuario elija dirección. |

---

### SESIÓN 2026-08-13 15:10 — S2: filtro ADC centralizado en FaderADC, PWM fino separado del arranque, Logic vuelve a ser fuente de verdad del AutoMode

**Origen:** continuación directa de la sesión de esta mañana (10:19). Empieza retomando el brief de PWM pendiente, se desvía a una investigación de ruido de ADC disparada por un log de MIDI Monitor (`PitchBend=4000` con el fader ya en 0), y termina con hallazgos de validación en banco tras cargar un proyecto real (tormenta de `touchState`, `RS485 ID MISMATCH`, header sin reflejar TRIM).

**MCU afectadas:** S2 ✅ (FaderADC, Motor, RS485Handler, Display, config) · S3 ✅ (MIDIProcessor, config, main — debounce nombres de pista, punto 11) · P4 ❌ — sin tocar protocolo RS485.

**Nota de corrección:** varios comentarios de código de esta sesión se escribieron inicialmente con fecha "2026-08-14" sin verificar el reloj del sistema — corregido a la hora real (`date`: 2026-08-13 15:10) en los 5 archivos afectados antes de cerrar la sesión. Recordatorio de por qué el CLAUDE.md exige verificar `date` siempre.

---

**1. S2 — `hardware/fader/FaderADC.h`/`.cpp` — filtro de ruido centralizado (único punto del sistema)**
- **Disparador:** log de MIDI Monitor mostrando `PitchBend=4000` en un canal con el fader físicamente en 0. Causa raíz: el EMA de salida en `RS485Handler.cpp` (`_rsFaderPosEMA`, alpha=0.15) solo se reseteaba en touch — en cualquier llegada automática a destino (target de Logic, `goToMin()`) arrastraba varios ciclos RS485 de valores intermedios "viejos" antes de converger al real.
- **Pregunta de fondo:** ¿hacen falta tantos filtros repartidos con el ADS1115? Sí hace falta *algún* filtro — el ruido real medido en banco (~28 cuentas) coincide con lo que predice el datasheet del ADS1115 a 860 SPS (~11 bits libres de ruido de los 16 nominales, a `GAIN_ONE`) — pero estaba repartido en 3 sitios distintos (umbral de touch, spike guard de Motor, EMA de salida de RS485) peleando cada uno por su cuenta con el mismo ruido crudo.
- **Fix:** `FaderADC::update()` aplica ahora un EMA sobre la lectura cruda del ADS1115 (`FADER_EMA_ALPHA_FAST=0.20`, constante ya existente en `config.h` pero sin uso hasta hoy). Sembrado en la primera lectura real (`_faderPosFiltered=-1` como centinela) para no arrastrar desde 0 — mismo bug que se acababa de diagnosticar en el EMA de salida, no se repite aquí. `getFaderPos()` mantiene la misma firma; `_rawLast` sigue exponiendo el crudo sin filtrar para diagnóstico (`dumpAdsLog`).

**2. S2 — `hardware/Motor/Motor.cpp` — eliminados los dos spike guards (`ADC_SPIKE_GUARD`)**
- `setADC()` (antes ~L608) y `setADCDelta()` (antes ~L673): ambos rechazaban/ignoraban una lectura si saltaba >500 cuentas de golpe — mecanismo redundante ahora que el ruido se absorbe en origen, y que arriesgaba el mismo patrón de "freeze" ya identificado como problemático el 2026-07-20 (rechazar en vez de saturar fabrica topes falsos en calibración).
- Validado en banco tras flashear: **todos los faders calibran correctamente.**

**3. S2 — `RS485/RS485Handler.cpp` — eliminado el EMA de salida (`_rsFaderPosEMA`)**
- `buildResponse()`: `resp.faderPos` pasa a ser el `pb` mapeado directo (sin segunda capa de suavizado). Elimina la cola de decay que causaba el `PitchBend=4000` con fader en 0 del punto 1.

**4. S2 — `config.h` — limpieza de constantes muertas**
- Eliminadas: `NOISE_WINDOW_SIZE`, `NOISE_K_MOVE`, `NOISE_K_MICRO` (declaradas previamente, nunca consumidas por ningún `.cpp` — confirmado por grep completo del repo), `ADC_SPIKE_GUARD` y `FADER_EMA_ALPHA` (sin consumidores tras los puntos 2-3). `FADER_EMA_ALPHA_FAST` pasa de sin uso a ser la única constante de filtrado del sistema.

**5. S2 — `hardware/Motor/Motor.cpp` + `config.h` — retomado el brief de PWM pendiente: suelo de frenado fino separado del PWM de arranque**
- Nueva `POSITION_FINE_PWM_K=0.5f` (`config.h`). En `_positionTick()`, la rampa cuadrática de la zona de frenado (`absErr < POSITION_CRUISE_ERR`) ya no usa `_pwm_min` como suelo — usa `pwmFineFloor = _pwm_min - K×(_pwm_max-_pwm_min)`, derivado siempre de los `pwm_min`/`pwm_max` ya calibrados por unidad en NVS, sin nuevo valor persistido ni pantalla SAT. El crucero (`absErr > POSITION_CRUISE_ERR` → `_pwm_max`) y toda la calibración (`KICK_UP/DOWN`, `GOING_UP/DOWN`) quedan intactos.
- **Validación parcial en banco:** faders llegan "más o menos a su sitio" — confirma que el mecanismo funciona, pero `K=0.5` es un punto de partida sin afinar (ver fila Pendientes 🟡 Media arriba).

**6. S2 — `RS485/RS485Handler.cpp` — eliminado el guard TRIM→READ (revierte decisión de la sesión 2026-07-30, nunca validada en hardware)**
- **Disparador:** validación en banco tras cargar un proyecto real — el header no reflejaba el cambio a TRIM. Decodificando el log de S3 (`[RS485-TX] slave=2 AUTOMODE 0 → 3 flags=0x69`): el canal pasó de OFF a TRIM, pero el guard (si `_rsCurrentMode==READ` y llega TRIM, se reescribe a READ) lo enmascaraba cuando aplicaba.
- El usuario, tras ver el comportamiento real, corrigió la decisión: **Logic es la única fuente de verdad** — mismo principio ya vigente para botones (CLAUDE.md), ahora extendido explícitamente a AutoMode. Guard eliminado en `onMasterData()` (antes ~L250). El enrutado de TRIM ya existía y funcionaba (tratado igual que TOUCH en `_applyFaderTarget()`) — quitar el guard no añade lógica nueva, solo deja de esconder un modo real.
- Resuelve la fila 🔴 Alta de Pendientes de la sesión 2026-07-30 (guard sin validar).

**7. S2 — `config.h` + `display/Display.cpp` — `AUTO_TRIM` tenía color aliasado a `AUTO_OFF` ("reservado")**
- Efecto colateral del punto 6: aunque TRIM ya llegara sin enmascarar, el header no cambiaba de color porque `AUTO_COLORS[AUTO_TRIM]` apuntaba al mismo gris que `AUTO_OFF` en dos tablas (`drawHeaderSprite()` y `autoModeColor()`, esta última alimenta también el VPot ring).
- Nueva `TFT_AUTO_TRIM=0x07FF` (cian) — color propio, distinto de los 5 ya usados. Ambas tablas actualizadas.

**8. Hallazgo — motores arrancan movimiento simultáneo al cargar proyecto**
- Validando el guard TRIM en banco, apareció una tormenta de `touchState=1` en casi todos los slaves a la vez (valores erráticos, saltos de miles de cuentas) junto con `[RS485] ID MISMATCH esperado=8 recibido=1` / `esperado=3 recibido=7` — coincidiendo con la carga de un proyecto (confirmado por el usuario).
- **Diagnóstico:** `setTargetFromS3()`/`setTargetForced()` (las rutas que aplican el target cuando llega un `PitchBend` de Logic) no tienen ningún jitter anti-cascada, a diferencia de `goToMin()` (que sí lo tiene desde la sesión de esta mañana, `GOTOMIN_JITTER_MAX_MS`). Al cargar un proyecto, Logic manda targets a los 8 canales casi a la vez → todos los motores arrancan a moverse en el mismo instante → mismo mecanismo de pico de corriente sincronizado que ya causaba cascadas en `goToMin()`, por una ruta que la sesión de esta mañana no cubrió. Ver fix en el punto 10.
- Los faders, pese a la tormenta de touch en el camino, acabaron llegando a su posición (motor internamente completa el viaje) — el usuario lo confirmó ("ahora los faders van más o menos a su sitio").

**9. Discusión sin resolver — ¿es "usuario es master" el problema, o la forma de detectarlo?**
- El usuario planteó si la arquitectura "usuario es master" es en sí un problema. Análisis: el *principio* (el motor cede al instante ante un toque real) es correcto y necesario. El punto débil es la *detección*: se infiere solo de deltas de ADC, sin sensor de touch fiable (`FaderTouch` capacitivo desactivado desde 2026-06-14). Ese heurístico ha sido la causa raíz de facto de la mayoría de bugs de las últimas 5 sesiones.
- Sin decisión tomada — ver fila 🟡 Media en Pendientes arriba.

**10. S2 — `hardware/Motor/Motor.cpp` + `config.h` — jitter anti-cascada en `setTargetFromS3()`/`setTargetForced()` (fix del hallazgo del punto 8)**
- Nueva `MOVING_JITTER_MAX_MS=150` y helper `_readyToMove()` (mismo patrón que `GOTOMIN_JITTER_MAX_MS`/`_goToMinJitterUntil`, ventana mucho menor para no introducir lag perceptible en automatización real). Arma un retardo aleatorio 0-150ms solo al ARRANCAR movimiento desde parado (`_motor_state != MOVING_TO_TARGET`) — el seguimiento ya en marcha nunca se retrasa. `_motor_targetADC` se actualiza igual durante el jitter; el motor arranca en cuanto expira.
- Aplicado en los dos puntos donde cada función pasa a `MOVING_TO_TARGET` (antes de la línea final de cada una).
- Investigación paralela descartada como causa principal: se revisó si `neopixels.show()` (RMT en ESP32, no bloquea interrupciones globales — descartada la teoría de "ISR cortada") podía colisionar con la ventana de respuesta RS485; el margen ya generoso de S3 (`RS485_RESP_TIMEOUT_US=8000`, ampliado una vez en sesión anterior por "timing variable de S2") hace la hipótesis menos limpia sin instrumentación real — aparcada, no se tocó código para eso.
- **SIN VALIDAR EN BANCO** — pendiente cargar un proyecto y confirmar que baja/desaparece el `[RS485] ID MISMATCH` (que se reportó aleatorio entre slaves, no un slave fijo — consistente con causa sistémica de timing, no una unidad defectuosa).

**11. S3 — `midi/MIDIProcessor.cpp/.h` + `config.h` + `main.cpp` — debounce anti-flash de nombres de pista**
- **Disparador:** Logic escribe temporalmente un texto más ancho que el slot de 7 caracteres del nombre de pista (ej. "Seleccionar"/"Selecting" al seleccionar canal), que se desborda sobre el canal vecino y se restaura ~34ms después. Sin retener el cambio, ese intermedio se aplicaba y se mandaba a S2 antes de la restauración.
- **Fix:** nuevo `TRACK_NAME_DEBOUNCE_MS=100` (`config.h`). En `processMackieSysEx()` (case 0x12), un nombre distinto ya no se aplica de inmediato — se arma en `_pendingName[8]`/`_pendingSince[8]`/`_pendingDirty[8]` con timer fresco. Nueva `tickTrackNameDebounce()` (llamada desde `taskCore0()` en `main.cpp`, junto a `tickCalibracion()`) aplica el pendiente a `trackNames[]` y lo manda a S2 (`rs485.setTrackName()`) solo si sigue igual tras `TRACK_NAME_DEBOUNCE_MS` sin más cambios. Si el nombre vuelve a coincidir con el ya mostrado antes de que expire el timer (caso típico del flash), se cancela el pendiente sin llegar a mandarse nada a S2.
- **SIN VALIDAR EN BANCO** — pendiente confirmar en hardware real que el flash "Seleccionar" ya no llega a S2 y que un renombrado real (persistente) sigue aplicándose sin lag perceptible.

**Validado en banco esta sesión:** calibración de todos los faders ✅. Posicionamiento automático llega "más o menos" a destino ✅ (precisión fina de `K` pendiente). AutoMode TRIM ya no se esconde (pendiente confirmar visualmente el cian en header tras reflash). **Pendiente de validar:** touch real cediendo control al instante tras quitar los spike guards; escenario original del `PitchBend=4000` corregido de verdad; el jitter del punto 10 reduce/elimina el `ID MISMATCH` al cargar proyecto.

---

### SESIÓN 2026-08-13 10:19 — S2: fix signo/doble-conteo encoder, calibración con jitter+retry, LEDs azul/splash tras SAT y desconexión

**Origen:** serie de ajustes solicitados por el usuario sobre el comportamiento del encoder en el SAT (dirección invertida, doble movimiento por clic, navegación errática), seguidos de mejoras a la calibración (jitter anti-cascada, reintento automático) y un fix de UX en LEDs/pantalla al salir del SAT o desconectar de Logic.

**MCU afectadas:** S2 ✅ (Encoder, SatMenu, RS485Handler, Motor, ButtonManager, Neopixel, Display, config, main) · S3 ❌ · P4 ❌ — todos los cambios locales al S2, sin tocar protocolo RS485 salvo el valor (no el formato) de `encoderDelta`.

---

**1. S2 — `hardware/encoder/Encoder.cpp:24` — signo del encoder invertido**
- `_counter += step` → `_counter -= step`. La tabla Gray code (`ENC_TABLE`) tenía el signo contrario al diseño documentado en `docs/ENCODER.md` §1.3 (derecha=+1). Confirmado decodificando la tabla a mano para la secuencia física real.
- Efecto: se propaga a SAT, VPot (`main.cpp`) y `encoderDelta` (RS485 → S3/P4 → Logic) — un solo punto de cambio, coherente con "Encoder.cpp única fuente de verdad".

**2. S2 — `SAT/SatMenu.cpp` `_readBtn()` — navegación errática ("a lo loco") y doble movimiento por clic**
- **Bug 1 (runaway):** `Encoder::reset()` nunca se llamaba mientras el SAT estaba abierto (el bloque que lo hace en `main.cpp` se salta con el SAT abierto) → el mismo delta se reprocesaba en cada tick indefinidamente. Fix: `Encoder::reset()` tras consumir el delta en ambas ramas (DOWN/UP).
- **Bug 2 (doble paso):** `pendingEvents = delta/4` contaba el total de detents en vez de los que quedan tras el evento ya devuelto en la misma llamada → 1 clic físico generaba 2 eventos de menú. Fix: `pendingEvents = max(1, delta/4) - 1`.

**3. S2 — `SAT/SatMenu.cpp`/`.h` — quitado autostart de calibración al entrar a MOTOR_CALIB**
- Eliminado el bloque que disparaba `Motor::startCalib()` automáticamente al entrar a esa pantalla (`_calibStarted`, guard eliminado del header). La calibración manual desde el SAT sigue disponible con REC.
- Efecto colateral conocido, no corregido: el mensaje "(recalibrado)" (`_calibRecalib_ms`) ya no se dispara nunca (solo se seteaba en el bloque eliminado, y el trigger REC en `main.cpp` no lo tocaba tampoco).

**4. S2 — `RS485/RS485Handler.cpp:343` — `encoderDelta` enviado ×4 de más**
- `Encoder::getCount()` → `Encoder::getCount() / 4`. El delta crudo (sin convertir a "muescas") se enviaba tal cual a S3, que lo usa como magnitud directa del CC relativo Mackie (`iMakie-ESP32_S3_EXTENDER/main.cpp:144-153`) — 1 clic físico movía el VPot en Logic ~4 posiciones en vez de 1. P4 no estaba afectado (solo usa el signo, no la magnitud).

**5. S2 — `main.cpp` (autocalib boot) + `config.h` — jitter anti-cascada**
- Nueva constante `CALIB_BOOT_JITTER_MAX_MS=2000`. Cada S2 espera un retardo aleatorio 0-2s (`random()`, hardware RNG del ESP32) antes de disparar `requestCalibration()` en boot, para no arrancar todos los motores a la vez al energizar el rig completo.

**6. S2 — `hardware/Motor/Motor.cpp:86-98` — reintento automático en timeout de calibración**
- Antes: `CALIB_TIMEOUT` (6s sin terminar) iba directo a `CalibPhase::ERROR` sin reintentar — único camino de fallo sin recuperación automática (el fallo por "span corto" ya reintentaba hasta `CALIB_MAX_RETRIES=3`).
- Ahora: el timeout reutiliza el mismo contador/mecanismo (`_motor_calibRetries`, `goToMin()` + `_pendingCalib=true` → vuelve a `startCalib()`). Solo cae en `ERROR` permanente tras agotar los 3 intentos combinados (ambos tipos de fallo comparten el contador).
- Pendiente conocido, no corregido: `_motor_calibRetries` solo se resetea a 0 en éxito — un reintento manual tras `ERROR` (SAT REC, o `FLAG_CALIB` vía MIDI) sin power-cycle no obtiene 3 intentos frescos si el contador ya estaba agotado.
- Auditoría de PWM de calibración realizada a petición del usuario: confirmado que `_hwUp()`/`_hwDown()` en calibración usan siempre `_pwm_min`/`_pwm_max` (cargados de NVS vía `Motor::initPWM()`, con fallback a `config.h` solo si NVS está vacío) — no hay PWM hardcodeado en ningún punto de la máquina de calibración.

**7. S2 — `hardware/button/ButtonManager.h`/`.cpp` + `main.cpp` — clic de encoder sin Logic conectado activa OTA en vez de abrir el SAT**
- Nuevo callback `setOtaCallback()` (patrón `std::function<void()>`, igual que los callbacks de `SatMenu`), registrado en `main.cpp` apuntando a la función ya existente `_satWiFiOta()` (guarda `otaMode=true` en NVS + `ESP.restart()`) — sin duplicar lógica.
- `ButtonId::ENCODER_SELECT` con `logicConnectionState != CONNECTED`: `_sat->open()` → `_cbOta()`. Con Logic conectado, sin cambios (clic normal de VPot). El SAT ya no se abre por clic simple del encoder.
- **Ampliado (mismo día) — REC rediseñado, mismo patrón que el encoder:** eliminado por completo el mecanismo de pulsación larga (`_holding`/`_holdStart`/`_fired`/`_drawBar`/`_clearBar`, barra de progreso "Mantener para SAT...", constantes `SAT_HOLD_MS`/`SAT_BAR_*`/`SAT_LABEL_Y` en `config.h` — todas eliminadas por quedar sin uso). `ButtonManager.cpp::update()` queda vacío (ya no hay nada que trackear tick a tick).
  - Ahora `_onRecReleased()` decide en el clic (release), sin temporizador: con Logic desconectado (splash) → `_sat->open()` directo, sin esperar 3s. Con Logic conectado → `FLAG_REC` normal (debounce 300ms), sin ninguna vía a SAT.
  - **REC ya no tiene función de "mantener" en absoluto — es solo REC** (conectado) o solo "abrir SAT" (desconectado), igual que el encoder. **El SAT ahora solo es accesible con Logic desconectado (splash), por clic simple de encoder o de REC.**

**8. S2 — `hardware/Neopixels/Neopixel.cpp`/`.h` + `SAT/SatMenu.cpp`/`.h` + `main.cpp` + `display/Display.cpp` — LEDs azul y splash no se restauraban tras desconexión de Logic ni al salir del SAT**
- **Root cause LEDs:** el patrón "todos los LEDs azul tenue" (`initNeopixels()`) solo se pintaba una vez, en el boot. Los comentarios en `RS485Handler.cpp` (`onMasterData`, `checkTimeout`) decían explícitamente "Cambio a azul" al desconectar, pero `updateAllNeopixels()` solo gestionaba los 4 LEDs de botones (REC/SOLO/MUTE/SELECT) — nunca repintaba el resto. Fix: `updateAllNeopixels()` ahora repinta todos los píxeles a azul tenue cuando `neoWaitingHandshake==true`, y solo aplica los colores de botón cuando es `false`.
- **Caso SAT:** `_cbLedsOff()` limpiaba los LEDs directamente (bypass de la caché de cambios de `updateAllNeopixels()`) — al cerrar el SAT sin que `neoWaitingHandshake` hubiera cambiado, la caché no detectaba diferencia y no repintaba nada. Fix: nueva `forceNeopixelRefresh()` invalida la caché; nuevo callback `SatMenu::onLedsRestore()` la dispara en `close()`.
- **Root cause pantalla:** `updateDisplay()` solo redibujaba el splash en la rama "desconectado" ante un flanco CONNECTED→DISCONNECTED — si el SAT se cerraba estando ya desconectado (sin flanco), la pantalla quedaba en negro (`SatMenu::close()` solo hace `fillScreen(C_BLACK)`). Fix: la rama desconectada de `updateDisplay()` ahora también respeta `needsTOTALRedraw` (ya puesto a `true` por `_cbRestore` en `close()`) y llama `drawOfflineScreen()`.

**9. Diagnóstico de hardware (sin cambio de código) — unidad con motor que sube pero nunca baja**
- Analizado a petición del usuario: patrón "sube perfecto, nunca baja" en una unidad aislada es consistente con un fallo del lado `IN2`/`OUT2` del DRV8833 (mitad del puente H dañada) — el motor DC de una sola bobina no tiene "canal de bajar" separado, así que un fallo así explica el síntoma exacto sin involucrar al firmware (mismo firmware en el resto de unidades, que funcionan bien). Diagnóstico no invasivo sugerido: medir con multímetro el pin `MOTOR_IN2` (config.h) durante SAT > Motor Test (SOLO=baja) para confirmar si el GPIO conmuta y el DRV8833 no responde, o si el fallo está antes (GPIO/pista). Por directiva del proyecto (hardware locked): no se propone ni se aplica ninguna corrección de cableado — decisión de reparación de hardware del usuario.

**10. S2 — `SAT/SatMenu.cpp` — reordenado menú SAT > Motor, quitado "Motor ON/OFF"**
- `_motorItems[]`/`_motorN` (5→4) y el switch de `_hMotor()` reindexado. Orden nuevo: **PWM Maximo → PWM Minimo → Calibrar → Test Mode**.
- Comprobado antes de quitarlo: `_cfg.motorDisabled` se guardaba/cargaba en NVS pero **nada en el firmware lo leía** — el toggle no tenía ningún efecto real sobre el motor. Se quita solo el ítem de menú; el campo `SatConfig.motorDisabled` y su persistencia NVS quedan intactos por si se conecta a algo en el futuro.

**11. S2 — `config.h` + `ButtonManager.cpp` — ventana de gracia tras boot para REC/encoder (fix de regresión del punto 7)**
- **Bug encontrado tras aplicar el punto 7:** con Logic desconectado, el SAT se abría solo al arrancar, sin que nadie tocara REC. Causa raíz identificada revisando la librería `Button2` vendida en el proyecto (`.pio/libdeps/.../Button2/src/Button2.cpp`): `Button2 buttonRec(...)` es un objeto global cuyo constructor llama `pinMode()` en inicialización estática, **antes de `setup()`** — ventana en la que el pin puede no estar eléctricamente estable (pull-up sin asentar, ruido de arranque). Un parpadeo del pin ≥50ms (debounce por defecto de Button2, `_releasedNow()`) se registra como pulsación real y dispara `released_cb`. Esto probablemente ocurría siempre, pero antes era inofensivo (solo mandaba un `FLAG_REC` perdido); tras el punto 7, el mismo evento abre el SAT directamente — visible.
- **Fix:** nueva constante `BOOT_INPUT_SETTLE_MS=1000`. `_onRecReleased()` y el case `ENCODER_SELECT` de `_onButtonEvent()` ignoran la acción (abrir SAT / activar OTA) si `millis() < BOOT_INPUT_SETTLE_MS`. No afecta el short-press normal de REC con Logic conectado.

**12. S2 — `config.h` + `ButtonManager.cpp` — reforzado el fix del punto 11: hold corto real en REC, más margen de boot en encoder**
- El usuario señaló que con el hold original de 3s este glitch nunca se disparaba — confirma que un hold (aunque corto) filtra mejor que solo la ventana de boot.
- **REC:** nueva `SAT_OPEN_HOLD_MS=400`. `_onRecReleased()` usa `Button2::wasPressedFor()` (duración real de la pulsación, ya disponible en el parámetro `btn` sin coste extra) — solo abre el SAT si la pulsación duró ≥400ms. Filtra toques/rebotes breves en cualquier momento (no solo en boot), no solo glitches de arranque.
- **Encoder:** no tiene hold propio — su callback (`ENCODER_SELECT`, vía `_onButtonEvent`) se dispara en el **press**, no en el release (a diferencia de REC), así que `wasPressedFor()` no está disponible ahí sin reestructurar a un handler de release dedicado (no se hizo, fuera de alcance). Como mitigación, `BOOT_INPUT_SETTLE_MS` sube de 1000 a 2000ms — única protección disponible para ese camino sin ese refactor.

**13. S2 — `config.h` + `Motor.cpp` — jitter anti-cascada también en `goToMin()` (no solo en calibración de boot)**
- Detectado en banco vía MIDI Monitor + logs serie S3: al desconectar Logic, **todas las S2 bajan a 0 a la vez** (`Motor::goToMin()` es "MASTER ABSOLUTO" si `!_connected`) — N puentes H conmutando en fase a PWM_MAX/20kHz simultáneamente. Correlacionado con una ráfaga de `[RS485] ID MISMATCH`/`CRC ERROR` en S3 (IDs random mezclados, no un solo esclavo tardío) — encaja con ruido eléctrico/caída de tensión en alimentación compartida durante el pico de corriente sincronizado, más que con acoplo de cable de un único motor. Mitigación de hardware sugerida al usuario (condensador en motor + desacoplo en DRV8833 + revisar capacidad de la fuente compartida), pendiente de decisión suya.
- **Fix de software (complementario):** nueva `GOTOMIN_JITTER_MAX_MS=2000`. `Motor::setConnected()` calcula un retardo aleatorio 0-2s en el flanco conectado→desconectado; `Motor::init()` hace lo mismo para el caso de encendido simultáneo de todo el rig (`_connected` arranca en `false`). Los dos disparadores de `goToMin()` (`IDLE` y `AT_TARGET`, `Motor.cpp`) respetan el nuevo `_goToMinJitterUntil`. No compromete la garantía "GoToMin MASTER ABSOLUTO, ejecuta SIEMPRE" — solo la retrasa hasta 2s por unidad, nunca la salta.

**14. S2 — `config.h` + `Display.cpp` — apagado por inactividad en Splash (2 min, fundido 8s)**
- Petición del usuario: en pantalla Splash (Logic desconectado) sin actividad durante `SPLASH_DIM_TIMEOUT_MS=120000` (2 min), el brillo empieza a atenuarse linealmente hasta 0 en `SPLASH_DIM_FADE_MS=8000` (8s).
- Se reinicia con cualquier actividad: touch del fader (`Motor::isManualTouchDetected()`), REC/SOLO/MUTE/SELECT, pulsador o rotación del encoder. Restaura brillo al instante (sin fundido de entrada) al detectar actividad tras haberse atenuado.
- El timer se reinicia también al entrar en Splash (flanco CONNECTED→DISCONNECTED y en el redraw forzado tras cerrar el SAT ya desconectado, punto 8 de esta misma sesión).

**15. S3 — `midi/MIDIProcessor.cpp` — Bug B3 resuelto: fader se movía solo al abrir Logic sin proyecto (pendiente desde 2026-05-20)**
- **Causa raíz confirmada con MIDI Monitor** (captura real del usuario): Logic manda PitchBend=0 en los 10 canales en ráfaga de microsegundos al conectar. El canal 0 dispara el flip `MIDI_HANDSHAKE_COMPLETE→CONNECTED` y fija `connectedSinceTime=millis()` a mitad de la ráfaga; el canal 1 (y siguientes), procesados justo después, ya ven `CONNECTED==true` y entran en el guard de grace period (`CONNECT_GRACE_MS=1500`) — pero ese guard hacía `return` incondicional, tirando **todo el mensaje**, no solo la heurística de desconexión que debía proteger. Resultado: el canal que dispara el flip sí reenvía su target=0 (se procesó antes del guard), el resto de la ráfaga inicial no — asimetría que dejaba algún fader con el target viejo que tuviera en memoria en vez de 0.
- **Decisión de diseño revisada en la propia sesión:** en vez de "arreglar" para que todos reciban el mismo target=0 al conectar, se decidió que **ningún fader debe moverse durante la ráfaga inicial** (si ya estaban en 0 por el `goToMin()` de desconexión, moverlos de nuevo es ruido/desgaste innecesario justo en el momento más sensible). El guard de grace period ahora bloquea el reenvío de target para los 8 canales por igual, no solo la heurística de desconexión — after el grace period, el reenvío normal (con deadband) se reanuda igual que siempre.
- **P4 explícitamente NO tocado** — mismo patrón (`CONNECT_GRACE_MS`) muy probablemente presente ahí también, pendiente de decisión del usuario para portar el fix.

**16. S2 — `hardware/Motor/Motor.cpp:353-364` (`_positionTick`) — denominador equivocado en el frenado fino de posición**
- **Síntoma reportado en banco:** con proyecto real cargado, los faders con el target más alto de la ráfaga inicial (post-fix del punto 15) se pasaban de largo hasta el tope mecánico real en vez de pararse en el target de Logic (targets del 62-76%, no proporcional). Movimientos cortos, además, iban a trompicones.
- **Causa raíz:** `targetPWM = _pwm_min + (min(absErr, _motor_adcSpan) * (_pwm_max-_pwm_min)) / _motor_adcSpan` usaba `_motor_adcSpan` (~26000, el recorrido completo calibrado) como denominador en vez de `POSITION_CRUISE_ERR` (2000, el ancho real de la zona de frenado fino). Con ese denominador, el PWM caía casi de golpe a `_pwm_min` en cuanto `absErr<POSITION_CRUISE_ERR` y se quedaba plano el resto del tramo — sin rampa progresiva real. En movimientos largos (llegando con inercia desde el crucero a `PWM_MAX`), ese frenado insuficiente y constante no bastaba para parar a tiempo → overshoot hasta el tope físico. En movimientos cortos (que nunca superan los 2000 counts), el PWM era casi mínimo desde el principio → trompicones si ese nivel está cerca del umbral de arranque/cogging del motor.
- **Fix:** denominador cambiado a `POSITION_CRUISE_ERR` — rampa lineal real de `PWM_MAX` (al entrar en la zona) a `PWM_MIN` (en el target) a lo largo de los 2000 counts, en vez de un salto casi inmediato a mínimo.
- Hipótesis alternativa descartada en la sesión (a petición del usuario): que la calibración del fader no se estuviera aplicando en algún punto de la cadena S3→S2 — confirmado que `_pbToADC()` se aplica siempre, para todos los AutoMode, con `_calibratedFaderMin/Max` correctamente delimitados. No es un problema de calibración, es un problema de control de posición.

**17. S2 — `hardware/Motor/Motor.cpp:353-370` (`_positionTick`) — rampa lineal → cuadrática (ajuste sobre el punto 16)**
- **Síntoma reportado en banco tras el punto 16:** el fader empezó a rebotar sobre el target (overshoot → corrige → overshoot menor → asienta) en vez de pararse limpio — confirmado por el usuario que se amortigua solo, no es oscilación infinita.
- **Causa:** la rampa lineal (punto 16) mantenía demasiado PWM hasta muy cerca del target, sin margen real de frenado en el tramo final.
- **Fix:** rampa cuadrática — `targetPWM = _pwm_min + (_pwm_max-_pwm_min) × (absErr/POSITION_CRUISE_ERR)²`. Mismo `PWM_MAX` al entrar en la zona (sigue venciendo la fricción del crucero), pero decae mucho antes y más fuerte según se acerca al target — más margen de frenado real sin perder el empuje inicial.

**18. S3 — `midi/MIDIProcessor.cpp:702-720` (`processPitchBend`) — refinado el fix del Bug B3 (punto 15): faders huérfanos al cerrar proyecto**
- **Síntoma confirmado con MIDI Monitor en banco:** al cerrar un proyecto, Logic manda PitchBend=0 a los 8 canales — pero varios faders que NO estaban ya en 0 (de pruebas anteriores) se quedaban abandonados en su posición real, nunca llegaban a 0. Causa: el fix del punto 15 bloqueaba TODO reenvío durante `CONNECT_GRACE_MS`, incluso cuando el fader realmente necesitaba corregirse — y como Logic no repite el mismo valor, esa única oportunidad se perdía para siempre.
- **Fix:** nueva condición `alreadyThere = |bendClamped - rs485.getChannel(id).faderPos| <= PITCHBEND_DEADBAND` (compara contra la posición real ya conocida del slave, no solo contra el último valor enviado). El grace period ahora solo bloquea cuando el fader **ya está** donde Logic pide — si difiere de verdad, se deja pasar aunque siga dentro del grace period.

**19. S2 — `hardware/Motor/Motor.cpp:651-673` (`setADCDelta`) — causa raíz real de los faders huérfanos: falso touch por rebote al frenar**
- **Hallazgo clave, con MIDI Monitor + logs RS485 correlacionados:** incluso con el fix del punto 18 aplicado, los faders seguían sin llegar limpio a destino tras cerrar proyecto — reportando `touchState=1` sostenido con `faderPos` congelado a medio camino (visible en `RS485.cpp:251` `[S3-RX] touchState=1 slave=X faderPos=...` repitiéndose sin que nadie tocara nada).
- **Cadena causal completa:** `setADCDelta()` ya tenía un guard direccional (2026-05-24): si el ADC se mueve en dirección CONTRARIA al target durante `MOVING_TO_TARGET`, lo interpreta como oposición real del usuario → `_motor_manualTouchDetected=true`. En AUTO_OFF/READ (DAW absoluto) esto no detiene el motor (sigue persiguiendo el target real, `Motor.cpp:702-704`) pero **sí reporta `touchState=1` a Logic** vía SELECT MIDI en S3 (`main.cpp:93-99`). Logic, al ver ese touch, trata el fader como "el usuario lo sujeta" y dejar de forzarle posición mientras cree eso es comportamiento estándar de automatización MCU — el fader queda huérfano donde estaba, aunque el motor internamente sí completara el viaje.
- Cualquier rebote/inversión momentánea de dirección al frenar cerca del target (el mismo overshoot ajustado en los puntos 16-17 — más probable cuanto más rápido llega el motor) dispara esta cadena. No es un bug introducido hoy — es una vulnerabilidad de la arquitectura "usuario es master" (mayo 2026) que los ajustes de velocidad de hoy hicieron más visible.
- **Fix:** una inversión de dirección dentro de la zona de frenado (`< POSITION_CRUISE_ERR` del target) ya no se interpreta como touch — se asume asentamiento mecánico. Lejos del target, una dirección opuesta sigue detectándose como oposición real del usuario sin cambios (la garantía de seguridad "usuario es master" se mantiene intacta fuera de esa zona).
- **RIESGO ALTO — pendiente validar en banco, en este orden:** (1) cerrar proyecto → faders deben llegar limpio a 0 sin quedarse huérfanos; (2) sujetar un fader de verdad mientras se mueve → debe seguir cediendo el control al instante, sin excepción — es la garantía que no se puede perder; (3) automatización normal con proyecto real.

**20. S2 — `hardware/Motor/Motor.cpp:603-619` (`setADC`) — spike guard del ADC ya no se desactiva por falso touch**
- El usuario insistió en que "la subida a tope en vez de posición" seguía siendo culpa del firmware, no de EMI de bus. Revisando `setADC()`: el guard contra picos eléctricos del ADC (`ADC_SPIKE_GUARD=500`) se desactivaba por completo cuando `_motor_manualTouchDetected` era `true` — pensado originalmente para no rechazar un movimiento rápido genuino del usuario, pero ese mismo flag puede activarse por un rebote al frenar (punto 19), justo en el instante de mayor corriente/ruido eléctrico real.
- **Fix:** quitado `_motor_manualTouchDetected` de la condición que desactiva el spike guard — solo se desactiva ya en calibración real o bajada a mínimo (movimiento grande intencionado y conocido). Un toque real del usuario es movimiento físico gradual, no debería generar picos que activen el guard de todas formas.

**21. S2 — `main.cpp:57-63` (`_satWiFiOta`) — LEDs no se apagaban al activar OTA por el camino nuevo**
- Regresión del punto 7 de esta misma sesión (encoder-click-en-splash → OTA directo, sin pasar por el SAT): `_satWiFiOta()` nunca apagaba los NeoPixels por sí misma — antes siempre se llegaba a través del SAT, que ya los apagaba al abrirse (`_cbLedsOff`). El camino nuevo salta el SAT, así que los LEDs se quedaban en su patrón azul de espera hasta reiniciar en modo OTA.
- **Fix:** `clearAllNeopixels(); showNeopixels();` añadido directamente en `_satWiFiOta()`, antes de guardar `otaMode=true` y reiniciar — cubre ambos caminos de entrada (SAT y clic directo).

**22. S3 — `RS485/RS485.cpp:191-202` — nuevo log de diagnóstico por cambio de AutoMode**
- El log TX existente (`[RS485-TX] slave=%d faderTarget=...`) solo disparaba cuando cambiaba `faderTarget` — un cambio de AutoMode sin mover el fader nunca se veía en el log. Necesario porque las unidades S2 ya están montadas en el rig, sin acceso a su log serie propio.
- Nuevo log independiente: `[RS485-TX] slave=%d AUTOMODE %d → %d flags=0x%02X`, con su propio array de último valor logueado por slave (`_lastLoggedAutoMode[9]`, inicializado a `-1` para no colisionar con los valores reales 0-5).

**23. S2 — `config.h:221` (`MANUAL_TOUCH_AT_TARGET_THRESHOLD`) — hallazgo central del día: umbral de touch en reposo dentro del ruido ADC real**
- **Log decisivo** (MIDI Monitor + serie S3 simultáneos, captura completa de apertura de proyecto): el slave 1 reportó `touchState=1` **sostenido durante más de 3 segundos seguidos** justo al conectar, con `faderPos` temblando en una banda de ~28 cuentas (4670↔4698, luego 9421↔9448) sin que nadie tocara nada — y **antes de que llegara el target real del proyecto**.
- **Causa:** `MANUAL_TOUCH_AT_TARGET_THRESHOLD` estaba en `30` (bajado de 50 el 2026-05-27) — el ruido eléctrico real del ADC en banco (~28 cuentas en la ventana de 80ms) roza ese umbral. Cada vez que el ruido lo superaba, se refrescaba `_motor_manualTouchStartTime` (línea 698: "Refresh en cada movimiento"), impidiendo que se acumularan los `MANUAL_TOUCH_DEBOUNCE_MS=600` de quietud necesarios para soltar el touch — de ahí el bloqueo sostenido.
- **Por qué explica "todos los S2 se van a la puta al abrir Logic, y solo se arreglan moviendo los faders en Logic":** justo tras conectar, con el fader recién asentado en `AT_TARGET` (umbral más sensible que en movimiento), es cuando este falso touch se dispara con más facilidad — bloqueando la corrección del fader hasta que Logic manda un target con un salto lo bastante grande como para colarse pese al touch fantasma.
- **Fix:** `MANUAL_TOUCH_AT_TARGET_THRESHOLD`: 30 → 70 — margen cómodo sobre el ruido real observado (28), sin perder sensibilidad a un toque genuino (salto mucho mayor en la misma ventana).
- **RIESGO ALTO — sexta capa tocada hoy sobre el mismo mecanismo crítico "usuario es master".** Pendiente validar en banco antes de dar la investigación por cerrada.

**BRIEF DE DISEÑO PENDIENTE (2026-08-13 14:17) — separar PWM de arranque del PWM de posicionamiento fino**

**Para sesión dedicada, con iteración en banco — no delegable a un agente sin acceso al hardware físico.**

**Contexto:** tras las seis capas de fixes de esta sesión (puntos 16, 17, 19, 20, 23 — denominador de frenado, rampa cuadrática, dirección/zona en `setADCDelta`, spike guard en `setADC`, umbral `AT_TARGET`), el usuario señaló que la velocidad de movimiento del motor sigue siendo alta y que además sospecha un problema de lectura ADC en paralelo. Aclaración clave del usuario: para que la calibración arranque de forma fiable en banco, tiene `pwmMin=125` y `pwmMax` variable por unidad (195-220, depende de la unidad física) guardados en NVS — **más alto que los valores de fábrica de `config.h` (100/160)**.

**El problema de fondo:** `_pwm_min`/`_pwm_max` es un único par de valores por unidad, usado tanto para:
1. **Arrancar el movimiento desde parado** (`KICK_UP`/`KICK_DOWN` en calibración, y el primer instante de cualquier movimiento) — necesita vencer fricción **estática**, típicamente alta.
2. **Movimiento fino cerca del target** (la cola de `_positionTick()`, ajustada dos veces hoy) — necesita solo vencer fricción **dinámica**, normalmente bastante menor.

Si `pwmMin` se sube (como ha hecho el usuario, a 125) para garantizar que el arranque de calibración funcione de forma fiable, ese mismo valor alto se aplica también como suelo de velocidad en el posicionamiento fino — por mucho que se ajuste la forma de la curva de frenado (rampa lineal, luego cuadrática, ambas probadas hoy), el "suelo" nunca baja de ese `pwmMin` elevado. Esto podría explicar por qué el frenado sigue sin sentirse suficientemente fino pese a los ajustes de curva.

**Arquitectura propuesta a explorar — SIN valores nuevos en NVS, todo relativo a `pwm_min`/`pwm_max` existentes:**
- **No** introducir un tercer valor persistido ni pantalla SAT nueva. `_pwm_min`/`_pwm_max` (NVS, por unidad, ya calibrados) siguen siendo la única fuente de verdad — igual que hoy.
- Calibración (`KICK_UP`/`KICK_DOWN`, `GOING_UP`/`GOING_DOWN`) **no cambia**: sigue usando `_pwm_min`/`_pwm_max` tal cual, ya confirmado que funciona con 125-220.
- El suelo de la rampa de `_positionTick()` (la cola de frenado cerca del target) pasa a ser **una fracción calculada del rango `_pwm_min`-`_pwm_max`**, no un valor independiente — por ejemplo `pwmFine = _pwm_min - K × (_pwm_max - _pwm_min)` para alguna fracción `K` a determinar en banco (constante en `config.h`, no en NVS). Se ajusta solo por unidad automáticamente porque deriva de los mismos `pwmMin`/`pwmMax` ya calibrados — sin tocar NVS, sin pantalla SAT nueva.
- Revisar también el filtrado de ruido ADC en origen (`FaderADC`: `NOISE_WINDOW_SIZE`, `NOISE_K_MOVE`, `NOISE_K_MICRO`, `FADER_EMA_ALPHA_FAST` en `config.h`) — el ruido real medido en banco (~28 cuentas) podría reducirse con un filtro EMA más agresivo, dando más margen a cualquier umbral de touch sin necesidad de subirlo tanto.

**Qué validar en banco, por unidad (la fricción varía):**
- Calibración sigue arrancando de forma fiable — no se toca, pero confirmar que no hay efecto colateral.
- Movimiento fino/frenado se siente más suave con el suelo derivado más bajo, sin perder la capacidad de vencer fricción localizada cerca de los extremos (el problema original que motivó el crucero a `PWM_MAX` en `POSITION_CRUISE_ERR`, sesión 2026-07-22) — ajustar la fracción `K` en banco hasta encontrar el punto correcto.
- Sujetar un fader real sigue cediendo el control al instante — ninguna de las capas ya arregladas hoy debe romperse.

**24. S2 — `display/Display.cpp` — LEDs se apagan junto con la pantalla en el apagado por inactividad**
- Petición del usuario: el apagado por inactividad en Splash (punto 14 de esta sesión) solo atenuaba el brillo de la pantalla — los LEDs (patrón azul de espera) seguían encendidos.
- **Fix:** nuevo `#include "../hardware/Neopixels/Neopixel.h"`. Estado `_splashLedsOff` (paralelo a `_splashDimmed`) — cuando el fundido de pantalla llega a brillo 0 (no durante el fundido, solo al terminar), `clearAllNeopixels()`+`showNeopixels()` apaga los LEDs. Cualquier actividad los restaura vía `forceNeopixelRefresh()` (misma función del punto 8, repinta el patrón azul en el siguiente tick de `updateAllNeopixels()`, que corre cada loop sin gate de conexión).

---

### SESIÓN 2026-08-07 20:06 — S3: fix identificación MCU (handshake en bucle) + S2: fixes Motor.cpp + splash screen rediseñada

**Origen:** continuación de la investigación "S2 fader sube a máximo y no se detiene" (filas 🔴 CRÍTICA y 🔴 Alta de Pendientes, sesiones 2026-07-24 y 2026-08-02). Sesión larga con MIDI Monitor + logs de serie en vivo del S3, contrastados byte a byte. Se confirmó con evidencia repetida que el S3 transmite exactamente lo que Logic envía (sin corrupción, sin desplazamiento de dominio) — el foco se desplazó a dos bugs reales encontrados en el camino, uno en S2 (target inalcanzable) y uno en S3 (identificación MCU), más mejoras de diagnóstico visual en la pantalla S2.

**MCU afectadas:** S2 ✅ (Motor, RS485Handler, Display, ButtonManager) · S3 ✅ (RS485, MIDIProcessor, config, main) · P4 ❌ (explícitamente fuera de alcance esta sesión).

---

**1. S2 — `hardware/Motor/Motor.cpp` — target inalcanzable tras recalibración (confirmado en banco: slaves 5/7/8 subían al tope y no paraban)**
- `setTarget(uint16_t target)`: eliminada la escritura a `_motor_lastMidiTarget` — la función pasa a anclar `_motor_targetADC` directamente en dominio ADC (coincide con sus dos únicos llamadores, que ya pasan `Motor::getRawADC()`).
- `_calibUpdate()` (rama `SETTLE_DOWN→DONE`): en vez de recalcular el target vía `map()` sin clamp sobre `_motor_lastMidiTarget` (que podía contener un valor en dominio ADC de hasta ~27000 tratado como si fuera PitchBend 0-16383, produciendo un target muy por encima del tope físico), ahora deja `_motor_targetADC = _motor_adcPos` (se queda donde está) — el "flanco de calibración" de `RS485Handler.cpp` aplica el target real de Logic, ya acotado, en el siguiente paquete.
- Protección STALL (`Motor::update()`, bloque de topes mecánicos): el temporizador `_stallProtectStart` no se armaba si el motor empezaba a empujar contra una posición YA parada (solo se armaba tras el primer movimiento >10 cuentas de ADC) — un motor persiguiendo un target inalcanzable, ya pinned contra el tope físico desde el primer tick, nunca activaba el corte de seguridad. Ahora se arma también en el instante en que el motor pasa de inactivo a activo.
- `S2/config.h`: eliminada la variable `_motor_lastMidiTarget`, sin uso tras el fix.

**2. S3 — identificación MCU incorrecta (causa raíz probable del handshake en bucle / `ID MISMATCH` / degradación RS485)**
- **Hallazgo confirmado con captura real de handshake (MIDI Monitor):** en `MIDIProcessor.cpp::processMackieSysEx()`, las respuestas a `Device Query` (`cmd 0x00`) y `Host Connection Query` (`cmd 0x13`) tenían la familia del dispositivo escrita a mano como `0x14` (identidad del P4) en vez de usar `DEVICE_FAMILY`. El S3 respondía "soy familia 0x14" sin importar qué familia sondeara Logic — incluso cuando Logic buscaba explícitamente el extender (`0x15`). Logic nunca lograba diferenciar el S3 del P4 y reiniciaba la negociación en bucle indefinidamente (visible en los logs `[HANDSHAKE] Device Query 0x00 — Logic reinicia negociación` repetidos).
- `S3/config.h`: `DEVICE_FAMILY`/`VERSION_REPLY_CMD` de la rama `DEVICE_S3_EXTENDER` corregidos de `0x14` a `0x15` — estaban copiados por error de la rama `DEVICE_P4_MASTER`. La plantilla equivalente en `P4_JC1060P470C/src/config.h` ya tenía el valor correcto (`0x15`), sirvió de referencia.
- `MIDIProcessor.cpp` (`cmd 0x00`, `cmd 0x13`): las dos respuestas ahora usan `DEVICE_FAMILY` en vez del literal `0x14`.
- `MIDIProcessor.cpp` (guarda de Fase 1+): `if (device_family != 0x14) return;` corregida a `if (device_family != DEVICE_FAMILY) return;` — con el fix anterior, Logic empezó a direccionar los comandos de Fase 1+ (incluido `0x21`, que marca `CONNECTED`) a familia `0x15`, y esta guarda seguía comparando contra `0x14`, bloqueando la transición a `CONNECTED` por completo. Confirmado en banco: sin este segundo fix, el dispositivo se quedaba sin conectar nunca tras el primer fix.
- Validado en banco con captura de handshake real: tras ambos fixes, Logic completa el handshake dirigido a familia `0x15`, envía la inicialización completa (V-Pot, LEDs, timecode, scribble strips con nombres reales de pista) y el dispositivo llega a `CONNECTED` de forma consistente.

**3. S3 — mitigaciones adicionales de robustez (no confirmadas como causa raíz, pero corrigen comportamiento incorrecto real)**
- `RS485.cpp::_handleResponse()`: al detectar que un slave pasa a `calibrado=1`, se fuerza `faderTarget=0` para ese slave en el mismo instante, en vez de dejar el valor cacheado de antes (potencialmente de otra sesión/proyecto). Evita que una recalibración en caliente reaplique una posición vieja sin relación con el estado actual de Logic.
- `main.cpp::processSlaveResponse()`: nuevo gate de conexión — no se envía feedback (PitchBend, notas de touch/SELECT, botones) hacia Logic mientras `logicConnectionState != CONNECTED`. Antes, el feedback salía sin condición alguna, incluso durante una renegociación de handshake (ej. el S2 bajando a 0 por desconexión podía colarse en Logic como si fuera movimiento real del usuario). Simétrico al gate que ya existía en la entrada (`MIDIProcessor.cpp`, 2026-08-02).

**4. S3 — diagnóstico añadido (logs, quitables cuando ya no hagan falta)**
- `RS485.cpp::_sendPacket()`: `[RS485-TX] slave=%d faderTarget=%u flags=... autoMode=... connected=...`, solo cuando `faderTarget` cambia.
- `MIDIProcessor.cpp::processPitchBend()`: `[MIDI-RX] PB ch=%d raw=%d connected=%d` (restringido a `channel<8`, los únicos que alimentan slaves reales) y `[MIDI-RX] → RS485 slave=%d ... (forwarded)` / `DESCARTADO (no conectado)`.
- Silenciados temporalmente (comentados, no borrados): el reporte periódico `[PROF] Ciclo ...` del profiler RS485 y el log repetido `[CALIB] Slave %d reporta CALIB_ERROR` — puro ruido durante esta sesión de depuración, la detección subyacente sigue activa.

**5. S2 — pantalla splash/offline rediseñada y unificada (`display/Display.cpp`, `Display.h`)**
- **Unificación arranque/offline:** el camino de desconexión explícita (`RS485Handler.cpp`, transición `pkt.connected→false`) no llamaba a `drawSplashScreen()` ni restauraba el brillo — se quedaba mostrando el último estado (nombre de pista, VU) con el brillo de conectado. Ahora llama a `drawSplashScreen()` + `setScreenBrightness(BRIGHTNESS_SPLASH)`, igual que el arranque y el timeout RS485 (que ya lo hacían bien).
- **Bug de redibujado único corregido:** `drawOfflineScreen()` tenía un guard `static bool splashDrawn` que nunca se reseteaba — solo dibujaba la primera desconexión de toda la sesión de encendido; desconexiones posteriores no refrescaban nada. Guard eliminado (el llamador ya hace detección de flanco). Brillo unificado a `BRIGHTNESS_SPLASH` (antes `100` hardcodeado).
- **Nueva info visual en la splash:** línea `PWM %u-%u` (rango PWM del motor guardado en NVS, `Motor::getPWMMin()/getPWMMax()`) debajo del Build ID, y debajo un punto de estado de calibración — verde si `Motor::isCalibrated()`, rojo si no.
- **Refresco ligero del punto de calibración:** nueva función `drawCalibDot()` (extraída, reutilizable), invocada desde `updateDisplay()` cada vez que `Motor::isCalibrated()` cambia mientras la splash está visible — redibuja solo el círculo, sin refrescar toda la pantalla ni provocar parpadeo.

**6. S2 — entrada al SAT menu desde el encoder (`hardware/button/ButtonManager.cpp`)**
- El botón del encoder (`ENCODER_SELECT`, GPIO11) ahora abre el SAT menu con pulsación simple, **solo mientras se muestra la splash screen** (`logicConnectionState != CONNECTED`). En operación normal sigue mandando el clic de VPot a Logic sin cambios — decisión explícita: el clic de VPot no tiene efecto útil durante la splash (no hay sesión activa que lo reciba), así que sustituirlo ahí no pierde funcionalidad. El mecanismo previo (mantener REC 3s, con barra de progreso) sigue intacto y funciona en cualquier estado.

**Pendiente / sin confirmar:**
- La causa raíz de por qué la calibración de los slaves 5, 7 y 8 en concreto queda mal medida sigue sin confirmar — S2 no fue accesible para reflashear/depurar en esta sesión. Hipótesis principal: inestabilidad del bus/alimentación durante el arranque en cascada (coincide en el tiempo con `ID MISMATCH` y `TO:%` altos del profiler RS485 vistos en la misma sesión).
- Validar en banco tras reflashear S2: que el fix de `Motor.cpp` realmente evita el target inalcanzable, y que la protección STALL corta dentro de `STALL_PROTECT_MS` si aun así el motor queda empujando contra un tope.
- Reactivar los logs `[PROF]`/`[CALIB] ... CALIB_ERROR` en S3 cuando ya no haga falta el silencio temporal (líneas comentadas, no borradas).

---

### SESIÓN 2026-08-02 17:05 — Investigación: motores S2 quemándose, empujados hacia arriba de forma sostenida — SIN RESOLVER

**MCU afectadas:** S2 (fallo), S3 (logging diagnóstico añadido, sin cambios de lógica).
**Origen:** el usuario reporta en banco, con hardware real, que los motores DRV8833 de varios S2 se están quemando por empuje sostenido del fader hacia arriba. Sesión larga de investigación, con varios cambios de foco — se documenta para continuar en otro chat.

**Punto de partida — guard aparcado:**
Primera propuesta (aparcada por el usuario antes de implementar): nuevo guard en `Motor.cpp` que limite el tiempo continuo que `_hwUp()` puede estar activo, excepto durante `KICK_UP`/`GOING_UP` de la calibración de arranque. Diagnóstico previo: `STALL_PROTECT_MS` (400ms, `Motor.cpp:439-461`) solo corta el motor si el ADC no cambia en ninguna dirección — con jitter real el timer se resetea indefinidamente sin que haya progreso neto hacia el target, dejando el motor empujando sin límite de tiempo. **Este guard es ahora requisito explícito y prioritario del usuario — ver fila 🔴 CRÍTICA en Pendientes arriba.**

**Redirección — foco en datos S3→S2:**
El usuario apuntó a que el problema podría estar en que el S3 no garantiza que el `faderTarget` que reenvía a cada S2 corresponda a datos en tiempo real. Revisado `RS485.cpp` (S3): `setFaderTarget()` sobrescribe `_ch[id].faderTarget` sin timestamp de frescura; el bus reenvía el último valor conocido cada ciclo (`POLL_CYCLE_MS=20ms`) indefinidamente aunque Logic deje de mandar PitchBend para ese canal. No se llegó a confirmar si esto es la causa raíz — quedó abierto.

**Hallazgo 1 — MIDI Monitor: handshake MCU repetido:**
El usuario aportó una captura de MIDI Monitor mostrando el handshake completo de Logic (Device Query `F0 00 00 66 14 00 F7` → `0x21` CONNECTED → reset total de superficie con los 10 Pitch Wheel a `-8192`) repetido dos veces con ~37s de diferencia. **Aclarado más tarde: Logic estaba abierto pero sin proyecto cargado — el reintento de handshake en bucle es esperable en ese estado** (Logic no puede completar el mapeo de canales sin proyecto), no es en sí mismo el bug.

**Diagnóstico añadido (S3, solo logging, sin lógica nueva):**
- `main.cpp::setup()`: log del motivo de reset (`esp_reset_reason()`) al arrancar, para confirmar/descartar reinicios físicos del S3 (brownout/watchdog/panic).
- `MIDIProcessor.cpp::processMackieSysEx()`: log con timestamp en `command==0x00` (Device Query entrante) y `command==0x21` (CONNECTED), para correlacionar con el log de MIDI Monitor.

**Hallazgo 2 — log serie S3 real, con Logic abierto sin proyecto:**
- Ráfaga de 10 `Device Query 0x00` en 150ms (cada ~13ms) — inicialmente sospechoso, descartado como bug tras confirmar que Logic no tenía proyecto cargado (reintento de handshake esperable en ese estado, no confirmado al 100% pero coherente).
- **Bus RS485 degradándose en vivo:** TO% subiendo de 13.8%→18.7% en ~20s (`Profiler.h`), con al menos un `[RS485] ID MISMATCH esperado=8 recibido=1`. Sin líneas `CRC ERROR` en la captura — la degradación es de **timeouts** (sin respuesta), no de paquetes corruptos aceptados.
- **`touchState=1` con `faderPos` fluctuando en los slaves 1, 2 y 7**, sin proyecto cargado en Logic (sin target real que los mueva) y sin confirmar todavía si había alguien tocando los faders físicamente en ese momento — **pregunta que quedó sin responder por el usuario**. Como los paquetes pasan CRC8, el dato es probablemente genuino desde el punto de vista del S2 (no corrupción de bus), lo que apunta a ruido eléctrico real en el ADC/circuito de touch de esos S2, o touch físico real sin confirmar.

**Conclusión de la sesión (sin cerrar):**
No se determinó la causa raíz eléctrica exacta (ruido en ADC, alimentación, bus RS485 marginal). El usuario cortó la investigación por cansancio ("llevo toda la tarde") y pidió centrar el esfuerzo en la única acción que puede tomarse ya, sin más diagnóstico: **el guard de firmware en S2** que hace estructuralmente imposible el empuje sostenido, independientemente de cuál sea la causa eléctrica de fondo. Ver fila 🔴 CRÍTICA en Pendientes arriba para el requisito exacto a implementar en el próximo chat.

**Pendiente para retomar (por orden lógico):**
1. Implementar el guard de 1s en `Motor.cpp`/`config.h` (requisito ya definido, ver Pendientes).
2. Confirmar con el usuario si había touch físico real en slaves 1/2/7 durante la captura del log.
3. Revisar `[BOOT] Reset reason` en el log serie del S3 (logging ya añadido, pendiente de que el usuario comparta esa línea concreta — no apareció en los fragmentos pegados hasta ahora).
4. Investigar causa de la degradación progresiva del bus RS485 (TO% subiendo en vivo) — posible origen eléctrico compartido con el ruido de touch/ADC en los S2.

---

### SESIÓN 2026-08-02 — S2: número de track más grande/blanco en splash + fix % real en pantalla OTA

**MCU afectadas:** S2 únicamente.
**Origen:** dos peticiones del usuario en la misma sesión — cosmética de la pantalla de arranque y visibilidad del progreso OTA en pantalla (antes solo por Serial).

| Archivo | Cambio |
|---|---|
| `S2/display/Display.cpp::drawSplashScreen()` | Número de track (`"Track %d"`) pasa de `FreeSans12pt7b` gris (`TFT_MCU_GRAY`) a `FreeSans24pt7b` blanco (`TFT_WHITE`), reposicionado (Y=105→85) para no solapar con la línea de versión FW (Y=130). |
| `S2/OTA/Otamanager.cpp::onOTAProgress()` | Añadido redibujo del % de subida en pantalla (antes solo `Serial.printf`), limitado a la franja del subtítulo (`fillRect` de 20px, no `fillScreen`) y al mismo throttle de 1s ya existente — evita añadir latencia relevante al ciclo `server.handleClient()` (confirmado: callback corre síncrono, mismo core/tarea que `Update.write()`, sin concurrencia real). |
| `S2/OTA/Otamanager.cpp` | Subtítulo `"Esperando upload..."` y el texto de % `"Subiendo... N%"` pasan de `TFT_DARKGREY` a `TFT_WHITE` (petición del usuario, mejor legibilidad). |
| `S2/OTA/Otamanager.cpp` | **Fix de bug real de ElegantOTA (modo síncrono):** el `final` que ElegantOTA pasa a `onOTAProgress()` es `upload.totalSize` del core `WebServer`, que es **acumulativo** (crece igual que `current`), no el tamaño real del archivo — por eso el % calculado salía siempre ≈100%. Confirmado leyendo el código vendorizado (`ElegantOTA.cpp:303-312`, `Parsing.cpp:306/492` del core Arduino-ESP32) y que en ESP32 `Update.begin(UPDATE_SIZE_UNKNOWN, ...)` nunca conoce el tamaño real. Fix: usar `server.clientContentLength()` (API pública del `WebServer`, ya parseada desde la cabecera `Content-Length` de la petición HTTP) como divisor real del %. Requirió mover `WebServer server(80)` de variable local en `enableForUpload()` a ámbito de archivo para que el callback pueda leerla. |

**Validación pendiente en hardware:** confirmar en banco que el splash se ve bien (sin solape) y que el % en la pantalla OTA avanza de forma progresiva y coherente con el log Serial durante una subida real.

---

### SESIÓN 2026-07-30 — Default AutoMode cambia de OFF a READ (boot) + guard TRIM no saca de READ — SIN VALIDAR EN HARDWARE

**MCU afectadas:** S2 (mayoría) + S3 + P4 (default en caché).
**Origen:** petición explícita del usuario — arrancar en `AUTO_OFF` deja el fader sin lógica de automatización hasta que Logic mande el primer estado real; se prefiere asumir `READ` al boot (el modo más común de facto) y corregir si el DAW indica otra cosa.

| Archivo | Cambio |
|---|---|
| `S2/config.h` | `_rsCurrentMode` (caché de modo RS485) arranca en `AUTO_READ` en vez de `AUTO_OFF`. |
| `S2/display/Display.cpp` | `currentAutoMode` (usado por el display) arranca también en `AUTO_READ`, coherente con el cambio anterior. |
| `S2/RS485Handler.cpp::onMasterData()` | Nuevo guard: si `_rsCurrentMode == AUTO_READ` y llega `pktMode == AUTO_TRIM`, se reescribe `pktMode = AUTO_READ` antes del resto del flujo — el S2 ya no sale de READ por un TRIM entrante. Un único punto de reescritura, sin duplicar el filtro en otro sitio. No afecta si el S2 ya está en TOUCH/WRITE/LATCH (TRIM se acepta igual que antes). |
| `P4/MIDIProcessor.cpp`, `S3/MIDIProcessor.cpp` | `g_channelAutoMode[8]` (caché local de modo por canal) inicializado a `AUTO_READ` en los 8 canales, en vez de `{}` (0 = `AUTO_OFF`). |

**Nota:** el reset offline de P4 (`memset(g_channelAutoMode, 0, ...)` en desconexión USB-MIDI) sigue reseteando a `AUTO_OFF` — no se tocó, no se consideró parte de "boot".

**Pendiente:** sin validar en hardware — ver fila 🔴 Alta en Pendientes arriba. Documentación detallada: `docs/AUTOMODE.md` (actualización 2026-07-30).

**Rig de banco ampliado a producción — confirmado por el usuario (2026-07-30):**

| Archivo | Cambio | Estado |
|---|---|---|
| `S3/config.h` | `NUM_SLAVES` **1 → 8** | **Confirmado intencionado.** El usuario conectó físicamente los 8×S2 al bus B — se pasa del banco reducido (1 slave, sesión 2026-07-22) al rig completo de producción. |
| `S3/RS485.cpp::runTask()` | Logs de TIMEOUT `log_w`/`log_e` → `log_d` | Acompaña al cambio anterior: con 8 slaves reales en vez de 1, los timeouts ocasionales por ciclo son más frecuentes de forma rutinaria — se baja la verbosidad para no saturar el log serie. |

El usuario también conectó 9×S2 al bus A del P4. **Pinout real localizado (sin testear todavía):** A=GPIO26, B=GPIO27 — se descartó la sospecha de colisión con `LCD_RST_PIN` (también "27" en `config.h`, pero corresponde a un GPIO distinto del ESP32-P4; la coincidencia era solo de numeración de conector). El transceiver de esta placa es **auto-direccional** (sin pin DE/RE), a diferencia del bus B en S3. `P4/config.h` y `P4/RS485.cpp` (que aún usa `RS485_ENABLE_PIN` en 4 puntos) no se han tocado — ver fila 🔴 Alta "P4: pinout RS485 bus A localizado... SIN TESTEAR" en Pendientes arriba y `docs/RS485_P4.md` §1.2.

---

### SESIÓN 2026-07-26 — P4: USB/MIDI antes en el boot + fix build LVGL (driver SD arrastrado)

**MCU afectadas:** P4 únicamente.

| Archivo | Cambio |
|---|---|
| `P4/main.cpp::setup()` | Reordenado: `USB.begin()` + `MIDI.begin()` pasan a ser los pasos 1-2 (antes iban después de LittleFS y Display, como pasos 6-7). Evita perder el handshake inicial si Logic Pro ya está abierto cuando el P4 arranca — la ventana de varios cientos de ms que tardaban LittleFS+initDisplay+Preferences+UI Offline en completarse retrasaba el `USB.begin()`, igual que ocurría antes en S3. |
| `P4/remove_lvgl_asm.py` | Nuevo paso: elimina `lvgl/src/libs/fsdrv/lv_fs_arduino_sd.cpp` del árbol de LVGL tras la descarga de la librería. El LDF de PlatformIO detecta el `#include SD.h` por texto (sin evaluar el `#if LV_USE_FS_ARDUINO_SD`, en 0 en `lv_conf.h`) y arrastraba la librería `SD` del framework Arduino, cuyo `sd_diskio.cpp` requiere `ff.h` (FatFS de ESP-IDF) — no empaquetado para `esp32-p4` en este release, rompiendo el build. |

**Validación:** cambio de build/boot únicamente, sin tocar RS485/Motor — no requiere validación en banco S2/S3, solo confirmar que el P4 sigue compilando y arrancando (lo hace el usuario en su máquina, CLAUDE.md prohíbe compilar).

---

### SESIÓN 2026-07-24 20:16 — Diagnóstico (solo análisis estático, SIN cambios de código): fader S2 sube a máximo indefinidamente tras calibración correcta

**MCU afectadas:** S2 (análisis). Ningún archivo modificado — sesión puramente diagnóstica, a la espera de confirmación con logs reales antes de proponer fix (regla CLAUDE.md: explicar y validar antes de tocar código en Motor/RS485).
**Origen:** reporte del usuario — "en determinadas circunstancias después de calibración correcta con Logic abierto el fader va a máximo y se queda subiendo indefinidamente".

**Hipótesis principal (alta confianza, pendiente confirmar con log en vivo):**

Bucle infinito empuje→STALL→cooldown→reintento en modo DAW-absoluto (`AUTO_OFF`/`AUTO_READ`):

1. `marginTop = 20` cuentas fijo (`Motor.cpp:262`) → `_calibratedFaderMax = _motor_adcTop - marginTop` (`Motor.cpp:272`). `_motor_adcTop` sale del ruido máximo medido en 200ms de `SETTLE_UP` — puede quedar optimista.
2. Si el track en Logic tiene el fader en/cerca del máximo (PB≈16383) y el AutoMode es OFF/READ, `RS485Handler::_applyFaderTarget()` llama `Motor::setTargetForced(target)` en cada paquete, sin guard de usuario (`RS485Handler.cpp:83`).
3. `setTargetForced()` no tiene límite de reintentos (a diferencia de `requestCalibration()`, que sí tiene `CALIB_MAX_RETRIES`). Si el motor no llega al target exacto (fricción real cerca del tope físico > margen de 20 cuentas), se dispara la protección STALL (`Motor.cpp:439-457`): apaga motor + cooldown de 2s (`STALL_COOLDOWN_MS`).
4. Al expirar el cooldown, si Logic sigue mandando el mismo target de máximo, el motor vuelve a empujar → vuelve a stallar → vuelve a esperar 2s → indefinidamente mientras la condición se mantenga. Percibido por el usuario como "sube y no para".

**Hallazgo secundario (bug real de unidades, probablemente inerte con la máquina de estados actual):**

`RS485Handler.cpp:186` y `:378` llaman `Motor::setTarget(Motor::getRawADC())` al desconectar. `Motor::setTarget(uint16_t midiPB14)` (`Motor.h:34`) espera PitchBend 0-16383, pero recibe ADC crudo (20-27000). Corrompe `_motor_lastMidiTarget`, consumido solo en `Motor.cpp:274-275` al completar calibración (`map()` sin clamp → puede extrapolar más allá de `_calibratedFaderMax`). Actualmente inerte porque el estado pasa a `IDLE` tras calibrar, que no persigue `_motor_targetADC` — pero viola el contrato de la función y debería corregirse.

**Pendiente antes de proponer fix:** confirmar con el usuario (a) si aparecen en el log serie del S2 los mensajes `[MOTOR] STALL — tope físico, motor apagado` y `[MOTOR] setTargetForced: cooldown STALL activo` repitiéndose, y (b) en qué AutoMode estaba el track (OFF/READ/WRITE/TOUCH/LATCH) cuando ocurrió. Sin esa confirmación, no se toca código (RS485/Motor requieren validación en hardware, CLAUDE.md).

---

### SESIÓN 2026-07-22 10:12 — S2 dueño único de calibración y mapeo PB↔ADC; S3 pasa a transparente — VALIDACIÓN PARCIAL EN BANCO (V1/V2), V3-V8 pendientes

**MCU afectadas:** S2 (mayoría) + S3. P4 explícitamente excluido — queda incompatible con este firmware S2 hasta portarlo (ver Pendientes).
**Origen:** implementa la propuesta de arquitectura de la sesión de chat 2026-07-17, confirmada el 2026-07-20 (ver nota en la sesión de abajo) y ejecutada hoy vía informe de ejecución determinista por fases.
**Estado:** código de las 6 fases aplicado y compilado limpio en S2 y S3 (compilación la ejecutó el usuario en su máquina — CLAUDE.md prohíbe que Code compile). Validación en banco arrancó con V1/V2 (boot + autocalibración + aplicar último PB) y encontró 3 bugs reales adicionales, todos corregidos y confirmados en banco (ver abajo). V3-V8 (READ/WRITE/LATCH, desconexión, 10× recalibración) siguen pendientes.

| Fase | Archivos | Cambio |
|---|---|---|
| 1 | `protocol.h` (S2+S3) | `faderTarget`/`faderPos` redefinidos semánticamente como PitchBend 0-16383 de extremo a extremo (antes: ADC 0-27000 gestionado por S3). `SLAVE_FLAG_CALIB_SENDING`/`IS_MIN` marcados OBSOLETO (bits no reciclados). |
| 2 | `S2/config.h` | Nuevas constantes: `CALIB_MIN_SPAN` (18000, provisional), `CALIB_MAX_RETRIES` (3), `FADER_EMA_ALPHA` (0.15f), `CALIB_DATA_TIMEOUT_MS` (200, sin consumidor aún). |
| 3 | `S2/Motor.h/.cpp` | `setDawAbsolute()`: en AUTO_OFF/READ el usuario ya no puede fijar posición vía `setADCDelta()` (antes solo `setTargetForced()` desde el handler lo corregía, con hueco de ~10ms). Cooldown STALL ahora cancelable también cuando el usuario resiste sin generar delta ADC (`_dawAbsolute`). Fix desconexión: `_floorADC()` + latch `_goToMinRestADC` — el fader real descansa ~115-135 cuentas, no 0, así que los umbrales fijos contra `MOTOR_ADC_MIN` nunca se cumplían por threshold, solo por stall → bucle de pulsos. `setADC()` satura en vez de rechazar (rechazar congelaba `_motor_adcPos` y fabricaba topes falsos en calibración). Calibración amputada: valida `_tentativeSpan >= CALIB_MIN_SPAN` antes de aceptar, reintenta localmente hasta `CALIB_MAX_RETRIES`. |
| 4 | `S2/FaderADC.cpp` | Mismo principio de saturar-no-descartar en la lectura ADC cruda. |
| 5 | `S2/RS485Handler.cpp`, `S2/main.cpp` | Nuevo `_pbToADC()` mapea PB→ADC localmente con el rango calibrado propio. `buildResponse()` reporta `faderPos` ya en PB (EMA propio, sin filtro en S3). Autocalibración de boot diferida a `loop()` (ver fix 5D.3 abajo — la primera versión en `setup()` tenía una condición de carrera real). |
| 6 | `S3/RS485.cpp/.h`, `S3/main.cpp` | `setFaderTarget()`/`_handleResponse()` pasan a passthrough puro de PB, sin remapeo. Eliminados: `_recomputeFaderTarget()`, cascada de auto-calibración del slave 1, toda la máquina `calibDone`/`calibError`/reintentos/`_triggerNextCalibration()` en `_handleResponse()` (sustituida por lectura pasiva de `CALIB_DONE`/`CALIB_ERROR`), rama de calibración del bloque TIMEOUT en `runTask()`, y campos huérfanos de `ChannelData` (`calibrating`, `calibRetries`, `stableRespCount`, `lastRawPitchBend`, `calibratedMin`, `calibratedMax`, `_filteredFaderPos[]`). `calibrated` se conserva (lo usa la lectura pasiva). `processSlaveResponse()` en `main.cpp` (no contemplado en el plan original) hacía el remapeo ADC→PB con `calibratedMin/Max` — corregido a passthrough directo, sin lo cual el sistema habría quedado roto pese a que ambos MCU "compilaran". |

**Fix 5D.3 (bloqueante encontrado antes de flashear):** la autocalibración de boot se disparaba en `setup()` llamando `Motor::requestCalibration()`, pero `_motor_adcPos` aún vale su inicial (0) en ese punto — el S2 creía el fader en el fondo estuviera donde estuviera, y saltaba a `startCalib()` sin bajar antes. Calibración desde premisa falsa, intermitente según posición real del fader al arrancar. Movido a `loop()`, disparo diferido tras 10 iteraciones con `Motor::getRawADC() > 0` como proxy de "ya hay lectura real" (`FaderADC` no expone `hasNewReading()` públicamente).

**`MOTOR_SETTLE_THRESHOLD` (S3 `config.h`):** quedó descuadrado por el cambio de escala ADC→PB — reescalado de `80` a `60` (ratio ~0,744 = 16383/22000 del span útil anterior). Marcado `// TODO BANCO: reescalado ADC→PB, verificar en V3` — es un valor de arranque, no confirmado.

**Bugs encontrados y corregidos durante la validación en banco (V1/V2), todos confirmados con log real:**

| Fix | Archivo | Detalle |
|---|---|---|
| `requestCalibration()` perdía `_pendingCalib` | `S2/Motor.cpp` | El guard de la rama "fader ≠ 0" comprobaba `_motor_state != GOING_TO_MIN` — pero en boot, sin S3 conectado, el motor ya entra en `GOING_TO_MIN` por el mecanismo normal de "sin conexión, ir a 0" *antes* de que el disparo de autocalibración (5D.3) llegue. El guard veía el motor ya en ese estado (por otro motivo) y nunca armaba `_pendingCalib` — la calibración de boot se perdía silenciosamente en casi todos los arranques. Cambiado el guard a `if (!_pendingCalib)`, idempotente igual pero sin depender del motivo por el que ya estuviera bajando. |
| Asimetría de PWM en `GOING_DOWN` — fondo real no alcanzado | `S2/Motor.cpp` (`_calibUpdate()`) | `GOING_UP` usa el mismo umbral (26000) para decidir el PWM objetivo y para aplicarlo al hardware. `GOING_DOWN` tenía un umbral de decisión (200) que no coincidía con el de aplicación real (1000) — el motor bajaba a `_pwm_min` (40 en este banco) desde 1000 cuentas de distancia, mucho antes de acercarse al fondo, y se quedaba corto por falta de fuerza (`MIN` calibrado pasó de `309` a `51` tras el fix, con el usuario confirmando a mano que el fondo físico real está en `~35`). Corregido el umbral de aplicación de `1000` a `200`, simétrico con `GOING_UP`. |
| Bucle STALL→cooldown→reintento parcial persiguiendo un target lejano | `S2/Motor.cpp` (`_positionTick()`), `S2/config.h` | El PWM en `_positionTick()` era proporcional al error de punta a punta y nunca llegaba a `PWM_MAX` salvo que el target estuviera en el extremo opuesto exacto — con errores grandes pero no máximos (~40-75% del span) el PWM resultante (125-145) no bastaba para vencer fricción localizada cerca de los extremos, y el fader avanzaba ~100 cuentas por ciclo de 2s antes de repetir STALL. Nueva constante `POSITION_CRUISE_ERR=2000` (provisional): por encima de ese error se usa `PWM_MAX` fijo ("crucero", igual que hace la calibración en `GOING_UP`/`GOING_DOWN`), por debajo se mantiene el proporcional actual (frenado fino, sin cambios). |
| Falso touch al llegar a `AT_TARGET` con movimiento desde Logic (efecto colateral del fix anterior) | `S2/Motor.cpp` (`setADCDelta()`), `S2/config.h` | El crucero a `PWM_MAX` da al motor más inercia al llegar al target — el overshoot/asentamiento mecánico puede superar `MANUAL_TOUCH_AT_TARGET_THRESHOLD=30` (umbral muy sensible, pensado para acercamiento suave) justo al cruzar de `MOVING_TO_TARGET` a `AT_TARGET`, momento en que la supresión por "el motor se mueve en la dirección del target" deja de aplicar. Síntoma observado en banco: log `[MOTOR] Usuario soltó fader` sin que nadie tocara el fader, al mover el target desde Logic. Nueva constante `AT_TARGET_TOUCH_GRACE_MS=200`: ignora la detección de touch durante ese periodo tras entrar en `AT_TARGET` (reutiliza el timestamp `_atTargetStartTime`, ya existente mas sin uso previo). |

**Nota (no fix, solo detectado):** el log `[S2-RESP] touchState=1 faderPos=%d mode=%d` en `RS485Handler.cpp::buildResponse()` es engañoso — el string dice `faderPos` pero el argumento real pasado es `Motor::getRawADC()` (ADC crudo), no `resp.faderPos` (que sí lleva el valor en PitchBend ya mapeado, el que realmente sale por RS485). Preexistente, no corregido en esta sesión — solo documentado para no confundir en futuras lecturas de logs.

**Deuda pendiente explícita (no ejecutar sin sesión propia):**
- P4 queda incompatible con este firmware S2 — interpretará `faderPos` como ADC crudo. Portar Fase 6 al P4 en sesión dedicada.
- Fader master ID 9 / canal 9: fuera de scope, sin tocar.
- Sketch ADS1115 (fondo de escala real con GAIN_ONE): pendiente medir — `MOTOR_ADC_MAX=27000` y `CALIB_MIN_SPAN=18000` siguen siendo provisionales.
- `delayMicroseconds(20)` en timing RS485: pendiente, no incluido en esta sesión.
- Documentación técnica (`docs/AUTOMODE.md`, `docs/RS485.md`, `docs/FADER.md`, `CLAUDE.md`) deliberadamente diferida hasta después de la validación en banco (V1-V8) — evita reescribirla dos veces si la validación obliga a tocar algo.

**Próximo paso:** continuar validación en banco S3+1×S2 — V3-V8 (READ absoluto con push manual, soltar y retomar, WRITE sin mover motor, desconexión de Logic, 10× recalibración).

---

### SESIÓN 2026-07-20 — Diagnóstico fader S2 no sigue a Logic (banco S3 + 1×S2) — NO RESUELTO, ver Pendientes

**MCU afectadas:** S2 (mayoría) + S3 (2 cambios). P4 explícitamente excluido de todos los fixes (decisión del usuario: banco de pruebas actual es solo S3+S2).
**Estado:** todos los fixes de abajo están aplicados en código, **ninguno confirmado como solución definitiva en hardware** — la sesión se cerró en limpieza de comentarios/docs, no en test end-to-end exitoso.

| Fix | Archivo(s) | Detalle |
|---|---|---|
| Cooldown STALL no cancela al soltar | `S2/Motor.cpp`, `config.h` | `_stallCooldownFromTouch` — si el STALL se originó con el usuario tocando el fader (típico en AUTO_READ, motor resistiendo la mano), cancela el cooldown de 2s en cuanto suelta, en vez de esperar el resto del tiempo a ciegas. |
| Recalibración fantasma al cerrar Logic | `S2/Motor.h/.cpp`, `RS485Handler.cpp` | Nuevo getter `Motor::isPendingCalib()`. El guard `pendingNewCalib` de `buildResponse()` usaba `GOING_TO_MIN` como proxy de "recalibración pedida" — pero `goToMin()` también se dispara por desconexión de Logic (sin relación con calibrar). S3 interpretaba la ausencia de `CALIB_DONE` como "el slave se reinició" y disparaba recalibración real sin motivo. |
| `Motor::off()` no reseteaba `_motor_state` | `S2/Motor.cpp` | Si la desconexión llegaba con el motor en `MOVING_TO_TARGET`, el siguiente `update()` reenganchaba `_positionTick()` persiguiendo el último target de Logic en vez de bajar a 0. Ahora `off()` fuerza `_motor_state = IDLE`. |
| goToMin automático pelea con el touch del usuario | `S2/Motor.cpp` | Los checks de `IDLE`/`AT_TARGET` que re-disparan `GOING_TO_MIN` cuando `!_connected` no miraban `_motor_manualTouchDetected` — con Logic desconectado, tocar el fader entraba en tira y afloja con el motor. Añadido el guard en ambos. |
| AutoMode por SysEx 0x0E no llegaba al slave | `S3/MIDIProcessor.cpp` | `case 0x0E` actualizaba `g_channelAutoMode[]` (caché local) pero nunca llamaba a `rs485.setAutoMode()` — a diferencia del camino de notas 74-78, que sí lo hace. Si Logic sincroniza el modo por este SysEx en vez de por notas, el slave se quedaba con el último modo que sí llegó por notas (reportado: atascado en `WRITE`). |
| Target tras calibración usaba mapeo teórico | `S3/RS485.cpp`, `RS485.h` | Nuevo campo `lastRawPitchBend` + helper `_recomputeFaderTarget()`. Al completar calibración, se recalcula el target con el rango recién calibrado en vez de esperar el próximo PitchBend real de Logic (que podía no llegar si el track estaba parado). |
| **`LOGIC_PITCHBEND_MAX` incorrecto (14845 en vez de 16383)** | `S3/config.h` | Confirmado con captura MIDI Monitor en vivo: Logic manda el rango 14-bit completo (0-16383), no 0-14845 como se asumía desde 2026-05-18. Con el valor viejo, targets cerca del tope del fader se calculaban más allá de `calibratedMax` — motor persiguiendo una posición físicamente inalcanzable, STALLs repetidos cerca del tope. Único punto de cambio, 4 usos heredan el valor corregido. Limpieza acompañante: todos los comentarios `14848`/`14845` en `S2/S3/docs/CLAUDE.md` actualizados a `16383` (código funcional no usaba el literal, solo comentarios y docs). |

**Pendiente para la próxima sesión:** ver fila 🔴 Alta en Pendientes arriba — repetir test end-to-end con Logic conectado, modo READ, target lejos de la posición actual, sin tocar el fader, y confirmar convergencia limpia sin STALL.

**Informe de arquitectura recibido (no ejecutado):** sesión de chat de arquitectura (2026-07-17) propuso mover el mapeo PB↔ADC completo al S2 (S3 deja de guardar `calibratedMin/Max` por canal) — decisión confirmada por el usuario en esta sesión pero **implementación no iniciada**, solo planificada (filtro EMA se movería a S2 sobre ADC nativo). Pendiente para sesión de arquitectura dedicada, junto con el enrutamiento del fader master (ID 9 / canal 9, notas de touch 104-112 nunca implementadas en ningún master).

---

### SESIÓN 2026-07-14 — ExPressif (P4_JC4880P433C): sistema de memorias Kaos configurables (20 slots NVS, canal por synth, editor en pantalla) + color por synth extendido + brillo pantalla + metrónomo visual + BPM

**Commit:** ver commit de esta sesión.
**MCU afectada:** solo P4_JC4880P433C (ExPressif).
**Origen:** `aitec_kaos_brief_2026-07-13.md` (catálogo verificado por synth, modelo NVS §2) + una serie larga de decisiones y correcciones del usuario en vivo, muchas mientras probaba en hardware real entre cambios (ver "Iteración" y "Fixes en vivo" abajo).

#### Sistema de memorias Kaos

| Cambio | Detalle |
|---|---|
| **Catálogo de parámetros nombrados** — `kaoss/KaosParams.h/.cpp` (nuevo) | Lista fija en flash, por synth, de `{cc, nombre}` **individuales** (Cutoff, Resonance, Tone1 Lvl, LFO1 Speed...) — no memorias pre-empaquetadas. Decisión explícita del usuario ("PARAMETROS COMPLETAMENTE CONFIGURABLES OPCION B"): el usuario compone la pareja X/Y libremente desde el editor. JV-2080 (6 parámetros), TRITON (6), MOTIF (2), WAVE (8, sin cambios respecto al array previo), TG55/D-110 (0, sin catálogo verificado). |
| **Almacén NVS** — `nvs/KaosStore.h/.cpp` (nuevo) | `kaos_slot[synth_id][slot 0-19] = {ccX, ccY, configured}`, namespace `"kaos"`, mismo patrón de bytes crudos que `FavStore.cpp`. Canal MIDI **separado**: `kaos_channel[synth_id] = ch` (clave `"c<synth>"`), un valor por synth — ver corrección de arquitectura abajo. `kaosInit()` siembra el catálogo de parámetros una sola vez (flag `"seeded"`) — después es editable sin reflashear. |
| **`KaossPad` reescrito** — `kaoss/KaossPad.h/.cpp` | Sin catálogos `const` en flash (se probaron y descartaron dos diseños intermedios, ver "Iteración" abajo) — cachea en RAM el slot activo (`_current`, recarga en `setPreset()`) y el canal del synth activo (`_channel`, recarga en `syncToSynth()`/`reload()`, **no** en `setPreset()`). |
| **NeoTrellis — 20 teclas de selección directa** — `neotrellis/NeoTrellis.cpp` | Sustituye el sistema de 2026-06-29 (SCALE cicla en L2 + 4 presets directos R0-R3). Mapeo: `L3,L7,L11,L15` (panel izq.) + `R0-R15` completo (panel der.) = 20 memorias, `preset = fila×5 + (0 si Lx, 1+col si Rx)`. Selección siempre directa, sin ciclar. |
| **Editor en pantalla** — `display/UIKaosEdit.h/.cpp` (nuevo) | Botón PRESET (antes SCALE) abre un overlay a pantalla completa: dos listas ("EJE X"/"EJE Y") de parámetros nombrados — tocar uno asigna ese eje — más canal MIDI (+/−, 1-16, justo debajo del título) y GUARDAR/CANCELAR. **Al Guardar, el canal se aplica a los 20 slots del synth**, no solo al slot que se estaba editando (es una propiedad del synth). TG55/D-110 (sin catálogo) muestran "Sin parámetros verificados" sin botón Guardar. |

#### Corrección de arquitectura en vivo — canal MIDI por synth, no por slot

El diseño inicial (implementado, documentado y luego corregido en la misma
sesión) guardaba el canal MIDI **dentro de cada slot** (`KaosSlot.ch`, un
valor de 1-16 por cada una de las 20 memorias). El usuario corrigió:
*"el canal midi es unico para el sinte, no por preset"* — cada sintetizador
físico del rack escucha en un canal fijo, independiente de qué memoria Kaos
esté activa. Refactor: `KaosSlot` pasa de `{ccX,ccY,ch}` a `{ccX,ccY,configured}`
(bytes bloqueados por la falta del campo `ch`); nueva pareja
`kaosLoadChannel()`/`kaosSaveChannel()` en `KaosStore` con clave propia por
synth; `KaossPad::getChannel()` deja de leer el slot y pasa a cachear
`_channel` recargado solo al cambiar de synth. El editor mueve el control de
canal de la franja inferior (pensada para "controles del slot") a justo
debajo del título (pensado para "controles del synth") — pedido explícito:
*"necesito ver el canal midi justo debajo del nombre del sinte"*.

#### Color por synth extendido a toda la UI

| Elemento | Antes | Ahora |
|---|---|---|
| 20 teclas de preset (NeoTrellis) | `modeColor()` (rojo KAOSS_XY fijo) — `modeColor()` eliminado de `NeoTrellis.cpp`, sin uso | `synthColor()` |
| Rejilla 8×8 de dots + scroll "ExPressive" (`UIKaoss.cpp`) | `mode_color()` (mismo rojo fijo) | `synth_color_rgb()` |
| Franja del botón PRESET | Verde fijo `0x00AA44`, sin cambiar ni en reposo | `synthColor()` con `darken()` (20%) en reposo |
| Franja blanca con nombre de synth (`UIBank.cpp`) | Blanco fijo `0xFFFFFF` | `synthColorHex()` (duplicado local, mismo patrón que `synthLabelText()`) |

Fix en vivo — reportado por el usuario: *"la seleccion por boton del sinte
DEBE cambiar el color de los [20] botones"*. `neotrellisUpdate()` solo
refrescaba el pixel del botón SYNTH + las teclas de selección de synth al
detectar `g_currentSynth` cambiado — las 20 teclas de preset (ahora coloreadas
por synth) se quedaban con el color del synth ANTERIOR hasta el siguiente
cambio de preset, sea cual sea la vía que cambió el synth (botón táctil,
cicla NeoTrellis o selección directa). Fix: esa rama pasa a llamar
`refreshLeft()`/`refreshRight()` completos en vez de actualizar pixels sueltos.

#### Brillo de pantalla — L2/L6

Las teclas NeoTrellis sin función tras retirar SCALE (L2) y sin usar desde el
principio (L6) controlan `displaySetBrightness()` en pasos de 10% (10-100%,
`g_displayBrightness`, solo RAM). Nuevo `display/UIBrightnessPopup.h/.cpp` —
overlay independiente, topmost, muestra el número y se oculta sola tras
~1.2s. **L2 = brillo +, L6 = brillo −** (invertido una vez respecto a la
primera implementación, corrección explícita del usuario).

#### Metrónomo visual — L5

L5 (última tecla sin función) parpadea sincronizado al **MIDI Clock entrante
por USB** (24 PPQN, realtime `0xF8`/`0xFA`/`0xFB`/`0xFC`) — confirmado con una
captura real de MIDI Monitor mostrando Logic mandando Clock a "ExPressif V1".
No hay BPM propio en el firmware. Nuevo `midi/MIDIClock.h/.cpp` — **primer y
único punto del firmware que lee MIDI entrante** (`MIDI.readPacket()`); hasta
ahora el proyecto solo enviaba MIDI. Sondeado en `taskCore0` cada ~20ms.
`NeoTrellis.cpp::metronomeUpdate()` consume el beat y decae en ~80ms (mismo
patrón que `s_glow_t` del pad XY).

**Fix en vivo — L5 no parpadeaba:** `midiClockPoll()` filtraba paquetes por
CIN (Code Index Number) del header USB-MIDI, esperando `0xF` ("Single Byte",
la convención estándar para mensajes realtime). El usuario reportó "L5 no se
ilumina" tras probarlo — distintos hosts/drivers USB-MIDI son inconsistentes
históricamente empaquetando Clock/Start/Stop (algunos usan CIN `0x5`). Fix:
se quitó el filtro por CIN — se compara `pkt.byte1` directamente contra
`0xF8`/`0xFA`/`0xFB`/`0xFC`, seguro porque un byte de datos MIDI nunca vale
≥0x80.

#### Fixes en vivo — geometría `UIKaosEdit.cpp` (reportados por el usuario tras compilar)

| Reporte | Causa | Fix |
|---|---|---|
| "CANAL SALE A LA IZQUIERDA Y MUY PEQUENO" | `s_lbl_ch` era una etiqueta suelta sin caja ni `set_size()` (font 18pt) | Caja del ancho de los botones −/+, fondo de color acento, texto centrado a 24pt |
| "tanto el canal como guardar y cerrar estan muy pegados abajo" | **Error de ejes**: toda la franja inferior usaba `x=0` fijo variando solo `y` — en este layout rotado (`screen_x=LVGL_y`, `screen_y=479−LVGL_x`) eso apila TODO en el mismo borde físico, solo separado en horizontal | Layout rehecho verificando cada posición contra el botón cerrar de `UIBank.cpp` (ya probado en hardware) antes de escribir números — título arriba-izq., canal en fila horizontal debajo, EJE X/EJE Y por debajo, Guardar/Cancelar en el borde derecho bajo Cerrar |
| "L2 debe subir el brillo y L6 bajarlo" | Se implementó al revés (L2=−, L6=+) | Invertido — `L2`=brillo +, `L6`=brillo − |
| "la seleccion por boton del sinte DEBE cambiar el color de los [20] botones" | `neotrellisUpdate()` solo refrescaba el pixel del botón SYNTH + teclas de selección al detectar cambio de `g_currentSynth` — las 20 teclas de preset (color por synth desde esta sesión) se quedaban con el color del synth ANTERIOR hasta el siguiente cambio de preset | Esa rama pasa a llamar `refreshLeft()`/`refreshRight()` completos, no pixels sueltos |
| "L5 no se ilumina" (metrónomo) | `midiClockPoll()` filtraba paquetes por CIN (`0xF`, "Single Byte") — distintos hosts/drivers USB-MIDI empaquetan Clock/Start/Stop con CIN inconsistente (algunos usan `0x5`) | Se quitó el filtro por CIN — se compara `pkt.byte1` directamente contra `0xF8`/`0xFA`/`0xFB`/`0xFC` (un byte de datos MIDI nunca vale ≥0x80, seguro sin mirar el CIN). De paso: `s_running` pasa a activarse con el primer `Clock` recibido, no solo con `Start`/`Continue` (evita depender de haber capturado ese mensaje concreto) |
| "ahi no puede ir. ponlo encima de hold... no se ve es muy pequeno" (BPM) | Readout hijo de `s_pad`, quedaba pegado al borde del pad (no al borde real de pantalla) con fuente 14pt | Overlay independiente (`s_box_bpm`, hijo de `parent`), esquina física superior-izquierda real, encima de HOLD, fuente 24pt |
| "y debe ser persistente" (BPM) | `midiClockGetBPM()` se reseteaba a 0 en `Stop` | Ya no se resetea — mantiene el último tempo conocido hasta medir un intervalo nuevo |

**BPM** (`midiClockGetBPM()`, `midi/MIDIClock.h/.cpp`) — calculado por el
intervalo real entre negras consecutivas (`60000/ms`), persistente tras
`Stop`. Mostrado en pantalla (`UIKaoss.cpp`, overlay `s_box_bpm`/`s_lbl_bpm`)
encima del botón HOLD, refrescado cada 50ms desde `decay_timer_cb`.

**Discrepancias detectadas entre el brief y el estado real del código, resueltas en conversación antes de implementar:**

| Punto del brief | Estado real encontrado | Resolución |
|---|---|---|
| §3: canal MIDI derivado del modo Bank (JV-2080 Patch=12/Performance=1) | `UIBank.cpp:131` — revertido el 2026-07-12, canal fijo=1, "Logic enruta por track, no el firmware" (commit `6fb4ffe`) | No aplica al Kaos pad — el canal ahora es por synth (ver corrección de arquitectura arriba), independiente del canal de selección de patch en Bank |
| §3: `value_mode` debía "corregir" el mapeo CC de `mapXtoCC`/`mapYtoCC` | Un CC MIDI siempre es un byte 0-127 — no hay fórmula distinta posible entre ABSOLUTE y RELATIVE_OFFSET_64 al nivel de bytes | Usuario confirmó: solo metadato, sin cambio de fórmula — irrelevante para el diseño final (Opción B eliminó el campo `value_mode` por completo) |
| §4: "20 memorias" mencionado por el usuario, brief solo documenta hasta 10 por synth | El brief nunca habla de 20 memorias ni de mapeo NeoTrellis L3-R15 | Información nueva de esta sesión — arquitectura de selección física (20 teclas) y modelo "parámetros sueltos" (Opción B) decididos en vivo, brief §2/§4 quedan como referencia del catálogo verificado, no del mecanismo de selección |

**Iteración de diseño dentro de la misma sesión (documentado porque cada paso quedó implementado y luego descartado — útil para no repetir el camino):**
1. Catálogo `const CCPreset[]` por synth en flash, 3-4 memorias fijas (fiel al brief, sin NVS) — descartado al pedir el usuario 20 memorias.
2. Catálogo `const CCPreset[20]` con slots vacíos, aún en flash — descartado al pedir el usuario edición desde pantalla (necesita NVS).
3. NVS + catálogo de parámetros sueltos + editor táctil, **canal por slot** — descartado al corregir el usuario que el canal es por synth.
4. **Diseño final:** NVS + parámetros sueltos + canal por synth + editor táctil, documentado arriba.

---

### SESIÓN 2026-07-04 — ExPressif (P4_JC4880P433C): rediseño Bank (grid 2×4 + tríos NeoTrellis), fix favoritos, Triton (JV-2080+Triton multi-synth)

**Commit:** ver commit de esta sesión.
**MCU afectada:** solo P4_JC4880P433C (ExPressif).

| Cambio | Detalle |
|---|---|
| Rediseño UIBank — grid unificado 2 cols × 4 filas | Objetivo: aligerar la interfaz para touch operativo (menos widgets vivos por página que el diseño anterior de 16/10). Las 3 tabs (Favoritos/Sonidos/Performances) comparten geometría. Se elimina `lv_tileview` — paginación explícita, solo por NeoTrellis (sin gesto de swipe). |
| Tab "Canal MIDI" eliminada | Performances ocupa su hueco físico (mismo orden Favoritos→Sonidos→Performances). El canal se sigue derivando automáticamente del modo/synth activo, no hay selector manual. |
| Construcción diferida por tab | `uiBankCreate()` ya no construye contenido — cada tab (`ensure_tab_built()`) se construye la primera vez que se activa, evitando bloquear Kaoss construyendo las 3 tabs de golpe en boot (causa raíz del lag táctil original). |
| NeoTrellis — mapeo por tríos | Sustituye el diseño por filas de 2026-07-01 (nunca implementado). Columna 0 (L0,L4,L8,L12) = página anterior, columna 7 (R3,R7,R11,R15) = página siguiente. Cada ítem = trío de 3 teclas contiguas (L1-3/R0-2 por fila) = 8 ítems/página, con LEDs sincronizados (azul=seleccionado, naranja=favorito). `k==4` (SYNTH) conserva long-press=abre/cierra Bank siempre; tap corto = cicla synth (Bank cerrado) o página anterior (Bank abierto). |
| Fix bug — favoritos no aparecían en pantalla | `favDelete()` nunca decrementaba el contador ni compactaba huecos (`FavStore.cpp`) — ciclos de prueba marcar/desmarcar repetidos dejaban huecos permanentes en slots bajos y subían el contador, empujando favoritos nuevos a páginas cada vez más lejanas. Fix: nueva `favFirstFreeSlot()` reutiliza el primer hueco libre antes de extender al final. |
| Fix cosmético — punto de favorito | Círculo naranja en Sonidos/Performances quedaba pegado al borde (margen ~3px) y pequeño (12px). Ahora 20px de diámetro, margen 26px desde el borde al centro (`UIBANK_FAV_DOT`/`UIBANK_FAV_DOT_MARGIN`, `config.h`). |
| Tipografía Favoritos unificada con Sonidos/Performances | Combo único centrado `montserrat_18` "NN Nombre" (antes dos labels separados en `montserrat_10`/`montserrat_14`). |
| Fix orientación grid — se cargaba de abajo a arriba | Al reescribir el grid se perdió la inversión de eje que compensa la rotación física (`screen_y = 479 − LVGL_x`). Corregido en las 3 tabs (`lvglRow = (UIBANK_GRID_ROWS-1) - row`). |
| **Korg Triton (Rack) — soporte nuevo** | `docs/Korg_Triton_Patches_Verificado.md` + `midi/TritonPatches.h/.cpp`: Bank Select verificado (`TRITON_Rack_MIDIimp.TXT` Rev 1.6) + 1152 nombres de Program/Combination (`TritonR_VNL_EFGJ1.pdf`, extracción automática validada por conteo exhaustivo 128/128 sin huecos en las 9 tablas). Bancos: INT-A/B/C/D (Program+Combination, comparten MSB/LSB) + Bank G (GM, solo Program). Nueva `sendTritonMode()` — SysEx MODE CHANGE (`F0 42 3g 50 4E 00 mm F7`, Func 4E) para alternar Program/Combination, análogo a `sendSoundMode()` del JV-2080. Nuevo `sendSysEx()` genérico en `MIDIOut.cpp` (longitud arbitraria, no solo múltiplos de 3 como `sendSysEx12`). |
| **UIBank multi-synth** | `SoundTabCtx` deja de estar cableado al JV-2080 — nuevo `SynthSoundDesc` por cada `ExSynth` (`kSynthDesc[]`), seleccionado en vivo por `g_currentSynth`. Nueva `uiBankSynthChanged()` (llamada desde `main.cpp` al ciclar SYNTH con Bank abierto) repuebla banco por defecto/página/LEDs/favoritos del nuevo synth. TG55/D110/WAVE quedan con descriptor vacío (grid vacío, sin datos aún, no crashea). |
| **Favoritos filtra por synth activo** | Decisión explícita del usuario: cada synth ve solo sus propios favoritos. `fav_render_page()`/`uiBankNeoKey()` escanean y pagina sobre la lista ya filtrada por `g_currentSynth` (ya no hay correspondencia 1:1 slot NVS↔posición en pantalla; la numeración mostrada es la posición entre los favoritos de ese synth). |
| Canal MIDI por synth/modo | JV-2080 sigue forzando Patch=12/Performance=1 (`MIDI_CH_PATCH/PERFORM`, sin cambios). Triton no fuerza canal (`chProg=chCombi=0` en su descriptor) — decisión explícita del usuario, no bloqueante. |

---

### SESIÓN 2026-07-02 — ExPressif (P4_JC4880P433C): toggle Patch/Performance, canal por modo — BLOQUEADA

**Commit:** sin commitear (working tree).
**MCU afectada:** solo P4_JC4880P433C (ExPressif).
**Estado:** ⚠️ NO RESUELTO — el JV-2080 no cambia de modo, ver entrada 🔴 Alta en Pendientes arriba.

| Cambio | Detalle |
|---|---|
| Toggle Patch↔Performance con toque simple | Antes requería long-press sostenido en el tab "SON" (`LV_EVENT_LONG_PRESSED`) — el usuario reportó que no se detectaba de forma fiable. Cambiado a `LV_EVENT_CLICKED` en `UIBank.cpp` (`son_tab_long_press_cb`, registrado en `uiBankCreate()`). Efecto colateral aceptado: cualquier toque sobre el tab "SON" alterna el modo, incluida la navegación entrante desde otra pestaña. |
| Canal MIDI sincronizado al modo | Nueva función `son_sync_channel_to_mode()` — fija `g_midiChannel` a `MIDI_CH_PATCH=12` o `MIDI_CH_PERFORM=1` (nuevas constantes en `config.h`), persiste en NVS (`favSaveMidiChannel`) y refresca el label del tab "CH". Se llama tanto en el toggle manual como en `son_apply_recall()` (recall de favoritos). Petición del usuario: en Logic, Patch y Performance son tracks distintos con canales distintos. |
| Recall de favoritos usa canal del modo, no el histórico | `son_apply_recall()`: `sendBankPC()` final pasó de usar `e.ch` (canal guardado dentro del favorito al crearlo) a usar `g_midiChannel` (recién sincronizado al modo) — evita que el PC salga por un canal desincronizado del Sound Mode activo. |
| Device ID del JV-2080 nombrado explícitamente | `JV2080_DEVICE_ID=0x10` en `config.h`, usado en `sendSoundMode()` (`MIDIOut.cpp`) en vez de `0x10` hardcodeado inline en los arrays SysEx. Motivado por hallazgo: el JV-2080 ignora en silencio cualquier SysEx cuyo Device ID no coincida con el configurado en su panel (`SYSTEM→F3(MIDI)→Device ID`, rango 10H-1FH, fábrica=10H) — ver `project_jv2080_device_id` en memoria. Probado con el valor de fábrica (0x10): **no resolvió el problema**. |

---

### SESIÓN 2026-07-01 — ExPressif (P4_JC4880P433C): lazy-build, favoritos, canal persistente

**Commit:** `18a34ad`
**MCU afectada:** solo P4_JC4880P433C (ExPressif).

#### Tab Sonidos

| Cambio | Detalle |
|---|---|
| Combo "BANCO:NNN Nombre" | Una sola fila, `montserrat_18` (bajado desde 20 a petición del usuario). Fix del bug de 2 filas: `LV_LABEL_LONG_DOT` necesita **alto fijo** además de ancho fijo, si no se comporta como `LONG_WRAP`. |
| Orden de filas invertido | Los sonidos salían de abajo a arriba (`row = i % ROWS`). Causa: `screen_y = 479 − LVGL_x`, a más `LVGL_x` más arriba en pantalla. Fix: `row = (ROWS-1) - (i % ROWS)`. |
| Círculo naranja de favorito | Movido de arriba (offset eje corto) a la izquierda del texto (offset eje largo, primera vez que se usa ese eje en este layout rotado — **sin validar en hardware**). Texto reserva 20px (`UIBANK_SON_FAV_RESERVE`) para no solaparse. |
| Ciclo de 3 pulsaciones | 1ª pulsación = seleccionar/recall. Pulsación sobre el ya seleccionado: si no es favorito → lo guarda; si ya lo es → lo quita (`favFindIndex`+`favDelete`, sin reconstruir la página — el círculo se borra vía puntero guardado en `lv_obj_set_user_data`). |
| Lazy-build del tileview | Antes se construían las 13 páginas del banco de golpe (~260-390 widgets rotados vivos simultáneos) → tacto lento (motivo: cada `rot_label()` usa una ruta de render más cara, y el bucle de LVGL corre cada 10ms para toda la app). Ahora solo la página activa ±1 (`son_populate_page`/`son_depopulate_page`/`son_update_page_window`, enganchado a `LV_EVENT_SCROLL_END`). |

#### Tab Favoritos

Mismo patrón lazy-build (`fav_populate_page`/`fav_depopulate_page`/`fav_update_page_window`) y mismo fix de orden de filas (`row = (ROWS-1) - (i % ROWS)`) portados desde Sonidos — mismo `lv_tileview`, mismo bug de origen.

#### FavStore (`src/nvs/FavStore.cpp/h`)

| Función | Uso |
|---|---|
| `favMarkBank()` | Chequeo masivo — un solo escaneo NVS por reconstrucción de grid (evita 10-16 escaneos, uno por botón) |
| `favFindIndex()` | Chequeo puntual — localizar el slot NVS a borrar al quitar un favorito |
| `favSaveMidiChannel()` / `favLoadMidiChannel()` | Canal MIDI activo persiste entre reinicios (clave NVS `"mc"`, namespace `"favs"`) — antes siempre arrancaba en canal 1 |

#### README (`MASTER_S3-P4/P4_JC4880P433C/README.md`)

Nueva sección "NeoTrellis — Matriz 4×8": mapa físico de índices (L0-15/R0-15), mapeo modo Kaoss (ya vigente en código) y diseño del modo Bank (fila superior=subir, inferior=bajar, centro=selección directa de slot) — documentado como **diseño objetivo pendiente de implementar**, motivado porque las funciones Kaoss (HOLD/PANIC/SCALE/SYNTH/preset) seguían activas sobre las 32 teclas mientras `UIBank` estaba abierto, sin sentido en ese contexto.

---

### SESIÓN 2026-06-30 — ExPressif (P4_JC4880P433C): UIBank, JV-2080, NeoTrellis

**Commits:** `54419d3` → `37d693b` → `c8406f3`  
**MCU afectada:** solo P4_JC4880P433C (ExPressif).

#### Nuevos módulos

| Archivo | Descripción |
|---------|-------------|
| `src/display/UIBank.cpp/h` | Pantalla Bank — 3 pestañas: FAV / Sonidos / Canal MIDI |
| `src/midi/JVPatches.cpp/h` | Tabla Roland JV-2080 completa: 768 patches en flash (USER + PR-A..E + GM) |
| `src/neotrellis/NeoTrellis.cpp/h` | Driver 2× Adafruit seesaw 4×4 NeoTrellis (I2C SDA=31 SCL=33) |
| `src/nvs/FavStore.cpp/h` | Almacén NVS: favoritos MIDI (`FavEntry`) + última selección |
| `docs/Roland_JV-2080_Patches_Verificado.md` | Fuente verificada de los 768 nombres de patch |

#### UIBank — comportamiento

- **Abrir:** long-press en botón sintetizador (touchscreen btn\[1\] ≥400ms) o NeoTrellis k=4 ≥600ms
- **Tab 0 (FAV):** tileview 16 slots/página — tap para recall; slot activo resaltado azul
- **Tab 1 (Sonidos):** browser JV-2080 — 10 patches/página (5×2), montserrat\_16
  - Selector de banco inferior: USER / PR-A / PR-B / PR-C / GM / PR-E
  - **Tap** → envía CC0→CC32→PC al JV-2080; patch activo resaltado azul
  - **Long-press** → guarda patch como siguiente favorito en Tab 0; botón verde = confirmado
- **Tab 2 (Canal MIDI):** selector ∧ número ∨, rango 1-16
- **Botón ×** (esquina top-right física): cierra Bank

#### Memoria de selección

| Dato | Almacenamiento | Persiste |
|------|---------------|----------|
| Último patch seleccionado (banco + PC) | NVS — claves `lm`/`ll`/`lp` | Entre reinicios ✅ |
| Slot FAV activo | RAM | Solo sesión |

Al arrancar, UIBank restaura el banco y patch del último uso (la tab Sonidos abre con ese patch resaltado).

#### Cambios en archivos existentes

| Archivo | Cambio |
|---------|--------|
| `config.h` | `ExSynth` enum, `COL_SYNTH_*`, `g_trellis_openBank/bankSlot`, `g_midiChannel`, `TRELLIS_DIM_ABS` (renombrado), pines I2C NeoTrellis 31/33 |
| `UIKaoss.cpp` | Long-press btn\[1\] abre/cierra Bank; short-press cicla sintetizador activo |
| `MIDIOut.cpp/h` | `sendBankPC(ch, msb, lsb, pc)` — secuencia CC0→CC32→PC |
| `main.cpp` | `favInit()`, `uiBankCreate()`, flags cross-core `openBank`/`bankSlot` |
| `lv_conf.h` | `LV_USE_TABVIEW=1`, `LV_USE_TILEVIEW=1` (estaban a 0) |

#### Nota técnica — labels rotados en LVGL

`lv_obj_set_width(lbl, W)` en un label con `rot_label()` (transform\_rotation=900):  
**W = chars\_visibles × px\_por\_char** — el "ancho" LVGL se convierte en altura física tras rotar.  
Diseño validado: botón 82px LVGL x, width=76, montserrat\_16 → 7-8 chars legibles.

---

### SESIÓN 2026-06-14b — S2 calibración: fix CALIB_DONE prematuro al recalibrar

**Bug:** Cuando S3 se reiniciaba pero S2 seguía corriendo con `_motor_phase=DONE`, al recibir el siguiente FLAG_CALIB S2 entraba en `GOING_TO_MIN` (para recalibrar) pero seguía enviando `SLAVE_FLAG_CALIB_DONE` con los datos de la calibración anterior. S3 marcaba al slave como "calibrado" inmediatamente con `MIN=0 MAX=0` porque sus variables estaban a 0 tras el reinicio. Resultado: `_motor_adcSpan=0` en S2 → `_positionTick()` abortaba → motor no respondía a targets.

**Fix aplicado (2026-06-14):**

| Archivo | Cambio |
|---------|--------|
| `S2/S2_V1/src/RS485/RS485Handler.cpp` | `buildResponse()`: añadir `pendingNewCalib` flag — detecta `DONE + GOING_TO_MIN` → suprime `SLAVE_FLAG_CALIB_DONE` y resetea `_calib_send_state=0` hasta que la nueva calibración complete |

**MCU afectadas:** solo S2 (Slave).

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
- `2026-08-13 12:33` · Commit S2 · **FW 0.6.47** (sin upload)
- `2026-08-02 13:23` · Flash S2 · **FW 0.5.46** · `lolin_s2_mini`
- `2026-06-22 17:05` · Flash S2 · **FW 0.5.45** · `lolin_s2_mini`
- `2026-06-22 16:59` · Flash S2 · **FW 0.5.44** · `lolin_s2_mini`
- `2026-06-15 15:09` · Flash S2 · **FW 0.5.43** · `lolin_s2_mini`
- `2026-06-15 14:44` · Flash S2 · **FW 0.5.42** · `lolin_s2_mini`
- `2026-06-15 14:15` · Flash S2 · **FW 0.5.41** · `lolin_s2_mini`
- `2026-06-15 14:14` · Flash S2 · **FW 0.5.40** · `lolin_s2_mini`
- `2026-06-15 13:43` · Flash S2 · **FW 0.5.39** · `lolin_s2_mini`
- `2026-06-15 13:42` · Flash S2 · **FW 0.5.38** · `lolin_s2_mini`
- `2026-06-15 13:06` · Flash S2 · **FW 0.5.37** · `lolin_s2_mini`
- `2026-06-15 12:28` · Flash S2 · **FW 0.5.36** · `lolin_s2_mini`
- `2026-06-15 12:25` · Flash S2 · **FW 0.5.35** · `lolin_s2_mini`
- `2026-06-15 12:19` · Flash S2 · **FW 0.5.34** · `lolin_s2_mini`
- `2026-06-15 11:39` · Flash S2 · **FW 0.5.33** · `lolin_s2_mini`
- `2026-06-15 11:36` · Flash S2 · **FW 0.5.32** · `lolin_s2_mini`
- `2026-06-15 11:32` · Flash S2 · **FW 0.5.31** · `lolin_s2_mini`
- `2026-06-15 11:30` · Flash S2 · **FW 0.5.30** · `lolin_s2_mini`
- `2026-06-14 18:43` · Commit S2 · **FW 0.5.29** (sin upload)
- `2026-06-14 18:09` · Flash S2 · **FW 0.5.28** · `lolin_s2_mini`
- `2026-06-14 18:08` · Flash S2 · **FW 0.5.27** · `lolin_s2_mini`
- `2026-06-14 15:58` · Flash S2 · **FW 0.5.26** · `lolin_s2_mini`
- `2026-06-14 15:57` · Flash S2 · **FW 0.5.25** · `lolin_s2_mini`
- `2026-06-14 15:27` · Commit S2 · **FW 0.5.24** (sin upload)
- `2026-06-14 14:55` · Commit S2 · **FW 0.5.23** (sin upload)
- `2026-06-14 14:42` · Commit S2 · **FW 0.5.22** (sin upload)
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
