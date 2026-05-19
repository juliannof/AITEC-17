# iMakie — ESP32-S3 Extender

Controlador Mackie MCU extendido para Logic Pro. Controla 1 track S2 (ID=1) vía RS485 bus B, y transport buttons (RW/FF/STOP/PLAY/REC) locales.

**Placa de desarrollo:** ESP32-S3-WROOM-1 N16R8  
**Flash:** 16MB (QIO)  
**PSRAM:** 8MB (OPI)  
**Conector:** USB Type-C  
**Pines:** 44 pines  
**Framework:** Arduino IDE / PlatformIO  

**Configuración actual:**
- **Chip:** ESP32-S3  
- **Familia Mackie:** 0x14  
- **Slaves controlados:** 1 (ID=1) en RS485 bus B (NUM_SLAVES=1, ver config.h)  
- **Transport buttons:** 5 (RW/FF/STOP/PLAY/REC)


<img alt="ESP32-S3-DevKitC-1" src="https://naylampmechatronics.com/img/cms/001206/Pinout%20ESP32-S3-DevKitC-1.jpg">


---

## Hardware S3

### Especificación de placa (2026-05-16)

**Módulo:** ESP32-S3-WROOM-1 N16R8 (Xtensa dual-core 240MHz)  
**Flash:** 16MB (QIO mode) — configurado en platformio.ini  
**PSRAM:** 8MB (OPI mode) — disponible via BOARD_HAS_PSRAM flag  
**Conector:** USB Type-C (directo a chip USB, sin regulator adicional)  
**Pines:** 44 pines totales  
**Disponibles para usuario:** ~27 pines GPIO (resto reservados Flash/PSRAM)

**Mapa de memoria:**
| Región | Dirección | Tamaño | Función |
|--------|-----------|--------|----------|
| Bootloader | 0x0 | 256KB | ROM bootloader |
| App | 0x10000 | 15.75MB | Firmware iMakie |
| Flash | 0x0-0xFFFFFF | 16MB | Total |
| PSRAM | Externa | 8MB | Heap dinámico |

**Energía:**
- Voltaje: 3.3V (USB 5V → regulador interno)
- Corriente: ~80mA idle, 160mA full power, picos 200mA
- USB: alimenta placa y periféricos (RS485, NeoPixel)

### Pinout definitivo S3

| Función | GPIO | Tipo |
|---------|------|------|
| **RS485 TX (bus B)** | 15 | UART TX |
| **RS485 RX (bus B)** | 16 | UART RX |
| **RS485 EN (driver enable)** | 1 | GPIO output |
| **NeoPixel (status RGB)** | 48 | WS2812B (Adafruit) |
| **LED REC** | 12 | GPIO output (PWM capable) |
| **BTN REC** | 11 | GPIO input (activo LOW) |
| **LED PLAY** | 10 | GPIO output (PWM capable) |
| **BTN PLAY** | 9 | GPIO input (activo LOW) |
| **LED FF (fast forward)** | 8 | GPIO output (PWM capable) |
| **BTN FF** | 7 | GPIO input (activo LOW) |
| **LED STOP** | 6 | GPIO output (PWM capable) |
| **BTN STOP** | 5 | GPIO input (activo LOW) |
| **LED RW (rewind)** | 4 | GPIO output (PWM capable) |
| **BTN RW** | 3 | GPIO input (activo LOW) |

---

## RS485 (Bus B)

**Especificación:**
- **Baudrate:** 500 kbaud
- **Protocolo:** Binario custom con CRC8 (ver [docs/RS485.md](../../../../docs/RS485.md))
- **Topología:** Star — S3 master, 8 slaves S2 (IDs 1–8)
- **Timing crítico:**
  - TX_EN: 30µs
  - TX_DONE: 30µs
  - RESP_TIMEOUT: 3000µs (3ms)
  - GAP: 300µs entre slaves
  - POLL_CYCLE: ~20ms (master interroga todos los slaves)

**Pinout RS485:**
- TX = GPIO15
- RX = GPIO16
- EN (driver enable) = GPIO1

**Timeout handling:**
- Si slave no responde en 3000µs → retry con FLAG_CALIB
- Si persiste >1s sin datos → marcar NOT_CALIBRATED
- S2 recibe timeout → LED11=rojo, motor suspendido

**Referencia:** [docs/RS485.md](../../../../docs/RS485.md) — especificación completa de protocolo, paquetes, máquina de estados

---

## NeoPixel Status LED (2026-05-16 19:40)

**GPIO:** 48 (WS2812B RGB)  
**Librería:** Adafruit_NeoPixel  

**Estados:**
- **Verde (normal):** Comunicación OK, calibración en progreso
- **Azul (info):** Aguardando conexión Logic
- **Rojo (error):** ✗ FALLO CRÍTICO en calibración — Sistema detenido

**Comportamiento en error calibración:**
- Después de MAX_CALIBRATION_RETRIES (5) timeouts en slave
- NeoPixel brilla rojo fijo
- Log error: `[CALIB] ✗ FALLO CRÍTICO Slave X — comunicación perdida. Sistema DETENIDO.`
- Sistema entra en loop infinito (requiere reset manual)
- Propósito: Alertar operador de fallo de hardware S2

---

## Transport — Botones y LEDs — 📌 Ver docs/Transport.md

**Documentación exhaustiva:**
→ **[docs/Transport.md](../../../../docs/Transport.md)** (pinout, MIDI notas 0x5B-0x5F, flujo bidireccional, handshake Mackie, troubleshooting)

---

## Comunicación

### RS485 Cycle (20ms típicamente)

```
T=0ms:      Master (S3) envía MasterPacket a Slave 1 (ID=1)
T≈150µs:    Slave 1 envía SlavePacket
T=600µs:    Master envía MasterPacket a Slave 2 (ID=2)
T≈750µs:    Slave 2 envía SlavePacket
...
T≈15ms:     Master envía MasterPacket a Slave 8 (ID=8)
T≈15.15ms:  Slave 8 envía SlavePacket
T≈20ms:     Ciclo repite
```

### MasterPacket (16B) → Slave

```cpp
struct MasterPacket {
    uint8_t start_byte;           // 0xAA
    uint8_t slave_id;             // 1-8
    char track_name[8];           // Nombre track
    uint16_t fader_target;        // Target ADC (0-27000)
    uint8_t vu_level;             // VU level
    uint8_t flags;                // CALIB, REC, SOLO, MUTE, SELECT, etc
    uint8_t crc8;                 // CRC8 (poly 0x07)
};
```

### SlavePacket (9B) ← Slave

```cpp
struct SlavePacket {
    uint8_t start_byte;           // 0xBB
    uint8_t slave_id;             // 1-8
    uint16_t fader_pos;           // Posición actual ADC
    uint8_t buttons;              // Bits 0-3: REC/SOLO/MUTE/SELECT
    int8_t encoder_delta;         // Encoder delta desde último ciclo
    uint8_t encoder_button;       // Encoder push button
    uint8_t flags;                // CALIB_DONE, CALIB_SENDING, etc
    uint8_t crc8;                 // CRC8
};
```

---

## Flujo de Trabajo (2026-05-16)

### Setup (main.cpp:205-248)

```
1. USB.begin()                    ← Inicializa USB-MIDI
2. Transporte::begin()            ← Config botones transport (GPIO 3-12)
3. rs485.begin(NUM_SLAVES=1)      ← RS485 bus B, NeoPixel azul (esperando)
4. MIDI.begin()                   ← USB MIDI stack
5. xTaskCreatePinnedToCore(taskCore0)  ← Task MIDI+RS485 responses (Core 0)
6. xTaskCreatePinnedToCore(taskCore1)  ← Task Transporte (Core 1)
7. rs485.startTask()              ← RS485 polling task (Core 1)
8. LED verde 1s (no bloqueante)   ← bootLEDTime
9. loop() → vTaskDelay(portMAX_DELAY)  ← Todo en tareas
```

### Handshake Mackie MCU (Fases 0–2)

**Fase 0: Probe**
```
Logic: SysEx F0 00 00 66 00 00 F7 (cmd=0x00)
S3:    → F0 00 00 66 14 01 00 00 00 01 00 00 00 00 F7
       (responde con device family 0x14)
```

**Fase 2: Keep Alive / Handshake Complete (cmd=0x21)**
```
Logic: SysEx F0 00 00 66 14 21 F7 (cada ~1s mientras conectado)

S3 recibe (MIDIProcessor:436-448):
  1. Envía echo INMEDIATAMENTE: F0 00 00 66 14 21 01 F7
  2. Si (logicConnectionState != CONNECTED):
       logicConnectionState = CONNECTED
       g_logicConnected = 1       ← Activa RS485 polling
       _calibPendingFrom = 1      ← Inicia calibración cascada
```

**Fase desconexión: GoOffline (cmd=0x0F)**
```
Logic: SysEx F0 00 00 66 14 0F F7

S3:
  1. g_logicConnected = 0
  2. rs485.beginDisconnectSequence()  ← Notifica slave DISCONNECTED
  3. g_switchToOffline = true
```

### Task Core 0 — MIDI + RS485 Responses (taskCore0, línea 131-190)

```
Ciclo principal (cada 1ms):

1. LED Boot (no bloqueante) — línea 136-141
   if (bootLEDTime > 0 && millis() - bootLEDTime > 1000)
     pixels.off()

2. Leer USB MIDI → processMidiByte() — línea 143-148
   SysEx, Notes, CC, PitchBend → actualiza state

3. Procesar respuestas RS485 (SOLO si CONNECTED) — línea 150-155
   if (logicConnectionState == CONNECTED):
     for (id=1 to NUM_SLAVES):
       if (rs485.hasNewSlaveData(id)):
         processSlaveResponse(id)  ← Convierte a MIDI OUT
           - Fader → PitchBend
           - Botones → NoteOn/Off
           - Encoder → CC

4. Calibración automática (1 slave a la vez) — línea 165-172
   for (id=1 to NUM_SLAVES):
     if (!calibrated && !calibrating):
       rs485.setCalibrate(id)
       break

5. Tick calibración (timeout handling) — línea 174
   tickCalibracion() → Envía FLAG_CALIB secuencial
```

### Task Core 1 — Transporte (taskCore1, línea 195-200)

```
Ciclo cada 10ms:

Transporte::update()
  for (i=0 to 4):
    buttons[i].loop()
      ├─ onButtonPressed()  → sendNoteOn(MCU_TRANSPORT_NOTES[i])
      └─ onButtonReleased() → sendNoteOff()

Notas transport (config.h):
  RW   = 0x5B
  FF   = 0x5C
  STOP = 0x5D
  PLAY = 0x5E
  REC  = 0x5F
```

### RS485 Polling Task (RS485.cpp:70-147)

```
Máquina de 3 estados (NUM_SLAVES=1 → ~300µs/ciclo):

BusState::SEND (T=0µs)
  └─ Envía MasterPacket a slave X (16 bytes)

BusState::WAIT_RESP (T=100-150µs)
  ├─ Lee SlavePacket (9 bytes)
  ├─ Si OK: _handleResponse() → almacena en _ch[]
  └─ Si timeout > 3000µs: retry contador

BusState::GAP (T=300µs)
  └─ Espera 300µs, pasa a siguiente slave

Timeout handling (RS485.cpp:115-133):
  - Si _consecutiveTimeouts > MAX_CALIBRATION_RETRIES (5):
    NeoPixel = ROJO
    Log: "[CALIB] ✗ FALLO CRÍTICO Slave X"
    while(1) delay(1000)  ← SISTEMA DETENIDO

Bloqueo si NO CONNECTED (RS485.cpp:68-72):
  if (!g_logicConnected):
    vTaskDelay(100)  ← RS485 bloqueado
```

### Procesamiento MIDI Incoming (processMidiByte)

```
Control Change (CC):
  CC 48-55: VPot values → rs485.setVPotValue()
  CC 64-73: Timecode display

Channel Pressure (0xD0):
  Channel 0: Master meter (encodeado)
  Channel 1-7: Strip VU levels → rs485.setVuLevel()

SysEx:
  0x00: Probe → responde family 0x14
  0x0F: GoOffline → desconexión
  0x12: Track names → rs485.setTrackName()
  0x21: Handshake complete → g_logicConnected=1
  0x61: AllFaderstoMinimum
  0x72: VU meters bulk
  0x0E: Auto mode
```

### Conversión RS485 → MIDI OUT (processSlaveResponse)

```
Slave → S3:
  faderPos (ADC 0-27000) 
    → PitchBend (Logic 0-14848)
       if (pb != lastSentPb[id]) → solo si cambió
       msg: 0xE0 + ch, pb_low, pb_high

  buttons (bits 0-3)
    → NoteOn/Off
       note = noteBase[bit] + midiCh
       velocity = 127 (press) / 0 (release)

  encoderDelta (int8_t)
    → CC 16+ch
       if (delta > 0): val 1-62 (CW)
       else: val 64-127 (CCW)
```

### Calibración Automática (main.cpp:165-172 + MIDIProcessor:55-66)

```
Flujo:
  1. Logic envía 0x21 → g_logicConnected=1, _calibPendingFrom=1
  2. taskCore0 loop: busca slave no calibrado
  3. rs485.setCalibrate(id) → Envía FLAG_CALIB=1 a slave
  4. Slave recibe, inicia calibración motor
  5. Slave completa, envía min/max calibrado
  6. Siguiente ciclo: siguiente slave
  7. Repetir hasta NUM_SLAVES

Timeout manejo:
  - RS485 timeout > 5 reintentos → LED rojo + HALT
  - Requiere RESET manual S3
```

---

## Compilación

### Build con PlatformIO

```bash
cd MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER
pio run -e esp32-s3-devkitc-1
```

**Nota:** El board `esp32-s3-devkitc-1` en platformio.ini es compatible con módulos genéricos ESP32-S3-WROOM-1. Ambos usan la misma configuración de particiones (default_16MB.csv) y USB CDC.

### Configuración PlatformIO

```ini
[env:esp32-s3-devkitc-1]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-s3-devkitc-1
board_build.partitions = default_16MB.csv
board_build.flash_size = 16MB
board_build.arduino.memory_type = qio_opi
```

**Flags críticos:**
- `-DBOARD_HAS_PSRAM` — Habilita PSRAM (8MB para N16R8)
- `-DARDUINO_USB_MODE=0` — USB nativo (no JTAG)
- `-DARDUINO_USB_CDC_ON_BOOT=0` — CDC activo en boot
- `-DDEVICE_S3_EXTENDER` — Identifica como S3 Extender (vs P4 Master)

### Platform y Framework

**Platform:** espressif32 (pioarduino 55.03.37 — IDF5 + Arduino core)  
**Framework:** Arduino  
**Librerías estándar:**
- Button2 (LennartHennigs)
- Adafruit NeoPixel
- USBMIDI (integrada en Arduino core)
- Serial (logging con log_i/log_e)

---

## Referencias

- **RS485 Protocol:** [docs/RS485.md](../../../../docs/RS485.md)
- **Mackie MCU:** [docs/SAT.md — Mackie handshake](../../../../docs/SAT.md)
- **Arquitectura general:** [CLAUDE.md](../../../../CLAUDE.md)
- **Estado técnico:** [STATUS.md](../../../../STATUS.md)
- **Master P4:** [MASTER_S3-P4/P4/README.md](../P4/README.md)
- **Slave S2:** [S2/README.md](../../../../S2/README.md)
