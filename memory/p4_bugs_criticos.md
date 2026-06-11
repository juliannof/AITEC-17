---
name: p4-bugs-criticos
description: Bugs 🔴 Alta en P4 que bloquean RS485 y arranque — pendientes de fix (2026-06-11)
metadata: 
  node_type: memory
  type: project
  originSessionId: 72917132-4bad-4fa3-95e7-b8409230997d
---

Tres bugs críticos en P4 (`P4_JC1060P470C/`) confirmados en código, sin fix aplicado aún (2026-06-11):

**1. `startTask()` RS485 nunca llamada — slaves sin comunicación**
- `main.cpp setup()` línea ~260: `rs485.begin(NUM_SLAVES)` presente, pero `rs485.startTask()` ausente.
- Efecto: el task de polling `runTask()` no arranca. P4 no envía ni recibe nada en bus A. `tickCalibracion()` encola calibraciones que nunca se envían.
- Fix: añadir `rs485.startTask();` en `setup()` tras `rs485.begin()`.

**2. `case 0x61` pone `g_logicConnected=0` — slaves muertos toda la sesión**
- `MIDIProcessor.cpp` ~línea 467: Logic envía `0x61` (AllFadersToMinimum) justo después de `0x21` en GoOnline. P4 lo interpreta como desconexión → `g_logicConnected=0` → slaves reciben `pkt.connected=0` → pantallas oscuras, motores inactivos.
- Fix: copiar case 0x61 de S3 — eliminar `g_logicConnected=0`, añadir `rs485.setFaderTarget(i,0)` para todos los slaves.

**3. Arranque sin monitor — CDC bloquea setup()**
- `main.cpp` línea 207: `Serial.begin(115200)` sin `Serial.setTxTimeoutMs(0)` a continuación.
- Efecto: con `ARDUINO_USB_CDC_ON_BOOT=1`, el core escribe al CDC antes de setup() y bloquea hasta que un host abre el puerto → pantalla negra hasta abrir monitor serie.
- Fix: añadir `Serial.setTxTimeoutMs(0);` inmediatamente después de `Serial.begin(115200)`.

**Why:** Bugs 1 y 2 hacen que P4 sea inútil como master RS485. Bug 3 hace el desarrollo incómodo (hay que abrir monitor para que arranque). Los tres se detectaron en auditoría 2026-05-27 y se documentaron, pero no se han fixeado aún.

**How to apply:** Al empezar cualquier sesión de trabajo en P4 RS485 → estos 3 fixes son prerequisito. Sin ellos, cualquier test de comunicación P4→S2 fallará aunque el resto del código esté correcto. Ver CHANGELOG.md tabla pendientes para lista completa P4.
