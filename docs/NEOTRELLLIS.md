# NEOTRELLLIS — Matriz 4×8 RGB (iMakie P4) ⚠️ LEGACY

> ⚠️ **[LEGACY — JC4880P433C]**
> NeoTrellis **no existe en la placa activa JC1060P470C**.
> Este documento describe el hardware de la placa antigua. Código fuente: `MASTER_S3-P4/P4_JC4880P433C/`
> Marcado como legacy (2026-06-11).

Documentación del subsistema NeoTrellis P4 **[LEGADO]**. Matriz táctil RGB 4×8 — placa GUITION JC4880P433C.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** ⚠️ LEGACY — Solo aplicable a P4_JC4880P433C

---

## 1. HARDWARE NEOTRELLLIS

### 1.1 Especificación

| Parámetro | Valor |
|-----------|-------|
| **Tipo** | 2× Adafruit NeoTrellis 4×4 |
| **Matriz total** | 4 filas × 8 columnas (32 botones) |
| **LEDs** | RGB addressable (WS2812B compatible) |
| **Interface** | I2C |
| **Direcciones I2C** | 0x2F (izquierda), 0x2E (derecha) |

### 1.2 Topología Física

```
┌─────────────────────────────┐
│  Izquierda (0x2F)│Derecha (0x2E) │
├─────────┼──────────┤
│ [0-3]   │ [4-7]    │  Fila 0
│ [8-11]  │ [12-15]  │  Fila 1
│ [16-19] │ [20-23]  │  Fila 2
│ [24-27] │ [28-31]  │  Fila 3
└─────────┴──────────┘

Índices lineales: 0-31 (o matriz [fila][col])
```

### 1.3 Pinout P4

```
NeoTrellis I2C (I2C_NUM_0):
  SDA = GPIO33
  SCL = GPIO31
  
Dirección izquierda: 0x2F
Dirección derecha:   0x2E
```

---

## 2. INICIALIZACIÓN

### 2.1 Setup I2C y Devices

```cpp
// P4 setup()
Wire.begin(GPIO33, GPIO31);  // I2C_NUM_0: SDA=33, SCL=31

// Inicializar NeoTrellis left
neotrellis_left.begin(0x2F);
neotrellis_left.pixels.setBrightness(255);

// Inicializar NeoTrellis right
neotrellis_right.begin(0x2E);
neotrellis_right.pixels.setBrightness(255);

// Activar eventos botones
for (int i = 0; i < 16; i++) {
    neotrellis_left.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    neotrellis_right.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
}
```

---

## 3. CONTROL DE BOTONES Y LEDS

### 3.1 Leer Presiones

```cpp
// En loop
uint32_t buttons_left = neotrellis_left.readKeys();
uint32_t buttons_right = neotrellis_right.readKeys();

// Decodificar botones presionados
for (int i = 0; i < 16; i++) {
    if (buttons_left & (1 << i)) {
        handleNeoTrellisPress(i, 0);  // Índice 0-15, lado left
    }
    if (buttons_right & (1 << i)) {
        handleNeoTrellisPress(i + 16, 1);  // Índice 16-31, lado right
    }
}
```

### 3.2 Control de LEDs RGB

```cpp
// Encender LED en posición (índice 0-31)
void setNeoTrellisLED(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 16) {
        neotrellis_left.pixels.setPixelColor(index, r, g, b);
        neotrellis_left.pixels.show();
    } else {
        neotrellis_right.pixels.setPixelColor(index - 16, r, g, b);
        neotrellis_right.pixels.show();
    }
}
```

### 3.3 Patrones de LED

```cpp
// Ejemplo: rainbow por fila
for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 8; col++) {
        int index = row * 8 + col;
        uint32_t color = Wheel((index * 256 / 32) & 255);
        setNeoTrellisLED(index, 
                         (color >> 16) & 0xFF,
                         (color >> 8) & 0xFF,
                         color & 0xFF);
    }
}
```

---

## 4. MAPEO A FADERS

### 4.1 Arquitectura

```
Fader 1-9 (Master P4)
    │
    ├─ NeoTrellis [0-8] (fila 0)
    │   RGB color: posición fader
    │   ON/OFF: grabando vs armed
    └─ Presión: mute/solo/select

Fader 10-17 (S3 Extender)
    ├─ NeoTrellis [8-15] (fila 1)

etc.
```

### 4.2 Feedback Visual

```cpp
void updateNeoTrellisFader(int track_idx, uint16_t fader_pos, uint8_t flags) {
    int neoindex = track_idx;  // 0-31 mapping
    
    // Color según posición fader (HSV)
    uint8_t hue = (fader_pos * 256) / 27000;  // 0-255 hue
    uint32_t color = Wheel(hue);
    
    // Brillo según estado
    uint8_t brightness = (flags & FLAG_ARMED) ? 255 : 128;
    
    setNeoTrellisLED(neoindex, 
                     ((color >> 16) & 0xFF),
                     ((color >> 8) & 0xFF),
                     (color & 0xFF));
}
```

---

## 5. TROUBLESHOOTING

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| NeoTrellis no responde | Dirección I2C incorrecta | Verificar 0x2F (left) / 0x2E (right) |
| LEDs no encienden | Pixels no inicializados | Verificar `pixels.begin()` en setup |
| Botones lag | Polling lento | Reducir tiempo loop, usar ISR si disponible |
| LEDs parpadeantes | Voltaje insuficiente | Verificar alimentación 3.3V |
| Colores incorrectos | Orden GRB vs RGB | Verificar librería Adafruit_NeoPixel |
| Solo funciona left o right | I2C bus problem | Verificar pull-ups 10kΩ en SDA/SCL |

### 5.2 Debugging

**I2C Scanner:**
```cpp
void scan_neotrelllis() {
    for (uint8_t addr = 0x20; addr < 0x30; addr++) {
        if (Wire.beginTransmission(addr) == 0) {
            Serial.printf("NeoTrellis en 0x%02X\n", addr);
            Wire.endTransmission();
        }
    }
}
```

**LED Test:**
```cpp
// Encender todos los LEDs en rojo
for (int i = 0; i < 32; i++) {
    setNeoTrellisLED(i, 255, 0, 0);
}
```

---

## 6. REFERENCIAS

- **Adafruit NeoTrellis:** https://learn.adafruit.com/adafruit-neotrellis
- **Adafruit_seesaw:** https://github.com/adafruit/Adafruit_seesaw
- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)
- **LEDS.md** — NeoPixel S2 (similar control RGB)

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado NEOTRELLLIS.md como documento P4-específico, extraído de CLAUDE.md
