# AITEC-17 PTxx — S2 Slave Fader

Controlador Mackie fader individual para iMakie. Controla 1 fader (ADS1115 16-bit), 1 motor (DRV8833), 4 botones, encoder rotatorio, y display IPS 240×280.

**Placa de desarrollo:** Lolin S2 Mini (basada ESP32-S2FN4R2)  
**Chip:** ESP32-S2FN4R2 (Xtensa single-core 240MHz)  
**Flash:** 4MB (QIO mode)  
**PSRAM:** 2MB (QSPI mode)  
**Conector:** Type-C USB (USB OTG nativo)  
**Pines:** 27 GPIO (muchos usados en subsistemas)  
**Familia Mackie:** 0x14

<img alt="ESP32 S2 Mini" src="https://www.wemos.cc/en/latest/_static/boards/s2_mini_v1.0.0_4_16x9.jpg">

---

## Especificación de placa (2026-05-16)

**Módulo:** Lolin S2 Mini (form factor compatible D1 mini shields)  
**Procesador:** ESP32-S2FN4R2 Xtensa single-core 240MHz  
**Memoria:**
- Flash: 4MB (QIO mode)
- PSRAM: 2MB (QSPI mode) — ⚠️ LIMITADO, cuidado con buffers
- Bootloader: 0x0 (192KB)
- App: 0x10000 (3.8MB)

**Conector:** Type-C USB (USB OTG nativo)  
**Pines disponibles:** 27 GPIO totales
- Usados por iMakie: RS485(3), Motor(3), ADS1115(4), Encoder(3), Display(6), Botones(4), NeoPixel(1), Misc(2)
- Libres: ~0 (saturado, todo asignado)

**Energía:**
- Voltaje: 5V USB → regulador interno 3.3V
- Corriente: ~60mA idle, 120mA operacional, picos 150mA
- Límite USB: 500mA (compartido con motor + display + otros)

**Limitaciones vs S3/P4:**
- Single-core (vs dual-core)
- 4MB Flash (vs 16MB S3/P4)
- 2MB PSRAM (vs 8MB S3, 32MB P4)
- Menos GPIO (27 vs 44 P4, 44 S3)

---

## Hardware

---

## Limitaciones y consideraciones

### Arquitectura
- **Single-core 240MHz** vs dual-core P4/S3
  - No hay Core0/Core1 paralelismo
  - RS485 + display + motor + encoder comparten CPU
  - Timing crítico en ISRs (ADS1115 Alert, Encoder, etc)

### Memoria
- **4MB Flash** — CUIDADO con binarios grandes
  - Bootloader: 192KB
  - App: 3.8MB (teórico, en práctica ~3.2MB usable)
  - No hay espacio para OTA dual-partition (vs P4 16MB)
- **2MB PSRAM** — LIMITADO
  - Display sprites: máx ~100KB
  - Evitar buffers grandes (JSON, logs, etc)
  - Profiling crítico (stack vs heap)

### GPIO
- **27 GPIO totales, 2 libres**
  - 25 asignados: RS485, Motor, ADS1115, Display, Encoder, Botones, NeoPixel
  - Expansión muy limitada sin reorganizar hardware

### Alimentación
- **500mA USB limit** compartido entre:
  - MCU ~60mA
  - Motor picos ~150mA (H-bridge)
  - Display backlight ~30mA
  - Botones/NeoPixel ~10mA
  - Margen: muy ajustado, riesgo reset si motor + display simultáneamente

### Serial/Debugging
- **USB OTG nativo** (Type-C)
  - ESP32-S2 tiene USB nativo — no requiere CH340 externo
  - Project usa `-DARDUINO_USB_MODE=0` → UART para programming (desactiva CDC nativo)
  - Velocidad: 115200 baud estándar
  - `Serial.printf()` recomendado (vs log_i/log_e inestables en S2/IDF5)

---

## Pinout (ESP32-S2FN4R2)

| Función | GPIO |
|---------|------|
| **RS485 TX** | 8 |
| **RS485 RX** | 9 |
| **RS485 EN** | 35 |
| **ADS1115 — Canal entrada (A0)** | 10 |
| **ADS1115 — SDA (I2C)** | 21 |
| **ADS1115 — SCL (I2C)** | 34 |
| **ADS1115 — Alert/RDY (ISR)** | 17 |
| **Encoder A** | 13 |
| **Encoder B** | 12 |
| **Encoder BTN** | 11 |
| **Display SCLK (SPI3)** | 7 |
| **Display MOSI (SPI3)** | 4 |
| **Display DC** | 6 |
| **Display CS** | 5 |
| **Display RST** | 33 |
| **Display BL (PWM 500Hz)** | 3 |
| **NeoPixels** | 36 |
| **Botón REC** | 37 |
| **Botón SOLO** | 38 |
| **Botón MUTE** | 39 |
| **Botón SELECT** | 40 |

### Motor (DRV8833 H-bridge)
- **EN (nSLEEP)** = GPIO14
- **IN1** = GPIO18
- **IN2** = GPIO16

### ADS1115 I2C ADC
- **Dirección I2C:** 0x48
- **Gain:** ±4.096V
- **Sample Rate:** 860 SPS
- **Modo:** Continuous (ISR-driven en GPIO17)
- **Resolución:** 16-bit (mejora 6-15× vs ADC nativo)

---

## Display

- **Panel:** ST7789V3 240×280
- **Interface:** SPI3_HOST
- **Frecuencia write:** 10MHz (unidireccional, sin MISO)
- **Config:** `memory_height=320`, `offset_y=20`, `invert=true`, `rgb_order=false`
- **Backlight:** PWM 500Hz, GPIO3
- **Alimentación:** Rail 5V (PCB V2)

---

## Comunicación

**RS485:**
- Baudrate: 500 kbaud
- Protocolo: Binario custom con CRC8
- Ciclo: ~20ms (master S3 controla)
- Timeout: 1000ms sin datos → LED11 rojo, motor suspendido

**I2C (ADS1115):**
- SDA=GPIO21, SCL=GPIO34
- Dirección: 0x48
- ISR Alert en GPIO17 (FALLING, 860 SPS)

---

## Inicialización (Orden Crítico)

**Orden de init en `setup()`:**

```cpp
setup() {
  1. Motor::init()           ← ANTES de Serial.begin() (silencia motor)
  2. Serial.begin()
  3. initNeopixels()
  4. initDisplay()
  5. faderADC.begin()
  6. initHardware()          ← botones + otros GPIO
  7. FaderTouch::init()
  8. Encoder::begin()
  9. ButtonManager::begin()
  10. Motor::begin()         ← habilita control
  11. rs485.begin(trackId)   ← ÚLTIMO (ya todo funcional)
}
```

**¿Por qué importa el orden?**
- **Motor::init()** debe ejecutarse ANTES de `Serial.begin()` para silenciar motor sin debug output
- **RS485** debe iniciar cuando todo está listo (si falla, no bloquea boot)
- **Display + NeoPixels** comparten SPI, timing crítico
- **Encoder** debe inicializarse DESPUÉS de hardware GPIO pero ANTES de RS485 (ISR attachInterrupt)
- **FaderADC** depende de I2C (Wire), debe estar antes que RS485 poll

---

## Control de Fader

**Arquitectura end-to-end:**
```
Logic Pro (PitchBend signed 14-bit)
    ↓
S3 MidiProcessor (mapea a ADC 0-27000)
    ↓
RS485 (20ms ciclo)
    ↓
S2 Motor::setTarget() (dead zone 50 cuentas)
    ↓
DRV8833 posiciona fader
    ↓
FaderADC (ADS1115) realimenta posición
    ↓
S2 responde SlavePacket.faderPos
    ↓
S3 mapea de vuelta a PitchBend
    ↓
Logic recibe feedback
```

**Calibración (no-bloqueante, 3-5s):**
1. S3 ordena FLAG_CALIB vía RS485
2. Motor ejecuta: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN
3. S2 captura min/max ADC y envía 2 paquetes de calibración
4. Guard cooldown: no reinicia si completó hace <2000ms

---

## Build S2

**Platform:** pioarduino 55.03.37 / IDF5 — unificado con P4 (requiere P4)

**Librerías clave:**
- **LovyanGFX 1.2.19** — Display driver ST7789V3 (cambio desde NeoPixelBus)
- **Adafruit NeoPixel** — Control LEDs WS2812B (compatible IDF5)
- **ADS1115 library** — ADC I2C 16-bit
- **Otras:** Wire (I2C moderno), FreeRTOS, ESP-IDF5

**Orden de init obligatorio en código:**
```cpp
// main.cpp setup()
initDisplay()      // LovyanGFX init ANTES
initNeopixels()    // Adafruit NeoPixel DESPUÉS (SPI timing)
initHardware()     // GPIO, botones, etc.
```

**Logging:**
- `Serial.printf()` para S2 (recomendado)
- `log_i()`/`log_w()` no son fiables en S2 (IDF5 limitation)

**Versioning:** `FW_VERSION` y `FW_BUILD_ID` inyectados vía `pre_build.py` en build time

---

## Compilación

### Build con PlatformIO

```bash
cd S2/S2_V1
pio run -e lolin_s2_mini
```

### Configuración PlatformIO

```ini
[env:lolin_s2_mini]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = lolin_s2_mini
board_build.flash_size = 4MB
board_build.arduino.memory_type = qio_qspi
```

**Flags críticos:**
- `-DBOARD_HAS_PSRAM` — Habilita PSRAM (2MB disponibles)
- `-DARDUINO_USB_MODE=0` — Desactiva CDC nativo (S2 SÍ soporta USB OTG, pero el proyecto usa UART para programming)
- `-DCORE_DEBUG_LEVEL=3` — Logging Serial.printf() activo

### Platform y Framework

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)  
**Framework:** Arduino  
**Librerías críticas:**
- LovyanGFX 1.2.19 (display ST7789V3 SPI3)
- Adafruit NeoPixel (LED WS2812B)
- ADS1115 library (fader ADC 16-bit I2C)
- Wire (I2C moderno)
- HardwareSerial (RS485 UART)

**⚠️ Limitaciones S2:**
- Single-core → sin dual-core FreeRTOS (vs P4/S3)
- 4MB Flash → cuidado con SPIFFS/LittleFS (vs 16MB P4/S3)
- 2MB PSRAM → buffers pequeños (evitar grandes arrays)
- Type-C USB OTG nativo → CDC desactivado por config, usa UART para programming
- Serial.printf() recomendado (log_i/log_e inestables en S2/IDF5)

---

## Referencias

- **Fader (ADS1115):** [docs/FADER.md](../docs/FADER.md)
- **Motor (DRV8833):** [docs/MOTOR.md](../docs/MOTOR.md)
- **RS485 (protocolo):** [docs/RS485.md](../docs/RS485.md)
- **Botones:** [docs/BUTTONS.md](../docs/BUTTONS.md)
- **Display (ST7789V3):** [docs/DISPLAY.md](../docs/DISPLAY.md)
- **Encoder (Gray code):** [docs/ENCODER.md](../docs/ENCODER.md)
- **LEDs (NeoPixel):** [docs/LEDS.md](../docs/LEDS.md)
- **SAT (Auto-Test):** [docs/SAT.md](../docs/SAT.md)
- **WiFi/OTA:** [docs/WIFI-OTA.md](../docs/WIFI-OTA.md)
- **Arquitectura general:** [CLAUDE.md](../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../STATUS.md)
- **Historial cambios:** [CHANGELOG.md](../CHANGELOG.md)
