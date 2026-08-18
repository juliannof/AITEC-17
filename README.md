# AITEC-17 — Mackie Control protocol on ESP32

> A DIY Mackie Control Universal surface built on ESP32-P4 (master) + ESP32-S3 (extender) + up to 17× ESP32-S2 Mini (slave) units, communicating over RS485 and interfacing with DAWs (Logic Pro) via USB MIDI.

---

## Table of Contents

- [System Overview](#system-overview)
- [Project Status](#project-status)
- [Hardware Architecture](#hardware-architecture)
  - [Master Unit — ESP32-P4](#master-unit--esp32-p4)
  - [Extender Unit — ESP32-S3](#extender-unit--esp32-s3)
  - [Slave Units — ESP32-S2 Mini](#slave-units--esp32-s2-mini)
- [Communication Protocol — RS485](#communication-protocol--rs485)
- [PitchBend ↔ ADC Mapping](#pitchbend--adc-mapping)
- [Firmware Modules](#firmware-modules)
- [Development Environment](#development-environment)
- [Known Hardware Issues](#known-hardware-issues)
- [Documentation](#documentation)

---

## System Overview

AITEC-17 emulates a **Mackie Control Universal** surface for Logic Pro. It targets up to **17 slave channels** (9 on the P4's bus A + 8 on the S3's bus B), each a fully motorized channel strip.

```
Logic Pro (macOS)
        │  USB-MIDI (Mackie Control protocol)
        ▼
  ┌─────────────┐
  │  ESP32-P4   │  ◄── Master (bus A, up to 9 slaves) — board JC1060P470C
  │  (Master)   │
  └──────┬──────┘
         │  RS485 @ 500 kbaud (half-duplex, CRC8)
  ┌─────────────┐
  │  ESP32-S3   │  ◄── Extender (bus B, up to 8 slaves)
  │ (Extender)  │
  └──────┬──────┘
         │
    ┌────┴──────────────── ... ──┐
    ▼                             ▼
┌──────────┐               ┌──────────┐
│ESP32-S2  │   ...(×17)... │ESP32-S2  │
│ Slave 1  │               │ Slave 17 │
└──────────┘               └──────────┘
```

Each slave owns one channel strip: motorized fader (position + touch), illuminated REC/SOLO/MUTE/SELECT buttons, rotary encoder (V-Pot), NeoPixel LEDs, and a TFT display.

---

## Project Status

| Unit | Status |
|---|---|
| **S2 (slave)** | Firmware más maduro del proyecto — calibración automática, control de motor con protección de stall, touch capacitivo, AutoMode (READ/WRITE/TRIM/TOUCH/LATCH), OTA. |
| **S3 (extender)** | Operativo en banco — RS485 bus B, handshake MIDI con Logic, calibración en cascada, transport, NeoTrellis. Es la pieza más probada del sistema junto al S2. |
| **P4 (master, JC1060P470C)** | En desarrollo activo — UI landscape (LVGL) funcionando; RS485 bus A, touch GT911 y calibración de sus 9 slaves aún sin completar. |
| **P4 (JC4880P433C)** | Proyecto legado / independiente (NeoTrellis + pantalla propia) — no se toca desde la migración a JC1060P470C. |

RP2040 fue descartado permanentemente como plataforma slave — S2 es la definitiva.

---

## Hardware Architecture

### Master Unit — ESP32-P4

| Function          | Detail                                      |
|-------------------|---------------------------------------------|
| USB MIDI          | Native USB, Mackie Control protocol         |
| RS485 Bus A       | UART half-duplex, 500 kbaud, hasta 9 slaves |
| Display           | JD9165 MIPI-DSI 1024×600 landscape (LVGL v9) — placa activa JC1060P470C |
| Touch             | GT911 capacitivo, I2C                       |
| DAW Integration   | Logic Pro (probado), compatible con cualquier DAW con soporte Mackie Control |

### Extender Unit — ESP32-S3

| Function          | Detail                                      |
|-------------------|---------------------------------------------|
| RS485 Bus B       | UART half-duplex, 500 kbaud, hasta 8 slaves |
| Transport LEDs    | REC / PLAY / FF / STOP / RW, feedback bidireccional |
| NeoTrellis        | 2× Adafruit seesaw 4×4 RGB (8×4 total)      |

### Slave Units — ESP32-S2 Mini

| Component             | Detail                                                      |
|-----------------------|---------------------------------------------------------------|
| Motorized fader       | DRV8833 H-bridge PWM + ADS1115 16-bit ADC feedback           |
| Touch detection       | T-pin capacitivo + detección por delta de ADC                |
| Illuminated buttons   | REC / SOLO / MUTE / SELECT — NeoPixel RGB                    |
| Display               | ST7789V3 vía LovyanGFX, sprites en PSRAM                      |
| Rotary encoder        | V-Pot por canal, ISR Gray code                                |
| RS485                 | Half-duplex UART, protocolo de esclavo direccionado          |

---

## Communication Protocol — RS485

- **Baud rate:** 500 kbaud
- **Topology:** Half-duplex, un master por bus, hasta 9 (bus A) / 8 (bus B) esclavos
- **Framing:** Paquetes binarios fijos con CRC8
- **Addressing:** ID fijo por esclavo (1-9 / 1-8)

### Packet Flow

```
Master → Slave (MasterPacket, 16 bytes): faderTarget (PitchBend 0-16383), flags
                                          (REC/SOLO/MUTE/SELECT/CALIB/AutoMode),
                                          trackName, vuLevel, vpotValue, connected
Slave  → Master (SlavePacket, 9 bytes):  faderPos (ADC), touchState, buttons,
                                          encoderDelta, encoderButton

SlavePacket path:
  ESP32-S2 (ADC real) → RS485 → ESP32-S3/P4 (mapea a PitchBend) → USB-MIDI → Logic Pro
```

### Protocol Integrity

- CRC8 en cada paquete
- Timeout + reintentos con contador de fallos consecutivos por slave
- Calibración automática en cascada al arrancar, con budget de reintentos por slave

---

## PitchBend ↔ ADC Mapping

Logic envía PitchBend MIDI de **14-bit completo (0-16383)**, confirmado con captura directa en MIDI Monitor (2026-07-20). El master (S3/P4) mapea ese rango al ADC calibrado real de cada slave:

```
faderTarget = calibratedMin + (pitchBend14bit × (calibratedMax - calibratedMin) / 16383)
```

`calibratedMin`/`calibratedMax` se aprenden por slave durante la calibración automática (secuencia KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN en `Motor.cpp`) y se reportan al master vía RS485. Ver [`docs/MOTOR.md`](docs/MOTOR.md) y [`docs/FADER.md`](docs/FADER.md) para el detalle completo.

---

## Firmware Modules

> Documentación exhaustiva por subsistema en [`docs/`](docs/) — esta tabla es solo un mapa de archivos.

### S2 (slave) — `S2/S2_V1/src/`

| Module               | File(s)                                          | Doc |
|-----------------------|---------------------------------------------------|-----|
| Motor control          | `hardware/Motor/Motor.cpp`                        | [MOTOR.md](docs/MOTOR.md) |
| Fader ADC / touch      | `hardware/fader/FaderADC.cpp`, `FaderTouch.cpp`    | [FADER.md](docs/FADER.md) |
| Buttons                | `hardware/button/ButtonManager.cpp`                | [BUTTONS.md](docs/BUTTONS.md) |
| Encoder (V-Pot)        | `hardware/encoder/Encoder.cpp`                     | [ENCODER.md](docs/ENCODER.md) |
| NeoPixel LEDs          | `hardware/Neopixels/Neopixel.cpp`                  | [LEDS.md](docs/LEDS.md) |
| Display                | `display/Display.cpp`                              | [DISPLAY.md](docs/DISPLAY.md) |
| RS485                  | `RS485/RS485.cpp`, `RS485Handler.cpp`              | [RS485.md](docs/RS485.md) |
| SAT (Sistema Auto-Test)| `SAT/SatMenu.cpp`                                  | [SAT.md](docs/SAT.md) |
| OTA / WiFi             | `OTA/Otamanager.cpp`                               | [WIFI-OTA.md](docs/WIFI-OTA.md) |
| Main loop               | `main.cpp`                                         | — |

### S3 (extender) — `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/`

RS485 master (`RS485/RS485.cpp`), procesamiento MIDI/SysEx (`midi/MIDIProcessor.cpp`), transport (`Transporte`), NeoTrellis. Ver [Transport.md](docs/Transport.md), [MIDI.md](docs/MIDI.md), [RS485.md](docs/RS485.md).

### P4 (master, activo) — `MASTER_S3-P4/P4_JC1060P470C/src/`

Display LVGL ([DISPLAY_P4.md](docs/DISPLAY_P4.md)), touch GT911 ([TOUCH.md](docs/TOUCH.md)), NeoTrellis ([NEOTRELLLIS.md](docs/NEOTRELLLIS.md)), RS485 bus A ([RS485_P4.md](docs/RS485_P4.md), en desarrollo).

---

## Development Environment

| Tool              | Detail                                    |
|-------------------|--------------------------------------------|
| Framework         | Arduino vía PlatformIO, plataforma pioarduino (IDF5) |
| IDE               | VS Code + extensión PlatformIO             |
| Target boards     | ESP32-P4 (master), ESP32-S3 (extender), ESP32-S2 Mini ×17 (slave) |
| MIDI testing      | Logic Pro (macOS), MIDI Monitor para captura de tráfico real |
| Compilación       | **No la ejecuta el asistente de IA — solo el usuario en su máquina** |

Cada subproyecto se compila de forma independiente:

```bash
# Master P4 (placa activa JC1060P470C)
cd MASTER_S3-P4/P4_JC1060P470C && pio run

# Extender S3
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER && pio run

# Slave S2
cd S2/S2_V1 && pio run
```

---

## Known Hardware Issues

### LED REC de transporte "fantasma" (S3) — realimentación eléctrica, no firmware

**Síntoma:** brillo tenue en el LED REC del transporte del S3 (`GPIO12`, `LED_REC`), sin que Logic ni el firmware lo enciendan.

**Diagnóstico (2026-08-18, confirmado en banco):** desaparece al desconectar la ATX (alimentación de la etapa de motores) o el USB del P4. Esto descarta origen firmware — el fantasma por GPIO flotante en boot ya se corrigió el 2026-08-16 (`Transporte::initPins()`) y ese fix no depende de fuentes de alimentación externas. La correlación con dos referencias externas distintas apunta a una **realimentación eléctrica (ground loop)** por tierra compartida: el enlace `S3LINK` entre S3 y P4 usa cableado cruzado + **GND común** (ver [`docs/S3LINK.md`](docs/S3LINK.md) §2), y la ATX alimenta la misma etapa de motores/S2 que comparte referencia con el S3.

Por la directiva de hardware bloqueado del proyecto (`CLAUDE.md`), el firmware **no** intenta "corregir" esto por software — no hay ninguna variable de código que explique una dependencia con alimentación externa. Se documenta aquí para que el propio usuario evalúe e implemente la solución física que prefiera.

**Propuestas de solución (a evaluar/implementar por el usuario, ninguna aplicada):**

| # | Propuesta | Qué resuelve | Coste/complejidad |
|---|-----------|--------------|--------------------|
| 1 | **Medir el lazo primero:** con todo conectado (ATX + S3 + P4), medir con multímetro la diferencia de potencial entre GND del S3 y GND del P4, y entre GND del S3 y GND de la ATX | Confirma la magnitud real del ground loop antes de tocar nada — evita "arreglar a ciegas" | Ninguno (solo medición) |
| 2 | **Aislamiento galvánico en S3LINK** — sustituir el UART directo por un opto-acoplador (p.ej. par TX/RX vía optoacopladores digitales rápidos) o un transceptor UART aislado (ADuM/ISO7xxx) entre S3 y P4 | Rompe el camino de retorno de tierra compartido entre ambas placas sin perder la comunicación Serial2 | Medio — requiere componente adicional y placa/protoboard |
| 3 | **Topología de tierra en estrella** — unir GND de ATX, S3 y P4 en un único punto físico (en vez de varios caminos de retorno independientes) | Elimina el bucle de tierra en el origen, sin aislar nada | Bajo — solo recableado del retorno de tierra |
| 4 | **Choke de modo común (ferrita)** en el cable S3LINK (UART+GND) y/o en la alimentación de la ATX hacia la etapa de motores | Atenúa el ruido de alta frecuencia acoplado por el lazo, sin cambiar la topología | Bajo — pinza de ferrita externa, no invasivo |
| 5 | **Condensador de desacoplo local** cerca de `LED_REC`/regulador del S3 | Filtra transitorios de baja energía acoplados por la tierra compartida en el nodo del LED | Bajo — un condensador cerámico pequeño |

Orden recomendado: **1 → 3 → 4 → 5 → 2** (de menor a mayor invasión; la medición del punto 1 determina si el resto son necesarios).

---

## Documentation

Toda la documentación técnica detallada vive en [`docs/`](docs/) y en [`CLAUDE.md`](CLAUDE.md) (directivas de desarrollo). Historial de cambios en [`CHANGELOG.md`](CHANGELOG.md).

---

*Last updated: 2026-08-18.*
