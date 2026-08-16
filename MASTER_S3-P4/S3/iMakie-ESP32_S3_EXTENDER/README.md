# iMakie — ESP32-S3 Extender

Extender Mackie Control Universal (MCU) para Logic Pro. Controla 8 tracks S2
vía RS485 bus B, gestiona 5 botones/LEDs de transporte (RW/FF/STOP/PLAY/REC)
locales, y reenvía el estado de sus 8 canales al P4 por un enlace serie
dedicado (S3LINK).

**Placa:** ESP32-S3-WROOM-1 N16R8 — Flash 16MB (QIO), PSRAM 8MB (OPI), USB Type-C
**Familia Mackie:** `0x14` (Main Unit — ver `docs/MIDI.md` §3.4.6 para el porqué)
**Slaves controlados:** 8 (IDs 1–8) en RS485 bus B — ver `NUM_SLAVES` en `config.h`, nunca asumir

---

## Pinout (verificado contra `src/config.h`)

| Función | GPIO | Notas |
|---------|------|-------|
| RS485 TX (bus B) | 15 | |
| RS485 RX (bus B) | 16 | |
| RS485 EN (driver enable) | 1 | |
| S3LINK TX (→ P4, Serial2) | 18 | 115200 baud, ver `docs/S3LINK.md` |
| S3LINK RX (← P4, Serial2) | 17 | |
| NeoPixel status (WS2812B) | 48 | Adafruit_NeoPixel, 1 LED |
| LED REC / BTN REC | 12 / 11 | LED en PWM (`analogWrite`, ver `docs/Transport.md` §1.2) |
| LED PLAY / BTN PLAY | 10 / 9 | |
| LED FF / BTN FF | 8 / 7 | |
| LED STOP / BTN STOP | 6 / 5 | |
| LED RW / BTN RW | 4 / 3 | |

Todos los pines, baudrates y constantes de timing viven en `src/config.h` —
es la fuente única de verdad, no asumir valores de este README.

## NeoPixel de estado

Azul = esperando conexión Logic · Verde = boot OK (1s, no bloqueante) ·
Rojo = slave con timeouts persistentes (**el sistema no se detiene** — el
slave se marca degradado y la cascada de polling continúa con el resto,
cambio deliberado 2026-05-27 tras causar HALTs falsos con reflashes de S2).
Detalle completo en `CLAUDE.md` (tabla "NeoPixel Status LED").

---

## Compilación

```bash
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER
pio run -e esp32-s3-devkitc-1
```

**`platformio.ini` (resumen, ver el archivo para la versión completa):**
- `board = esp32-s3-devkitc-1`, `board_build.partitions = s3_extender_16MB.csv`
  (⚠️ no renombrar a `default_16MB.csv` — colisiona con la tabla del framework pioarduino)
- `board_build.arduino.memory_type = qio_opi`
- Flags: `-DDEVICE_S3_EXTENDER`, `-DBOARD_HAS_PSRAM`, `-DCORE_DEBUG_LEVEL=3`
- `lib_deps`: `LennartHennigs/Button2`, `adafruit/Adafruit NeoPixel`

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)

---

## Subsistemas — documentación centralizada

Toda la arquitectura, protocolos y troubleshooting viven en `docs/`, no en
este README — evita mantener el mismo dato en dos sitios que acaban
divergiendo.

| Tema | Documento |
|---|---|
| Protocolo RS485 (bus B), paquetes, máquina de estados, calibración | [docs/RS485.md](../../../docs/RS485.md) |
| Botones/LEDs de transporte, brillo PWM, handshake Mackie | [docs/Transport.md](../../../docs/Transport.md) |
| Protocolo Mackie MCU completo (SysEx, notas, faders, VU) | [docs/MIDI.md](../../../docs/MIDI.md) |
| AutoMode (READ/WRITE/TRIM/TOUCH/LATCH), notas 74-78 | [docs/AUTOMODE.md](../../../docs/AUTOMODE.md) |
| Enlace serie S3↔P4 (Serial2): canal + heartbeat | [docs/S3LINK.md](../../../docs/S3LINK.md) |

**Arquitectura de tasks (resumen, sin números de línea que se desactualizan):**
- `taskCore0` (Core 0) — USB-MIDI in/out, respuestas RS485, VU timeout, calibración cascada.
- `taskCore1` (Core 0, movido 2026-08-16) — botones/LEDs de transporte. Antes compartía Core 1
  con el task RS485 y perdía CPU frente a él (prioridad 5 vs 1) — ver `CHANGELOG.md` sesión 2026-08-16.
- Task RS485 (Core 1, exclusivo) — polling de los 8 slaves, timing crítico en microsegundos.

---

## Referencias

- **Arquitectura general y directivas:** [CLAUDE.md](../../../CLAUDE.md)
- **Master P4:** [MASTER_S3-P4/P4_JC1060P470C/README.md](../../P4_JC1060P470C/README.md)
- **Slave S2:** [S2/README.md](../../../S2/README.md)
- **Historial de cambios:** [CHANGELOG.md](../../../CHANGELOG.md)
