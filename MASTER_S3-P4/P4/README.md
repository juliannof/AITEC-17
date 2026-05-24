# iMakie — ESP32-P4 Master MCU

Master Mackie Control Universal (MCU) para Logic Pro. Controla 9 tracks S2 locales vía RS485 bus A, display IPS táctil 480×800, y matriz NeoTrellis 4×8.

**Placa de desarrollo:** GUITION ESP32-P4 Capacitive Touch IPS 4.3"  
**Chip:** ESP32-P4 (Xtensa dual-core 400MHz)  
**Flash:** 16MB (QIO)  
**PSRAM:** 32MB (OPI)  
**Display:** IPS 4.3" 480×800 (ST7701S MIPI-DSI 2-lane)  
**Touch:** Capacitivo GT911 (I2C)  
**Familia Mackie:** 0x14 (testing) / 0x15 (producción)  
**Slaves controlados:** 9 (IDs 1–9) en RS485 bus A  
**NeoTrellis:** 2× Adafruit seesaw 4×4 (matriz 4×8)

---

## Especificación de placa (2026-05-16)

**Módulo:** GUITION JC4880P443C-I-W (placa de desarrollo integrada)  
**Procesador principal:** ESP32-P4 Xtensa dual-core 400MHz (Core0 + Core1)  
**Procesador secundario:** ESP32-C6 (Wi-Fi 6 + Bluetooth 5)  
**Memoria:**
- Flash: 16MB (QIO mode)
- PSRAM: **32MB** (OPI mode) — ⚠️ ABUNDANTE para LVGL + multimedia
- HP L2MEM: 768KB
- LP SRAM: 32KB
- HP ROM: 128KB
- Bootloader: 0x0 (256KB)
- App: 0x10000 (15.75MB)

**Display:** IPS capacitivo 4.3" (color, alta definición)
- Resolución: 480×800 píxeles (70.4 ppi)
- Interface: MIPI-DSI 2-lane (ST7701S driver integrado)
- Colores: 16M (24-bit RGB)
- Touch: Capacitivo multitouch GT911 (I2C, detección gesto)
- Brillo: Ajustable 0-255 (backlight PWM control)

**Procesamiento multimedia:** (ESP32-P4 integrado)
- JPEG codec (encode/decode)
- Pixel Processing Accelerator (PPA)
- Image Signal Processor (ISP) — RAW sensor input
- H.264 video encoder
- Propósito: soporta cámara MIPI-CSI + procesamiento en tiempo real

**Audio:** (opcional, ES8311 codec en algunas variantes)
- I2S stereo (microfono + altavoz)
- ADC/DAC 16-bit
- Propósito: soporte para synth/metrónomo futuro

**Energía:**
- Voltaje: USB 5V → regulador interno 3.3V
- Corriente: ~200mA idle, 400mA full power, picos 500mA
- USB: alimenta placa, display, touch y periféricos

**Conectividad integral (ESP32-P4 + ESP32-C6):**

ESP32-P4 periféricos:
- **RS485 bus A:** 500 kbaud (9 slaves S2) — GPIO 50/51/52 UART
- **I2C_NUM_0:** NeoTrellis seesaw (GPIO 33/31) — dirección 0x2F/0x2E
- **I2C_NUM_1:** GT911 touch (GPIO 7/8) — multitouch capacitivo
- **MIPI-CSI:** entrada cámara (interfaz física en placa)
- **MIPI-DSI:** display ST7701S (integrado)
- **SPI:** periféricos (DDR, SDIO)
- **I2S:** audio stereo (micrófono, altavoz)
- **LED PWM:** backlight display + 8 canales PWM auxiliares
- **MCPWM:** motor control PWM (future expansion)
- **ADC:** analog input (sensor temperatura, batería, etc)
- **TWAI (CAN):** bus industrial (future)
- **USB OTG 2.0 HS:** host + device mode

ESP32-C6 wireless (suplementario):
- **Wi-Fi 6** (802.11ax)
- **Bluetooth 5** (BLE + classic)
- Propósito: conectividad Logic Pro remota (futuro), OTA firmware

---

## Pinout definitivo P4

| Función | GPIO | Tipo | Notas |
|---------|------|------|-------|
| **RS485 TX (bus A)** | 50 | UART TX | 500 kbaud |
| **RS485 RX (bus A)** | 51 | UART RX | 500 kbaud |
| **RS485 EN (driver enable)** | 52 | GPIO output | Transceiver externo |
| **NeoTrellis SDA (I2C_NUM_0)** | 33 | I2C SDA | Dirección 0x2F/0x2E |
| **NeoTrellis SCL (I2C_NUM_0)** | 31 | I2C SCL | Dirección 0x2F/0x2E |
| **Touch SDA (GT911, I2C_NUM_1)** | 7 | I2C SDA | Capacitivo |
| **Touch SCL (GT911, I2C_NUM_1)** | 8 | I2C SCL | Capacitivo |
| **Display MIPI-DSI** | Integrado | MIPI 2-lane | ST7701S en placa |

---

## Compilación

### Build con PlatformIO

```bash
cd MASTER_S3-P4/P4
pio run -e esp32-p4
```

### Configuración PlatformIO

```ini
[env:esp32-p4]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-p4
board_build.partitions = default_16MB.csv
board_build.flash_size = 16MB
board_build.psram_type = opi
```

> ⚠️ **Riesgo conocido (2026-05-24):** `default_16MB.csv` puede colisionar con la tabla del framework pioarduino (mismo nombre → PlatformIO usa la del framework, no la local). S3 tuvo este bug exacto y lo resolvió renombrando el archivo. Si P4 da problemas de tamaño de partición, renombrar a `p4_master_16MB.csv` y actualizar `board_build.partitions`.

**Flags críticos:**
- `-DBOARD_HAS_PSRAM` — Habilita PSRAM (32MB OPI)
- `-DARDUINO_USB_MODE=1` — USB CDC nativo (USBMIDI)
- `-DDEVICE_P4_MASTER` — Identifica como P4 Master (vs S3 Extender)

### Platform y Framework

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)  
**Framework:** Arduino  
**Librerías (`lib_deps`):**
- `lvgl/lvgl@^9.5.0` — UI framework 480×800
- `tamctec/TAMC_GT911@^1.0.2` — Touch capacitivo GT911

**Nota:** el display ST7701S usa un driver custom ESP-IDF (`src/lcd/st7701_lcd.cpp`) — **no LovyanGFX**. La comunicación MIDI usa la librería USBMIDI incluida en el framework Arduino ESP32.

---

## Capacidades no utilizadas (expansion futura)

| Capacidad | Hardware | Uso potencial |
|-----------|----------|----------------|
| **Cámara MIPI-CSI** | Interfaz física, ISP integrado | Análisis visual de escena, grabación |
| **Audio I2S** | ES8311 codec (opcional en PCB) | Synth integrado, metrónomo, realtime monitor |
| **Wi-Fi 6** | ESP32-C6 | Control remoto Logic Pro, streaming OSC |
| **Bluetooth 5** | ESP32-C6 | Control inalámbrico de transporte, MIDI remote |
| **USB OTG 2.0** | Integrado P4 | Host mode para periféricos USB futuros |
| **TWAI (CAN)** | GPIO dedicados | Bus industrial para expansión modular |
| **ADC 12-bit** | 7 canales | Sensores analog (temperatura, batería, presión) |
| **MCPWM** | 6 canales PWM | Motor control (proyector, cortinas, luces escena) |
| **JPEG codec** | Acelerador HW | Captura y envío foto de contraseña/escena |
| **H.264 encoder** | Acelerador HW | Streaming video Logic → redes |

---

## Subsistemas P4

### Display P4 (ST7701S MIPI-DSI)
→ **[docs/DISPLAY_P4.md](../../docs/DISPLAY_P4.md)** (ST7701S 480×800, LVGL v9, portrait/landscape, rotación widgets)

### Touch (GT911)
→ **[docs/TOUCH.md](../../docs/TOUCH.md)** (GT911 capacitivo, I2C_NUM_1, calibración, gestos)

### NeoTrellis
→ **[docs/NEOTRELLLIS.md](../../docs/NEOTRELLLIS.md)** (2× Adafruit seesaw 4×4, matriz 4×8, direcciones 0x2F/0x2E, RGB LEDs)

### RS485 P4 (Bus A)
→ **[docs/RS485_P4.md](../../docs/RS485_P4.md)** (500 kbaud, 9 slaves, timing, diferencias vs bus B S3)

### Arquitectura Tareas P4
→ **[docs/ARCHITECTURE_P4.md](../../docs/ARCHITECTURE_P4.md)** (dual-core Core0/Core1, flags g_switchToPage, VU meter decay, race conditions)

---

## Referencias

- **RS485 (general):** [docs/RS485.md](../../docs/RS485.md)
- **Transport:** [docs/Transport.md](../../docs/Transport.md)
- **SAT:** [docs/SAT.md](../../docs/SAT.md)
- **Arquitectura general:** [CLAUDE.md](../../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../../STATUS.md)
- **S2 Slave:** [S2/README.md](../../S2/README.md)
- **S3 Extender:** [MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md](../S3/iMakie-ESP32_S3_EXTENDER/README.md)
