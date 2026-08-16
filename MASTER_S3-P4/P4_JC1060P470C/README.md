# iMakie — ESP32-P4 Master MCU

Master Mackie Control Universal (MCU) para Logic Pro. Controla slaves S2 vía
RS485 bus A, display táctil MIPI-DSI 1024×600, y reenvía/recibe estado de
los 8 canales del S3 (Extender) por un enlace serie dedicado (S3LINK).

**Placa activa:** GUITION JC1060P470C-I-W-Y — display **JD9165 MIPI-DSI 1024×600
landscape nativo**, touch **GT911**, **sin NeoTrellis**.
**Chip:** ESP32-P4 (dual-core), Flash 16MB (QIO), PSRAM 32MB (OPI)
**Familia Mackie:** `0x14` (Main Unit)
**Slaves controlados (RS485 bus A):** `NUM_SLAVES=0` en `config.h` — **bus A sin
activar todavía**, pinout heredado de la placa antigua sin confirmar contra el
esquemático real (ver §Pinout). El rig de 9×S2 ya está cableado, pendiente
validar el pinout antes de subir `NUM_SLAVES` a 9. Nunca asumir el valor —
verificar `config.h`.

> ⚠️ **Placa descartada del desarrollo activo: `P4_JC4880P433C/`** (480×800,
> ST7701S, NeoTrellis). Es un proyecto futuro independiente — no tocar, no
> referenciar desde aquí.

---

## Pinout (verificado contra `src/config.h`)

| Función | GPIO | Notas |
|---------|------|-------|
| RS485 TX / RX / EN (bus A) | 52 / 51 / 50 | ⚠️ **Heredado de la placa antigua, SIN confirmar** contra el esquemático JC1060P470C — ver `docs/RS485_P4.md` |
| S3LINK TX / RX (↔ S3, Serial2) | 1 / 2 | 115200 baud, ver `docs/S3LINK.md` |
| LCD_RST / LCD_BL (JD9165) | 27 / 23 | |
| Touch GT911 SDA / SCL | 7 / 8 | I2C_NUM_1 (I2C_NUM_0 lo usa Wire del core), addr `0x5D`, RST/INT en NC |

Todos los pines, baudrates y constantes de timing viven en `src/config.h` —
es la fuente única de verdad, no asumir valores de este README.

**⚠️ Dato pendiente de portar desde S3:** `LOGIC_PITCHBEND_MAX` sigue en `14845`
en este `config.h` — S3 lo corrigió a `16383` (confirmado por MIDI Monitor,
2026-07-20) y nunca se replicó aquí. No es urgente mientras `NUM_SLAVES=0`,
pero hay que portarlo antes de activar el bus A.

---

## Driver del display — lo crítico

El `esp_lcd_jd9165` de esp-iot-solution **no sirve** para este panel (init
genérico, timing distinto — "enciende pero no pinta"). Se usa el driver del
demo del fabricante, vendorizado en `src/lcd/esp_lcd_jd9165.c/.h` (`lane_bit_rate=550`,
`dpi_clock=56MHz`, `num_fbs=1`). Detalle completo: `docs/DISPLAY_P4.md`.

## Orientación de pantalla

La UI se dibuja en lienzo portrait nativo 480×800 pero el panel se monta
girado 90° y se ve en landscape — la rotación es **por-objeto** (`transform_rotation`
en cada label), no global. Ver `docs/DISPLAY_P4.md` §3 para el patrón completo
y sus trampas.

---

## Compilación

```bash
cd MASTER_S3-P4/P4_JC1060P470C
pio run -e esp32-p4
```

**`platformio.ini` (resumen, ver el archivo para la versión completa):**
- `board = esp32-p4`, `board_build.partitions = default_16MB.csv`,
  `board_build.psram_type = opi`
- Flags: `-DDEVICE_P4_MASTER`, `-DBOARD_HAS_PSRAM`, `-DARDUINO_USB_MODE=1`,
  `-DARDUINO_USB_CDC_ON_BOOT=1`
- `lib_deps`: `lvgl/lvgl@^9.5.0`, `tamctec/TAMC_GT911@^1.0.2`

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)

---

## Subsistemas — documentación centralizada

Toda la arquitectura, protocolos y troubleshooting viven en `docs/`, no en
este README — evita mantener el mismo dato en dos sitios que acaban
divergiendo.

| Tema | Documento |
|---|---|
| Display JD9165, LVGL v9, orientación portrait→landscape | [docs/DISPLAY_P4.md](../../docs/DISPLAY_P4.md) |
| Touch GT911, calibración, integración LVGL | [docs/TOUCH.md](../../docs/TOUCH.md) |
| RS485 bus A (pinout pendiente confirmar), timing vs bus B | [docs/RS485_P4.md](../../docs/RS485_P4.md) |
| Enlace serie P4↔S3 (Serial2): canal + heartbeat | [docs/S3LINK.md](../../docs/S3LINK.md) |
| Arquitectura de tasks (dual-core, flags de página, VU decay) | `docs/ARCHITECTURE_P4.md` — **pendiente de crear**, no existe todavía (enlace roto heredado del README anterior) |
| Protocolo Mackie MCU completo (SysEx, notas, faders, VU) | [docs/MIDI.md](../../docs/MIDI.md) |

---

## Referencias

- **Arquitectura general y directivas:** [CLAUDE.md](../../CLAUDE.md)
- **Extender S3:** [MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md](../S3/iMakie-ESP32_S3_EXTENDER/README.md)
- **Slave S2:** [S2/README.md](../../S2/README.md)
- **Historial de cambios:** [CHANGELOG.md](../../CHANGELOG.md)
