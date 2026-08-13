# LEDS — NeoPixels WS2812B (iMakie S2)

Documentación exhaustiva del subsistema de LEDs. Incluye hardware WS2812B, librería Adafruit NeoPixel, estados LED, control, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (12 LEDs RGB, Adafruit NeoPixel)

---

## 1. HARDWARE LEDS

### 1.1 Especificación WS2812B

| Parámetro | Valor |
|-----------|-------|
| **Chip** | WS2812B RGB LED |
| **Cantidad** | 12 unidades |
| **Tipo** | 5050 RGB (common anode) |
| **Tensión** | 5V |
| **Protocolo** | 1-wire (GPIO PWM) |
| **Frecuencia** | 800 kHz (NeoPixel timing) |
| **Colores** | RGB 24-bit (8-8-8) |
| **Brillo máximo** | 255 per canal |

### 1.2 Pinout

| Función | GPIO |
|---------|------|
| **Data (DIN)** | GPIO36 |

**Conexión física:**
```
S2 GPIO36 → WS2812B Data In
          GND → WS2812B GND
          5V  → WS2812B VCC (capacitor 100µF + resistor 470Ω recomendados)
```

### 1.3 Librería

**Adafruit NeoPixel** (cambio desde NeoPixelBus 2.8.4)

**Razón cambio:**
- NeoPixelBus 2.8.4 incompatible con pioarduino 55.03.37 / IDF5
  - Error: `tx_pcm_bypass` no disponible en IDF5
- Adafruit NeoPixel simple, mantenido, compatible IDF5

```cpp
// platformio.ini
lib_deps = adafruit/Adafruit NeoPixel
```

---

## 2. INICIALIZACIÓN Y CONTROL

### 2.1 Una Sola Instancia Global

```cpp
// Neopixel.cpp
#define NEOPIXEL_PIN        36
#define NEOPIXEL_COUNT      12

// Una única instancia global
Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
```

**Crítico:** Una sola instancia. Acceso exclusivamente vía funciones públicas en `Neopixel.cpp`

### 2.2 Funciones API

```cpp
// Neopixel.cpp
void initNeopixels();
void setNeopixelState(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);
void clearAllNeopixels();
void showNeopixels();
void setNeopixelBrightness(uint8_t brightness);
```

### 2.3 Inicialización Boot

```cpp
// main.cpp setup()
initNeopixels();

// Neopixel.cpp
void initNeopixels() {
    neopixels.begin();           // Inicializa I/O
    clearAllNeopixels();
    showNeopixels();
    setNeopixelBrightness(30);   // 30/255 inicial
}
```

---

## 3. ASIGNACIÓN LED POR FUNCIÓN

### 3.1 Mapeo Índice → Función

| Índice | Función | GPIO Botón | Color On | Color Off | Estado |
|--------|---------|-----------|----------|-----------|--------|
| **0** | REC button | 37 | Rojo (255,0,0) | Oscuro (5,0,0) | MIDI feedback |
| **1** | SOLO button | 38 | Amarillo (255,255,0) | Oscuro (5,5,0) | MIDI feedback |
| **2** | MUTE button | 39 | Rojo (255,0,0) | Oscuro (5,0,0) | MIDI feedback |
| **3** | SELECT button | 40 | Blanco tenue (150,150,150) | Oscuro (5,5,5) | MIDI feedback |
| **4-10** | VU meter visual (7 LEDs) | — | Verde/Amarillo/Rojo | Oscuro | ADC level |
| **11** | Status indicator | — | Verde/Naranja/Rojo | Apagado | RS485 health |

### 3.2 Distribución Física

```
[0] REC
[1] SOLO
[2] MUTE
[3] SELECT
[4] [5] [6] [7] [8] [9] [10]  ← VU meter (7 barras)
[11]  ← Status (RS485 health)
```

---

## 4. ESTADOS LED POR FUNCIÓN

### 4.1 Botones (Índices 0-3)

**Origen:** MIDI feedback de Logic Pro

```cpp
// MidiProcessor.cpp - recibe Note On/Off
void processMidiNote(uint8_t note, uint8_t velocity) {
    switch (note) {
        case 0xA0:  // REC
            setNeopixelState(0, 255, 0, 0);  // Rojo si velocity=127
            break;
        case 0xA1:  // SOLO
            setNeopixelState(1, 255, 255, 0);  // Amarillo
            break;
        // ...
    }
}
```

**Estados:**
- **ON (velocity=127):** Color brillante
- **OFF (velocity=0):** Color oscuro (no apagar completamente, ~5% visibilidad)
- **Parpadea:** Semi-active (ej: fader en movimiento pero no grabando)

### 4.2 VU Meter (Índices 4-10)

**Origen:** vuLevel de MasterPacket RS485

```cpp
// updateAllNeopixels() en loop
uint8_t vuLevel = /* lectura RS485 */;  // 0-127

// Mapear a 7 LEDs + color
if (vuLevel > 24) {
    // Rojo: clipping
    for (int i = 4; i <= 10; i++) setNeopixelState(i, 255, 0, 0);
} else if (vuLevel > 12) {
    // Amarillo: fuerte
    for (int i = 4; i <= 10; i++) setNeopixelState(i, 255, 255, 0);
} else {
    // Verde: normal
    for (int i = 4; i <= 10; i++) setNeopixelState(i, 0, 255, 0);
}
```

**Rango:**
- 0-11: Verde (normal)
- 12-23: Amarillo (fuerte)
- 24-127: Rojo (clipping)

### 4.3 Status (Índice 11)

**Origen:** Salud RS485

```cpp
// RS485Handler.cpp
if (lastRxTime < 1000ms ago) {
    setNeopixelState(11, 0, 255, 0);      // Verde: OK
} else if (lastRxTime < 3000ms ago) {
    setNeopixelState(11, 255, 127, 0);    // Naranja: timeout
} else {
    setNeopixelState(11, 255, 0, 0);      // Rojo: desconectado
}
```

**Estados:**
- **Verde:** Última RX <1s
- **Naranja:** Última RX <3s
- **Rojo:** Sin RX >3s o error CRC

---

## 5. ACTUALIZACIÓN LED

### 5.1 Ciclo Principal

```cpp
// main.cpp loop()
updateAllNeopixels();

// Neopixel.cpp
void updateAllNeopixels() {
    // Procesar cambios de estado
    // (botones via MIDI, VU via RS485, status via timeout)
    
    // Mostrar cambios
    showNeopixels();
}
```

**Timing:** ~100µs (no bloquea loop)

### 5.2 SAT Test Mode

```cpp
// SatMenu.cpp - callback LedsTest
static void _satLedsTest(int idx, uint8_t r, uint8_t g, uint8_t b) {
    log_i("[SAT-LED] idx=%d rgb=(%d,%d,%d)", idx, r, g, b);
    setNeopixelState(idx, r, g, b);
    showNeopixels();
}
```

**Acceso desde SAT:** Menú "LEDs Test" para diagnosticar cada LED

---

## 6. BRILLO Y CONFIGURACIÓN

### 6.1 Control Brillo

```cpp
// config.h
#define NEOPIXEL_DEFAULT_BRIGHTNESS 30   // 30/255 (12%)
#define NEOPIXEL_DIM_BRIGHTNESS 5        // 5/255 (2%)
#define NEOPIXEL_ULTRA_DIM 1             // 1/255 (<1%)

// Neopixel.cpp
void setNeopixelBrightness(uint8_t brightness) {
    neopixels.setBrightness(brightness);
    showNeopixels();
}
```

**Rango:** 0-255 (0=apagado, 255=máximo)

### 6.2 SAT Brillo Control

```
SAT menu → Brightness
  ↓
Slider 0-255
  ↓
setNeopixelBrightness()
  ↓
Cambio inmediato visible
```

---

## 7. TROUBLESHOOTING

### 7.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| LEDs no encienden | GPIO36 no configurado o power issue | Verificar initNeopixels() ejecuta |
| Colores incorrectos | Orden GRB vs RGB | Revisar NEO_GRB en constructor |
| Parpadeo | Buffer timing o showNeopixels() llamado demasiado | Reducir frecuencia actualización |
| LED pegado (no responde) | Chip WS2812B defectuoso | Reemplazar LED |
| Todos LEDs encienden | NeoPixel.begin() falló | Check capacitor 100µF + resistor 470Ω |
| Brillo muy débil | setBrightness() bajo | Aumentar valor (SAT Brightness slider) |

### 7.2 Debugging

**Log esperado — boot OK:**
```
[NEOPIXEL] Inicializando WS2812B
[NEOPIXEL] GPIO36 init
[NEOPIXEL] begin() OK
[NEOPIXEL] ✓ 12 LEDs ready
```

**Test SAT:**
```
SAT menu → LEDs Test
  ↓
Índice 0: Rojo
Índice 1: Amarillo
...
Índice 11: Verde (status)
```

**Debug serial:**
```cpp
// Neopixel.cpp
void debugAllLeds() {
    for (int i = 0; i < 12; i++) {
        uint32_t color = neopixels.getPixelColor(i);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        Serial.printf("[LED%d] RGB(%d,%d,%d)\n", i, r, g, b);
    }
}
```

---

## 8. HISTORIA CAMBIOS

### 8.1 2026-05-15: Migración NeoPixelBus → Adafruit NeoPixel

**Problema:** NeoPixelBus 2.8.4 incompatible pioarduino 55.03.37 / IDF5
- Error `tx_pcm_bypass` no disponible

**Fix:** Cambiar a Adafruit NeoPixel
- ✅ Compatible IDF5
- ✅ Simple y mantenido
- ✅ Mismo comportamiento visual

**Commit:** Historial CHANGELOG.md

---

## 9. REFERENCIAS

- **BUTTONS.md** — MIDI feedback → LED color (REC/SOLO/MUTE/SELECT)
- **RS485.md** — vuLevel → VU meter LEDs
- **MOTOR.md** — SAT callback LedsTest
- **DISPLAY.md** — Brillo LED vs display

---

## Últimas Actualizaciones

- **(2026-08-13)** Fix: el patrón "todo azul tenue" (`initNeopixels()`) solo se pintaba una vez, en el boot — `updateAllNeopixels()` no lo repintaba nunca en desconexiones posteriores pese a que `neoWaitingHandshake=true` lo indicaba. Ahora `updateAllNeopixels()` repinta todos los píxeles a azul cuando `neoWaitingHandshake` está activo. Nueva `forceNeopixelRefresh()` para invalidar la caché de cambios (necesaria tras `SatMenu` limpiar los LEDs directamente). Detalle: `CHANGELOG.md` sesión 2026-08-13.
- **(2026-05-16)** Creado LEDS.md como documento exhaustivo, trasladado contenido de CLAUDE.md
