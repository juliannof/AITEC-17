# iMakie — Estado, Bugs y Pendientes

---

## S2 (ESP32-S2)

### **RS485**
**Estado:** funcional pero con pérdida de paquetes

#### Bugs
- **Pérdida de paquetes** — S2 pierde paquetes ocasionalmente, causa timeouts en master. Investigación pendiente.

#### Pendientes
- Identificar causa raíz de pérdida de paquetes (posible: buffer overflow, timing ISR, baudrate stability)
- Verificar pines en P4 (TX=50, RX=51, EN=52) — confirmar compilación y funcionamiento

#### Detalles técnicos
- 500 kbaud, protocolo binario custom, CRC8
- Comunicación master-slave: MasterPacket (16B) → SlavePacket (9B)
- Timing crítico: sendResponse() < 150µs después de recibir
- Timeout 1000ms → LED11 rojo, suspende motor
- RxBuffer: `Serial1.setRxBufferSize()` antes de `Serial1.begin()` — verificar tamaño actual

---

### **NEOPIXEL**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Cambio a Adafruit NeoPixel 3.1.7 (incompatible NeoPixelBus con IDF5)
- Secuencia de brillo (2026-04-28 16:15) — IMPLEMENTADO:
  - Azul tenue (NEOPIXEL_DIM_BRIGHTNESS=5) al inicio/reposo
  - Colores muy tenues (NEOPIXEL_ULTRA_DIM=1) cuando Logic conecta
  - O encendido (NEOPIXEL_DEFAULT_BRIGHTNESS=30) o tenue de morir
  - Optimización monocore: detección de cambios interna sin flags
- HW_STATUS display en boot: 10 componentes color-coded

---

### **BOTONES**
**Estado:** funcional pero lento

#### Bugs
- **Botones perezosos** — Respuesta lenta en botones (REC, SOLO, MUTE, SELECT). Investigar debounce y latencia en ButtonManager.

#### Pendientes
(ninguno)

#### Detalles técnicos
- 4 contactos digitales: REC (GPIO37), SOLO (GPIO38), MUTE (GPIO39), SELECT (GPIO40)
- Debounce 20ms, flanco ascendente (release) → flag `buttonPressed`
- Bits 0-3 en SlavePacket.buttons → MIDI notes al master
- Logic es fuente única de verdad (S2 nunca toggle local)

---

### **FADER**
**Estado:** no funciona

#### Bugs
- **Fader no funciona** — Lectura ADC no responde o valores incorrectos. Investigar FaderADC y configuración ADC1_CH9 (GPIO10).

#### Pendientes
- Investigar causa: ADC API, configuración atenuación, timing lectura
- ADS1015 pedido (no llegó) — reemplazar ADC nativa por I2C ADS1015 (resuelve ruido ±30 cuentas, añade 16-bit)
- ADS1015 pedido — reemplazar ADC nativa por I2C ADS1015 (solución definitiva)

#### Detalles técnicos
- ESP32-S2 ADC1_CH9 (GPIO10), 13-bit (0-8191), atenuación 11dB
- EMA lowpass: alpha=0.20f
- Salida: SlavePacket.faderPos (13-bit) → master interpola 14-bit → Motor sigue

---

### **FADER TOUCH**
**Estado:** EN DESARROLLO (roto con plástico)

#### Bugs
- **No detecta con plástico** — Detección funciona perfecto sin plástico, pero con plástico real no detecta toque. Threshold demasiado bajo o atenuación del plástico no calculada.

#### Pendientes
- Investigar atenuación del plástico y ajustar thresholds
- Posible: aumentar sensitivity, recalibrar baseline con plástico presente
- Validar solución con hardware real

#### Detalles técnicos
- Detección por sostenimiento: raw > baseline×1.015 sostenido > 6 frames (120ms) = TOQUE
- Baseline actualizado siempre con IIR (alpha=1/16, no congelado)
- TEST_TOUCH en SAT testea correctamente

---

### **MOTOR**
**Estado:** no funciona

#### Bugs
- **Motor no funciona** — No responde a comandos. Investigar DRV8833 driver, GPIO control (IN1, IN2, PWM), y secuencia de init.

#### Pendientes
- Verificar GPIO IN1 (GPIO14), IN2 (GPIO16), PWM (GPIO18)
- Investigar init: analogWrite vs ledcAttach/ledcWrite
- Validar calibración secuencia

#### Detalles técnicos
- Driver DRV8833: IN1=GPIO14, IN2=GPIO16 (dirección), PWM=GPIO18 (analogWrite)
- Calibración no-bloqueante: KICK_UP → GOING_UP → SETTLE_UP → KICK_DOWN → GOING_DOWN → SETTLE_DOWN
- Control normal: dead zone (error < 50 cuentas = apagado)
- Si tocado → motor para inmediatamente

---

### **DISPLAY**
**Estado:** funcional con issue de brillo

#### Bugs
- **Brillo máximo al inicio** — Pantalla enciende con brillo máximo en boot (debería ser moderado). Investigar `displaySetBrightness()` y orden de init.

#### Pendientes
(ninguno)

#### Detalles técnicos
- ST7789V3 240×280, SPI3_HOST, freq_write=10MHz (unidireccional, sin MISO)
- Pulso RST manual (GPIO33 LOW 100ms) antes de tft.init()
- Layout: header 40px + main area + VU meter + VPot ring
- Sprites PSRAM: header, mainArea, vuSprite, vPotSprite

---

### **ENCODER**
**Estado:** funcional en SAT, roto en Logic

#### Bugs
- **No funciona en Logic** — Encoder responde en SAT pero no controla VPot en Logic. Bug en RS485 encoding, SlavePacket, o protocolo Mackie.

#### Pendientes
- Investigar encoderDelta en SlavePacket (¿se envía correctamente?)
- Verificar RS485 transmission de delta
- Validar mapping MIDI VPot en Logic (CC 49-56)

#### Detalles técnicos
- Rotario Gray code: A=GPIO12, B=GPIO13, Push=GPIO21
- Debounce 3ms, derecha=+1, izquierda=-1
- Delta acumulado resetea post-buildResponse() (línea 242, post-VPot)
- Rango VPot: -7 a +7 (mapea Display 0-14)
- Resuelto (2026-04-28): reset() ahora post-VPot (antes causaba VPot ring no responda)

---

### **BUILD / PLATFORM**
**Estado:** estable

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Platform: pioarduino 55.03.37 / IDF5 (unificado con P4)
- Libs: LovyanGFX 1.2.19, Adafruit NeoPixel (NOT NeoPixelBus — incompatible IDF5)
- Orden init: Motor::init() → Serial.begin() → Display → NeoPixels → Hardware → RS485.startTask()
- Versioning: FW_VERSION + FW_BUILD_ID inyectados vía pre_build.py

---

### **SAT**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
(ninguno)

#### Detalles técnicos
- Menú en Display (Encoder push > 3s): Motor On/Off, Motor Drive, Brightness, RS485 On/Off, LEDs Test, WiFi OTA, Reboot
- Suspende PSRAM y sprites → libera RAM para diagnósticos
- TEST_TOUCH, TEST_ENCODER funcionales

### Sesiones

#### WiFi OTA (2026-05-04 22:28)
- ElegantOTA funciona perfecto (reemplazo de ArduinoOTA muerto)
- Menu SAT: "WiFi OTA" carga firmware vía WiFi
- Credenciales: configuradas en NVS (namespace `ptxx`)
- Provisioning: sketch USB guarda en NVS namespace `"ptxx"`

---

## S3 (ESP32-S3 Extender)

### **RS485**
**Estado:** funcional con timeouts ocasionales

#### Bugs
- **RS485 intermitente** — **FUNCIONANDO CON TIMEOUTS** — Sistema comunica: LEDs actualizan, datos OK. Timeouts ocasionales e impredecibles (~10-20 consecutivos, luego OK, repite). Comunicación física funciona a 500kbaud. NO modificar arquitectura sin probar compilación first.

#### Pendientes
- Identificar causa raíz de timeouts (posible: timing hardware, timeout 1500µs, ISR conflicts)
- No intentar buffer circular o cambios arquitectónicos sin validación en hardware

#### Detalles técnicos
- Bus B: 500 kbaud, protocolo binario custom, CRC8
- TX=GPIO15, RX=GPIO16, EN=GPIO1 (pinout definitivo)
- Controla 8 slaves S2 (IDs 1–8)
- Timing: TX_EN=30µs, TX_DONE=30µs, RESP_TIMEOUT=3000µs, GAP=300µs, POLL_CYCLE=20ms

---

### **TRANSPORTE**
**Estado:** funcional

#### Bugs
- **Note Off en botones de transporte** — **RESUELTO** — `onButtonReleased` envía `0x80 + note + 0x00`
- **Transport LEDs** — **RESUELTO** — notas 91–95 mapeadas a LEDs físicos en `setLedByNote()`

#### Pendientes
(ninguno)

#### Detalles técnicos
- Botones: REC (GPIO11), PLAY (GPIO9), FF (GPIO7), STOP (GPIO5), RW (GPIO3)
- LEDs: REC (GPIO12), PLAY (GPIO10), FF (GPIO8), STOP (GPIO6), RW (GPIO4)
- Notas MIDI Transporte: RW=0x5B (91), FF=0x5C (92), STOP=0x5D (93), PLAY=0x5E (94), REC=0x5F (95)
- LEDs controlados por `Transporte::setLedByNote()` con velocidad 127 (on) / 0 (off)

---

### **HANDSHAKE MCU**
**Estado:** funcional (protocolo correcto 2026-05-04)

#### Bugs
- **Handshake MCU incorrecto** — **RESUELTO** — código antiguo implementaba challenge/response propio. Ahora protocolo correcto per CLAUDE.md

#### Pendientes
(ninguno)

#### Detalles técnicos
- Familia Mackie: 0x14 (S3 Extender)
- Protocolo: sondeo inicial (cualquier familia) → handshake familia 0x14
- Respuestas: 0x00→0x01, 0x13→0x14
- Surface Type: 0x00 (Master) — hardcodeado, necesario para recibir transport LEDs
- Suscripción: 0x10 00 (feedback subscription)
- Ver CLAUDE.md "Mackie MCU — handshake correcto" para detalles completos

---

### **BUILD / PLATFORM**
**Estado:** estable

#### Bugs
(ninguno)

#### Pendientes
- Verificar pines RS485 (TX=15, RX=16, EN=1) — confirmar compilación y funcionamiento en hardware

#### Detalles técnicos
- Chip: ESP32-S3 (familia Mackie: 0x14)
- Slaves: 8 (IDs 1–8)
- Config.h: independiente del S2 (proyecto separado S3/)
- DEVICE_S3_EXTENDER flag en platformio.ini

---

## P4 (ESP32-P4 Master)

### **RS485**
**Estado:** funcional

#### Bugs
(ninguno)

#### Pendientes
- Verificar pines (TX=50, RX=51, EN=52) — confirmar compilación y funcionamiento

#### Detalles técnicos
- Bus A: 500 kbaud, protocolo binario custom, CRC8
- Controla 9 slaves S2 (IDs 1–9)
- Master MCU — P4 es el controlador central, reemplaza S3 #1 original
- Transceiver externo (no integrado de la placa)

---

### **HANDSHAKE MCU**
**Estado:** funcional (protocolo correcto 2026-05-04)

#### Bugs
- **Handshake MCU incorrecto** — ~~código antiguo implementaba challenge/response propio; P4 actuaba como HOST generando challenges aleatorios.~~ **RESUELTO** — protocolo correcto implementado

#### Pendientes
(ninguno)

#### Detalles técnicos
- Familia Mackie: 0x14 (P4 Master)
- Protocolo: sondeo inicial (cualquier familia) → handshake familia 0x14
- Respuestas: 0x00→0x01, 0x13→0x14
- Surface Type: 0x00 (Master) — hardcodeado
- Suscripción: 0x10 00 (feedback subscription para MIDI notes de transporte)
- Ver CLAUDE.md "Mackie MCU — handshake correcto" para detalles completos

---

### **DISPLAY**
**Estado:** funcional

#### Bugs
- **Pantalla negra en boot** — ~~backlight off + UIOffline label timing.~~ **RESUELTO** — backlight en setup(), label visible desde inicio

#### Pendientes
- Investigar regresión: no muestra datos en Display tras conectarse (posible issue en transición CONNECTED)

#### Detalles técnicos
- **Placa activa JC1060P470C:** JD9165 MIPI-DSI 1024×600 landscape nativo (LVGL v9)
- ~~ST7701S MIPI-DSI 2-lane, 480×800 portrait~~ ← legacy JC4880P433C, ver `MASTER_S3-P4/P4_JC4880P433C/`
- Landscape nativo — no requiere rotación software

---

### **TOUCH**
**Estado:** funcional pero lento en vista de faders

#### Bugs
(ninguno)

#### Pendientes
- Respuesta táctil lenta en vista de faders — investigar qué bloquea el hilo de touch (especialmente en UIPage faders)

#### Detalles técnicos
- GT911 I2C en I2C_NUM_1 (SDA=GPIO7, SCL=GPIO8)
- Detector capacitivo multi-touch
- Issue conocido: latencia en UIPage faders (posible core contentión, mutex contention)

---

### **NEOTRELLS** ⚠️ LEGACY
**Estado:** ⚠️ No existe en placa activa JC1060P470C

#### Notas
- NeoTrellis existía únicamente en la placa antigua JC4880P433C (legacy)
- La placa activa JC1060P470C **no** tiene NeoTrellis — hardware eliminado en migración
- Código legacy: `MASTER_S3-P4/P4_JC4880P433C/`
- Documentación: `docs/NEOTRELLLIS.md` (marcado LEGACY)

---

### **VU LEVELS**
**Estado:** funcional pero sin sincronización thread-safe

#### Bugs
(ninguno)

#### Pendientes
- Implementar mutex o double-buffer en `vuLevels[]` — Core 0 escribe, Core 1 lee en `handleVUMeterDecay` sin protección

#### Detalles técnicos
- Dual-core: taskCore0 / taskCore1
- Flags de cambio de página: `volatile bool g_switchToPage3/3A/3B/Offline`
- Race condition conocido en lectura/escritura de vuLevels

---

### **BUILD / PLATFORM**
**Estado:** estable (LVGL v9)

#### Bugs
- **`uiOfflineCreate()` doble llamada en `setup()`** — ~~leak de LVGL.~~ **RESUELTO** — una sola llamada post-prefs.begin()

#### Pendientes
(ninguno)

#### Detalles técnicos
- Chip: ESP32-P4 (placa activa: GUITION JC1060P470C — display JD9165 1024×600)
- Familia Mackie: 0x14 (9 slaves)
- DEVICE_P4_MASTER flag en platformio.ini
- LVGL v9 (portrait orientation por defecto)

---

## Cross-system

### **RS485 BUSES**
**Estado:** funcional (verificación pendiente)

#### Bugs
(ninguno)

#### Pendientes
- Verificar pines en P4 (TX=50, RX=51, EN=52) — confirmar compilación y funcionamiento
- Verificar pines en S3 (legacy TX=15, RX=16, EN=1) — confirmar compilación y funcionamiento

#### Detalles técnicos
- Bus A (P4 Master): 9 slaves S2 (IDs 1–9)
- Bus B (S3 Extender): 8 slaves S2 (IDs 1–8)
- Ambos buses: 500 kbaud, protocolo binario custom, CRC8
- Topología star: master → slaves, timeouts y retransmisiones

---

### **NEOPIXEL MASTER**
**Estado:** sin implementar

#### Bugs
(ninguno)

#### Pendientes
- Implementar control de brillo centralizado — estudiar si viable vía MIDI (CC o SysEx dedicado)
- Permite controlar brillo de todos los slaves desde Logic/P4

#### Detalles técnicos
- Cada slave S2 tiene 12 × WS2812B (NeoPixels)
- Master debe poder enviar comandos de brillo via MIDI o protocolo RS485
- Posibles enfoques: CC dedicado (CC 7?) o SysEx custom

