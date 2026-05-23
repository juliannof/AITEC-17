# iMakie — Contexto para Claude Code

<!-- 🔒 ⚠️ DESHABILITADO (2026-05-16 09:00): CLAUDE.md se sube a GitHub por ahora
- Contiene arquitectura sensible, pines GPIO, vulnerabilidades conocidas
- Si el repo es público → agregar a `.gitignore`
- Si el repo es privado → OK mantenerlo
- Comando: `echo "CLAUDE.md" >> .gitignore && git rm --cached CLAUDE.md`
-->

---

## ⚠️ DIRECTIVA OBLIGATORIA

**IDIOMA — Español de España SIEMPRE (2026-05-11 18:30):**
- Comunicación con el usuario: únicamente en español de España
- Tono: directo, sin rodeos, profesional
- Esta regla es VINCULANTE en CADA respuesta

**NUNCA modificar código sin explicar PRIMERO con detalle qué vas a hacer.**

- Antes de tocar cualquier archivo: describe el cambio (qué líneas, por qué, qué efecto tiene)
- Espera aprobación explícita del usuario
- Si el usuario dice "hazlo" o "proceed", ENTONCES ejecuta los cambios
- No importa si es un cambio "pequeño" o "obvio" — SIEMPRE explica primero
- Esta regla es VINCULANTE en cada nuevo chat

**Documentación — Fecha y hora obligatorios (2026-05-04 19:20):**
- Cuando documentes cambios en CLAUDE.md o en comentarios de código: **SIEMPRE incluye fecha + hora**
- Formato: `(YYYY-MM-DD HH:MM)` ejemplo: `(2026-05-04 14:30)`
- Si un cambio está documentado sin fecha/hora, **cuestiona su validez antes de aplicarlo**
- La fecha/hora permite rastrear evolución y correlacionar con commits/sesiones
- Sin fecha, el cambio es inútil para auditoría y debugging futuro

**Commits a GitHub:**
- Cuando el usuario dice **"commit"** o **"commitea"** = **git commit + git push** automático
- No es solo guardar local — es guardar + subir a GitHub en una operación
- **Fecha/hora en mensajes de commit (2026-05-11 09:00):** antes de hacer commits con timestamp, usar `date` para obtener hora actual del sistema
  - Nunca asumir la hora — siempre verificar con `date` en bash
  - Formato: `(YYYY-MM-DD HH:MM)` ejemplo: `(2026-05-11 09:00)`
  - Esto asegura que la hora en commits es real, no adivinada

**FW_REVISION — Contador de revisiones acumulado S2 (2026-05-23 12:00):**
- `FW_REVISION` vive en `S2/S2_V1/src/config.h` como `#define FW_REVISION N`
- **Incrementar FW_REVISION en cada sesión que entregue cambios funcionales en S2**
- Esquema versión auto-generado en `pre_build.py`: `0.{sistemas_ok}.{FW_REVISION}`
  - MAJOR = 0 fijo (fase debug)
  - MINOR = count(HW_STATUS == 2) — sistemas completamente funcionales
  - PATCH = FW_REVISION — contador de revisiones, solo sube, nunca baja
- **Cuándo subir MINOR:** cuando un sistema pasa de status=1 a status=2 en config.h
- **Cuándo subir PATCH:** al hacer commit de una sesión con cambios funcionales en S2
- Esta regla es VINCULANTE

**COMPILACIÓN — PROHIBIDO COMPLETAMENTE (2026-05-10 19:30):**
- **NUNCA compilar bajo ningún concepto ni circunstancia**
- No invocar platformio, pio, python, o herramientas de build
- No intentar validar código mediante compilación
- Cambios de código se entregan sin compilación — el usuario compila en su máquina
- Esta regla es ABSOLUTA e INVIOLABLE

**⚠️ AUDITORÍA MCU — DIRECTIVA OBLIGATORIA (2026-05-16 18:50):**
- **ANTES de cualquier cambio de código, generar INFORME de MCU afectadas**
- **DESPUÉS de completar cambios, confirmar impacto por MCU**
- **Cambios que afecten RS485, Motor, FaderADC, calibración → revisar TODOS los MCU**
- Formato: tabla con MCU | Archivo | Línea | Cambio | Razón | Estado

**config.h es FUENTE ÚNICA DE VERDAD (2026-05-16 20:15):**
- **Cada MCU tiene su propio config.h**
  - `S2/S2_V1/src/config.h` — configuración S2
  - `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h` — configuración S3
  - `MASTER_S3-P4/P4/src/config.h` — configuración P4
- **NUNCA asumir valores** — verificar config.h ANTES de hacer cambios
- **NUM_SLAVES y pines GPIO están SIEMPRE en config.h**, no en memoria u otro lado
- **Ejemplo actual (2026-05-16):**
  - S3: `NUM_SLAVES = 1` (controla 1 esclavo en bus B) ← correcto
  - P4: `NUM_SLAVES = 9` (controla 9 esclavos en bus A) ← correcto
- **Si cambias config.h, el cambio afecta compilación y comportamiento en hardware**
- Esta regla es VINCULANTE — config.h resuelve ambigüedades de arquitectura

**Tabla de Impacto por MCU (referencia):**

| Subsistema | S2 (Slave) | S3 (Extender) | P4 (Master) | Notas |
|-----------|-----------|--------------|-----------|-------|
| Motor | ✅ DRV8833 local | ❌ No aplica | ❌ No aplica | Solo S2 tiene motor |
| FaderADC | ✅ ADS1115 + cap | ❌ No aplica | ❌ No aplica | Solo S2 tiene fader |
| RS485 Slave | ✅ RS485Handler.cpp | ✅ RS485Master.cpp | ✅ RS485Master.cpp | Ambos masters |
| FLAG_CALIB | ✅ Recibe en onMasterData | ✅ Envía setCalibrate | ✅ Envía setCalibrate | S2 recibe, S3/P4 envían |
| Calibración | ✅ Motor::requestCalibration | ❌ No tiene motor | ❌ No tiene motor | Solo S2 calibra |
| protocol.h | ✅ S2 version | ✅ S3 version | ✅ P4 version | 3 copias independientes |
| NUM_SLAVES | ✅ Definido en config.h | ✅ Definido en config.h | ✅ Definido en config.h | Ver config.h, nunca asumir |

**Protocolo de Informe (obligatorio para CADA sesión con cambios):**

```
═══════════════════════════════════════════════════════════════
  INFORME DE CAMBIOS — [FECHA HORA]
═══════════════════════════════════════════════════════════════

SUBSISTEMA AFECTADO: [Motor / RS485 / FaderADC / Calibración / Otro]

MCU IMPACTADAS:
┌─────────────────────────────────────────────────────────────┐
│ S2 (Slave)     ✅ AFECTADO / ❌ No afectado                 │
│ S3 (Extender)  ✅ AFECTADO / ❌ No afectado                 │
│ P4 (Master)    ✅ AFECTADO / ❌ No afectado                 │
└─────────────────────────────────────────────────────────────┘

CAMBIOS DETALLADOS:

S2 (si aplica):
  Archivo: [ruta/archivo.cpp]
    Línea NNN: [cambio exacto]
    Razón: [por qué]
    Validación: [cómo testear]

S3 (si aplica):
  Archivo: [ruta/archivo.cpp]
    Línea NNN: [cambio exacto]
    Razón: [por qué]
    Validación: [cómo testear]

P4 (si aplica):
  Archivo: [ruta/archivo.cpp]
    Línea NNN: [cambio exacto]
    Razón: [por qué]
    Validación: [cómo testear]

INCONSISTENCIAS DETECTADAS:
  ⚠️ [Si hay duplicación de código entre MCU, documentar]
  ⚠️ [Si hay diferencias de versión en protocol.h, documentar]

RIESGO TOTAL: [BAJO / MEDIO / ALTO / CRÍTICO]
  - BAJO: Solo S2, cambios locales, no afecta comunicación
  - MEDIO: S2 + S3, cambios en RS485 protocol, validación hardware requerida
  - ALTO: P4 + S3 + S2, cambios en architecture, impacto sistémico
  - CRÍTICO: RS485 protocol, puede dejar sistema no operacional

PRÓXIMO PASO: [Commit message, validación, deploy]
═══════════════════════════════════════════════════════════════
```

**Ejemplos de aplicación:**

❌ **INCORRECTO** — Cambio sin informe:
```
Usuario: "Cambia setCalibrate en RS485"
Claude: [hace cambio en S3]
→ ERROR: No verificó si S2 también necesita cambio en RS485Handler
```

✅ **CORRECTO** — Con informe obligatorio:
```
Usuario: "Cambia setCalibrate en RS485"
Claude: 
  INFORME:
    S2: RS485Handler.cpp línea 67 — cambio startCalib→requestCalibration
    S3: RS485.cpp línea 41 — ya correcto, sin cambio
    P4: RS485Master.cpp línea XX — verificar si duplica S3
  RIESGO: MEDIO (RS485, requiere validación hardware)
  
[Procede con cambios]
```

Esta regla es VINCULANTE — SIEMPRE generar informe antes de código.

**🔒 HARDWARE LOCKED — NO CAMBIOS FÍSICOS (2026-05-10 22:00):**
- **El hardware ES el que ES. Hacerlo funcionar POR SOFTWARE.**
- **NUNCA proponer cambiar cables, pines, soldaduras, o conexiones físicas**
- Incluso si "sería más fácil", la respuesta es: invertir lógica/compensar en código
- Esto incluye: no sugerir intercambiar motores, cambiar DRV8833, resoldear, etc.
- Ejemplo: cables al revés → invertir _hwUp/_hwDown en software, NO cambiar cables físicos
- Si diagnosticas problema físico → documentar, no corregir
- Esta regla es ABSOLUTA — hardware no se toca bajo ninguna circunstancia

**Código Moderno — Alineación con Stack de Librerías (2026-05-10 19:45):**
- **Usar MISMAS APIs que las librerías del proyecto usan internamente**
- **Motor S2:** usa `analogWrite` (no LEDC) — conflicto de canales LEDC con LovyanGFX backlight (2026-05-10 19:54)
  - analogWrite es simple, robusta, sin conflictos de recursos
  - LEDC migración fue revertida tras identificar agotamiento de canales
- Adafruit librerías usan Wire moderno → I2C DEBE usar Wire (no legacy)
- IDF5/pioarduino 55.03.37 es la base → APIs deben ser compatibles
- Validar SIEMPRE retorno de inicializaciones: `if (!Wire.begin())`, etc.
- Logging: usar log_i/log_e (no Serial legacy) — ya integrado en setup()
- Comentar versión de API usada en código (ej: "analogWrite PWM 20kHz 8-bit")

**Desarrollo de código:**
- Cambios quirúrgicos, no rewrites completos salvo petición explícita
- `Serial.printf` en S2, `log_i`/`log_w` en P4
- SatMenu callbacks en `main.cpp`: funciones estáticas nombradas, no lambdas (ICE en xtensa)
- Logic es la única fuente de verdad para estados de botones — S2 nunca hace toggle local
- Un proyecto PlatformIO por variante MCU — `config.h` de S3/S2 son independientes
- No tocar código sin ver primero los ficheros reales

**Variables de estado y configuración — SIEMPRE en config.h (2026-05-10 13:43):**
- **NUNCA** declarar `static` variables de estado/configuración en .cpp archivos
- Todas las variables `static` deben estar en `config.h` como fuente única de verdad
- Incluye: constantes PWM, umbrales calibración, variables de estado de hardware (ADC min/max, etc.)
- Razón: config.h es el punto central de referencia — permite auditoría, debugging, y cambios sin búsquedas de código
- Excepción: funciones helper privadas file-scope en .cpp (sufijos `_hw`, `_calib`) pueden tener variables locales transitorias
- Si una variable afecta comportamiento o calibración → va en config.h

**Sincronización Código ↔ CLAUDE.md:**
- Cuando cambies arquitectura de `loop()`, orden de `init()`, pines GPIO, o tiempos críticos → **actualiza CLAUDE.md con los diagramas/tablas correspondientes**
- Cambios pequeños (bug fixes, optimizaciones locales) no requieren actualización
- CLAUDE.md es fuente de verdad para decisiones de diseño

**🏗️ ARQUITECTURA DE MÓDULOS — REGLAS A FUEGO (2026-05-12 00:45):**
- **NUNCA reescribir funciones que existan en módulos de producción**
- **Módulos de producción:** Motor, FaderADC, FaderTouch, ButtonManager, Encoder, RS485Handler, Display
- **SatMenu es CONSUMIDOR de módulos, NUNCA reimplementación**
- **Antes de escribir cualquier función nueva:**
  1. Verificar si existe en Motor.cpp, FaderADC.cpp, FaderTouch.cpp, ButtonManager.cpp, Encoder.cpp, RS485Handler.cpp, Display
  2. Si existe → usar directamente (include + llamada a función pública)
  3. Si NO existe → crear en módulo correspondiente, NO en SatMenu
- **Si algo es inesperado → preguntar antes de actuar**
- **Un cambio a la vez, confirmar antes de continuar**
- Esta regla es ABSOLUTA — evita duplicación de código y mantiene cohesión arquitectónica

**🎮 COMPORTAMIENTO MOTOR S2 — v3 Usuario es Master (2026-05-16 18:45):**

**Jerarquía de prioridades VINCULANTE:**
```
MÁXIMA:  Usuario mueve fader → Motor para INMEDIATAMENTE
         GoToMin ejecuta SIEMPRE si !_connected (MASTER control)
MEDIA:   S3 ordena posición → Motor se mueve SOLO si usuario NO toca
MÍNIMA:  Sin comando: Motor idle en posición actual
```

**Cambios críticos v3 (vs v2):**
1. **`Motor::requestCalibration()` reemplaza `startCalib()` en RS485**
   - Cuando S3 manda FLAG_CALIB → RS485Handler llama `Motor::requestCalibration()`
   - Si fader ≠ 0: `Motor::goToMin()` PRIMERO, luego `startCalib()`
   - Si fader = 0: `startCalib()` directo
   - Garantiza fader EN 0 antes de calibración (arquitectura limpia)

2. **`Motor::goToMin()` es MASTER absoluto**
   - Ejecuta SIEMPRE si `!_connected` (S3 desconectado)
   - No se rechaza por ningún otro estado
   - Si `_connected = true` → goToMin() retorna (espera órdenes S3)
   - Propósito: Fader siempre baja a 0 si S3 no controla

3. **Usuario toma control con movimiento manual**
   - `setADCDelta()` detecta: delta > 500 cuentas O `FaderTouch::isTouched()`
   - Motor para INMEDIATAMENTE: `Motor::stop()`
   - ADC actual = nuevo target (_motor_targetADC = currentADC)
   - Estado: AT_TARGET (usuario es master absoluto)
   - touchState=1 reportado a S3 vía RS485

4. **S3 NO puede overridear mientras usuario toca**
   - `setTargetFromS3()` tiene guard: `if (_motor_manualTouchDetected || FaderTouch::isTouched()) return;`
   - S3 targets ignorados mientras usuario toque (debounce 200ms tras soltar)

5. **Calibración es independiente de Logic (2026-05-16 19:24)**
   - FLAG_CALIB procesado ANTES de checar pkt.connected en RS485Handler::onMasterData()
   - Motor puede calibrar sin Logic conectado
   - S3 envía calibración secuencial al boot (todos los slaves)
   - Propósito: ADC mapeado ANTES de que Logic tome control

6. **Conexión S3 controla automática (post-calibración)**
   - Si CONNECTED: Motor NO baja a 0 automáticamente, espera órdenes S3
   - Si DISCONNECTED: Motor baja a 0 indefinidamente (goToMin loop)

**Implementación v3 (2026-05-16 18:45 → 19:26):**

*S2 (Slave):*
- `Motor.cpp`: función `Motor::requestCalibration()` (línea ~580) — NUEVA, idempotente
  - Evalúa estado ACTUAL cada llamada: `if (ADC <= MIN+10) → CALIBRATING else → GOING_TO_MIN`
  - NO guarda estado `_pendingCalib` — S3 llama cada ciclo sin bloqueo
- `Motor.cpp`: `Motor::goToMin()` — MASTER ABSOLUTO si `!_connected`
- `Motor.cpp`: `setADCDelta()` (línea ~441) — usuario detection con `inCalibFlow` guard
  - Permite cambios ADC grandes durante GOING_TO_MIN/CALIBRATING
- `Motor.cpp`: `setTargetFromS3()` — con guards usuario touch
- `RS485Handler.cpp` (línea 36): **CAMBIO CRÍTICO**
  - FLAG_CALIB procesado **ANTES** de procesar pkt.connected/desconexión
  - Motor::requestCalibration() ejecuta SIEMPRE que FLAG_CALIB=1
  - Calibración independiente de Logic state

*S3 (Extender):*
- `config.h` (línea ~52): `#define MAX_CALIBRATION_RETRIES 5`
- `RS485.cpp` (línea ~9): `extern Adafruit_NeoPixel pixels` — globales
- `RS485.cpp::begin()` (línea ~18): Initialize NeoPixel azul (aguardando conexión)
- `RS485.cpp::runTask()` (línea ~103): Timeout handling con contador
  - Si `_consecutiveTimeouts > MAX_CALIBRATION_RETRIES`:
    - NeoPixel **ROJO** (error crítico)
    - Log error: `[CALIB] ✗ FALLO CRÍTICO Slave X`
    - Sistema entra en **halt infinito** (requiere reset manual)
  - Si timeout ≤ 5: salta a siguiente slave, log warning
- `main.cpp` (línea ~20): `static uint32_t bootLEDTime = 0;` — boot LED tracking
- `main.cpp::setup()` (línea ~240): Verde LED por 1s
  - `pixels.setPixelColor(0, pixels.Color(0, 255, 0)); bootLEDTime = millis();`
- `main.cpp::taskCore0()` (línea ~137): Non-blocking LED off
  - `if (bootLEDTime > 0 && millis() - bootLEDTime > 1000) → apagar`

- Documentación: MOTOR.md + FADER.md + RS485.md + README.md (S3) actualizados (2026-05-16 19:26)

**🔴 NeoPixel Status LED — S3 Error Handling (2026-05-16 19:50):**

| Estado | Color | Situación | Acción |
|--------|-------|-----------|--------|
| **Init** | Azul | S3 arranca, esperando comunicación Logic | Sistema operacional, en espera |
| **Boot OK** | Verde 1s | S3 init completado, LED enciende no-bloqueante | Timer auto-apaga tras 1s |
| **Calibración activa** | Gris apagado | S3 ordena calibración a slaves | LED apagado durante ciclos normal |
| **Error crítico** | **Rojo fijo** | TIMEOUT > 5 reintentos en slave → calibración irrecuperable | **Sistema HALTED** — loop infinito, requiere reset manual |

**Detalles error crítico:**
- Evento: S3 timeout en slave X durante calibración → contador llega a `MAX_CALIBRATION_RETRIES = 5`
- LED: `pixels.setPixelColor(0, pixels.Color(255, 0, 0))` — ROJO
- Log: `[CALIB] ✗ FALLO CRÍTICO Slave X — comunicación perdida. Sistema DETENIDO.`
- Sistema: `while(1) delay(1000);` — loop infinito (evita continuar con slave borrado)
- Recuperación: Reset manual S3 (botón reset o power cycle)
- Propósito: Alertar operador de fallo de hardware S2 — no continuar calibración sin comunicación

**Test mínimo requerido:**
- [ ] Boot: fader baja a 0 (Motor::goToMin() en setup)
- [ ] S3 conecta: motor NO baja, espera target
- [ ] S3 manda FLAG_CALIB: Motor::requestCalibration() ejecuta
  - [ ] Si fader ≠ 0: baja a 0, LUEGO calibra
  - [ ] Si fader = 0: calibra inmediatamente
- [ ] S3 manda target: motor va (si usuario NO toca)
- [ ] Usuario mueve fader: motor para INMEDIATAMENTE
- [ ] S3 manda target MIENTRAS usuario toca: motor ignora (no se mueve)
- [ ] Usuario suelta: motor queda en posición usuario
- [ ] S3 manda nuevo target TRAS soltar (después 200ms): motor va
- [ ] S3 desconecta: Motor::goToMin() ejecuta, fader baja a 0

**⚠️ VALIDACIÓN EN HARDWARE — OBLIGATORIA (2026-05-13 15:59):**
- **TODO cambio en RS485, Motor, FaderADC, o protocolo REQUIERE validación en hardware**
- **NUNCA desplegar cambios RS485 sin test físico en rig S3-S2**
- **Mínimo requerido:**
  - S3 arranca → ordena calibración a S2
  - S2 calibra → envía min/max → S3 captura
  - Logic envía PitchBend → S3 mapea → S2 motor sigue
  - Validar: fader llega a 100%, motor es suave, sin lag
- **Si cambio afecta Motor, FaderADC, o calibración → test en bench ANTES**
- **RS485 es crítico.** Un bug puede dejar el sistema no operacional (incomunicación total S3-S2)
- Esta regla es VINCULANTE — sin validación hardware, NO merge/deploy

**📝 MEMORY — Sistema de Persistencia (2026-05-16 09:34):**
- **Ubicación:** `/Users/julianno/.claude/projects/.../memory/`
- **Propósito:** Mantener contexto, decisiones, y aprendizajes entre conversaciones
- **Estructura:**
  - `MEMORY.md` — índice central (máx. 200 líneas, una línea por entrada)
  - Archivos `.md` individuales por tema (user, feedback, project, reference)
- **Obligatorio ACTUALIZAR cuando:**
  - Descubres preferencias nuevas del usuario (feedback)
  - Cambios arquitectónicos significativos ocurren (project)
  - Bugs críticos encontrados y solucionados (project)
  - Patrones de trabajo establecidos (feedback)
- **NO guardar:** código, filepaths normales, git history, información ya en CLAUDE.md
- **Formato:** Frontmatter YAML + contenido markdown. Enlazar con `[[nombre]]`
- **Esta regla es VINCULANTE** — memory es fuente de verdad entre sesiones, mantenerla actualizada

---

## Qué es esto
Controlador DAW compatible Mackie Control Universal (MCU) para Logic Pro, construido sobre ESP32. Tres subproyectos en PlatformIO, todos en este repo.

---

## Arquitectura general

```
Logic Pro (macOS)
    │ USB-MIDI
    ▼
ESP32-P4  ←→  RS485 bus A  ←→  17× ESP32-S2 (PTxx Track)
(MCU, 9 tracks, IDs 1–9)

ESP32-S3  ←→  RS485 bus B  ←→  8× ESP32-S2 (PTxx Track)
(Extender, IDs 1–8)
```

- **P4** es el master MCU (reemplaza S3 #1 original). **S3 Extender** controla 8 slaves adicionales (hardware actual).
- **S2 slaves** son el hardware definitivo — 17 unidades Lolin S2 Mini.
- RP2040 descartado permanentemente. S2 es la plataforma slave definitiva.

---

## Estructura de carpetas (2026-05-12 17:47)

```
/AITEC/
├── MASTER_S3-P4/
│   ├── P4/                            ← Master MCU (ESP32-P4)
│   ├── S3/iMakie-ESP32_S3_EXTENDER/   ← Extender (ESP32-S3)
├── S2/S2_V1/                          ← Slaves (ESP32-S2 ×17)
├── C3_PowerRelay/                     ← Proyecto separado (power control)
├── CLAUDE.md                          ← Este archivo (directivas obligatorias)
├── STATUS.md                          ← Referencia técnica de componentes
├── docs/                             ← 📌 DOCUMENTACIÓN CENTRALIZADA
│   ├── FADER.md                      ← ADS1115, calibración, mapping (2026-05-16)
│   ├── MOTOR.md                      ← DRV8833, control, SAT (2026-05-16)
│   ├── RS485.md                      ← Protocolo, timing, troubleshooting (2026-05-16)
│   ├── WIFI.md                       ← Provisioning, OTA, ElegantOTA (2026-05-16)
│   ├── BUTTONS.md                    ← Debounce, ButtonManager, mapeo MIDI (2026-05-16)
│   ├── DISPLAY.md                    ← ST7789V3, sprites PSRAM, layout (2026-05-16)
│   ├── ENCODER.md                    ← ISR Gray code, sequenciamiento, SAT (2026-05-16)
│   ├── LEDS.md                       ← NeoPixels WS2812B, asignación, estados (2026-05-16)
│   ├── SAT.md                        ← Sistema Auto-Test, navegación, integración (2026-05-16)
│   ├── MIDI.md                       ← Protocolo Mackie MCU completo: handshake, faders, VU, botones (2026-05-18)
│   ├── Transport.md                  ← Control transport RW/FF/STOP/PLAY/REC, MIDI (2026-05-16)
│   ├── DISPLAY_P4.md                 ← ST7701S MIPI-DSI 480×800, LVGL v9 (2026-05-16)
│   ├── TOUCH.md                      ← GT911 capacitivo I2C, calibración (2026-05-16)
│   ├── NEOTRELLLIS.md                ← 2× seesaw 4×4 RGB, I2C (2026-05-16)
│   ├── RS485_P4.md                   ← Bus A GPIO50-52, 9 slaves (2026-05-16)
│   └── ARCHITECTURE_P4.md            ← Dual-core FreeRTOS, race conditions (2026-05-16)
├── CHANGELOG.md                       ← Historial de cambios
├── README.md                          ← Intro repo
└── platformio.ini                     ← Índice de subproyectos
```

**📌 DOCUMENTACIÓN CENTRALIZADA (2026-05-16):**

**Subsistemas S2 (Slave):**
- **FADER.md** — ADS1115 ADC, calibración bidireccional, mapping Logic↔ADC
- **MOTOR.md** — DRV8833, máquina de estados, guard cooldown, SAT calibración
- **BUTTONS.md** — Debounce, ButtonManager, mapeo MIDI, troubleshooting
- **DISPLAY.md** — ST7789V3, sprites PSRAM, layout, LovyanGFX
- **ENCODER.md** — ISR Gray code, sequenciamiento, SAT, VPot ring
- **LEDS.md** — WS2812B NeoPixel, asignación LEDs, estados, brillo

**Sistemas Compartidos (S2/S3/P4):**
- **RS485.md** — Protocolo binario, timing, paquetes, máquina estados (Buses A y B)
- **MIDI.md** — Protocolo Mackie MCU completo: handshake SysEx, faders, VU, botones, encoders, desconexión
- **WIFI.md** — Provisioning credenciales, OTA, ElegantOTA, NVS
- **SAT.md** — Sistema Auto-Test, navegación menú, integración módulos
- **Transport.md** — Controles transport (S3), MIDI notas, handshake Mackie, bidireccional

**Subsistemas P4 (Master MCU):**
- **DISPLAY_P4.md** — ST7701S MIPI-DSI 480×800, LVGL v9, portrait/landscape
- **TOUCH.md** — GT911 capacitivo I2C, calibración, LVGL integration
- **NEOTRELLLIS.md** — 2× seesaw 4×4 RGB, I2C control
- **RS485_P4.md** — Bus A (GPIO50-52), 9 slaves, vs Bus B timing
- **ARCHITECTURE_P4.md** — Dual-core FreeRTOS, race conditions, sincronización

**Directivas:**
- **CLAUDE.md** — Directivas vinculantes únicamente (no duplicar técnica)

---

## Subproyectos PlatformIO

| Directorio | MCU | Rol | Compilar |
|---|---|---|---|
| `MASTER_S3-P4/P4/` | ESP32-P4 | Master MCU — USB-MIDI + RS485 bus A | `cd MASTER_S3-P4/P4 && pio run` |
| `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/` | ESP32-S3 | Extender — RS485 bus B (8 slaves) | `cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER && pio run` |
| `S2/S2_V1/` | ESP32-S2 | Slave — 1 canal físico completo | `cd S2/S2_V1 && pio run` |

---

## Hardware PTxx Track S2 (slave) — 📌 Ver S2/README.md

**Chip:** ESP32-S2FN4R2 — 4MB Flash, 2MB PSRAM (QSPI)

**Pinout, Display, PSRAM, NeoPixels, Motor, ADS1115:** 
→ **[Ver S2/README.md](S2/README.md)** (fuente única de verdad)

**Motor.md y FADER.md:** Documentación exhaustiva específica de subsistemas

### Fader & Motor — 📌 Ver FADER.md y MOTOR.md

**Documentación centralizada (2026-05-16):**
- **FADER.md** — ADS1115 (16-bit ISR 860 SPS), calibración bidireccional, mapping Logic↔ADC, EMA filter
- **MOTOR.md** — DRV8833 H-bridge, máquina de estados (KICK_UP…SETTLE_DOWN), guard cooldown, SAT

### RS485 — 📌 Ver RS485.md

**Documentación exhaustiva:** 
→ **[RS485.md](docs/RS485.md)** (topología, timing, paquetes, máquina estados, troubleshooting)

### WiFi / OTA — 📌 Ver WIFI.md

**Documentación exhaustiva:** 
→ **[WIFI.md](docs/WIFI.md)** (provisioning, OTA ElegantOTA, NVS, boot OTA-only mode)

---

## Arquitectura S2 — 📌 Ver MOTOR.md y FADER.md

**Toda la documentación técnica de S2 está centralizada:**
- **MOTOR.md** — Loop principal, máquina calibración, SAT, APIs Motor
- **FADER.md** — ADS1115, touch capacitivo, EMA filter, mapping Logic↔ADC

**CLAUDE.md mantiene solo directivas críticas.** Para detalles arquitectura S2: leer MOTOR.md + FADER.md.

---

## ~~Arquitectura S2 — Flujo de datos (MOVIDO A MOTOR.md)~~

### Loop principal (main.cpp)

```
loop() {
  0. ButtonManager::update()        ← botones REC/SOLO/MUTE/SELECT → flags
     - Si SAT abierto en MOTOR_CALIB + REC presionado: Motor::startCalib() (2026-05-12 19:07)
  1. SAT menu — si está abierto, return (máxima prioridad)
  2. Encoder::update()              ← ANTES de RS485, captura delta actualizado
     - Si changed: actualiza VPotLevel, setea needsVPotRedraw
  3. RS485.update() (si no suspended):
     - Si hasNewData():
       - RS485Handler::onMasterData() ← recibe faderTarget, nombre, flags
       - buildResponse() + sendResponse() ← arma y envía SlavePacket
       - Encoder::reset()             ← limpia delta acumulado
     - checkTimeout()
  4. faderADC.update()              ← lee ADC 13-bit: 0-8191
  5. FaderTouch::update()           ← detección sostenimiento
  6. Motor::setADC(faderADC.getFaderPos()) ← Motor recibe ADC actual
  7. Motor::update()                ← máquina de estado calibración/control
     - CRÍTICO: ejecuta SIEMPRE (incluso si SAT abierto)
     - SAT NO ejecuta Motor::update() (evita race condition) (2026-05-12 19:07)
     - Si FaderTouch::isTouched() → Motor::stop()
     - Si no → Motor::update()
  8. updateButtons()                ← procesa botones para display
  9. updateDisplay()                ← Display SPI3
  10. updateAllNeopixels()          ← LEDs
}
```

### Botones — 📌 Ver BUTTONS.md

**Documentación exhaustiva:** 
→ **[BUTTONS.md](docs/BUTTONS.md)** (hardware GPIO37-40, debounce 20ms, ButtonManager, mapeo MIDI notas 160-163)

### Fader ADC — 📌 Ver FADER.md

**Documentación exhaustiva:** 
→ **[FADER.md](docs/FADER.md)** (ADS1115, ISR 860 SPS, FaderTouch capacitivo, salida SlavePacket)

### Motor — 📌 Ver MOTOR.md

**Documentación exhaustiva:** 
→ **[MOTOR.md](docs/MOTOR.md)** (DRV8833, máquina calibración KICK_UP..SETTLE_DOWN, control, SAT)

### Pantalla — 📌 Ver DISPLAY.md

**Documentación exhaustiva:** 
→ **[DISPLAY.md](docs/DISPLAY.md)** (ST7789V3, sprites PSRAM, layout, LovyanGFX, SAT suspend/restore)

### Encoder — 📌 Ver ENCODER.md

**Documentación exhaustiva:** 
→ **[ENCODER.md](docs/ENCODER.md)** (ISR Gray code, sequenciamiento critical, SAT push >3s, VPot ring -7..+7)

### NeoPixels — 📌 Ver LEDS.md

**Documentación exhaustiva:**
→ **[LEDS.md](docs/LEDS.md)** (WS2812B 12 LEDs, asignación 0-3/4-10/11, estados color, brillo, SAT test)

### Ciclo de comunicación RS485 — 📌 Ver RS485.md

**Documentación exhaustiva:**
→ **[RS485.md](docs/RS485.md)** (topología, MasterPacket/SlavePacket, timing, máquina estados, CRC8, troubleshooting)

### Inicialización S2 — 📌 Ver S2/README.md

**Documentación exhaustiva:**
→ **[S2/README.md](S2/README.md)** (orden init crítico, dependencias, timing SPI/I2C/RS485)

### SAT (Sistema de Auto-Test) — 📌 Ver SAT.md

**Documentación exhaustiva:**
→ **[SAT.md](docs/SAT.md)** (navegación, menú, integración Motor/Display/Encoder/LEDs/WiFi/RS485, troubleshooting)

### Build S2 — 📌 Ver S2/README.md

**Documentación exhaustiva:**
→ **[S2/README.md](S2/README.md)** (platform pioarduino/IDF5, librerías, orden init, logging, versioning)

---

## Hardware S3 (Extender) — 📌 Ver MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md

**Documentación exhaustiva:**
- **Pinout y RS485:** [MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md](MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md) (pinout, RS485 bus B, timing, handshake)
- **Transport (RW/FF/STOP/PLAY/REC):** [docs/Transport.md](docs/Transport.md) (MIDI notas, flujo bidireccional, feedback)

---

## Hardware P4 (master) — 📌 Ver MASTER_S3-P4/P4/README.md

**Documentación exhaustiva:**
→ **[MASTER_S3-P4/P4/README.md](MASTER_S3-P4/P4/README.md)** (pinout, subsistemas diferenciados)

**Subsistemas P4 (documentados por MCU):**
- **Display P4:** [docs/DISPLAY_P4.md](docs/DISPLAY_P4.md)
- **Touch:** [docs/TOUCH.md](docs/TOUCH.md)
- **NeoTrellis:** [docs/NEOTRELLLIS.md](docs/NEOTRELLLIS.md)
- **RS485 P4:** [docs/RS485_P4.md](docs/RS485_P4.md)
- **Arquitectura Tareas:** [docs/ARCHITECTURE_P4.md](docs/ARCHITECTURE_P4.md)

---

<!-- ⚠️ DESACTUALIZADO (2026-05-16): STATUS.md información obsoleta. Consultar docs/ para contenido actual -->
**→ [Ver STATUS.md](STATUS.md)** para bugs conocidos y pendientes. ⚠️ *Desactualizado — consultar docs/ para info actual*


---

## Encoder — Arquitectura y sequenciamiento (S2) — 📌 Ver ENCODER.md

**Documentación exhaustiva:**
→ **[docs/ENCODER.md](docs/ENCODER.md)** (ISR Gray code, sequenciamiento critical, SAT push >3s, VPot ring -7..+7, bug fix 2026-04-28)

---

## Mackie MCU — comportamiento, handshake, transport — 📌 Ver Transport.md y FADER.md

**Documentación exhaustiva:**
- **Transport.md** — Botones transport (RW/FF/STOP/PLAY/REC), LEDs, MIDI feedback, handshake Mackie MCU familia 0x14, troubleshooting (2026-05-16)
- **FADER.md (sección "Rango de Faders")** — Mapeo Logic 0-14848 ↔ ADC 0-27000, arquitectura bidireccional S3-S2 (2026-05-16)

---


## Repositorio (2026-05-12 17:47)

**GitHub:** `juliannof/iMakie`  
**Local:** `/Users/julianno/Documents/PlatformIO/Projects/AITEC/` (reorganizado como carpeta unificada)

**Compilar cada subproyecto:**
```bash
# Master P4
cd MASTER_S3-P4/P4 && pio run

# Extender S3
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER && pio run

# Slave S2
cd S2/S2_V1 && pio run
```

---

**📌 Historial de sesiones de debugging:** Ver [CHANGELOG.md](CHANGELOG.md) sección "Historial de Sesiones de Debugging"
